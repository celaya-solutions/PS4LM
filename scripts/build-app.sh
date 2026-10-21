#!/usr/bin/env bash
# Builds the PS4LM app into build/app/PSLM00001.pkg. With a USB volume path,
# also copies the PKG to the volume root for GoldHEN's Package Installer.
#
#   scripts/build-app.sh
#   scripts/build-app.sh /Volumes/USB
#   PROBE=1 scripts/build-app.sh      the probe app instead of the launcher
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$PS4DEV/OpenOrbis/PS4Toolchain}"

make -C "$root/app" test
rm -f "$root/build/app/PSLM00001.pkg"
make -C "$root/app" ${PROBE:+PROBE=1}
pkg="$root/build/app/PSLM00001.pkg"
ls -l "$pkg"

if [ -n "${1:-}" ]; then
    [ -d "$1" ] || { echo "build-app.sh: no volume at $1" >&2; exit 1; }
    cp "$pkg" "$1/"
    echo "copied to $1/$(basename "$pkg")"
fi
