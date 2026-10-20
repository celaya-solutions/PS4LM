```text
Document:    PS4LM Roadmap
Version:     v1.2.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      a4c943e5db4956c6e117df1b891304f6cba0fbe8cc53557b6fe2a277f4c6fc87
Chain:       solana-mainnet
Tx:          4JoeDakFn98mNX15VwmvDowwh1Dt55C7LjzKXMjNeXV1sUR1wuBivS4ASimBgAHwF7LBRPtWuoHW4Xngyz7Vsz36
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
| 2 | Build llama.cpp for Jaguar | 2 h | Not started |
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

## Stop and ask

- Neither toolchain option builds C++17 llama.cpp.
- The console shuts down twice at the same settings.
- Phase 0 shows less than 1.5 GB usable memory.
- Any step looks like it needs system-level or kernel changes.
- A timebox runs over.
