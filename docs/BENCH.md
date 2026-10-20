```text
Document:    PS4LM Benchmarks
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      66ce8d3b2e1eb9cbacd380d8053647217477e8f857408bb69168ec8a5dd4fa6b
Chain:       solana-mainnet
Tx:          xCGP6u9iYFyavKDGk8RYgSaDXS4WpapDdu47muBXY1oi3EaiBcwJRRzpSYsHvn2dXAW15Ev4W3GZZcJgReMB3nc
License:     GPL-3.0-or-later
```

# Benchmarks

Filled in Phases 3 and 5.

## Thread sweep (Phase 3)

`scripts/bench.sh`: llama-bench from `-t 2` up to allowed CPUs minus 1, three
runs per step, stopping at the first hang, crash or shutdown.

| Threads | Run 1 | Run 2 | Run 3 | Result |
|---|---|---|---|---|

## Models (Phase 5)

`llama-bench -p 512 -n 128` at the safe thread maximum, with the Mac (M5) for
reference. Tokens per second.

| Model | Quant | PS4 prompt | PS4 generation | Mac prompt | Mac generation |
|---|---|---|---|---|---|
