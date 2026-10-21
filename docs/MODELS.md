```text
Document:    PS4LM Models
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      77bc5d8696fec63345b62bb8cfc67f54407fb6d47f87f811fc87022ea0936a12
Chain:       solana-mainnet
Tx:          5hUHhFr1btMZJNBnqDYJxpg1EJwaqA5jDbM25K6ARYzhGswoTBujhdmKgn5waw9Tt7qy2qe2gBf1Ned8hCGsbF7h
License:     GPL-3.0-or-later
```

# Models

Filled in Phase 4. One row per model and setting that ran. All with
`llama-server -t 2 --prio -1 -lm none` (weights read into memory, not mmap'd).

| Model | Quant | File size | Context | Threads | mmap | Memory headroom | Notes |
|---|---|---|---|---|---|---|---|
| Qwen3.5 0.8B (unsloth) | Q4_K_M | 532 MB | 1024, 4 slots (256 each) | 2 | no | RSS ~1.13 GB, swap out ~270 MB | 2026-10-20: web UI and curl answered; prompt 6.5-10 t/s, generation 4.0-4.2 t/s |
| Qwen3.5 0.8B (unsloth) | Q4_K_M | 532 MB | 2048, 1 slot | 2 | no | not measured | 2026-10-21: long chat to 1,725 tokens, recall right, overflow refused (BENCH.md) |
| Qwen3.5 0.8B (unsloth) | Q4_K_M | 532 MB | 4096, 1 slot | 2 | no | system RSS ~1.17 GB (of 3.5 GiB a payload can get), swap out up to ~690 MB | 2026-10-21: long chat to 2,927 tokens, stopped there by choice; generation 4.1 down to 3.0 t/s (BENCH.md) |

Ladder (Q4_K_M unless noted): the smallest current Qwen instruct GGUF (0.5B to
0.6B class), then 1B to 1.7B, then 3B to 4B only if Phase 0 shows room for
weights + KV cache + 20% headroom. Models arrive by FTP into
`/data/PS4LM/models` (`scripts/fetch-model.sh`).

Use `-np 1` for one long chat: the server splits `-c` across its slots, so the
default 4 slots at `-c 1024` leave each chat 256 tokens. Qwen3.5 thinks by
default and can spend all of `max_tokens` doing it; send
`"chat_template_kwargs": {"enable_thinking": false}` for plain replies.
