#!/usr/bin/env bash
# Starts llama-server on the console and returns once the chat is up, the same
# way the home-screen app does (app/src/main.cpp): one launch line to elfldr,
# then the socket is closed. Also writes /data/PS4LM/server.conf so the app and
# the Mac agree, and uploads stop.elf so the app's O button works.
#
#   scripts/serve.sh Qwen3.5-0.8B-Q4_K_M.gguf
#   scripts/serve.sh Qwen3.5-0.8B-Q4_K_M.gguf 2048 -- --no-jinja
#
# Needs llama-server built with the SIGPIPE fix in ps4/compat/log.c: nothing
# reads its output once this script hangs up.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

model=${1:?usage: serve.sh MODEL_FILE [CTX] [-- EXTRA_ARGS...]}
shift
ctx=1024
if [ $# -gt 0 ] && [ "$1" != "--" ]; then ctx=$1; shift; fi
[ "${1:-}" = "--" ] && shift
# Same range as app/src/conf.c, so the app can read the server.conf this writes.
case $ctx in
    ''|*[!0-9]*) echo "serve.sh: CTX must be a number, got '$ctx' (put extra args after --)" >&2; exit 2 ;;
esac
if [ "$ctx" -lt 256 ] || [ "$ctx" -gt 32768 ]; then
    echo "serve.sh: CTX must be 256..32768, got $ctx" >&2; exit 2
fi
extra=("$@")
threads=2
port=8080

host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
elfldr=${PS4_ELFLDR_PORT:-9021}
ftp=${PS4_FTP_PORT:-2121}
bin=/data/PS4LM/bin
open_port() { nc -z -G 2 "$host" "$1" >/dev/null 2>&1; }
send_line() { printf '%s\n' "$1" | socat -t 2 - "TCP:$host:$elfldr"; }
launch() { # elf args...
    local elf=$1; shift
    send_line "file:$elf?args=$(python3 -I "$root/scripts/elfldr-args.py" "$@")"
}

open_port "$elfldr" || "$root/scripts/console-setup.sh"

server="$root/build/llama-ps4/bin/llama-server.elf"
[ -f "$server" ] || { echo "serve.sh: build llama-server first (scripts/build-llama.sh)" >&2; exit 1; }
curl -sS --ftp-create-dirs -T "$server" "ftp://$host:$ftp$bin/llama-server.elf"
if [ -d "$root/probes/stop" ]; then
    make -s -C "$root/probes/stop"
    curl -sS --ftp-create-dirs -T "$root/build/probes/stop.elf" "ftp://$host:$ftp$bin/stop.elf"
else
    echo "serve.sh: no probes/stop in this checkout; the app's O button needs stop.elf" >&2
fi

printf 'model=%s\nctx=%s\nthreads=%s\nport=%s\nextra=%s\n' "$model" "$ctx" "$threads" "$port" "${extra[*]:-}" |
    curl -sS --ftp-create-dirs -T - "ftp://$host:$ftp/data/PS4LM/server.conf"

if open_port "$port"; then
    echo "serve.sh: stopping the running server"
    launch "$bin/stop.elf" llama-server
    for _ in $(seq 1 15); do open_port "$port" || break; sleep 1; done
    if open_port "$port"; then echo "serve.sh: server still up on $port" >&2; exit 1; fi
fi

launch "$bin/llama-server.elf" -m "/data/PS4LM/models/$model" --host 0.0.0.0 --port "$port" \
    -t "$threads" --prio -1 -lm none -c "$ctx" ${extra[@]+"${extra[@]}"}

for _ in $(seq 1 180); do
    if [ "$(curl -s -o /dev/null -w '%{http_code}' "http://$host:$port/health")" = 200 ]; then
        echo "http://$host:$port"
        exit 0
    fi
    sleep 1
done
echo "serve.sh: no /health 200 after 180 s; see /data/PS4LM/logs" >&2
exit 1
