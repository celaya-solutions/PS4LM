#!/usr/bin/env bash
# Prints one elfldr launch line per case, encoded by scripts/elfldr-args.py, so
# app/tests/test_host.c can check that conf.c encodes exactly as run.sh does.
# Keep the case order in step with test_elfldr_line_matches_run_sh.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
line() { local elf=$1; shift; printf 'file:%s?args=%s\n' "$elf" "$(python3 -I "$root/scripts/elfldr-args.py" "$@")"; }

line /data/PS4LM/bin/hello.elf hello
line /data/PS4LM/bin/hello.elf "two words"
line /data/PS4LM/bin/hello.elf "a%b"
line /data/PS4LM/bin/hello.elf "x&y#z"
line /data/PS4LM/bin/hello.elf 'back\slash'
line /data/PS4LM/bin/hello.elf "end%"
line /data/PS4LM/bin/llama-server.elf -m /data/PS4LM/models/Qwen3.5-0.8B-Q4_K_M.gguf \
    --host 0.0.0.0 --port 8080 -t 2 --prio -1 -lm none -c 1024 --no-jinja
line /data/PS4LM/bin/stop.elf llama-server
