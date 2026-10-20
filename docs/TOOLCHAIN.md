```text
Document:    PS4LM Toolchain
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      65c04ef2ba1fe379b34bd38b04d50811b731ad8dea88d78e5454f155215c23e3
Chain:       solana-mainnet
Tx:          E5VhYEtiHGcRm2bHgc1YgBGnTSg7bVNgQA1FDSRGfEvtDs2xkYC8BiWE7qjF5TgB3poiy1Fs22gm1TgR3vZrGMU
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
| elfldr | ps4-payload-dev/elfldr `v0.7` | `~/ps4dev/payloads/elfldr.elf`, sha256 pinned in `scripts/setup-sdk.sh` |
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

## Load path

1. The owner loads the homebrew enabler (GoldHEN on the test console).
2. `scripts/console-setup.sh` sends `elfldr.elf` to the enabler's payload port,
   `PS4_LOADER_PORT`: GoldHEN's BinLoader listens on 9090, HEN VTX on 9020.
3. elfldr listens on 9021 (`PS4_ELFLDR_PORT`).
4. FTP on 2121: GoldHEN runs its own FTP server there. If nothing answers on
   2121, `console-setup.sh` sends `ftpsrv-ps4.elf`.
5. Project ELFs go to 9021 (`scripts/send.sh`). elfldr gives the payload the
   socket as stdout, so its output streams back to the Mac.

elfldr also takes `file:/path/to.elf?args=...` on 9021, the same protocol as
the PS5 elfldr PS5LM's `run.sh` uses, so argv can be passed without a file.
GoldHEN also serves a kernel log on 3232.
