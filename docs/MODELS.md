```text
Document:    PS4LM Models
Version:     v1.1.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-21
SHA256:      86f8ed1a9e03e71bd2068415026f3e74f4ec4193a85ed7da8012d180be326a85
Chain:       solana-mainnet
Tx:          3vHihg6zJTDJobsew9RvihWcp2HA9fyBHtL6A9xtYCstqCeUqQFLMGSPehEA97wJ6MUGfjGd8gKRogWF8hTde1hw
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

## Gemma 4 E2B

Gemma 4 E2B is the smallest Gemma 4 (`gemma4` arch, in llama.cpp b11608).
Every E2B file is 2.2 to 3.2 GB, but about half of each is one tensor,
`per_layer_token_embd` (per-layer embeddings, PLE). A token reads only its own
row of it, so `-lzm on` (`--lazy-mode on`) leaves it on disk and reads rows on
demand through the file mapping. Everything else is loaded into RAM as usual
with `-lm none`. The default (`auto`) keeps PLE resident, which does not fit.

File: `unsloth/gemma-4-E2B-it-GGUF` / `gemma-4-E2B-it-Q4_K_M.gguf`,
3,106,738,272 bytes, sha256
`740185b21d22ceb83a11c3aa62ad5842ef32c70f6096d756bbee85a1e4ec34b8`.

| Q4_K_M part | Size | Where |
|---|---|---|
| `per_layer_token_embd` (Q5_K) | 1540 MiB | disk, `-lzm on` |
| `token_embd` (Q4_K, also the output head) | 216 MiB | RAM |
| blocks, norms, projections | 1192 MiB | RAM |

Smaller files keep about as much in RAM (from the GGUF headers): Q3_K_M
1144 MiB, IQ4_XS 1291 MiB, unsloth Q4_0 1345 MiB, ggml-org Q4_0 1435 MiB
(its `token_embd` is Q8_0). Q4_K_M is the pick: the usual quality default,
and only ~120 MiB more than IQ4_XS. Try IQ4_XS or Q3_K_M if RAM runs short.

```sh
scripts/fetch-model.sh unsloth/gemma-4-E2B-it-GGUF gemma-4-E2B-it-Q4_K_M.gguf
scripts/run.sh llama-server -m /data/PS4LM/models/gemma-4-E2B-it-Q4_K_M.gguf \
    --host 0.0.0.0 --port 8080 -t 2 --prio -1 -lm none -lzm on \
    -c 4096 -np 1 -b 128 -ub 128 -rea off
```

- `-rea off`: Gemma 4 thinks first by default and spends `max_tokens` on it
  (the reply comes back empty with `finish_reason: length`).
- `-b 128 -ub 128`: compute buffer 32 MiB instead of 127 MiB.
- `-np 1`: one slot. KV is small (36 MiB at 4096; most layers use a sliding
  window and the last layers share KV).

Mac check (2026-10-21, CPU only, same flags, 2 threads): lazy on 2045 MiB
RSS, lazy off 3585 MiB. Log: model 348 MiB + 1275 MiB (repacked for ARM),
PLE 1540 MiB mapped, KV 33 MiB, compute 32 MiB. Phase 0 measured ~3.5 GiB
for a payload (3.0 GiB on a tired console), so it fits with ~1 GiB to
spare; expect some of the system to go to swap. Not run on the console yet.
Speed guess: about 2x the work per token of Qwen3.5 0.8B, so 1 to 2 t/s.
