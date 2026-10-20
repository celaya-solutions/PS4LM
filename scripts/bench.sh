#!/usr/bin/env bash
# Thread sweep: runs llama-bench on the console at 1 thread, then 2, and so on
# up to the top step, three runs each, and stops at the first hang, crash or
# shutdown. Prints a Markdown table for docs/BENCH.md and keeps every run's
# output in logs/bench-<time>/.
#
#   scripts/bench.sh /data/PS4LM/models/qwen3-0.6b-q4_k_m.gguf
#   BENCH_MAX_THREADS=3 BENCH_RUNS=1 scripts/bench.sh MODEL.gguf
#
# The top step is the allowed CPUs minus 1 but at least 2: a payload gets only
# CPUs 6 and 7 (docs/CONSOLE.md), so the spec's 2..allowed-1 would be empty.
# Weights are read into memory (-lm none); mmap'd weights generate about 13x
# slower on the console (docs/PORTING.md 13). Needs elfldr and FTP
# (scripts/console-setup.sh).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=env.sh
source "$root/scripts/env.sh" 2>/dev/null

model=${1:?usage: bench.sh /data/PS4LM/models/MODEL.gguf}
host=${PS4_HOST:?set PS4_HOST to the console IP (scripts/env.local.sh)}
allowed=${PS4LM_ALLOWED_CPUS:-2}
top=${BENCH_MAX_THREADS:-$(( allowed - 1 > 2 ? allowed - 1 : 2 ))}
runs=${BENCH_RUNS:-3}
cap=${BENCH_TIMEOUT:-900}
args=(-p "${BENCH_PROMPT:-512}" -n "${BENCH_GEN:-128}" -r 1 -o jsonl -lm "${BENCH_LOAD_MODE:-none}")

out="$root/logs/bench-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"

# One llama-bench run with a time cap (macOS has no timeout). Leaves its
# output in $out/t<threads>-r<run>.txt.
run_once() { # threads run
    local log="$out/t$1-r$2.txt"
    "$root/scripts/run.sh" llama-bench -m "$model" -t "$1" "${args[@]}" > "$log" 2>&1 &
    local pid=$! waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$waited" -ge "$cap" ]; then
            kill "$pid" 2>/dev/null
            echo "hang (no exit after $cap s)" >> "$log"
            return 1
        fi
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid" 2>/dev/null || true
}

# "pp t/s / tg t/s" from a run's JSON lines, or nothing.
speeds() { # log
    python3 -I -c '
import json, sys
pp = tg = None
for line in open(sys.argv[1], errors="replace"):
    line = line.strip()
    if not line.startswith("{"):
        continue
    r = json.loads(line)
    if r.get("n_gen"):
        tg = r["avg_ts"]
    elif r.get("n_prompt"):
        pp = r["avg_ts"]
if pp is not None and tg is not None:
    print("%.2f / %.2f" % (pp, tg))
' "$1"
}

echo "bench.sh: $model, threads 1..$top, $runs runs each, logs in $out" >&2
printf '| Threads |'; for r in $(seq 1 "$runs"); do printf ' Run %s |' "$r"; done; printf ' Result |\n'
printf '|---|'; for _ in $(seq 1 "$runs"); do printf -- '---|'; done; printf -- '---|\n'

safe=0
for t in $(seq 1 "$top"); do
    row="| $t |"
    result=clean
    for r in $(seq 1 "$runs"); do
        run_once "$t" "$r" || true
        s=$(speeds "$out/t$t-r$r.txt")
        if [ -n "$s" ]; then
            row="$row $s |"
            continue
        fi
        if ! ping -c 3 -t 5 "$host" >/dev/null 2>&1; then
            result="console gone (shutdown?)"
        elif grep -q '^hang' "$out/t$t-r$r.txt"; then
            result=hang
        else
            result="crash, see t$t-r$r.txt and the kernel log"
        fi
        row="$row - |"
        break
    done
    echo "$row $result |"
    [ "$result" = clean ] || break
    safe=$t
done

echo "bench.sh: safe thread maximum $safe (pp t/s / tg t/s per run)" >&2
echo "$safe" > "$out/safe-max.txt"
