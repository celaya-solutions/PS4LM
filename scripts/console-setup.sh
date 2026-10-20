#!/usr/bin/env bash
# Starts the helper payloads on a console where the homebrew enabler is
# already loaded: elfldr (port 9021) through the enabler's payload port, then
# ftpsrv (port 2121) unless an FTP server is already up (GoldHEN has one).
# They stay up until the console reboots.
#
#   scripts/console-setup.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
loader=${PS4_LOADER_PORT:-9090}
elfldr=${PS4_ELFLDR_PORT:-9021}
ftp=${PS4_FTP_PORT:-2121}
dir="$PS4DEV/payloads"

open_port() { nc -z -G 2 "$host" "$1" >/dev/null 2>&1; }

wait_port() { # port seconds
    for _ in $(seq 1 "$2"); do
        open_port "$1" && return 0
        sleep 1
    done
    return 1
}

[ -f "$dir/elfldr.elf" ] || { echo "console-setup: run scripts/setup-sdk.sh first" >&2; exit 1; }

if open_port "$elfldr"; then
    echo "console-setup: elfldr already listening on $host:$elfldr"
else
    open_port "$loader" || { echo "console-setup: nothing on $host:$loader, is the homebrew enabler loaded?" >&2; exit 1; }
    socat -u "FILE:$dir/elfldr.elf" "TCP:$host:$loader"
    wait_port "$elfldr" 20 || { echo "console-setup: elfldr did not come up on $host:$elfldr" >&2; exit 1; }
    echo "console-setup: elfldr listening on $host:$elfldr"
fi

if open_port "$ftp"; then
    echo "console-setup: FTP already up on $host:$ftp"
else
    socat -u "FILE:$dir/ftpsrv-ps4.elf" "TCP:$host:$elfldr"
    wait_port "$ftp" 20 || { echo "console-setup: ftpsrv did not come up on $host:$ftp" >&2; exit 1; }
    echo "console-setup: ftpsrv listening on $host:$ftp"
fi
