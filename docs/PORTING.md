```text
Document:    PS4LM Porting Notes
Version:     v1.2.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      34ccd19cfe6b36578b8a9d160fab77393b9b1f37d6f075fd7f76861e177d3d19
Chain:       solana-mainnet
Tx:          3gG3Vujc3PKTm2UaiCnhar1GYN4J5F8mbBUBzfmgZm6s798uYQLVeSTigPGcHBKc2B6uiXsP7XYqpUXkRHFj5QyS
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

## 3. llamafile's sgemm does not compile for AVX without AVX2

- Symptom: `sgemm.cpp: use of undeclared identifier '_mm256_and_si256'`, and
  `_mm256_shuffle_epi8`, `_mm256_andnot_si256`.
- Cause: in llama.cpp b11608, `tinyBLAS_Q0_AVX`'s helpers `denibble`,
  `bittobyte` and `updot` call AVX2 integer intrinsics with only an
  `__AVX__` guard. Jaguar has AVX, not AVX2.
- Fix: `-DGGML_LLAMAFILE=OFF`. ggml's own kernels do the matrix products; the
  K-quants the model ladder uses never went through tinyBLAS anyway.
- File: `scripts/build-llama.sh`.

## 4. FreeBSD 9 headers use `u_int` without `sys/types.h`

- Symptom: `ifaddrs.h:34: unknown type name 'u_int'` in cpp-httplib.
- Cause: the same as entry 2: the SDK's headers expect `sys/types.h` first.
- Fix: every C and C++ file starts with `ps4/compat/compat.h`
  (`-include`), which includes `sys/types.h`.
- File: `ps4/toolchain.cmake`, `ps4/compat/compat.h`.

## 5. `posix_spawn_file_actions_addchdir_np` missing (PS5LM fix 1, needed)

- Symptom: `subprocess.h:1603: use of undeclared identifier
  'posix_spawn_file_actions_addchdir_np'`.
- Cause: the SDK's libc has `posix_spawn` but not the FreeBSD 13 chdir action.
- Fix: `SUBPROCESS_HAVE_CWD=0`; nothing on the console spawns a process with a
  working directory.
- File: `ps4/CMakeLists.txt`.

## 6. `pipe2` missing

- Symptom: `subprocess.h:782: use of undeclared identifier 'pipe2'`.
- Cause: `subprocess.h` calls `pipe2(fds, O_CLOEXEC)` on every `__FreeBSD__`;
  FreeBSD 9 has no `pipe2` and libkernel does not export it.
- Fix: `pipe2` on `pipe` and `fcntl` (`O_CLOEXEC`, `O_NONBLOCK`), declared in
  `compat.h`.
- File: `ps4/compat/compat.c`, `ps4/compat/compat.h`.

## 7. No `libm` (PS5LM fix 2, needed)

- Symptom: `ld.lld: error: unable to find library -lm`.
- Cause: ggml links `m`; the math functions are in Sony's libc.
- Fix: `MATH_LIBRARY=SceLibcInternal`.
- File: `ps4/CMakeLists.txt`.

## 8. `openat`, `unlinkat`, `fchmodat`, `pathconf` undefined

- Symptom: `ld.lld: error: undefined symbol: openat` (and the other three)
  from libc++'s `std::filesystem` (`operations.cpp.o`).
- Cause: `orbis-clang` links the `libkernel_web` stub by default, and only the
  `libkernel_sys` stub exports these four.
- Fix: link the tools with `kernel_sys` (memprobe does the same).
- File: `ps4/CMakeLists.txt`.

## 9. `posix_madvise` undefined

- Symptom: `ld.lld: error: undefined symbol: posix_madvise` from
  `llama-mmap.cpp`.
- Cause: declared in the SDK's `sys/mman.h`, exported by no stub.
- Fix: `posix_madvise` on `madvise`; the `POSIX_MADV_*` values are the
  `MADV_*` ones.
- File: `ps4/compat/compat.c`.

## 10. `execlp` undefined

- Symptom: `ld.lld: error: undefined symbol: execlp` from
  `ggml_print_backtrace` in `ggml.c`.
- Cause: declared in the SDK's `unistd.h`, exported by no stub. ggml forks and
  starts gdb when it aborts.
- Fix: `execlp` on `execvp`. There is no gdb on the console, so it fails and
  ggml carries on.
- File: `ps4/compat/compat.c`.

## 11. `mbtowc` raises PRX_RUNTIME_ERROR

- Symptom: every C++ tool, and a three-line `std::cout` program, dies before
  `main` with no output. The kernel log (port 3232) shows
  `exception: 0xa0020103 (PRX_RUNTIME_ERROR)`; the backtrace runs
  `ios_base::Init` -> `__stdinbuf<wchar_t>::imbue` ->
  `codecvt<wchar_t>::do_encoding` -> `mbtowc_l` -> Sony's `mbtowc`.
- Cause: Sony's `mbtowc` raises that exception when a payload calls it, with or
  without a string (`probes/libcprobe mbtowc` and `mbtowc_reset`), while
  `setlocale`, `MB_CUR_MAX` and `mbrtowc` work. The same tools printed their
  `--version` twice earlier the same day, so it may depend on console state.
- Fix: PS4LM's own `mbtowc` on `mbrtowc`. A definition in the program wins
  over the shared library's, so the tools no longer import `mbtowc`.
- File: `ps4/compat/compat.c`.
- Status: works on the console (2026-10-20): `llama-cli --version`,
  `llama-server --version`, `llama-bench --help` and `--list-devices` all
  print.

## 12. Shallow submodule: build 1 and the "latest" web UI

- Symptom: `llama-cli --version` says `build 1`; configure prints
  `UI: download dist.tar.gz.sha256 from b1 failed` and embeds the `latest` UI.
- Cause: llama.cpp counts commits for its build number, and the UI version is
  `b<build number>`. A shallow submodule has one commit.
- Fix: `build-llama.sh` passes `LLAMA_BUILD_NUMBER` from the pinned tag
  (b11608) and stops if the submodule is at another commit.
- File: `scripts/build-llama.sh`.

## 13. Thread count and busy-wait defaults (patches/0001)

- Symptom: none yet; the guard comes from PS5LM, where llama.cpp's default
  thread count plus busy-wait polling took every allowed CPU and the console
  shut itself off. Here the default was 4 threads (half of the 8 CPUs the OS
  reports) on a payload that may use 2, with `--poll 50`.
- Fix: under `PS4LM`, `common_cpu_get_num_math()` returns the CPUs in the
  payload's affinity mask minus 2, at least 1 (so 1 here), and `--poll`
  defaults to 0 in `common`, `llama-bench` and ggml's own thread-pool
  defaults. `-t` and `--poll` still override.
- File: `patches/0001-ps4-safe-cpu-defaults.patch`.

## 14. ggml's thread affinity and priority do nothing (patches/0002)

- Symptom: `-C`, `--cpu-strict` and `--prio` were accepted and ignored.
- Cause: ggml-cpu.c has affinity and priority code for Windows, Apple and
  Linux only; the PS4 target falls into its "unsupported platforms" branch.
- Fix: a `PS4LM` branch. Affinity is FreeBSD's `cpuset_setaffinity` on the
  calling thread. Priority is only ever lowered: `--prio -1` sets Sony's
  lowest thread priority (767; payload threads start at 700, and a lower
  number runs first), higher values are ignored, and `set_process_priority`
  warns and keeps normal instead of raising the nice value. The calling
  (main) thread is never pinned; only ggml's worker threads are. The payload
  cannot widen its mask (`probes/cpuprobe`, CONSOLE.md), so a mask outside
  CPUs 6 and 7 just warns.
- File: `patches/0002-ps4-thread-affinity-priority.patch`.

## 15. mmap'd weights generate about 13x slower

- Symptom: `llama-bench` on Qwen3 0.6B Q4_K_M, 1 thread: 4.74 t/s prompt,
  0.34 t/s generation with the default load mode (mmap); 8.83 and 4.32 t/s
  with `-lm none`.
- Cause: not found. Phase 0 measured file mmap faulting in at 48 MB/s, and
  generation behaves as if the 493 MiB of weights are read again for every
  token rather than staying in memory.
- Fix: read the weights into memory with `-lm none`. At b11608 that is the
  flag for every tool; `--no-mmap` is gone (Phase 4).
- File: `scripts/bench.sh`.

## 16. SIGPIPE killed llama-server when the Mac hung up

- Symptom: stopping `scripts/run.sh` (its socat) while `llama-server` ran
  killed the server on its next log line.
- Cause: the tool's stdout goes through ps4/compat/log.c to elfldr's socket
  and the Mac's log listener. A write to a closed socket raises SIGPIPE,
  whose default action ends the process.
- Fix: both sockets set `SO_NOSIGPIPE`; a write then just fails.
- File: `ps4/compat/log.c`.
- Status: works on the console (2026-10-20): run.sh's socat killed, the next
  chat answered and `/health` stayed ok.

## 17. Every payload is named "Payload"; kinfo_proc is not the SDK's size

- Symptom: a process list shows each tool as `Payload`, so a name cannot pick
  the one to stop. Stepping the `KERN_PROC` list by `sizeof(struct
  kinfo_proc)` reads garbage after the first record.
- Fix: log.c writes the tool's pid to `/data/PS4LM/run/<tool>.pid`, and
  `probes/stop` sends SIGTERM to that pid only while it is still a
  `Payload`. It steps the list by each record's `ki_structsize`.
- Files: `ps4/compat/log.c`, `probes/stop/main.c`.
- Status: works (`scripts/run.sh stop llama-server` shuts it down cleanly).

## 18. `localeconv` has an empty decimal point: JSON numbers with a fraction abort llama-server

- Symptom: the first chat that sent `"temperature": 0.7` killed the server.
  Log: `json.hpp:8323: GGML_ASSERT(endptr == token_buffer.data() +
  token_buffer.size()) failed`. Requests with only whole numbers worked,
  which is why the web UI and the Phase 4 curl tests had not hit it.
- Cause: Sony's `localeconv()->decimal_point` is `""` in a payload, even
  after the server's `setlocale(LC_NUMERIC, "C")` (`probes/libcprobe
  localeconv` prints 0). nlohmann/json writes that character in place of
  each `.` before `strtod`, so `strtod` stops at the fraction and the
  lexer's check fails. Sony's `strtod` itself is fine (`strtod_end` = 3 for
  "0.7").
- Fix: PS4LM's own `localeconv`, returning the "C" locale's values.
- File: `ps4/compat/compat.c`.
- Status: works on the console (2026-10-21): the long chats in BENCH.md send
  `temperature` 0.7 on every turn.

## Checked and not needed

- `accept4` (PS5LM fix 3): the SDK's `sys/socket.h` has no `SOCK_CLOEXEC`, so
  cpp-httplib calls plain `accept`.
- Exit path (PS5LM fix 4, `--wrap=main` and `_Exit`): `llama-cli --version`
  and `llama-server --version` returned from `main` and exited with nothing in
  the kernel log. Look again once a model runs (Phase 4), since PS5LM saw the
  heap error only after real work.
- Passing `CMAKE_C_FLAGS` or `CMAKE_CXX_FLAGS` to CMake replaces the
  toolchain's `*_FLAGS_INIT`, `-march=btver2` included. `build-llama.sh` never
  sets them.

## Crashes seen on the console, 2026-10-20

- The first `llama-bench --help` died with SIGSEGV (fault address 0x69) in
  `ggml_backend_load_best`, which scans the executable's directory and `/` for
  backend `.so` files. After fix 11, `llama-bench --help` and
  `--list-devices` run clean, so this was most likely the same broken
  `mbtowc`. Not seen again.
- Payload addresses in the kernel log are the ELF's addresses plus
  `0x200000000 + 0x4000` (the first PT_LOAD is file offset 0x4000 at
  vaddr 0). `llvm-symbolizer --obj=<tool>.elf <address - 0x200004000>`.
- After about seven crashed payloads, FTP (2121), elfldr (9021) and the kernel
  log (3232) stopped answering while the console still answered ping. GoldHEN
  has to be loaded again.
- The compiler's PS4 target makes C++ `wchar_t` 16-bit; the SDK's C headers
  say `int`. Wide-character calls between libc++ and Sony's libc may disagree
  on size. llama.cpp's tools barely use them; watch for it.

## Imports (Phase 2 acceptance)

`scripts/build-llama.sh` writes each tool's import list to
`build/llama-ps4/<tool>.imports` (`llvm-readelf --dyn-syms`, undefined,
non-weak) and fails if any import is not exported by a stub the tool names as
NEEDED. All three tools need `libkernel_sys`, `libkernel_web`,
`libSceLibcInternal` and `libSceNet`, like memprobe. `llama-server` has 260
imports, `llama-bench` 244, `llama-cli` 267; together, at llama.cpp b11608
with patches 0001 and 0002:

### libkernel_sys only (8)

```text
fchmodat fork link openat pathconf readlink symlink unlinkat
```

### libkernel (libkernel_web and libkernel_sys) (98)

```text
__error __inet_ntop __inet_pton _close _open _read accept bind chdir
clock_gettime close connect cpuset_getaffinity cpuset_setaffinity dlclose dlerror dlopen dlsym dup dup2 environ
execvp fchmod fcntl fstat fsync ftruncate getpeername getpid getrlimit
getsockname getsockopt gettimeofday getuid ioctl kill listen lseek lstat
madvise mkdir mlock mmap munmap nanosleep open pipe poll
pthread_cond_broadcast pthread_cond_destroy pthread_cond_init
pthread_cond_signal pthread_cond_timedwait pthread_cond_wait pthread_create
pthread_detach pthread_getschedparam pthread_getspecific pthread_join pthread_key_create
pthread_mutex_destroy pthread_mutex_init pthread_mutex_lock
pthread_mutex_trylock pthread_mutex_unlock pthread_mutexattr_destroy
pthread_mutexattr_init pthread_mutexattr_settype pthread_once
pthread_rwlock_rdlock pthread_rwlock_unlock pthread_rwlock_wrlock
pthread_self pthread_setschedparam pthread_setspecific pthread_sigmask read recv rename select send
setpriority setsockopt shutdown sigaction sigaddset sigemptyset signal socket
stat sysconf sysctl tcgetattr tcsetattr truncate utimes waitpid write
```

### libSceLibcInternal (158)

```text
__cxa_atexit __divti3 __inet_aton __isinff __isnanf __isthreaded __mb_cur_max
__stderrp __stdinp __stdoutp _Exit abort atexit atof atoi atol calloc clock
closedir cosf erff exit exp exp2f expf expm1f fclose fdopen fdopendir feof
ferror fflush fgetc fgets fgetwc fileno fmod fopen fprintf fputc fputs fread
free freeifaddrs fscanf fseek fseeko ftell ftello fwrite getc getenv
getifaddrs isalnum isalpha islower isprint isspace isupper iswspace isxdigit
ldexp ldexpf localeconv localtime log log10 log2 log2f logf lround lroundf
malloc mbrlen mbrtowc mbsrtowcs memchr memcmp memcpy memmove memset opendir
posix_memalign posix_spawn posix_spawn_file_actions_addclose
posix_spawn_file_actions_adddup2 posix_spawn_file_actions_destroy
posix_spawn_file_actions_init posix_spawnp pow powf printf putc putchar puts
qsort rand readdir realloc realpath remove setenv setlocale setvbuf sin sinf
snprintf sscanf statvfs strcasecmp strchr strcmp strcoll strcpy strcspn
strdup strerror strerror_r strftime strlcpy strlen strncmp strncpy strrchr
strstr strtod strtof strtol strtold strtoll strtoul strtoull strxfrm swprintf
tanhf time tolower toupper ungetc unsetenv vasprintf vfprintf vsnprintf
vsscanf wcrtomb wcscoll wcslen wcsrtombs wcstod wcstof wcstol wcstold wcstoll
wcstoul wcstoull wcsxfrm wmemchr wmemcmp
```

### libSceNet (8)

```text
sceNetErrnoLoc sceNetInit sceNetPoolCreate sceNetPoolDestroy
sceNetResolverCreate sceNetResolverDestroy sceNetResolverStartAton
sceNetResolverStartNtoa
```

Nothing unexpected: libc, pthreads, sockets and files. The few that look
odd have a reason: `fork` and `execvp` are ggml's crash backtrace (entry 10);
`posix_spawn*` and `kill` are `common/subproc.cpp`, which the server uses for
its tools and MCP servers; `dlopen` is ggml's backend loader, which finds no
`.so` in a static build; `sysctl` is the SDK's own `arc4random`; the
`sceNetResolver*` and `sceNetPool*` calls are the SDK's own `getaddrinfo`.
The one weak import, `__cxa_thread_atexit_impl`, stays unresolved and
libc++abi falls back.
