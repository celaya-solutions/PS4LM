```text
Document:    PS4LM Console Notes
Version:     v1.0.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      8287f434129c161b9449eaa48a7359eb7768044653588e8b9831d64932dd6a3f
Chain:       solana-mainnet
Tx:          5sWgRncLe1menq8pkdkCDdfynp6V27iPBwj2Y3buJemfC6SrowfM8c5vHYoFXHT7WYQvG6pENR7AD3HpDFEDPMBe
License:     GPL-3.0-or-later
```

# Console

## The test console

| Field | Value |
|---|---|
| Model | Base (original) PS4. Exact CUH number: [pending, from the label] |
| Firmware | 11.02 |
| Homebrew enabler | GoldHEN (BinLoader 9090, FTP 2121, klog 3232) |
| CPU | AMD Jaguar, 8 cores in two 4-core clusters, 1.6 GHz |
| Memory | 8 GB GDDR5, shared. What a payload gets: see memprobe below |

## memprobe

`scripts/send.sh build/probes/memprobe.elf`. The output below is verbatim.
The console also keeps it in `/data/PS4LM/memprobe.txt`.

[pending: first console run]
