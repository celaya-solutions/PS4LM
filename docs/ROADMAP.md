```text
Document:    PS4LM Roadmap
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      7b29d60717a09acc5d90f6842ed0efd313d7164f671cee4874cec3847186a673
Chain:       solana-mainnet
Tx:          398jkFqT135p5gzEo1wLaeYuEE4RRmB5H241axpwbKiD8mvgfEm7J4DviiYaqFE4s8ft15pjZUqugoPzJSBg8wSy
License:     GPL-3.0-or-later
```

# Roadmap

Mission: a Mac browser chats with a model running on the PS4's CPU, served by
`llama-server` over the home network. Anything that does not move toward that
is out of scope (no GPU backend, no PKG or UI before Phase 6, no in-app
downloads, no TLS).

| Phase | What | Timebox | Status |
|---|---|---|---|
| 0 | Toolchain and console probe | 90 min | In progress: SDK and probes built; console run pending |
| 1 | Plumbing: send, logs, run, FTP upload | 60 min | Not started |
| 2 | Build llama.cpp for Jaguar | 2 h | Not started |
| 3 | CPU safety patches and thread sweep | 90 min | Not started |
| 4 | First model, web UI from the Mac | 90 min | Not started |
| 5 | Benchmarks and docs, fresh-clone check | 60 min | Not started |
| 6 | Optional: auto-start, PKG, model picker | | Only after Phase 5 |

## Phase 0 acceptance

- memprobe output saved verbatim in [CONSOLE.md](CONSOLE.md).
- Toolchain decision in [TOOLCHAIN.md](TOOLCHAIN.md).
- Console model recorded in [CONSOLE.md](CONSOLE.md).

## Stop and ask

- Neither toolchain option builds C++17 llama.cpp.
- The console shuts down twice at the same settings.
- Phase 0 shows less than 1.5 GB usable memory.
- Any step looks like it needs system-level or kernel changes.
- A timebox runs over.
