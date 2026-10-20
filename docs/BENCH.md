```text
Document:    PS4LM Benchmarks
Version:     v1.1.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      bbb3f429078b178e8506c49d7866d869dccf0b58cc132dbd00d74d696073cf3c
Chain:       solana-mainnet
Tx:          21ZTSi7SQtBT1UrADJdoRJm4VSJYM9t3EM5gv5Xahk6uCvdhXStKyzt18741Vobd4875XJv8LEqx1p8LM54WE3Js
License:     GPL-3.0-or-later
```

# Benchmarks

Filled in Phases 3 and 5.

## Thread sweep (Phase 3)

`scripts/bench.sh /data/PS4LM/models/qwen3-0.6b-q4_k_m.gguf`, 2026-10-20:
llama-bench `-p 512 -n 128 -r 1 -lm none`, three runs per thread count,
stopping at the first hang, crash or shutdown. A payload may use 2 CPUs
(CONSOLE.md), so the spec's range of 2 to allowed-minus-1 is empty; the sweep
runs 1 (the patched default) and 2. Qwen3 0.6B Q4_K_M (493 MiB), default
priority and `--poll 0`. Tokens per second, prompt / generation.

| Threads | Run 1 | Run 2 | Run 3 | Result |
|---|---|---|---|---|
| 1 | 8.09 / 4.20 | 8.20 / 4.23 | 8.20 / 4.23 | clean |
| 2 | 14.08 / 6.54 | 13.77 / 6.63 | 13.84 / 6.54 | clean |

**Safe thread maximum: 2.** No hang, crash or shutdown. But in two of the
three 2-thread runs the kernel log showed `No heart beat of Shell UI`, for up
to 32 s in the third run; never at 1 thread. Payload threads (Sony priority
700) share CPUs 6 and 7 with the shell, and the shell was partly swapped out
(about 200 MB swap) by then.

With `--prio -1` (priority 767, patches/0002), two more 2-thread runs gave
13.16 / 6.18 and 13.21 / 6.18 with no heartbeat warning. So the launch
setting is **`-t 2 --prio -1`**, about 5% slower than normal priority.

Other single runs of note, same model: mmap'd weights (the default load mode)
at 1 thread gave 4.74 / 0.34 (PORTING.md 15).

## Models (Phase 5)

`llama-bench -p 512 -n 128` at the safe thread maximum, with the Mac (M5) for
reference. Tokens per second.

| Model | Quant | PS4 prompt | PS4 generation | Mac prompt | Mac generation |
|---|---|---|---|---|---|
