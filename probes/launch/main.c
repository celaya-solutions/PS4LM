/* launch: THROWAWAY spike. Can a payload open apps, show a TV notice, and
 * keep running while a game is up?
 *
 *   launch info               user, running app, temperatures
 *   launch notify <words>     notice on the TV
 *   launch launch <TITLEID>   open an app (never kills the running one)
 *   launch beat <MB> <secs>   hold MB of memory, time a pass over it each second
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct { char pad[45]; char message[3075]; } notify_request_t;
typedef struct {
    uint32_t size, user_id, app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} LncAppParam;

int sceKernelSendNotificationRequest(int, notify_request_t *, size_t, int);
int sceKernelGetCpuTemperature(int *);
int sceKernelGetSocSensorTemperature(int, int *);
int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);
int sceSystemServiceGetAppIdOfBigApp(void);
int sceLncUtilGetAppTitleId(int, char *);
int sceSystemServiceLaunchApp(const char *, const char **, LncAppParam *);

static void notify(const char *msg) {
    notify_request_t r;
    memset(&r, 0, sizeof r);
    strncpy(r.message, msg, sizeof r.message - 1);
    printf("notify rc 0x%x\n", sceKernelSendNotificationRequest(0, &r, sizeof r, 0));
}

static void big_app(char tid[16]) {
    int id = sceSystemServiceGetAppIdOfBigApp();
    strcpy(tid, "-");
    if (id > 0)
        sceLncUtilGetAppTitleId(id, tid);
    printf("big app id 0x%x title %s\n", id, tid);
}

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(int argc, char *argv[]) {
    const char *mode = argc > 1 ? argv[1] : "info";
    char tid[16];
    int user = -1, t = -1;

    printf("init rc 0x%x\n", sceUserServiceInitialize(0));
    printf("foreground rc 0x%x user 0x%x\n", sceUserServiceGetForegroundUser(&user), user);

    if (!strcmp(mode, "info")) {
        big_app(tid);
        printf("cpu temp rc 0x%x %d C\n", sceKernelGetCpuTemperature(&t), t);
        printf("soc temp rc 0x%x %d C\n", sceKernelGetSocSensorTemperature(0, &t), t);
    } else if (!strcmp(mode, "notify") && argc > 2) {
        char msg[512] = "";
        for (int i = 2; i < argc; i++)
            snprintf(msg + strlen(msg), sizeof msg - strlen(msg), "%s%s", i > 2 ? " " : "", argv[i]);
        notify(msg);
    } else if (!strcmp(mode, "launch") && argc > 2) {
        big_app(tid);
        LncAppParam p = { sizeof p, (uint32_t)user, 0, 0, 2 /* SkipSystemUpdateCheck */ };
        printf("launch %s rc 0x%x\n", argv[2], sceSystemServiceLaunchApp(argv[2], NULL, &p));
        sleep(5);
        big_app(tid);
    } else if (!strcmp(mode, "beat") && argc > 3) {
        size_t n = (size_t)atoi(argv[2]) << 20;
        int secs = atoi(argv[3]);
        volatile uint8_t *b = malloc(n);
        if (!b) {
            printf("malloc %zu failed\n", n);
            return 1;
        }
        for (size_t i = 0; i < n; i += 4096)
            b[i] = (uint8_t)i;
        for (int s = 0; s < secs; s++) {
            double t0 = now_ms();
            unsigned sum = 0;
            for (size_t i = 0; i < n; i += 4096)
                sum += b[i];
            double t1 = now_ms();
            big_app(tid);
            printf("beat %d pass %.1f ms sum %u\n", s, t1 - t0, sum);
            fflush(stdout);
            usleep(1000000);
        }
    } else {
        printf("usage: launch info|notify <words>|launch <TITLEID>|beat <MB> <secs>\n");
        return 2;
    }
    return 0;
}
