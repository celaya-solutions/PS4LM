/* cpuprobe: what a payload may do with CPU affinity and thread priority.
 * Reads the process and thread masks and priorities, tries to widen the
 * mask to every CPU the OS reports, then puts the old mask back. It never
 * raises a priority and starts no work, so it is safe to run on an idle
 * console.
 *
 *   scripts/run.sh cpuprobe
 *
 * docs/CONSOLE.md has the results.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
/* FreeBSD 9 headers: sys/types.h and sys/param.h come before sys/cpuset.h. */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

int pthread_getprio(pthread_t thread);

static void show(const char *what, cpuwhich_t which) {
    cpuset_t m;
    CPU_ZERO(&m);
    if (cpuset_getaffinity(CPU_LEVEL_WHICH, which, -1, sizeof m, &m) != 0) {
        printf("%-22s unavailable (errno %d)\n", what, errno);
        return;
    }
    printf("%-22s", what);
    for (int i = 0; i < 64; i++)
        if (CPU_ISSET(i, &m))
            printf(" %d", i);
    printf("\n");
}

static void widen(const char *what, cpuwhich_t which, int ncpu) {
    cpuset_t old, all;
    CPU_ZERO(&old);
    CPU_ZERO(&all);
    for (int i = 0; i < ncpu; i++)
        CPU_SET(i, &all);
    if (cpuset_getaffinity(CPU_LEVEL_WHICH, which, -1, sizeof old, &old) != 0)
        return;
    int rc = cpuset_setaffinity(CPU_LEVEL_WHICH, which, -1, sizeof all, &all);
    printf("widen %-16s rc %d errno %d\n", what, rc, rc ? errno : 0);
    show("  now", which);
    if (rc == 0) {
        rc = cpuset_setaffinity(CPU_LEVEL_WHICH, which, -1, sizeof old, &old);
        printf("  restore rc %d errno %d\n", rc, rc ? errno : 0);
    }
}

static void *child(void *arg) {
    (void)arg;
    show("new thread mask", CPU_WHICH_TID);
    printf("new thread prio        %d\n", pthread_getprio(pthread_self()));
    return NULL;
}

int main(void) {
    int ncpu = 0;
    size_t len = sizeof ncpu;
    sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
    printf("hw.ncpu                %d\n", ncpu);
    show("process mask", CPU_WHICH_PID);
    show("thread mask", CPU_WHICH_TID);

    struct sched_param sp;
    int policy = -1;
    memset(&sp, 0, sizeof sp);
    int rc = pthread_getschedparam(pthread_self(), &policy, &sp);
    printf("main schedparam        rc %d policy %d priority %d\n", rc, policy, sp.sched_priority);
    printf("main pthread_getprio   %d\n", pthread_getprio(pthread_self()));
    errno = 0;
    int nice = getpriority(PRIO_PROCESS, 0);
    printf("getpriority            %d errno %d\n", nice, errno);

    pthread_t t;
    if (pthread_create(&t, NULL, child, NULL) == 0)
        pthread_join(t, NULL);

    if (ncpu > 0) {
        widen("thread", CPU_WHICH_TID, ncpu);
        widen("process", CPU_WHICH_PID, ncpu);
    }
    show("process mask at end", CPU_WHICH_PID);
    return 0;
}
