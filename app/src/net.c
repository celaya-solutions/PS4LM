/* PS4LM app: the few network calls the launcher needs (POSIX sockets, IPv4).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

/* Waits until fd is readable (or writable) or deadline passes; 1 when ready. */
static int wait_fd(int fd, int for_write, long deadline) {
    long left = deadline - now_ms();
    if (left <= 0)
        return 0;
    fd_set s;
    FD_ZERO(&s);
    FD_SET(fd, &s);
    struct timeval tv = {left / 1000, (left % 1000) * 1000};
    return select(fd + 1, for_write ? NULL : &s, for_write ? &s : NULL, NULL, &tv) == 1;
}

/* A connected, non-blocking socket, or -1. Same steps as connect_timeout in
 * ps4/compat/log.c. */
static int connect_to(const char *ip, int port, long deadline) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (!ip || !ip[0] || inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
        return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    int rc = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (rc < 0 && errno == EINPROGRESS) {
        int err = 0;
        socklen_t len = sizeof err;
        if (wait_fd(fd, 1, deadline) && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0)
            rc = 0;
    }
    if (rc < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int write_all(int fd, const char *p, size_t n, long deadline) {
    while (n > 0) {
#ifdef MSG_NOSIGNAL
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
#else
        ssize_t w = send(fd, p, n, 0);
#endif
        if (w > 0) {
            p += w;
            n -= (size_t)w;
        } else if (w < 0 && (errno == EAGAIN || errno == EINTR)) {
            if (!wait_fd(fd, 1, deadline))
                return -1;
        } else {
            return -1;
        }
    }
    return 0;
}

int net_port_open(const char *ip, int port, int timeout_ms) {
    int fd = connect_to(ip, port, now_ms() + timeout_ms);
    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

int net_send_line(const char *ip, int port, const char *line, int timeout_ms) {
    long deadline = now_ms() + timeout_ms;
    int fd = connect_to(ip, port, deadline);
    if (fd < 0)
        return -1;
    int rc = write_all(fd, line, strlen(line), deadline);
    close(fd);
    return rc;
}

int net_http_get(const char *ip, int port, const char *path, char *body, size_t cap, int timeout_ms) {
    long deadline = now_ms() + timeout_ms;
    int fd = connect_to(ip, port, deadline);
    if (fd < 0)
        return -1;
    char req[512], head[4096];
    int n = snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, ip);
    if (n < 0 || (size_t)n >= sizeof req || write_all(fd, req, (size_t)n, deadline) != 0) {
        close(fd);
        return -1;
    }
    /* Headers into head; once the blank line is seen, body bytes go to body
     * (cut at cap - 1) and the rest is read and dropped until the peer closes. */
    size_t h = 0, b = 0;
    int status = -1;
    char *end = NULL;
    if (cap > 0)
        body[0] = '\0';
    for (;;) {
        char buf[2048];
        ssize_t r = recv(fd, buf, sizeof buf, 0);
        if (r < 0 && (errno == EAGAIN || errno == EINTR)) {
            if (!wait_fd(fd, 0, deadline))
                break;
            continue;
        }
        if (r <= 0)
            break;
        const char *p = buf;
        size_t left = (size_t)r;
        if (!end) {
            size_t take = left < sizeof head - 1 - h ? left : sizeof head - 1 - h;
            memcpy(head + h, p, take);
            h += take;
            head[h] = '\0';
            end = strstr(head, "\r\n\r\n");
            if (!end) {
                if (h == sizeof head - 1)
                    break;
                continue;
            }
            int code;
            if (sscanf(head, "HTTP/%*d.%*d %d", &code) == 1)
                status = code;
            size_t body_start = (size_t)(end + 4 - head);
            /* Bytes of this read that landed in head past the blank line. */
            p = buf + (take - (h - body_start));
            left = h - body_start + (left - take);
        }
        while (left > 0 && b + 1 < cap) {
            body[b++] = *p++;
            left--;
        }
        if (cap > 0)
            body[b] = '\0';
    }
    close(fd);
    return status;
}

int slots_busy(const char *json) {
    int n = 0;
    const char *key = "\"is_processing\"";
    for (const char *p = json ? strstr(json, key) : NULL; p; p = strstr(p, key)) {
        p += strlen(key);
        p += strspn(p, " \t\r\n:");
        if (strncmp(p, "true", 4) == 0)
            n++;
    }
    return n;
}
