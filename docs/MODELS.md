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

Filled in Phase 4. One row per model that ran.

| Model | Quant | File size | Context | Threads | mmap | Memory headroom | Notes |
|---|---|---|---|---|---|---|---|

Ladder (Q4_K_M unless noted): the smallest current Qwen instruct GGUF (0.5B to
0.6B class), then 1B to 1.7B, then 3B to 4B only if Phase 0 shows room for
weights + KV cache + 20% headroom. Models arrive by FTP into
`/data/PS4LM/models` (`scripts/fetch-model.sh`).
