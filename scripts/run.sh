#!/usr/bin/env bash
# Uploads a tool to /data/PS4LM/bin over FTP and starts it through elfldr with
# arguments, streaming its output back until it exits.
#
#   scripts/run.sh hello
#   scripts/run.sh llama-cli -m /data/PS4LM/models/model.gguf -p "Hello" -n 32
#   scripts/run.sh llama-server -m /data/PS4LM/models/model.gguf --host 0.0.0.0 --port 8080
#
# A probe name (hello, memprobe) is rebuilt first. Needs elfldr on 9021 and FTP
# on 2121 (scripts/console-setup.sh).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

tool=${1:?usage: run.sh TOOL [ARGS...]}
shift
host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
elfldr=${PS4_ELFLDR_PORT:-9021}
ftp=${PS4_FTP_PORT:-2121}
remote_dir=/data/PS4LM/bin

if [ -d "$root/probes/$tool" ]; then
    make -s -C "$root/probes/$tool"
    elf="$root/build/probes/$tool.elf"
elif [ -f "$root/build/llama-ps4/bin/$tool" ]; then
    elf="$root/build/llama-ps4/bin/$tool"
else
    elf="$tool"
fi
[ -f "$elf" ] || { echo "run.sh: no $tool, build it first" >&2; exit 1; }
name=$(basename "$elf")

# Where the tools send their logs (ps4/compat/log.c reads it at startup).
if [ -n "${PS4LM_LOG_HOST:-}" ]; then
    printf '%s:%s\n' "$PS4LM_LOG_HOST" "${PS4LM_LOG_PORT:-9999}" |
        curl -sS --ftp-create-dirs -T - "ftp://$host:$ftp/data/PS4LM/loghost.txt"
fi
curl -sS --ftp-create-dirs -T "$elf" "ftp://$host:$ftp$remote_dir/$name"

# elfldr v0.7 splits args on spaces and drops a backslash before any character
# (socksrv.c args_split). The URI ends at a newline and the args value at & or
# #, so only %, &, # and control characters are percent-encoded. Its decoder
# skips a %XX in the last three characters (uri.c uri_decode), so a trailing
# backslash, which args_decode removes, keeps one off the end. The whole line
# must fit in 1024 bytes (PATH_MAX).
args=$(python3 -I -c '
import sys
parts = [a.replace("\\", "\\\\").replace(" ", "\\ ") for a in sys.argv[1:]]
out = "".join(c if c not in "%&#" and ord(c) >= 32 else "".join("%%%02X" % b for b in c.encode()) for c in " ".join(parts))
print(out + "\\" if out[-3:-2] == "%" else out)
' "$@")

echo "file:$remote_dir/$name?args=$args" | socat -t 9999999 - "TCP:$host:$elfldr"
