/* PS4LM hello: the plumbing probe.
 *
 * Prints its arguments and one line about the console, and writes the line to
 * /data/PS4LM/hello.txt. ps4/compat/log.c, linked in, sends the output back to
 * scripts/send.sh or run.sh, to the Mac's log listener (scripts/logs.sh) and to
 * /data/PS4LM/logs.
 *
 *   scripts/run.sh hello "two words" three
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <ps4/kernel.h>

#define OUT_DIR  "/data/PS4LM"
#define OUT_FILE OUT_DIR "/hello.txt"

int main(int argc, char *argv[]) {
    char line[256];

    for (int i = 0; i < argc; i++)
        printf("hello: argv[%d] = \"%s\"\n", i, argv[i]);

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
    return 0;
}
