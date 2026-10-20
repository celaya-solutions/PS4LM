/* Functions the PS4's libc and libkernel don't export, built on ones they do.
 * ps4/CMakeLists.txt compiles this into every llama.cpp tool; compat.h
 * declares them.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

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
