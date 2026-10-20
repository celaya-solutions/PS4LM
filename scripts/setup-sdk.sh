#!/usr/bin/env bash
# Install the PS4 payload SDK (with libc++) into ~/ps4dev/sdk and fetch the
# elfldr and ftpsrv payloads into ~/ps4dev/payloads. Safe to re-run.
set -euo pipefail

SDK_REPO=https://github.com/ps4-payload-dev/sdk
SDK_TAG=v0.9
# The .bin build: GoldHEN's BinLoader rejects elfldr.elf ("Error handling payload").
ELFLDR_URL=https://github.com/ps4-payload-dev/elfldr/releases/download/v0.7/elfldr.bin
ELFLDR_SHA256=9525dc95393ab943758db69a1f49fea6e82f36f2821bf5f41bc00bfc4d6f75df
FTPSRV_URL=https://github.com/ps5-payload-dev/ftpsrv/releases/download/v0.21.1/ftpsrv-ps4.elf
FTPSRV_SHA256=5f00e2df7fa8ab7daa084343f38394e6672cd389e03fc8598a54d7d9e1d58d16

PS4DEV=${PS4DEV:-$HOME/ps4dev}
SDK_DIR=$PS4DEV/sdk
SRC_DIR=$PS4DEV/src/sdk-$SDK_TAG
PAYLOAD_DIR=$PS4DEV/payloads

# The SDK documents LLVM 18 on macOS (Homebrew llvm@18 ships clang and ld.lld).
export LLVM_CONFIG=${LLVM_CONFIG:-/opt/homebrew/opt/llvm@18/bin/llvm-config}
if [ ! -x "$LLVM_CONFIG" ]; then
    echo "error: $LLVM_CONFIG not found. Run: brew install llvm@18" >&2
    exit 1
fi
command -v wget >/dev/null || { echo "error: wget is needed by the SDK's libcxx.sh" >&2; exit 1; }

fetch() { # url dest sha256
    if [ ! -f "$2" ]; then
        curl -fL --retry 3 -o "$2.part" "$1"
        mv "$2.part" "$2"
    fi
    if [ -n "$3" ]; then
        echo "$3  $2" | shasum -a 256 -c - >/dev/null || { echo "error: sha256 mismatch for $2" >&2; exit 1; }
    fi
}

mkdir -p "$PAYLOAD_DIR"
fetch "$ELFLDR_URL" "$PAYLOAD_DIR/elfldr.bin" "$ELFLDR_SHA256"
fetch "$FTPSRV_URL" "$PAYLOAD_DIR/ftpsrv-ps4.elf" "$FTPSRV_SHA256"

if [ "$(cat "$SDK_DIR/.ps4lm-sdk-tag" 2>/dev/null)" = "$SDK_TAG" ] &&
   [ -f "$SDK_DIR/target/lib/libc++.a" ]; then
    echo "SDK $SDK_TAG already installed in $SDK_DIR"
    exit 0
fi

if [ ! -d "$SRC_DIR/.git" ]; then
    mkdir -p "$(dirname "$SRC_DIR")"
    git clone --depth 1 --branch "$SDK_TAG" "$SDK_REPO" "$SRC_DIR"
fi

rm -rf "$SDK_DIR"
mkdir -p "$SDK_DIR"
make -C "$SRC_DIR" DESTDIR="$SDK_DIR" install
PS4_PAYLOAD_SDK="$SDK_DIR" bash "$SRC_DIR/libcxx.sh"
echo "$SDK_TAG" > "$SDK_DIR/.ps4lm-sdk-tag"
echo "SDK $SDK_TAG installed in $SDK_DIR"
