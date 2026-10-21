/* PS4LM stop: lists processes, or asks a running tool to quit.
 *
 * With no arguments it prints every process it can see (pid and name). With a
 * tool name, it reads the pid ps4/compat/log.c saved in
 * /data/PS4LM/run/<tool>.pid and sends that process SIGTERM, so a long-running
 * tool such as llama-server shuts down cleanly. Every payload is named
 * "Payload" in the process list, so the pid file picks the tool; the process
 * must still be a Payload, or nothing is sent. A tool still running 10 s after
 * SIGTERM gets SIGKILL.
 *
 *   scripts/run.sh stop
 *   scripts/run.sh stop llama-server
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>

#define RUN_DIR "/data/PS4LM/run"
#define STOP_WAIT_MS 10000 /* after SIGTERM, before SIGKILL */

/* The name of process pid, or NULL if it is gone. With list set, prints every
 * process instead. */
static const char *proc_name(pid_t pid, int list) {
    static char name[COMMLEN + 1];
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC};
    size_t len = 0;

    if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0) {
        printf("stop: sysctl size failed (errno %d)\n", errno);
        return NULL;
    }
    len += len / 4;
    char *buf = malloc(len);
    if (!buf || sysctl(mib, 3, buf, &len, NULL, 0) != 0) {
        printf("stop: sysctl list failed (errno %d)\n", errno);
        free(buf);
        return NULL;
    }

    /* Step by the kernel's record size: the console's kinfo_proc is not the
     * size of the SDK header's. */
    const struct kinfo_proc *first = (const struct kinfo_proc *)buf;
    const size_t step = len ? (size_t)first->ki_structsize : 0;
    const char *found = NULL;
    if (step < offsetof(struct kinfo_proc, ki_comm) + sizeof(first->ki_comm)) {
        printf("stop: kinfo_proc size %zu, header %zu\n", step, sizeof(*first));
        free(buf);
        return NULL;
    }
    for (size_t off = 0; off + step <= len; off += step) {
        const struct kinfo_proc *p = (const struct kinfo_proc *)(buf + off);
        if (list) {
            printf("%6d %s\n", (int)p->ki_pid, p->ki_comm);
        } else if (p->ki_pid == pid) {
            snprintf(name, sizeof(name), "%s", p->ki_comm);
            found = name;
            break;
        }
    }
    free(buf);
    return found;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        proc_name(0, 1);
        return 0;
    }

    const char *tool = argv[1];
    char path[256];
    snprintf(path, sizeof(path), RUN_DIR "/%s.pid", tool);
    FILE *f = fopen(path, "r");
    int pid = 0;
    if (!f || fscanf(f, "%d", &pid) != 1 || pid <= 0) {
        printf("stop: no pid in %s\n", path);
        if (f)
            fclose(f);
        return 1;
    }
    fclose(f);

    const char *name = pid == getpid() ? NULL : proc_name(pid, 0);
    if (!name || strcmp(name, "Payload") != 0) {
        printf("stop: %s pid %d is not running (%s)\n", tool, pid, name ? name : "gone");
        unlink(path);
        return 1;
    }
    if (kill(pid, SIGTERM) != 0) {
        printf("stop: kill %d failed (errno %d)\n", pid, errno);
        return 1;
    }
    printf("stop: sent SIGTERM to %s, pid %d\n", tool, pid);

    /* llama-server can hang in its exit path after closing its port (seen
     * 2026-10-21, "cleaning up before exit..."), still holding its memory. */
    for (int i = 0; i < STOP_WAIT_MS / 100; i++) {
        usleep(100 * 1000);
        if (!proc_name(pid, 0)) {
            printf("stop: %s exited\n", tool);
            unlink(path);
            return 0;
        }
    }
    if (kill(pid, SIGKILL) != 0) {
        printf("stop: SIGKILL %d failed (errno %d)\n", pid, errno);
        return 1;
    }
    printf("stop: %s still running after %d ms, sent SIGKILL\n", tool, STOP_WAIT_MS);
    unlink(path);
    return 0;
}
