```text
Document:    PS4LM Console Notes
Version:     v1.2.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      d40853a4917b3940b88d552c50173a9dda8f1a9f4d3773a8111c94843df7ee2e
Chain:       solana-mainnet
Tx:          4JVvHLZYsPdhN2o2f2ciKTW8mZzARBYECiyynyrE3r4dkQSTNZZuDBMPmFb2LrRyBtFQymy1Ddtwb4fa2t7HGTJr
License:     GPL-3.0-or-later
```

# Console

## The test console

| Field | Value |
|---|---|
| Model | Base (original) PS4, CUH-1115A B02 (from `sceKernelGetHwModelName`) |
| Firmware | 11.02 (`0x11020001`) |
| Homebrew enabler | GoldHEN, loaded from the browser (BinLoader 9090, FTP 2121, klog 3232, web file browser 8080) |
| CPU | AMD Jaguar, 8 cores in two 4-core clusters, 1.6 GHz. A payload may run on 2 of them |
| Memory | 8 GB GDDR5, shared. A payload gets about 3.5 GiB of malloc |

## What a payload gets

From the first console run, 2026-10-20:

- **CPUs: 2.** The OS reports 8, but the payload's affinity mask allows only
  cores 6 and 7, and the payload may not widen it: `cpuset_setaffinity` to
  all 8 fails with EPERM for the thread and for the process (cpuprobe below).
  Widening would need a kernel change, which is out of scope. This is the
  biggest limit for llama.cpp. The tools default to 1 thread (patches/0001).
- **Thread priority:** payload threads run with policy 3 (round robin) at
  Sony priority 700, where 256 is the highest and 767 the lowest. The shell
  UI runs at 605, yet at 2 normal-priority threads its heartbeat lapsed for
  up to 32 s; at `--prio -1` (767) it did not (BENCH.md).
- **Memory pressure:** a ~1 GB payload (493 MiB model read into memory) pushed
  about 200 MB of the system to swap (`VM Stats ... swap out` in the kernel
  log). The payload's memory is the shell's "flexible" memory.
- **Memory: about 3.5 GiB.** The largest single malloc that could be touched was
  3.56 GiB; many 256 MiB blocks stopped at 3.50 GiB (`errno 22`). malloc
  accepts 6 GiB but touching it fails, so always touch, never trust malloc.
  Model weights + KV cache must fit in ~3.4 GiB.
- **Model files: mmap works, but slowly.** A 1 GiB file in `/data/PS4LM`
  mapped and faulted in with no bad pages, at 48 MB/s. Writing it ran at
  45 MB/s. At that rate a 2 GiB model takes about 45 s to load from `/data`.
  llama.cpp generates about 13x slower from mmap'd weights than from weights
  read into memory (PORTING.md 15), so the tools run without mmap.
- **FTP upload from the Mac:** a 499 MiB model went up in 8 s (~63 MB/s).
- **Bandwidth:** memcpy 1.67 GB/s on 1 thread, 4.52 GB/s on 2.
- **Compute:** AVX fp32 dot product 5.70 GFLOPS on 1 thread, 10.26 on 2.
  No AVX2 and no FMA, as expected for Jaguar.
- **Storage:** 189.7 GiB free under `/data`; a USB drive shows at `/mnt/usb0`.

## Load path that works

1. The owner loads GoldHEN from the PS4 browser.
2. `scripts/console-setup.sh` sends `elfldr.bin` (not `.elf`: GoldHEN's
   BinLoader logs `Error handling payload` for the ELF) to 9090. elfldr then
   serves on 9021 until the console reboots.
3. `scripts/send.sh build/probes/<probe>.elf` sends a payload through elfldr
   and streams its stdout back.

The BinLoader takes one connection and then stops listening. A port check
(`nc -z`) counts as that connection, so never test 9090; send straight away.
If 9090 refuses, switch the BinLoader off and on in GoldHEN > Server Settings.

## hello

`scripts/send.sh build/probes/hello.elf`, verbatim:

```text
PS4LM hello: firmware 0x11020001, pid 72, unix time 1792526469
hello: wrote /data/PS4LM/hello.txt
hello: sent the line to 192.168.50.254:9999
```

The same line reached `scripts/logs.sh` on the Mac.

## cpuprobe

`scripts/run.sh cpuprobe`, 2026-10-20, verbatim:

```text
hw.ncpu                8
process mask           6 7
thread mask            6 7
main schedparam        rc 0 policy 3 priority 700
main pthread_getprio   700
getpriority            0 errno 0
new thread mask        6 7
new thread prio        700
widen thread           rc -1 errno 1
  now                  6 7
widen process          rc -1 errno 1
  now                  6 7
process mask at end    6 7
```

## memprobe

`scripts/send.sh build/probes/memprobe.elf`. The output below is verbatim,
except that the system libc's heap report after each refused malloc (6.00 to
5.44 GiB) is shown once and the nine repeats are cut. The console also keeps it
in `/data/PS4LM/memprobe.txt`.

```text
PS4LM memprobe
== system
fw (kernel)   0x11020001
fw (system)   11.020.001 (0x11020001)
hw model      CUH-1115A B02
cpu freq      1600 MHz
cpu temp      67 C
pid/uid       73/1
direct mem    0.75 GiB (whole console)
flexible cfg  0.75 GiB (rc=0x0)
flexible free 0.74 GiB (rc=0x0)
== cpuid
cpu           DG1002FGF84HT                                   (AuthenticAMD)
features      sse4.2=1 avx=1 f16c=1 bmi1=1 avx2=0 fma=0 bmi2=0 avx512f=0
expected      sse4.2=1 avx=1 f16c=1 bmi1=1 avx2=0 fma=0 bmi2=0 avx512f=0
== cpus
sysctl hw.ncpu         8
hardware_concurrency   8
sysconf online         8
affinity mask          2 allowed: 6,7
== storage
/data         nullfs, 189.7 GiB free of 413.5 GiB
/user         ufs, 189.7 GiB free of 413.5 GiB
/mnt/usb0     exfatfs, 29.0 GiB free of 29.1 GiB
/mnt/ext0     tmpfs, 0.0 GiB free of 0.0 GiB
== largest single malloc (binary search down from 6 GiB, 64 MiB resolution)
malloc says   6.00 GiB
touching      6.00 GiB ...
[SceLibcInternalHeap] A too large request for the heap.
[SceLibcInternalHeap] The requested size cannot be allocated.
[SceLibcInternalHeap]  - Requested size       : 0x180000000 (6442450944)
[SceLibcInternalHeap] Sum of the Size of Chunks:
[SceLibcInternalHeap]  - Current Free Chunks  : 0x0001eda0 (126368)
[SceLibcInternalHeap]  - Current Inuse Chunks : 0x00011260 (70240)
[SceLibcInternalHeap]  - Peak of Inuse Chunks : 0x00011260 (70240)
[SceLibcInternalHeap] System Size:
[SceLibcInternalHeap]  - Current System Size  : 0x00030000 (196608)
[SceLibcInternalHeap]  - Peak of System Size  : 0x00030000 (196608)
[SceLibcInternalHeap]  - Maximum System Size  : 0x100000000 (4294967296)
[SceLibcInternalHeap] backtrace:
[SceLibcInternalHeap]    0x00000008000bcae8 => libSceLibcInternal.sprx+0x2cae8
[SceLibcInternalHeap]    0x00000008000c3c4c => libSceLibcInternal.sprx+0x33c4c
[SceLibcInternalHeap]    0x00000008000b9143 => libSceLibcInternal.sprx+0x29143
[SceLibcInternalHeap]    0x00000008000b8d6a => libSceLibcInternal.sprx+0x28d6a
[SceLibcInternalHeap]    0x0000000200004a2d => (NoName)SceSpZeroConfMain+0xa2d
[SceLibcInternalHeap]    0x0000000200037eb7 => (NoName)SceSpZeroConfMain+0x33eb7
[SceLibcInternalHeap]    0x0000000000400592 => executable+0x592
[SceLibcInternalHeap]    0x0000000000000000
[SceLibcInternalHeap] pid:<0x49> tid:<0x18a64>
touching      5.94 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.88 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.81 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.75 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.69 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.63 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.56 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.50 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.44 GiB ...
[... the same SceLibcInternalHeap report, cut ...]
touching      5.38 GiB ...
touching      5.31 GiB ...
touching      5.25 GiB ...
touching      5.19 GiB ...
touching      5.13 GiB ...
touching      5.06 GiB ...
touching      5.00 GiB ...
touching      4.94 GiB ...
touching      4.88 GiB ...
touching      4.81 GiB ...
touching      4.75 GiB ...
touching      4.69 GiB ...
touching      4.63 GiB ...
touching      4.56 GiB ...
touching      4.50 GiB ...
touching      4.44 GiB ...
touching      4.38 GiB ...
touching      4.31 GiB ...
touching      4.25 GiB ...
touching      4.19 GiB ...
touching      4.13 GiB ...
touching      4.06 GiB ...
touching      4.00 GiB ...
touching      3.94 GiB ...
touching      3.88 GiB ...
touching      3.81 GiB ...
touching      3.75 GiB ...
touching      3.69 GiB ...
touching      3.63 GiB ...
touching      3.56 GiB ...
touched       3.56 GiB in 12.20 s
single max    3.56 GiB
== total malloc in 256 MiB blocks (pages touched)
  0.25 GiB allocated and touched
  0.50 GiB allocated and touched
  0.75 GiB allocated and touched
  1.00 GiB allocated and touched
  1.25 GiB allocated and touched
  1.50 GiB allocated and touched
  1.75 GiB allocated and touched
  2.00 GiB allocated and touched
  2.25 GiB allocated and touched
  2.50 GiB allocated and touched
  2.75 GiB allocated and touched
  3.00 GiB allocated and touched
  3.25 GiB allocated and touched
  3.50 GiB allocated and touched
  stop at 3.50 GiB: malloc errno=22
total max     3.50 GiB
== mmap of a 1 GiB file in /data/PS4LM
write         1.00 GiB in 23.79 s: 45 MB/s
file mmap     ok, 1.00 GiB faulted in 22.22 s: 48 MB/s, 0 bad pages
== memcpy bandwidth
buffer        2 x 512 MiB
memcpy        1 threads: 1.67 GB/s
memcpy        2 threads: 4.52 GB/s
== AVX fp32 dot product (mul + add, no FMA)
dot           1 thread:  5.70 GFLOPS
dot           2 threads: 10.26 GFLOPS total
== summary
cpus allowed 2, single malloc 3.56 GiB, total malloc 3.50 GiB, 1 GiB file mmap ok
memcpy 1.67 GB/s (1 thread), 4.52 GB/s (2 threads); AVX dot 5.70 GFLOPS (1 thread), 10.26 (2 threads)
/data free 189.7 GiB
log: /data/PS4LM/memprobe.txt
```
