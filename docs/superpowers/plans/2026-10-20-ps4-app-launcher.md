# PS4LM App and Launcher Icon Implementation Plan

```text
Document:    PS4LM App and Launcher Icon Implementation Plan
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      077cb287c53a15b4c427e3b2526ea13ca8bb14654f922c497dca72324b1c8a23
Chain:       solana-mainnet
Tx:          kCuhjtABnQ5CGZf5gd74cYGxdJeYtuskhgjNvLEQSw2JYE7EBZDBF1XbJ1LR9fwGWsxRtc7RXKBPQd67eivggUi
License:     GPL-3.0-or-later
```

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A PS4LM app with its own icon on the PS4 home screen. Opening it starts llama-server, shows the chat address and a QR code on the TV, lets the owner pick a model with the controller, and stops the server.

**Architecture:** A small OpenOrbis app (fake PKG, title PSLM00001) is a launcher and status screen only. It does not contain llama.cpp. It starts and stops the existing payload tools by sending one launch line to elfldr on 127.0.0.1:9021, exactly as `scripts/run.sh` does from the Mac, then watches the server over HTTP on 127.0.0.1:8080. The logic that decides things (config, launch line, state) is plain C built and tested on the Mac. Only the screen, controller and network-info calls are console-only.

**Tech Stack:** OpenOrbis PS4 Toolchain (clang, create-fself, PkgTool.Core with the .NET runtime), C11, Nayuki qrcodegen (MIT, vendored), one OFL font, GoldHEN Package Installer (owner step).

**Spec:** `build.md` (repo root, untracked on purpose), section "Phase 6 (optional, only after Phase 5 passes)": auto-start script for llama-server, PKG app packaging via OpenOrbis, model picker. This plan covers all three: the app is the auto-start, the TV model list is the picker.

## Before you start (gate)

- Phase 6 starts only after Phase 5 passes (spec). On 2026-10-20 Phase 4 is in progress (branch `phase-4`). Finish Phases 4 and 5 first, or get Christopher's explicit go-ahead to run this plan early.
- Installing a PKG writes outside `/data/PS4LM`, which the spec forbids for this repo's scripts. The install is the owner's step, by hand, through GoldHEN's Package Installer from a USB drive. Ask Christopher before the first install. Scripts in this plan never write outside `/data/PS4LM` on the console.
- One agent drives the console at a time. Another session also sends payloads (scripts/ps4_mcp.py). A second payload sent during a server load has broken a run before.

## Global Constraints

- No jailbreak, exploit, kernel or homebrew-enabler code; do not bundle elfldr or GoldHEN payloads in the PKG.
- No GPU backend. The app draws its 2D screen through the OpenOrbis video-out samples only.
- No changes to llama.cpp source beyond `patches/`. The app runs the existing `llama-server.elf`.
- No Sony SDK files, keys, firmware or model weights in git. No TLS, libcurl or in-app downloads; models still arrive by `scripts/fetch-model.sh`.
- Console paths: read and write only under `/data/PS4LM`.
- Server launch defaults (Phase 3/4 findings): `-t 2 --prio -1 -lm none -c 1024 --host 0.0.0.0 --port 8080`. `--no-mmap` does not exist at b11608; `-lm none` is the flag.
- elfldr launch line rules (scripts/run.sh): `file:<path>?args=<args>\n`, args joined by spaces, a space inside an arg is `\ `, a backslash is `\\`, `%`, `&`, `#` and control characters are percent-encoded, a trailing `\` is added when the third-last character is `%`, whole line at most 1024 bytes.
- Title ID `PSLM00001`, content ID `IV0000-PSLM00001_00-PS4LMLAUNCHER000`, app name `PS4LM`.
- Icon `sce_sys/icon0.png` 512x512 PNG; background `sce_sys/pic1.png` 1920x1080 PNG.
- No em dashes and no emojis in code, comments, docs or commit messages. Small commits, one fix each.
- Every new doc starts with the standard header; run `docseal anchor <files>` before committing (fall back to `docseal stamp` and say why).
- Repo is public: no pushing without asking. Push trap and credentials are in `~/.claude/project-notes/ps4lm.md`.

## Review Focus

1. elfldr not running (console rebooted, GoldHEN not loaded): the app must say so in plain words and never hang on a connect. Test: `test_state_no_loader` (Task 4).
2. Server already running when the app opens (started from the Mac): the app must show READY and must not launch a second server. Test: `test_state_server_already_up` (Task 4).
3. No `.gguf` in `/data/PS4LM/models`, or `server.conf` names a model that is gone: the app shows "no model" and Cross does nothing. Test: `test_pick_model_missing` (Task 2).
4. Server dies while loading (seen 2026-10-20: PRX_RUNTIME_ERROR at "load_model: initializing"): LOADING must turn into STOPPED with "server stopped, see /data/PS4LM/logs" after 120 s or when 8080 closes. Test: `test_state_load_timeout` (Task 4).
5. No network or no IP: show "no network" instead of a wrong address and QR. Test: `test_chat_url_no_ip` (Task 2).

---

## File Structure

```
app/
  Makefile               OpenOrbis build: eboot.bin, sce_sys/param.sfo, PSLM00001.pkg
  src/conf.c conf.h      server.conf parsing, model choice, launch lines, chat URL (pure C)
  src/net.c net.h        TCP port check, send one line, tiny HTTP GET (POSIX sockets)
  src/state.c state.h    app state from what net.c saw (pure C)
  src/screen.c screen.h  framebuffer, text, QR (console only)
  src/main.c             loop: pad input, poll, draw (console only)
  third_party/qrcodegen/ Nayuki qrcodegen C files + LICENSE
  assets/                icon0.png, pic1.png, font .ttf + OFL.txt
  tests/test_host.c      Mac test runner for conf.c, net.c, state.c (cc + assert)
  tests/vectors.sh       expected launch lines from scripts/elfldr-args.py
scripts/
  elfldr-args.py         the launch-line encoder, moved out of run.sh (one copy)
  build-app.sh           build the PKG; copy it to a USB volume if given
  serve.sh               Mac auto-start: setup if needed, write server.conf, launch
docs/APP.md              install, use, uninstall, what each screen means
```

---

### Task 1: Toolchain, a hello app on the TV, and the facts the design rests on

**Files:**
- Create: `app/Makefile`, `app/src/main.c` (probe version), `app/assets/icon0.png` (placeholder), `scripts/build-app.sh`
- Modify: `docs/TOOLCHAIN.md` (new "OpenOrbis (app only)" section), `docs/CONSOLE.md` (new "App process" section)

**Interfaces:**
- Produces: `scripts/build-app.sh [USB_VOLUME]` builds `build/app/PSLM00001.pkg`; with a volume path, copies it to the volume root. `make -C app` honours `OO_PS4_TOOLCHAIN`.

- [ ] **Step 1: Install OpenOrbis.** Use the macOS installer from the OpenOrbis-PS4-Toolchain GitHub releases (latest tagged release; record the version) and the .NET runtime PkgTool.Core needs. Check: `echo $OO_PS4_TOOLCHAIN` is set and `create-fself` and `PkgTool.Core` run with `--help`. If no macOS path works, stop and ask (spec: toolchain problems are a stop point).
- [ ] **Step 2: Write the probe `app/src/main.c`.** On start it writes `/data/PS4LM/app-probe.txt` with: affinity mask (`cpuset_getaffinity`, same calls as `probes/cpuprobe`), CPU count allowed, direct and flexible memory (same calls as `probes/memprobe`), whether `connect()` to 127.0.0.1:9021 succeeds within 1 s, whether to 127.0.0.1:8080 succeeds, and the console IP from `sceNetCtlGetInfo`. It shows "PS4LM probe done" on screen using the OpenOrbis graphics sample code, then waits for Circle to exit.
- [ ] **Step 3: Write `app/Makefile` and `scripts/build-app.sh`.** Start from the OpenOrbis `hello_world` sample's Makefile; title ID, content ID and name from Global Constraints. Run `scripts/build-app.sh`. Expected: `build/app/PSLM00001.pkg` exists and `PkgTool.Core` prints no error.
- [ ] **Step 4: Owner installs.** Ask Christopher first. `scripts/build-app.sh /Volumes/<usb>`; owner sets GoldHEN > Debug Settings > Package Installer > Package Source to USB and installs PS4LM, then opens it. Expected: the icon on the home screen, the probe text on the TV.
- [ ] **Step 5: Read the facts.** `curl ftp://$PS4_HOST:2121/data/PS4LM/app-probe.txt`. Write them into `docs/CONSOLE.md` "App process". Decision rules:
  - 127.0.0.1:9021 refuses: stop and ask (the launcher design depends on it; the fallback is the Mac starting the server and the app only showing status).
  - The app gets more than CPUs 6,7: write it down as the case for a later native-server plan. Do not start that port here.
- [ ] **Step 6: Commit** `app/`, `scripts/build-app.sh`, docs: "app: OpenOrbis probe app PSLM00001 and build-app.sh".

### Task 2: Config, model choice, launch lines and chat URL (pure C, tested on the Mac)

**Files:**
- Create: `scripts/elfldr-args.py`, `app/src/conf.c`, `app/src/conf.h`, `app/tests/test_host.c`, `app/tests/vectors.sh`
- Modify: `scripts/run.sh` (call `scripts/elfldr-args.py` instead of the inline Python), `app/Makefile` (`make -C app test` builds and runs `test_host` with the Mac's `cc`)

**Interfaces:**
- Produces, in `conf.h`:
  - `typedef struct { char model[256]; int ctx; int threads; int port; char extra[256]; } ps4lm_conf;`
  - `void conf_defaults(ps4lm_conf *c);` model "", ctx 1024, threads 2, port 8080, extra "".
  - `int conf_parse(const char *text, ps4lm_conf *c);` `key=value` lines for model, ctx, threads, port, extra; `#` comments and unknown keys ignored; returns 0, or -1 for a non-number or out-of-range ctx (256..32768), threads (1..2), port (1..65535).
  - `int conf_pick_model(const ps4lm_conf *c, const char *const *files, int n);` index of `c->model` in `files`, else the first `.gguf` that does not start with `mmproj-` or `.`, else -1.
  - `int elfldr_line(char *out, size_t cap, const char *elf, const char *const *argv, int argc);` the run.sh encoding plus `\n`; returns the length, or -1 above 1024 bytes or `cap`.
  - `int server_line(char *out, size_t cap, const ps4lm_conf *c, const char *model_file);` `/data/PS4LM/bin/llama-server.elf` with `-m /data/PS4LM/models/<model_file> --host 0.0.0.0 --port <port> -t <threads> --prio -1 -lm none -c <ctx>` then `extra` split on spaces.
  - `int stop_line(char *out, size_t cap);` `/data/PS4LM/bin/stop.elf` with argument `llama-server`.
  - `int chat_url(char *out, size_t cap, const char *ip, int port);` `http://<ip>:<port>`; returns -1 for an empty or `0.0.0.0` ip.

- [ ] **Step 1: Move the encoder.** `scripts/elfldr-args.py ARGS...` prints the encoded args (the exact code now inline in run.sh); run.sh calls it. Check: `scripts/run.sh hello "two words" a%b` on the console prints the same argv as before.
- [ ] **Step 2: Write the failing tests** in `app/tests/test_host.c`:
  - `test_conf_defaults_and_parse`: `"model=a.gguf\nctx=2048\n# x\nfoo=1\n"` gives model `a.gguf`, ctx 2048, threads 2, port 8080; `"ctx=abc"` and `"threads=3"` return -1.
  - `test_pick_model`: files `{"mmproj-F16.gguf","b.gguf","a.gguf"}` with model "" gives 1; with model `a.gguf` gives 2.
  - `test_pick_model_missing`: model `gone.gguf` with files `{}` gives -1; files `{".x.gguf","notes.txt"}` gives -1.
  - `test_elfldr_line_matches_run_sh`: each case in `vectors.sh` output (args `hello`, `two words`, `a%b`, `x&y#z`, `back\slash`, and one that ends in `%`) equals `elfldr_line`; a 1100-byte arg returns -1.
  - `test_server_line`: defaults with `Qwen3.5-0.8B-Q4_K_M.gguf` and extra `--no-jinja` equals the line `vectors.sh` prints for the same args.
  - `test_chat_url_no_ip`: `""` and `"0.0.0.0"` return -1; `"192.168.50.2"`, 8080 gives `http://192.168.50.2:8080`.
- [ ] **Step 3: Run** `make -C app test`. Expected: FAIL (undefined `conf_defaults` and others).
- [ ] **Step 4: Implement** `app/src/conf.c` to the signatures above.
- [ ] **Step 5: Run** `make -C app test`. Expected: all PASS.
- [ ] **Step 6: Commit**: "scripts/elfldr-args.py: one launch-line encoder" then "app/conf: server.conf, model choice, launch lines".

### Task 3: Network helpers (POSIX, tested on the Mac against a fake server)

**Files:**
- Create: `app/src/net.c`, `app/src/net.h`
- Modify: `app/tests/test_host.c`

**Interfaces:**
- Produces, in `net.h`:
  - `int net_port_open(const char *ip, int port, int timeout_ms);` 1 if a TCP connect finishes in time, else 0. Non-blocking connect + select, like `connect_timeout` in `ps4/compat/log.c`.
  - `int net_send_line(const char *ip, int port, const char *line, int timeout_ms);` connect, write all, close; SO_NOSIGPIPE set; 0 or -1.
  - `int net_http_get(const char *ip, int port, const char *path, char *body, size_t cap, int timeout_ms);` HTTP/1.0 GET; returns the status code and copies the body (truncated to `cap - 1`, NUL-terminated), or -1 on no connection or timeout.
  - `int slots_busy(const char *json);` number of `"is_processing":true` in a `/slots` reply (string search, no JSON library).
- Consumes: nothing from earlier tasks.

- [ ] **Step 1: Write the failing tests.** The test runner starts `python3 -I -m http.server` style fixtures from a small script it writes to a temp dir (a server that answers `/health` with 503 then 200, and `/slots` with a fixed body), on a free port:
  - `test_port_open`: open port gives 1; a closed port gives 0 within 1.2 s.
  - `test_http_get`: `/health` gives 503 then 200; body `{"status":"ok"}`; a body longer than `cap` is cut and NUL-terminated.
  - `test_send_line`: a listener receives exactly the bytes sent, newline included; sending to a peer that closed does not kill the test (SIGPIPE).
  - `test_slots_busy`: `[{"id":0,"is_processing":true},{"id":1,"is_processing":false}]` gives 1; `""` gives 0.
- [ ] **Step 2: Run** `make -C app test`. Expected: FAIL (undefined `net_port_open` and others).
- [ ] **Step 3: Implement** `app/src/net.c`.
- [ ] **Step 4: Run** `make -C app test`. Expected: PASS.
- [ ] **Step 5: Check `/slots` on the real server** (only when the console is free): `curl -s http://$PS4_HOST:8080/slots`. If it is 404 or 501 at b11608, `state.c` treats "busy" as unknown (READY only) and the test fixture stays; note it in `docs/APP.md`.
- [ ] **Step 6: Commit**: "app/net: port check, send line, HTTP GET, slots count".

### Task 4: App state (pure C)

**Files:**
- Create: `app/src/state.c`, `app/src/state.h`
- Modify: `app/tests/test_host.c`

**Interfaces:**
- Produces, in `state.h`:
  - `typedef enum { ST_NO_NETWORK, ST_NO_LOADER, ST_NO_MODEL, ST_STOPPED, ST_LOADING, ST_READY, ST_BUSY, ST_FAILED } app_state;`
  - `typedef struct { int have_ip; int loader_open; int model_index; int server_open; int health; int busy; int launched_ms_ago; } app_seen;` `health` is the status code or -1; `launched_ms_ago` is -1 when this app has not launched a server.
  - `app_state state_next(const app_seen *s);` order: no IP gives NO_NETWORK; server answering 200 gives READY or BUSY (busy > 0), whatever else is true; 503 gives LOADING; launched within 120000 ms and not answering gives LOADING; launched more than 120000 ms ago and not answering gives FAILED; no loader gives NO_LOADER; model_index < 0 gives NO_MODEL; else STOPPED.
  - `const char *state_text(app_state st);` the TV line for each state, exact copy:
    - NO_NETWORK "No network. Connect the PS4 to the network."
    - NO_LOADER "Loader not running. Load GoldHEN, then run scripts/console-setup.sh on the Mac."
    - NO_MODEL "No model in /data/PS4LM/models. Run scripts/fetch-model.sh on the Mac."
    - STOPPED "Ready to start. Press X."
    - LOADING "Loading the model..."
    - READY "Chat is open. Scan the code or type the address."
    - BUSY "Writing a reply..."
    - FAILED "Server stopped. See /data/PS4LM/logs."
- Consumes: nothing (main.c fills `app_seen` from conf.c and net.c).

- [ ] **Step 1: Write the failing tests:** `test_state_no_loader` (have_ip 1, loader 0, server 0, model 0, not launched: NO_LOADER); `test_state_server_already_up` (loader 0, server 1, health 200, not launched: READY); `test_state_busy` (health 200, busy 2: BUSY); `test_state_loading` (health 503: LOADING; launched 5000 ms ago, server 0: LOADING); `test_state_load_timeout` (launched 121000 ms ago, server 0: FAILED); `test_state_no_model` (loader 1, model -1: NO_MODEL); `test_state_no_ip` (have_ip 0, server 1: NO_NETWORK).
- [ ] **Step 2: Run** `make -C app test`. Expected: FAIL.
- [ ] **Step 3: Implement** `app/src/state.c`.
- [ ] **Step 4: Run** `make -C app test`. Expected: PASS.
- [ ] **Step 5: Commit**: "app/state: what the launcher shows, from what it saw".

### Task 5: Screen, controller and the launcher loop (console)

**Files:**
- Create: `app/src/screen.c`, `app/src/screen.h`, `app/third_party/qrcodegen/` (qrcodegen.c, qrcodegen.h, LICENSE), `app/assets/<font>.ttf` + `OFL.txt`
- Modify: `app/src/main.c` (replace the Task 1 probe; keep the probe as `app/src/probe.c` behind `make PROBE=1`), `app/Makefile`

**Interfaces:**
- Consumes: `conf.h`, `net.h`, `state.h` from Tasks 2 to 4.
- Produces, in `screen.h`: `int screen_init(void);` `void screen_clear(uint32_t rgb);` `void screen_text(int x, int y, int px, uint32_t rgb, const char *s);` `void screen_qr(int x, int y, int module_px, const char *text);` `void screen_present(void);`. Built on the OpenOrbis graphics and font samples; QR from `qrcodegen_encodeText` with ECC medium.

Loop, every 500 ms: read `/data/PS4LM/server.conf` (missing file means defaults) and the `/data/PS4LM/models` listing; fill `app_seen` (IP from `sceNetCtlGetInfo`; loader `net_port_open("127.0.0.1", 9021, 300)`; server `net_port_open("127.0.0.1", port, 300)`; health and `/slots` with `net_http_get`); `state_next`; draw.

Screen (1920x1080): "PS4LM" title; `state_text`; when READY or BUSY, the chat URL in large text and its QR code (module 12 px); the model list with the chosen one marked; model, ctx and threads; a footer "X start  O stop  Up/Down model  OPTIONS quit".

Buttons: Up/Down move the model choice (only when STOPPED or FAILED). X when STOPPED or FAILED sends `server_line` to 127.0.0.1:9021 and writes the chosen model back to `server.conf`. O when LOADING, READY or BUSY sends `stop_line`. OPTIONS exits the app (the server keeps running; say so on screen before exit).

- [ ] **Step 1: Build** `make -C app test && scripts/build-app.sh`. Expected: tests PASS, PKG builds.
- [ ] **Step 2: Console check** (owner reinstalls the PKG; ask first). Walk each state and write what the TV showed into `docs/APP.md`: with elfldr not loaded (after a PS4 restart, before console-setup): NO_LOADER text; after console-setup: STOPPED and the model list; X: LOADING, then READY with URL and QR; scan the QR with a phone on the same network: the chat page opens; send a message: BUSY, then READY; O: STOPPED within 10 s and port 8080 closed (`nc -z -G 2 $PS4_HOST 8080` fails); start the server from the Mac with `scripts/run.sh`, then open the app: READY, and no second server (`scripts/run.sh stop` lists one extra Payload only).
- [ ] **Step 3: Commit**: "app: launcher screen, controller, start and stop".

### Task 6: Icon and home-screen art

**Files:**
- Modify: `app/assets/icon0.png`, `app/assets/pic1.png`, `app/Makefile` (copy both into `sce_sys/`)

- [ ] **Step 1: Make the art.** Use the `csr-app-icons` skill for the house look (black background, one accent color, CSR mark top-left, accent dot bottom-right). Motif: a game-controller outline whose face buttons are text bubbles. Export `icon0.png` 512x512 and `pic1.png` 1920x1080 (same motif, large, left side dark for the PS4's text). No Sony logos, PlayStation names or button-glyph trademarks in the art.
- [ ] **Step 2: Check:** `sips -g pixelWidth -g pixelHeight app/assets/icon0.png app/assets/pic1.png` prints 512x512 and 1920x1080.
- [ ] **Step 3: Rebuild and look** on the console (owner reinstall, ask first): icon on the home screen, art behind the app tile.
- [ ] **Step 4: Commit**: "app: launcher icon and background".

### Task 7: Mac auto-start, docs, and Phase 6 close-out

**Files:**
- Create: `scripts/serve.sh`, `docs/APP.md`
- Modify: `README.md` (an "App on the home screen" section), `docs/ROADMAP.md` (Phase 6 row), `docs/PORTING.md` (app entries: what Task 1 found)

**Interfaces:**
- Consumes: `server.conf` keys from Task 2; `stop.elf` from `probes/stop`.
- Produces: `scripts/serve.sh MODEL_FILE [CTX] [-- EXTRA_ARGS...]`: runs `scripts/console-setup.sh` when 9021 is closed; uploads `probes/stop` and `llama-server.elf` to `/data/PS4LM/bin`; writes `/data/PS4LM/server.conf` (so the app and the Mac agree); stops a running server; launches with the same args as `server_line`; waits for `/health` 200 (180 s cap) and prints the chat URL.

- [ ] **Step 1: Write `scripts/serve.sh`.** Check on the console: `scripts/serve.sh Qwen3.5-0.8B-Q4_K_M.gguf 1024` prints `http://<ip>:8080` and the app (opened after) shows READY with the same model.
- [ ] **Step 2: Write `docs/APP.md`** (standard header): what the app is and is not (a launcher; the model runs in the existing llama-server payload), the one-time Mac step (`scripts/serve.sh` once, which puts `llama-server.elf` and `stop.elf` in `/data/PS4LM/bin`; without them X and O do nothing), owner install steps from USB, each screen's meaning (Task 4 copy), uninstall (Settings > Storage > Applications, delete PS4LM), the Task 1 facts.
- [ ] **Step 3: README and ROADMAP.** README gets a short section with one screenshot slot; ROADMAP Phase 6 row set to done with the date.
- [ ] **Step 4: Anchor and commit:** `docseal anchor docs/APP.md docs/superpowers/plans/2026-10-20-ps4-app-launcher.md README.md docs/ROADMAP.md docs/PORTING.md`, then commit "Phase 6: serve.sh, app docs, roadmap".
- [ ] **Step 5: End-of-phase report** (spec): three lines, done, blocked, next.
