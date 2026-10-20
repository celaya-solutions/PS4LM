/* Sends a tool's stdout and stderr to the Mac and to a file on the console.
 *
 * At startup, stdout and stderr are pointed at a pipe. A thread reads the pipe
 * and copies every byte to three places:
 *   - the socket elfldr gave the tool as stdout (scripts/send.sh, run.sh),
 *   - the Mac's log listener (scripts/logs.sh), if it answers within a second,
 *   - /data/PS4LM/logs/<tool>-<unix-time>.log, always.
 * The listener's address comes from /data/PS4LM/loghost.txt ("host:port",
 * written by scripts/run.sh), else from PS4LM_LOG_HOST and PS4LM_LOG_PORT at
 * build time. If the socket fails, the file still gets everything. Both
 * sockets are set SO_NOSIGPIPE: a write after the Mac or elfldr hangs up would
 * otherwise raise SIGPIPE and kill the tool (docs/PORTING.md).
 *
 * The tool's pid goes in /data/PS4LM/run/<tool>.pid, for probes/stop: every
 * payload is named "Payload" in the process list, so a name cannot pick one.
 *
 * Link it into a tool and it starts by itself. ps4lm_log_finish() drains the
 * pipe; it runs as a destructor, and tools that leave through _Exit call it
 * first.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

#ifndef PS4LM_LOG_HOST
#define PS4LM_LOG_HOST ""
#endif
#ifndef PS4LM_LOG_PORT
#define PS4LM_LOG_PORT 9999
#endif

#define LOG_DIR       "/data/PS4LM/logs"
#define RUN_DIR       "/data/PS4LM/run"
#define LOGHOST_FILE  "/data/PS4LM/loghost.txt"
#define CONNECT_MS    1000

static int g_orig = -1; /* elfldr's socket, the tool's stdout before us */
static int g_orig_ok = 1; /* cleared once a write to it fails; it stays open */
static int g_sock = -1; /* the Mac's log listener */
static int g_file = -1;
static int g_pipe_rd = -1;
static pthread_t g_pump;
static int g_running;

static int write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        const ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void *pump(void *arg) {
    (void)arg;
    char buf[4096];
    ssize_t n;
    while ((n = read(g_pipe_rd, buf, sizeof(buf))) != 0) {
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (g_orig >= 0 && g_orig_ok && write_all(g_orig, buf, (size_t)n) < 0)
            g_orig_ok = 0;
        if (g_sock >= 0 && write_all(g_sock, buf, (size_t)n) < 0) {
            close(g_sock);
            g_sock = -1;
        }
        if (g_file >= 0)
            write_all(g_file, buf, (size_t)n);
    }
    return NULL;
}

static void no_sigpipe(int fd) {
    const int on = 1;
    if (fd >= 0)
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
}

static void write_pid(const char *tool) {
    char path[256];
    mkdir(RUN_DIR, 0777);
    snprintf(path, sizeof(path), RUN_DIR "/%s.pid", tool);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", (int)getpid());
        fclose(f);
    }
}

/* A non-blocking connect with a timeout, so a Mac that is not listening (or not
 * there) costs at most CONNECT_MS. */
static int connect_timeout(const char *host, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (!host[0] || inet_pton(AF_INET, host, &addr.sin_addr) != 1)
        return -1;

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    const int fl = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set w;
        FD_ZERO(&w);
        FD_SET(fd, &w);
        struct timeval tv = {CONNECT_MS / 1000, (CONNECT_MS % 1000) * 1000};
        int err = 0;
        socklen_t len = sizeof(err);
        rc = -1;
        if (select(fd + 1, NULL, &w, NULL, &tv) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 &&
            err == 0)
            rc = 0;
    }
    if (rc < 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fl);
    return fd;
}

static void log_target(char *host, size_t host_len, int *port) {
    snprintf(host, host_len, "%s", PS4LM_LOG_HOST);
    *port = PS4LM_LOG_PORT;

    FILE *f = fopen(LOGHOST_FILE, "r");
    if (!f)
        return;
    char line[128] = {0};
    if (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = 0;
            snprintf(host, host_len, "%s", line);
            *port = atoi(colon + 1);
        }
    }
    fclose(f);
}

__attribute__((constructor(101))) static void ps4lm_log_start(int argc, char **argv) {
    /* elfldr sets argv[0] to the file name, "hello.elf"; the log is hello-<time>.log. */
    char tool[64] = "tool";
    if (argc > 0 && argv && argv[0]) {
        const char *slash = strrchr(argv[0], '/');
        snprintf(tool, sizeof(tool), "%s", slash ? slash + 1 : argv[0]);
        char *dot = strrchr(tool, '.');
        if (dot && !strcmp(dot, ".elf"))
            *dot = 0;
    }

    mkdir("/data/PS4LM", 0777);
    mkdir(LOG_DIR, 0777);
    char path[256];
    snprintf(path, sizeof(path), LOG_DIR "/%s-%ld.log", tool, (long)time(NULL));
    g_file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    char host[64];
    int port;
    log_target(host, sizeof(host), &port);
    g_sock = connect_timeout(host, port);
    no_sigpipe(g_sock);
    write_pid(tool);

    int fds[2];
    if (pipe(fds) != 0)
        return;
    fflush(stdout);
    fflush(stderr);
    g_orig = dup(STDOUT_FILENO);
    no_sigpipe(g_orig);
    g_pipe_rd = fds[0];
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (pthread_create(&g_pump, NULL, pump, NULL) != 0) {
        dup2(g_orig, STDOUT_FILENO);
        dup2(g_orig, STDERR_FILENO);
        return;
    }
    g_running = 1;
    printf("[ps4lm] %s: log file %s, Mac %s:%d %s\n", tool, g_file >= 0 ? path : "(none)", host[0] ? host : "-",
           port, g_sock >= 0 ? "connected" : "not connected");
}

void ps4lm_log_finish(void) {
    if (!g_running)
        return;
    g_running = 0;
    fflush(stdout);
    fflush(stderr);
    /* Put elfldr's socket back on 1 and 2: that closes the pipe's last write
     * ends, so the pump reads EOF once it has copied everything. */
    const int orig = g_orig >= 0 ? g_orig : open("/dev/null", O_WRONLY);
    dup2(orig, STDOUT_FILENO);
    dup2(orig, STDERR_FILENO);
    pthread_join(g_pump, NULL);
    if (g_sock >= 0)
        close(g_sock);
    if (g_file >= 0) {
        fsync(g_file);
        close(g_file);
    }
}

__attribute__((destructor(101))) static void ps4lm_log_stop(void) { ps4lm_log_finish(); }
