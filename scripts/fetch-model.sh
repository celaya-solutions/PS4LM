#!/usr/bin/env bash
# Downloads a GGUF from Hugging Face to ~/ps4lm/models on this Mac, checks its
# sha256, then uploads it over FTP to /data/PS4LM/models on the console.
#
#   scripts/fetch-model.sh unsloth/Qwen3.5-0.8B-GGUF Qwen3.5-0.8B-Q4_K_M.gguf
#   scripts/fetch-model.sh REPO FILE SHA256
#
# The expected sha256 is the file's LFS id from the Hugging Face API, unless one
# is given as the third argument. A file already on the Mac with the right hash
# is not downloaded again; the upload is checked by size. Needs FTP on 2121
# (scripts/console-setup.sh).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

repo=${1:?usage: fetch-model.sh HF_REPO FILE [SHA256]}
file=${2:?usage: fetch-model.sh HF_REPO FILE [SHA256]}
want=${3:-}
host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
ftp=${PS4_FTP_PORT:-2121}
dir=${PS4LM_MODELS:-$HOME/ps4lm/models}
remote_dir=/data/PS4LM/models
name=$(basename "$file")
local_file="$dir/$name"

if [ -z "$want" ]; then
    sub=$(dirname "$file")
    [ "$sub" = . ] && sub= || sub="/$sub"
    want=$(curl -fsSL "https://huggingface.co/api/models/$repo/tree/main$sub" | python3 -I -c '
import json, sys
for f in json.load(sys.stdin):
    if f["path"] == sys.argv[1] and f.get("lfs"):
        print(f["lfs"]["oid"])
' "$file")
    [ -n "$want" ] || { echo "fetch-model.sh: no LFS sha256 for $file in $repo" >&2; exit 1; }
fi

mkdir -p "$dir"
have() { [ -f "$local_file" ] && [ "$(shasum -a 256 "$local_file" | awk '{print $1}')" = "$want" ]; }

if have; then
    echo "fetch-model.sh: $name already here, sha256 ok"
else
    curl -fL --progress-bar -o "$local_file.part" "https://huggingface.co/$repo/resolve/main/$file"
    mv "$local_file.part" "$local_file"
    have || { echo "fetch-model.sh: sha256 mismatch for $name, kept at $local_file" >&2; exit 1; }
    echo "fetch-model.sh: $name downloaded, sha256 ok"
fi

curl -sS --ftp-create-dirs --progress-bar -T "$local_file" "ftp://$host:$ftp$remote_dir/$name"
size=$(stat -f %z "$local_file")
remote=$(curl -sSI "ftp://$host:$ftp$remote_dir/$name" | awk 'tolower($1)=="content-length:"{print $2}' | tr -d '\r')
[ "$remote" = "$size" ] || { echo "fetch-model.sh: console has $remote bytes, expected $size" >&2; exit 1; }
echo "fetch-model.sh: $remote_dir/$name on the console ($size bytes)"
