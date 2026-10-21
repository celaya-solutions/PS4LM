/* Functions the PS4's libc and libkernel don't export, built on ones they do.
 * ps4/CMakeLists.txt compiles this into every llama.cpp tool; compat.h
 * declares them.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wchar.h>

#include "compat.h"

static int set_fd_flags(int fd, int flags) {
    if (flags & O_CLOEXEC) {
        const int fl = fcntl(fd, F_GETFD);
        if (fl == -1 || fcntl(fd, F_SETFD, fl | FD_CLOEXEC) == -1)
            return -1;
    }
    if (flags & O_NONBLOCK) {
        const int fl = fcntl(fd, F_GETFL);
        if (fl == -1 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1)
            return -1;
    }
    return 0;
}

/* FreeBSD 10; libkernel only exports pipe. subprocess.h (llama.cpp's
 * common/subproc) calls pipe2(fds, O_CLOEXEC) on every __FreeBSD__. */
int pipe2(int fds[2], int flags) {
    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        errno = EINVAL;
        return -1;
    }
    if (pipe(fds) == -1)
        return -1;
    if (set_fd_flags(fds[0], flags) == -1 || set_fd_flags(fds[1], flags) == -1) {
        const int saved = errno;
        close(fds[0]);
        close(fds[1]);
        errno = saved;
        return -1;
    }
    return 0;
}

/* libSceLibcInternal exports mbtowc, but in a payload it raises
 * PRX_RUNTIME_ERROR on every call (probes/libcprobe), and libc++'s iostream
 * setup calls it through codecvt<wchar_t>::do_encoding before main. mbrtowc
 * works, so this one replaces Sony's: the linker takes a definition in the
 * program over the shared library's. */
int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n) {
    static mbstate_t state;
    if (s == NULL) {
        memset(&state, 0, sizeof state);
        return 0; /* no state-dependent encodings */
    }
    const size_t r = mbrtowc(pwc, s, n, &state);
    if (r == (size_t)-1 || r == (size_t)-2) {
        memset(&state, 0, sizeof state);
        errno = EILSEQ;
        return -1;
    }
    return (int)r;
}

/* Sony's localeconv returns an empty decimal_point in a payload, even after
 * setlocale(LC_NUMERIC, "C") (probes/libcprobe). nlohmann/json puts that
 * character in place of each '.' before strtod, so every JSON number with a
 * fraction (llama-server's "temperature": 0.7) failed its endptr assert and
 * aborted the server. This one is the "C" locale's, which is the only one a
 * payload uses. */
struct lconv *localeconv(void) {
    static char dot[] = ".", empty[] = "";
    static struct lconv c = {
        .decimal_point = dot, .thousands_sep = empty, .grouping = empty,
        .int_curr_symbol = empty, .currency_symbol = empty,
        .mon_decimal_point = empty, .mon_thousands_sep = empty,
        .mon_grouping = empty, .positive_sign = empty, .negative_sign = empty,
        .int_frac_digits = CHAR_MAX, .frac_digits = CHAR_MAX,
        .p_cs_precedes = CHAR_MAX, .p_sep_by_space = CHAR_MAX,
        .n_cs_precedes = CHAR_MAX, .n_sep_by_space = CHAR_MAX,
        .p_sign_posn = CHAR_MAX, .n_sign_posn = CHAR_MAX,
        .int_p_cs_precedes = CHAR_MAX, .int_n_cs_precedes = CHAR_MAX,
        .int_p_sep_by_space = CHAR_MAX, .int_n_sep_by_space = CHAR_MAX,
        .int_p_sign_posn = CHAR_MAX, .int_n_sign_posn = CHAR_MAX,
    };
    return &c;
}

/* Declared by the SDK's sys/mman.h, exported by nothing; libkernel exports
 * madvise, and the POSIX_MADV_* values are the MADV_* ones. llama.cpp's
 * llama-mmap.cpp calls it to prefetch the model file. */
int posix_madvise(void *addr, size_t len, int advice) {
    return madvise(addr, len, advice) == -1 ? errno : 0;
}

/* Declared by the SDK's unistd.h, exported by nothing; libkernel exports
 * execvp. ggml_print_backtrace calls it to start gdb, which the console does
 * not have, so this only has to link and fail cleanly. */
int execlp(const char *file, const char *arg, ...) {
    va_list ap;
    int n = 1;
    va_start(ap, arg);
    while (va_arg(ap, const char *) != NULL)
        n++;
    va_end(ap);

    /* execvp takes char *const[]; the strings are not written to. */
    char *argv[n + 1];
    argv[0] = (char *)(uintptr_t)arg;
    va_start(ap, arg);
    for (int i = 1; i <= n; i++)
        argv[i] = va_arg(ap, char *);
    va_end(ap);
    return execvp(file, argv);
}
