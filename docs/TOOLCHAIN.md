```text
Document:    PS4LM Toolchain
Version:     v1.3.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      e6b87f4ffad31224c359b267775d7b0353f11a35f9868dbe4769f1767db9006c
Chain:       solana-mainnet
Tx:          3hxGVobZL92ebZgqmh5Rn7kDwbgBTy1CBpafAFKLAtQRiKGa2YXxXP89EEvjvMRbi344cXKpiGtDURt8jxKJxgDq
License:     GPL-3.0-or-later
```

# Toolchain

## Decision: Option A, the PS4 payload SDK

[ps4-payload-dev/sdk](https://github.com/ps4-payload-dev/sdk) by John Törnblom,
tag `v0.9`, built from source by `scripts/setup-sdk.sh` into `~/ps4dev/sdk`.
It is the same author and the same model as the PS5 SDK that PS5LM uses: ELF
payloads sent over TCP to elfldr, dynamically linked against stubs of the
console's own libraries (`libkernel`, `libSceLibcInternal`, `libSceNet`, ...).
The SDK's README lists firmware 11.02 as confirmed working.

Option B (OpenOrbis) is not needed: Option A builds C++17 with libc++.

## Versions

| Part | Version | Notes |
|---|---|---|
| Host compiler | Homebrew `llvm@18` 18.1.8 | The SDK documents llvm@18 on macOS and its macOS CI uses it. `LLVM_CONFIG` points at it (`scripts/env.sh`). |
| Linker | `ld.lld` 18 from the `llvm@18` keg | The separate `lld` formula (23.x) is not used. |
| SDK | ps4-payload-dev/sdk `v0.9` | `make DESTDIR=~/ps4dev/sdk install` |
| C++ runtime | libc++, libc++abi, libunwind 15.0.7, static | Built by the SDK's own `libcxx.sh`. C++17 including `std::thread` and `std::optional` builds and links (`probes/memprobe`). |
| elfldr | ps4-payload-dev/elfldr `v0.7` | `~/ps4dev/payloads/elfldr.bin`, sha256 pinned in `scripts/setup-sdk.sh` |
| ftpsrv | ps5-payload-dev/ftpsrv `v0.21.1` | `ftpsrv-ps4.elf`, only used when nothing listens on 2121 |
| Host tools | cmake, ninja, socat, wget | wget is needed by the SDK's `libcxx.sh` |

A full SDK install takes about 90 seconds on an M5.

## Compiler target

`orbis-clang` and `orbis-clang++` compile for `x86_64-scie-ps4` with the SDK as
sysroot. The target defines `__ORBIS__`, `__SCE__` and `__FreeBSD__ 9`, not
`__linux__`. Without `-march` it builds generic x86-64 (`__tune_k8__`, no AVX).
With `-march=btver2` it defines `__AVX__`, `__F16C__`, `__BMI__` and
`__SSE4_2__`, and not `__AVX2__`, `__FMA__` or `__BMI2__`, which matches Jaguar.
Every PS4LM build passes `-march=btver2 -mtune=btver2`.

Programs are position independent executables. `orbis-clang` links
`-lkernel_web` by default; pass `-lkernel_sys` for the functions only that stub
exports (`statfs`, `sceKernelGetHwModelName`).

## Building llama.cpp

`scripts/build-llama.sh` builds `llama-server`, `llama-bench` and `llama-cli`
into `build/llama-ps4/bin/<tool>.elf` (about 30 s from clean on an M5).

- `third_party/llama.cpp` is a shallow submodule at tag `b11608`, PS5LM's pin
  today. The script checks the commit and passes the build number, so
  `--version` and the embedded web UI match the tag.
- `ps4/CMakeLists.txt` is the top-level project: llama.cpp as a subdirectory,
  plus `ps4/compat/compat.c` and `log.c` in every tool, linked against
  `libkernel_sys`.
- `ps4/toolchain.cmake` includes the SDK's `orbis.cmake` and adds
  `-march=btver2 -mtune=btver2 -DPS4LM=1` and `-include ps4/compat/compat.h`.
- The script checks every CMake option against the pinned `CMakeLists.txt`,
  fails if CMake ignored one, and fails if a tool imports a symbol no SDK stub
  exports.

Every workaround is in [PORTING.md](PORTING.md).

## Load path

1. The owner loads the homebrew enabler (GoldHEN on the test console).
2. `scripts/console-setup.sh` sends `elfldr.bin` to the enabler's payload port,
   `PS4_LOADER_PORT`: GoldHEN's BinLoader listens on 9090, HEN VTX on 9020.
   The `.bin` build, because GoldHEN's BinLoader rejects `elfldr.elf`
   (`Error handling payload` in klog). The BinLoader takes one connection and
   then stops listening, so the script never tests 9090: it sends and retries.
3. elfldr listens on 9021 (`PS4_ELFLDR_PORT`).
4. FTP on 2121: GoldHEN runs its own FTP server there. If nothing answers on
   2121, `console-setup.sh` sends `ftpsrv-ps4.elf`.
5. Project ELFs go to 9021 (`scripts/send.sh`). elfldr gives the payload the
   socket as stdout, so its output streams back to the Mac.

GoldHEN also serves a kernel log on 3232.

## Running a tool with arguments

`scripts/run.sh TOOL [ARGS...]` builds a probe (or takes a built tool), uploads
it with curl to `/data/PS4LM/bin/` over FTP, then sends elfldr one line:
`file:/data/PS4LM/bin/TOOL.elf?args=...`. elfldr v0.7 reads the file and passes
the args as argv; argv[0] is the file name. This is the path that works, so no
args file is used. The rules, from elfldr's `socksrv.c` and `uri.c`:

- Args are split on spaces. A backslash keeps the next character, so `run.sh`
  writes a space inside an argument as `\ ` and a backslash as `\\`.
- `%`, `&`, `#` and control characters are percent-encoded. elfldr's decoder
  skips a `%XX` in the last three characters, so `run.sh` then adds a trailing
  backslash, which elfldr drops.
- The whole line must fit in 1024 bytes. Longer prompts need a file.

Checked on the console with `run.sh hello "two words" 'back\slash' 'a&b#c' 50%`.

## Logs

Tools link `ps4/compat/log.c`. At startup it points stdout and stderr at a pipe,
and a thread copies every byte to elfldr's socket (the terminal running
`send.sh` or `run.sh`), to the Mac's `scripts/logs.sh`, and to
`/data/PS4LM/logs/<tool>-<unix-time>.log`. The Mac address comes from
`/data/PS4LM/loghost.txt`, which `run.sh` rewrites on every run; without that
file, from the address baked in at build time. If the Mac does not answer within
a second, the file still gets everything. `send.sh` does not touch
`loghost.txt`, so it uses whatever the last `run.sh` wrote.
