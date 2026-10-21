```text
Document:    PS4LM Home-Screen App
Version:     v1.4.1
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-21
SHA256:      55316009c4fb846c3d64ba10a6452a386dfac4aba2032e62b3d22dad3a15c502
Chain:       solana-mainnet
Tx:          2psVFeboHENMaDvKTUqVqJFHwgX93SL7nFX6xcspKYR1kX3q64JYqnNk6mcu4LeficoDhUiLbMAwL5SHStdP7gGV
License:     GPL-3.0-or-later
```

# Home-screen app

PS4LM can sit on the PS4 home screen as an app with its own icon. Open it, pick
a model, press X, and the TV shows the chat address and a QR code. Scan the
code with a phone on the same network to chat.

**Status (2026-10-21):** installed and working on the console. Walked
through on the TV: Stopped with the model list, X to Loading to Ready with the
address and QR code, Busy while a chat ran, O to stop. The server's own
checks, also on the console, with `--no-jinja` (see below):

- `scripts/serve.sh Qwen3.5-0.8B-Q4_K_M.gguf 1024 -- --no-jinja` started the
  server and printed the chat address in 13 s. The server kept running after
  the script hung up on elfldr, and answered a chat.
- `/slots` answers at llama.cpp b11608 and reports `is_processing`, so Busy
  works.
- The app's stop line (`stop.elf` with `llama-server`) stopped the server in
  about 1 s; elfldr stayed up.

**Fixed:** llama-server died starting with jinja (chat templates) until
2026-10-21: Sony's `localtime` fails in a payload (PORTING 19). With the fix,
`--no-jinja` is no longer needed. `extra` in `server.conf` holds
`-sysf /data/PS4LM/prompts/ps4lm-system.txt` (`serve.sh` writes it); keep it,
or the server starts without the system prompt and the model introduces itself
as Qwen.

## What it is and is not

- It is a launcher. The model runs in the same `llama-server.elf` payload that
  `scripts/run.sh` starts. The app sends elfldr (127.0.0.1:9021) the same
  launch line, then watches `http://127.0.0.1:8080/health` and `/slots`.
- It is not a chat screen on the TV. You chat on a phone or laptop.
- It needs GoldHEN and elfldr loaded, like everything else here.

## One-time Mac step

```sh
scripts/serve.sh Qwen3.5-0.8B-Q4_K_M.gguf
```

This puts `llama-server.elf` and `stop.elf` in `/data/PS4LM/bin`, the system
prompt in `/data/PS4LM/prompts`, writes `/data/PS4LM/server.conf` (with `-sysf`
in `extra`, so the X button starts the server with the prompt), starts the
server and prints the chat address. Without those two programs, X and O in the
app do nothing.

`llama-server.elf` must include the SIGPIPE fix in `ps4/compat/log.c`
(branch `phase-4`): the app hangs up on elfldr after the launch line, so
nothing reads the server's output. Without the fix, the server dies on its
first log line. `stop.elf` comes from `probes/stop` on the same branch.

## Install

1. `scripts/build-app.sh /Volumes/<USB>` builds `build/app/PSLM00001.pkg` and
   copies it to the USB drive.
2. On the PS4: GoldHEN > Debug Settings > Package Installer, source USB,
   install PS4LM.
3. Open PS4LM from the home screen.

Uninstall: Settings > Storage > Applications, delete PS4LM.

## Screens

| State | TV text |
|---|---|
| No network | No network. Connect the PS4 to the network. |
| No loader | Loader not running. Load GoldHEN, then run scripts/console-setup.sh on the Mac. |
| No model | No model in /data/PS4LM/models. Run scripts/fetch-model.sh on the Mac. |
| Stopped | Ready to start. Press X. |
| Loading | Loading the model... |
| Ready | Chat is open. Scan the code or type the address. |
| Busy | Writing a reply... |
| Failed | Server stopped. See /data/PS4LM/logs. |

Buttons: Up/Down picks a model (when stopped), X starts, O stops. Close the
app with the PS button; OPTIONS only says so. The app never returns from
`main`: an installed app that does dies with SIGSYS. Closing leaves the server
running.

If a server is already running (for example, started by `serve.sh`), the app
shows Ready and does not start a second one. A load that has not answered
after 120 s shows Failed.

## Build

See [Toolchain](TOOLCHAIN.md#openorbis-app-only). `make -C app test` runs the
plain-C tests (config, launch lines, network, state) on the Mac.
`PROBE=1 scripts/build-app.sh` builds a probe app instead, which shows and
writes to `/data/PS4LM/app-probe.txt` what an installed app may do: CPUs,
memory, ports 9021 and 8080, the console IP, and whether it can read and write
`/data/PS4LM`.

## Still to check on the console

1. Done 2026-10-21: the probe app reaches 127.0.0.1:9021 and reads and writes
   `/data/PS4LM` (facts in [Console](CONSOLE.md#app-process)).
2. Done 2026-10-21: launcher walk-through (see Status).
3. Icon and background on the home screen.
5. The app lets you pick a model too big for the console (Gemma 4 E2B froze it
   with swap near 900 MB). It needs a size warning or Gemma's own flags.
6. `stop.elf` now sends SIGKILL 10 s after SIGTERM: llama-server once hung in
   its exit path ("cleaning up before exit...") holding its memory. The
   SIGKILL branch has not fired on the console yet.
4. Done 2026-10-21: the jinja start crash is fixed (PORTING 19).
