```text
Document:    PS4LM: llama.cpp on a PS4
Version:     v1.4.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-21
SHA256:      e7b2c75ab738da81f55453851fb5a6cf7afa6ceac09bf4c17bc5cc3ca9fbacb5
Chain:       solana-mainnet
Tx:          54FAqzxrgE6rhR5LjW3rHcQ9QRrp9QPqMPBXm5oMtXNLhxmto6XpzrLaTxqoKueZn9oFxK2MWsKEHFVW1HjDFSiD
License:     GPL-3.0-or-later
```

# PS4LM

llama.cpp as a homebrew app on your own PS4. A small GGUF model runs on the
console's Jaguar CPU (the two of its eight cores a payload may use), and `llama-server` serves it to your home
network, so a browser on your Mac can chat with a model running on the PS4.

CPU only, one console, one interface. It follows the layout and fixes of
[PS5LM](https://github.com/cobanov/PS5LM), which does the same on a PS5.

## Status

Phase 3 done: llama.cpp builds for the PS4, and `llama-bench` runs Qwen3 0.6B
on the console at about 14 tokens/s reading and 6 tokens/s writing (2
threads). Next is Phase 4, `llama-server` with the web UI. See
[docs/ROADMAP.md](docs/ROADMAP.md).

## What you need

- A PS4 you own, with a homebrew enabler (GoldHEN, HEN VTX or similar) already
  loaded. Enabling homebrew is your step, done with existing public tools
  before each session. This repo contains no jailbreak, exploit or kernel code.
- Tested target: firmware 11.02, base PS4, GoldHEN.
- An Apple Silicon Mac with Homebrew: `brew install llvm@18 cmake ninja socat wget`.

## Quick start

```sh
scripts/setup-sdk.sh                       # PS4 payload SDK + libc++ into ~/ps4dev/sdk
echo 'PS4_HOST=192.168.1.50' > scripts/env.local.sh   # your console's IP
source scripts/env.sh
make -C probes                             # build/probes/hello.elf, memprobe.elf

scripts/console-setup.sh                   # elfldr on 9021 (through GoldHEN's 9090)
scripts/logs.sh                            # in a second terminal
scripts/run.sh hello "two words"           # build, upload over FTP, run with args
scripts/run.sh memprobe
scripts/send.sh build/probes/hello.elf     # or send an ELF straight to elfldr

git submodule update --init                # llama.cpp b11608
scripts/build-llama.sh                     # build/llama-ps4/bin/llama-{server,bench,cli}.elf
curl -T model.gguf ftp://$PS4_HOST:2121/data/PS4LM/models/model.gguf
scripts/bench.sh /data/PS4LM/models/model.gguf   # thread sweep, docs/BENCH.md
```

On the console, run the tools with `-t 2 --prio -1` and without mmap
(`--no-mmap`, or `-lm none` for llama-bench); docs/BENCH.md says why.

Settings live in `scripts/env.sh`. `PS4_LOADER_PORT` is the enabler's payload
port: 9090 for GoldHEN (the default), 9020 for HEN VTX.

## Agent access (MCP)

`scripts/ps4_mcp.py` is an MCP server that gives an AI agent the same reach:
console status, `console-setup.sh`, FTP list/read/upload, `run.sh` (long runs
become jobs to poll or stop), the kernel log on 3232 and the Mac log listener.
It needs [uv](https://docs.astral.sh/uv/); `.mcp.json` registers it for Claude
Code in this folder. Other clients: `uv run --script scripts/ps4_mcp.py` over
stdio. It never touches the loader port, which GoldHEN closes after one
connection.

## App on the home screen

An optional launcher app puts PS4LM on the home screen: pick a model, press X,
and the TV shows the chat address and a QR code. It starts the same server
payload; you still chat from a phone or laptop. Built on the Mac, not yet
tried on the console. See [App](docs/APP.md).

## Docs

[Roadmap](docs/ROADMAP.md) · [Toolchain](docs/TOOLCHAIN.md) ·
[Console](docs/CONSOLE.md) · [Porting](docs/PORTING.md) ·
[Models](docs/MODELS.md) · [Benchmarks](docs/BENCH.md) · [App](docs/APP.md)

## Credits

- [llama.cpp and ggml](https://github.com/ggml-org/llama.cpp), which do the actual work.
- [PS5LM](https://github.com/cobanov/PS5LM) by cobanov, whose layout, scripts
  and fixes this port follows.
- [John Törnblom](https://github.com/ps4-payload-dev) for the PS4 payload SDK
  and elfldr, and [ftpsrv](https://github.com/ps5-payload-dev/ftpsrv).
- [GoldHEN](https://github.com/GoldHEN/GoldHEN), the enabler this was tested with.

## Licence

GPL-3.0-or-later, see [LICENSE](LICENSE). llama.cpp keeps its MIT licence. No
Sony SDK files, keys, firmware or model weights are in this repository.

PS4LM is not affiliated with Sony Interactive Entertainment. "PlayStation" and
"PS4" are trademarks of Sony Interactive Entertainment.
