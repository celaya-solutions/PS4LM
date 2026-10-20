#!/usr/bin/env bash
# Sends one payload ELF to elfldr (port 9021) and streams the payload's stdout
# back until it exits.
#
#   scripts/send.sh build/probes/memprobe.elf
#   PS4_HOST=192.168.1.50 scripts/send.sh x.elf
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

elf=${1:?usage: send.sh PAYLOAD.elf}
host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
port=${PS4_ELFLDR_PORT:-9021}

command -v socat >/dev/null || { echo "send.sh: socat is required (brew install socat)" >&2; exit 1; }

# Not `socat -u`: elfldr hands the payload this socket as stdout, so keep
# reading until the payload closes it.
socat -t 9999999 - "TCP:$host:$port" < "$elf"
