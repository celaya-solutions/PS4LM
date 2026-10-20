```text
Document:    PS4LM Roadmap
Version:     v1.3.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      8e4e14a2e9e2542e8a0b0890249cdccc93dc7a7c755f45f88963b7e61bde3eaf
Chain:       solana-mainnet
Tx:          2uvvWSrRSz9pYW5QGr5goxsEPkwFbQVpNrGLs3h2ssar2X7kaaiFc58HEnXvTs4m6Y39LBHuXp3jNhAopcRPkRQP
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
| 3 | CPU safety patches and thread sweep | 90 min | Not started |
| 4 | First model, web UI from the Mac | 90 min | Not started |
| 5 | Benchmarks and docs, fresh-clone check | 60 min | Not started |
| 6 | Optional: auto-start, PKG, model picker | | Only after Phase 5 |

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

## Stop and ask

- Neither toolchain option builds C++17 llama.cpp.
- The console shuts down twice at the same settings.
- Phase 0 shows less than 1.5 GB usable memory.
- Any step looks like it needs system-level or kernel changes.
- A timebox runs over.
