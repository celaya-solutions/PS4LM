#!/usr/bin/env bash
# Listens for console log lines on PS4LM_LOG_PORT (default 9999), prints them
# and appends them to logs/<date>.log. Leave it running in its own terminal.
#
#   scripts/logs.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

port=${PS4LM_LOG_PORT:-9999}
command -v socat >/dev/null || { echo "logs.sh: socat is required (brew install socat)" >&2; exit 1; }

mkdir -p "$root/logs"
log="$root/logs/$(date +%Y-%m-%d).log"
echo "logs.sh: listening on ${PS4LM_LOG_HOST:-*}:$port, writing $log" >&2

# fork: one child per connection, so several tools can log at once.
socat -u "TCP-LISTEN:$port,reuseaddr,fork" STDOUT | tee -a "$log"
