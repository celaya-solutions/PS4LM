```text
Document:    PS4LM Porting Notes
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      56065565604d83d19817f330f5dd744ae1b1fe60e9295711693f00d69531621e
Chain:       solana-mainnet
Tx:          4HnQ31FeYaL5hSFnCqe8FRLKHMJGPwyeiD9jkd2sAtxt6uvcCCjtTtEbkUo4kA7ZnTxqFQgMxpNwx5Wr3NyRbnfB
License:     GPL-3.0-or-later
```

# Porting notes

Every workaround: symptom, cause, fix, file. Newest last.

## 1. No AVX without `-march`

- Symptom: `orbis-clang -dM -E` shows `__tune_k8__` and no `__AVX__`.
- Cause: the SDK's target triple is `x86_64-scie-ps4`, which clang treats as
  generic x86-64, not as its built-in PS4 target (whose default CPU is btver2).
- Fix: pass `-march=btver2 -mtune=btver2` everywhere.
- File: `probes/*/Makefile`; `ps4/toolchain.cmake` in Phase 2.

## 2. `sys/cpuset.h` does not compile on its own

- Symptom: `use of undeclared identifier 'NBBY'`, `unknown type name 'cpusetid_t'`.
- Cause: the SDK's FreeBSD 9 headers expect `sys/types.h` and `sys/param.h`
  before `sys/cpuset.h`, as FreeBSD 9 itself did.
- Fix: include them first.
- File: `probes/memprobe/main.cpp`.
