```text
Document:    PS4LM Roadmap
Version:     v1.5.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-21
SHA256:      f18a023b1876e9706a8616b89a52cea673fcc26547c16b215146409a21a8f2d7
Chain:       solana-mainnet
Tx:          2A8iv3hkGfwadXEKasbEu98UTwyxFxxsdRQZHyciDZwpSHS6BJUFosEdATvSazg6LuQJG9WJPJGk3UygefFubgUV
License:     GPL-3.0-or-later
```

# Roadmap

Mission: a Mac browser chats with a model running on the PS4's CPU, served by
`llama-server` over the home network. Anything that does not move toward that
is out of scope (no GPU backend, no PKG or UI before Phase 6, no in-app
downloads, no TLS).

| Phase | What | Timebox | Status |
|---|---|---|---|
| 0 | Toolchain and console probe | 90 min | Done 2026-10-20: 2 CPUs, ~3.5 GiB, see CONSOLE.md |
| 1 | Plumbing: send, logs, run, FTP upload | 60 min | Done 2026-10-20: `scripts/run.sh hello` builds, uploads and runs in 0.3 s |
| 2 | Build llama.cpp for Jaguar | 2 h | Done 2026-10-20: three tools link, imports checked; see PORTING.md |
| 3 | CPU safety patches and thread sweep | 90 min | Done 2026-10-20: safe max 2 threads with `--prio -1`, see BENCH.md |
| 4 | First model, web UI from the Mac | 90 min | Done 2026-10-21: Qwen3.5 0.8B chats from the Mac, long chats to 4096 context, see BENCH.md |
| 5 | Benchmarks and docs, fresh-clone check | 60 min | Not started |
| 6 | Optional: auto-start, PKG, model picker | | Started early 2026-10-21 (owner OK): app, `serve.sh` and art built on the Mac; console checks open, see APP.md |

## Phase 0 acceptance

- memprobe output saved verbatim in [CONSOLE.md](CONSOLE.md).
- Toolchain decision in [TOOLCHAIN.md](TOOLCHAIN.md).
- Console model recorded in [CONSOLE.md](CONSOLE.md).

## Phase 1 acceptance

- `scripts/run.sh hello` builds, uploads over FTP to `/data/PS4LM/bin`, runs,
  and the output shows up in `scripts/logs.sh` and in
  `/data/PS4LM/logs/hello-<unix-time>.log`. Checked on the console 2026-10-20.
- argv goes through elfldr's `file:` URI (see [TOOLCHAIN.md](TOOLCHAIN.md)),
  so the `/data/PS4LM/args/<tool>.txt` fallback is not needed.

## Phase 2 acceptance

- `llama-server`, `llama-bench` and `llama-cli` link (`scripts/build-llama.sh`).
- Import lists in [PORTING.md](PORTING.md); every import is exported by an SDK
  stub, and none is unexpected.
- Beyond the acceptance: `llama-cli --version` and `llama-server --version`
  ran on the console. Then every C++ tool crashed before `main` in Sony's
  `mbtowc`; the replacement (PORTING.md 11) is built but not yet run, because
  the console's homebrew services went down. First step of Phase 3: reload
  GoldHEN, run each tool with `--version`, and `llama-bench --help`.

## Phase 3 acceptance

- The Phase 2 crash is gone: every tool prints `--version`, and
  `llama-bench --help` and `--list-devices` run (PORTING.md 11).
- `patches/0001` (default threads = allowed CPUs minus 2, at least 1;
  `--poll 0`) and `patches/0002` (affinity per worker thread, priority only
  lowered) are applied by `build-llama.sh`. Checked on the console: defaults
  print as 1 thread and poll 0; `-C 0xC0 --cpu-strict 1 --prio -1` runs at
  priority 767 on CPUs 6 and 7; `--prio 2` warns and keeps normal.
- `scripts/bench.sh`: three clean runs at 1 and 2 threads. Safe max 2, used
  with `--prio -1` (the shell's heartbeat lapsed at normal priority). Table in
  [BENCH.md](BENCH.md).
- A payload cannot widen its CPU mask (`probes/cpuprobe`, EPERM), so 2 CPUs
  is the ceiling without kernel changes, which are out of scope.
- For Phase 4: load weights with `-lm none` (mmap'd weights generate 13x
  slower, PORTING.md 15), launch with `-t 2 --prio -1`, and watch swap: a
  ~1 GB payload already pushed about 200 MB of the shell to swap.

## Phase 4 acceptance

- Qwen3.5 0.8B Q4_K_M runs in `llama-server` on the console; the web UI and
  curl from the Mac got answers (2026-10-20, `-c 1024`).
- The server lives through the Mac hanging up (PORTING.md 16) and stops
  cleanly with `scripts/run.sh stop llama-server` (PORTING.md 17).
- Fractional JSON numbers no longer abort it (PORTING.md 18).
- Long chats at `-c 2048` and `-c 4096` with `-np 1`: numbers in
  [BENCH.md](BENCH.md), settings in [MODELS.md](MODELS.md).

## Stop and ask

- Neither toolchain option builds C++17 llama.cpp.
- The console shuts down twice at the same settings.
- Phase 0 shows less than 1.5 GB usable memory.
- Any step looks like it needs system-level or kernel changes.
- A timebox runs over.
