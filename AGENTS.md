```text
Document:    PS4LM Agent Guide
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-21
SHA256:      f74a96172db3e629b30f4ff2f8d22637ff743f35b6d8abeb173a0d7b70bd7f00
Chain:       solana-mainnet
Tx:          2RCvV9xqDvfByCWrQ3nNgParvbhADkWXXCAN64g6mvT1ds2DoLEcB5vCb9oUG4SXrfVRcpgQ7amfSQx8xaxQzqSj
License:     GPL-3.0-or-later
```

# PS4LM agent guide

Rules for AI coding agents (Codex reads this file; Claude Code reads it
through CLAUDE.md). README.md says what the project is; docs/ hold the details.

## Ground rules

- This repo is public (GPL-3.0-or-later). Never push without the owner's OK.
  Never commit `build.md` (it names the owner; it sits in `.git/info/exclude`).
- Work in a git worktree under `.claude/worktrees/` (ignored). Each worktree
  needs its own `git submodule update --init` and its own `build/`.
- No Sony SDK files, keys, firmware, exploit or kernel code, model weights or
  `*.gguf` in git.
- Docs start with the standard header block (see any file in docs/). After
  changing one, run `docseal anchor <files>` before committing.

## One console, one agent

There is one PS4 and it has little memory. Before you run anything on it:

1. Call the `ps4` MCP `status` tool (or `nc -z $PS4_HOST 9021`) to see what is up.
2. If another session is running a payload (a server, a bench, a probe), do not
   start yours. A second payload can kill elfldr, FTP and klog until a reboot.
3. Run `memprobe` first; a model needs ~1.5 GiB free.

Never connect to the loader port (9090, `PS4_LOADER_PORT`): GoldHEN's
BinLoader takes one connection and closes, so even `nc -z` burns it. Only
`scripts/console-setup.sh` uses it.

## Running llama.cpp on the console

- Always `-t 2 --prio -1`: a payload gets 2 CPUs, and normal priority starves
  the system shell.
- Load weights with `-lm none` (b11608 has no `--no-mmap`); mmap'd weights run 13x slower.
- Qwen3.5 thinks by default and burns `max_tokens`: send
  `"chat_template_kwargs": {"enable_thinking": false}`.
- Stop a tool with `scripts/run.sh stop <tool>`, not by killing the Mac side.

## Build traps

- Never pass `CMAKE_C_FLAGS` / `CMAKE_CXX_FLAGS` to CMake: they drop the
  toolchain's `-march` and `-include`.
- llama.cpp's `json` here is `common_json`: no `insert(pos, val)`.
- `scripts/build-llama.sh` pins llama.cpp b11608 and applies `patches/`.
  docs/PORTING.md lists every fix; add new ones there.

## Agent tools

- `ps4` MCP server (`scripts/ps4_mcp.py`, needs `uv`): status, setup, FTP,
  run jobs, kernel log, Mac logs. Claude Code loads it from `.mcp.json`, Codex
  from `.codex/config.toml` (trust the project in Codex first).
- Bigger brain (`scripts/brain.py`): an OpenAI-style endpoint on the Mac that
  answers with Claude (Claude Agent SDK, uses your `claude` login) or Codex
  (`codex exec`, uses your `codex login`). No API keys. See README.md.

## Where things are

- `scripts/`: Mac side (env, setup, run, send, logs, bench, MCP, brain).
- `probes/`: small payloads that measure the console.
- `ps4/`: CMake toolchain and the compat layer linked into every tool.
- `patches/`: llama.cpp patches, applied in order by `build-llama.sh`.
- `prompts/`: system prompts for the console model and the bigger brain.
- `docs/`: roadmap, toolchain, console, porting, models, benchmarks.

<!-- graft:start -->
## Graft — repo context graph

This repo is indexed in `graft/`: small linked markdown nodes that explain each
system and carry exact file:line spans, kept in sync with the code through git.

For ANY task here — understanding how something works, finding where code lives,
or scoping a change — get context from the graph before grepping or opening
source files. Re-ask freely (it's cheap) and reuse literal identifiers you
already have (symbol, error string, file name) as the query. New to this repo?
Run `graft map` first — a token-budgeted orientation (dir clusters, hubs,
hotspots), no LLM, no key.

- Run `graft ask "<your question>" --source` → ranked nodes with the relevant
  code spans inlined (each hit's ≤8-line crux by default; `--full` for whole
  definitions when the crux isn't enough). Match the tool to the task shape:
  for understanding or editing, the top node IS the answer — cite its
  `covers:` file:line spans and edit straight from `--source`. For
  exhaustive tasks ("every occurrence / every caller of this pattern"), ranked
  results are top-N, not complete — run `graft grep "<literal>"` instead
  (exhaustive over indexed files, grouped by enclosing symbol), falling back
  to raw `grep -rn` only for unindexed files.
- `graft skeleton <file>` → every definition's signature + span, ~10× cheaper
  than reading the file; use it to skim an API surface.
- `graft callers <symbol>` gives precomputed, exact edges — who calls this.
  Add `--direction out` for what it calls, or `--depth N` to walk
  transitively for the full blast radius. For structural questions, skip
  ranking and use this directly.
- Or browse: `graft/INDEX.md` lists every node; follow the links.
- Monorepos and folders of multiple repos rank fairly across sub-projects —
  hits carry `[scope/]` labels naming which one they're from. Narrow with
  `graft ask "<task>" --in <scope>/` once you know where you're working.

If a returned span is truncated ("+N more lines"), open the file at that exact
range before finalizing. Only open source files when a node genuinely lacks a
needed detail, and then at the exact file:line the node points to — never
re-read whole files.

After big code changes, refresh the graph with `graft build` (deterministic,
no API key, $0).
<!-- graft:end -->
