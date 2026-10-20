```text
Document:    PS4LM Porting Notes
Version:     v1.1.0
Author:      Celaya Solutions
Contact:     hello@celayasolutions.com
Date:        2026-10-20
SHA256:      414a3a29c58e3f923f63ab1b54162d614116f97101e0c4faf72dae75e0e9ef46
Chain:       solana-mainnet
Tx:          4YbMzG6w8MXjSoU5Rhqwaf322DgN4r3uX1YtQJWdXA28G3WhJ19pxQwWWACWyhiZ1u8HeRCT5NG7xpocmxBDPgZX
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
- Status: built; not yet run on the console (it went offline, see below).

## 12. Shallow submodule: build 1 and the "latest" web UI

- Symptom: `llama-cli --version` says `build 1`; configure prints
  `UI: download dist.tar.gz.sha256 from b1 failed` and embeds the `latest` UI.
- Cause: llama.cpp counts commits for its build number, and the UI version is
  `b<build number>`. A shallow submodule has one commit.
- Fix: `build-llama.sh` passes `LLAMA_BUILD_NUMBER` from the pinned tag
  (b11608) and stops if the submodule is at another commit.
- File: `scripts/build-llama.sh`.

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

## Open: crashes seen on the console, 2026-10-20

- The first `llama-bench --help` died with SIGSEGV (fault address 0x69) in
  `ggml_backend_load_best`, which scans the executable's directory and `/` for
  backend `.so` files. A probe walking `/` and `/data/PS4LM/bin` with
  `std::filesystem::directory_iterator` the same way ran clean, so the cause is
  not found yet. Rerun after fix 11.
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
`libSceLibcInternal` and `libSceNet`, like memprobe. `llama-server` has 256
imports, `llama-bench` 240, `llama-cli` 263; together, at llama.cpp b11608:

### libkernel_sys only (8)

```text
fchmodat fork link openat pathconf readlink symlink unlinkat
```

### libkernel (libkernel_web and libkernel_sys) (94)

```text
__error __inet_ntop __inet_pton _close _open _read accept bind chdir
clock_gettime close connect dlclose dlerror dlopen dlsym dup dup2 environ
execvp fchmod fcntl fstat fsync ftruncate getpeername getpid getrlimit
getsockname getsockopt gettimeofday getuid ioctl kill listen lseek lstat
madvise mkdir mlock mmap munmap nanosleep open pipe poll
pthread_cond_broadcast pthread_cond_destroy pthread_cond_init
pthread_cond_signal pthread_cond_timedwait pthread_cond_wait pthread_create
pthread_detach pthread_getspecific pthread_join pthread_key_create
pthread_mutex_destroy pthread_mutex_init pthread_mutex_lock
pthread_mutex_trylock pthread_mutex_unlock pthread_mutexattr_destroy
pthread_mutexattr_init pthread_mutexattr_settype pthread_once
pthread_rwlock_rdlock pthread_rwlock_unlock pthread_rwlock_wrlock
pthread_self pthread_setspecific pthread_sigmask read recv rename select send
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
