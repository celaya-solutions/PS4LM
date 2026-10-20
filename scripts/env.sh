# Source this file: `source scripts/env.sh`
# Settings for every script in this repo. Override any of them in the shell
# or in scripts/env.local.sh (gitignored), which is read first.

PS4LM_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]:-${(%):-%x}}")/.." && pwd)"
export PS4LM_ROOT

if [ -f "$PS4LM_ROOT/scripts/env.local.sh" ]; then
    # shellcheck source=/dev/null
    source "$PS4LM_ROOT/scripts/env.local.sh"
fi

export PS4DEV="${PS4DEV:-$HOME/ps4dev}"
export PS4_PAYLOAD_SDK="${PS4_PAYLOAD_SDK:-$PS4DEV/sdk}"

# The console. PS4_LOADER_PORT is the homebrew enabler's payload port:
# GoldHEN 9090, HEN VTX and remote_lua_loader 9020.
export PS4_HOST="${PS4_HOST:-}"
export PS4_LOADER_PORT="${PS4_LOADER_PORT:-9090}"
export PS4_ELFLDR_PORT="${PS4_ELFLDR_PORT:-9021}"
export PS4_FTP_PORT="${PS4_FTP_PORT:-2121}"

# Where the console sends its logs: this Mac, listening with scripts/logs.sh.
# The Mac's address on the interface that reaches the console, so a console on
# a direct cable gets the cable's address, not the Wi-Fi one.
if [ -z "${PS4LM_LOG_HOST:-}" ] && [ "$(uname -s)" = "Darwin" ]; then
    if [ -n "$PS4_HOST" ]; then
        _iface="$(route -n get "$PS4_HOST" 2>/dev/null | awk '/interface:/{print $2}')"
        [ -n "$_iface" ] && PS4LM_LOG_HOST="$(ipconfig getifaddr "$_iface" 2>/dev/null || true)"
        unset _iface
    fi
    if [ -z "${PS4LM_LOG_HOST:-}" ]; then
        PS4LM_LOG_HOST="$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || true)"
    fi
fi
export PS4LM_LOG_HOST="${PS4LM_LOG_HOST:-}"
export PS4LM_LOG_PORT="${PS4LM_LOG_PORT:-9999}"

# The SDK drives the host clang through llvm-config; it documents LLVM 18.
if [ -z "${LLVM_CONFIG:-}" ] && [ -x /opt/homebrew/opt/llvm@18/bin/llvm-config ]; then
    export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config
fi


if [ ! -x "$PS4_PAYLOAD_SDK/bin/orbis-clang" ]; then
    echo "env.sh: no SDK at $PS4_PAYLOAD_SDK, run scripts/setup-sdk.sh" >&2
fi
