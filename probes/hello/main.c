/* PS4LM hello: the Phase 0 plumbing probe.
 *
 * Writes one line to stdout (elfldr streams it back to scripts/send.sh), to
 * /data/PS4LM/hello.txt, and to the Mac's log listener (scripts/logs.sh) at
 * PS4LM_LOG_HOST:PS4LM_LOG_PORT. The host and port are baked in at build time
 * from scripts/env.sh; argv[1] and argv[2] override them.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <ps4/kernel.h>

#ifndef PS4LM_LOG_HOST
#define PS4LM_LOG_HOST ""
#endif
#ifndef PS4LM_LOG_PORT
#define PS4LM_LOG_PORT 9999
#endif

#define OUT_DIR  "/data/PS4LM"
#define OUT_FILE OUT_DIR "/hello.txt"

static int send_line(const char *host, int port, const char *line) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
        return -1;

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == 0 && write(fd, line, strlen(line)) != (ssize_t)strlen(line))
        rc = -1;
    const int saved = errno;
    close(fd);
    errno = saved;
    return rc;
}

int main(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : PS4LM_LOG_HOST;
    const int port = argc > 2 ? atoi(argv[2]) : PS4LM_LOG_PORT;
    char line[256];

    snprintf(line, sizeof(line), "PS4LM hello: firmware 0x%08x, pid %d, unix time %ld\n",
             kernel_get_fw_version(), (int)getpid(), (long)time(NULL));
    printf("%s", line);

    mkdir(OUT_DIR, 0777);
    FILE *f = fopen(OUT_FILE, "w");
    if (f) {
        fputs(line, f);
        fclose(f);
        printf("hello: wrote %s\n", OUT_FILE);
    } else {
        printf("hello: cannot write %s (errno %d)\n", OUT_FILE, errno);
    }

    if (!host[0])
        printf("hello: no log host set, skipped the socket\n");
    else if (send_line(host, port, line) == 0)
        printf("hello: sent the line to %s:%d\n", host, port);
    else
        printf("hello: cannot reach %s:%d (errno %d), is scripts/logs.sh running?\n", host, port, errno);

    fflush(stdout);
    return 0;
}
