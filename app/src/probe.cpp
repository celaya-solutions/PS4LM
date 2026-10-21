/* PS4LM app probe: what an installed app (not a payload) may do on this
 * console. Writes /data/PS4LM/app-probe.txt when it can, and always shows the
 * same lines on the TV. Circle exits. Build with `make -C app PROBE=1`.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sstream>
#include <string>

#include <orbis/NetCtl.h>

#include "controller.h"
#include "graphics.h"
#include "log.h"

extern "C" {
#include "net.h"
/* libkernel exports these; the toolchain headers lack cpuset_getaffinity and
 * declare sceKernelAvailableFlexibleMemorySize without its pointer, so it is
 * bound here under another name. */
int cpuset_getaffinity(int level, int which, int64_t id, size_t size, void *mask);
int flexible_free(size_t *size) __asm__("sceKernelAvailableFlexibleMemorySize");
}

#define CPU_LEVEL_WHICH 3
#define CPU_WHICH_PID 2

std::stringstream debugLogStream;

static std::string probe_text(void) {
    char line[256];
    std::string out;

    uint64_t mask[2] = {0, 0};
    int rc = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof mask, mask);
    int allowed = 0;
    std::string cpus;
    for (int i = 0; i < 128; i++)
        if (mask[i / 64] >> (i % 64) & 1) {
            allowed++;
            cpus += (cpus.empty() ? "" : ",") + std::to_string(i);
        }
    snprintf(line, sizeof line, "affinity      rc %d, %d allowed: %s\n", rc, allowed, cpus.c_str());
    out += line;

    size_t flex = 0;
    rc = flexible_free(&flex);
    snprintf(line, sizeof line, "direct mem    %.2f GiB (whole console)\nflexible free rc %d, %.2f GiB\n",
             sceKernelGetDirectMemorySize() / 1073741824.0, rc, flex / 1073741824.0);
    out += line;

    snprintf(line, sizeof line, "127.0.0.1:9021 elfldr  %s\n127.0.0.1:8080 server  %s\n",
             net_port_open("127.0.0.1", 9021, 1000) ? "open" : "closed",
             net_port_open("127.0.0.1", 8080, 1000) ? "open" : "closed");
    out += line;

    OrbisNetCtlInfo info;
    memset(&info, 0, sizeof info);
    sceNetCtlInit();
    rc = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
    snprintf(line, sizeof line, "ip            rc %d, %s\n", rc, info.ip_address);
    out += line;

    DIR *d = opendir("/data/PS4LM/models");
    int models = 0;
    if (d) {
        while (struct dirent *e = readdir(d))
            models += strstr(e->d_name, ".gguf") != NULL;
        closedir(d);
    }
    snprintf(line, sizeof line, "read models   %s, %d .gguf\n", d ? "yes" : "no", models);
    out += line;
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    std::string text = probe_text();

    FILE *f = fopen("/data/PS4LM/app-probe.txt", "w");
    if (f) {
        fputs(text.c_str(), f);
        fclose(f);
    }
    text += f ? "write /data   yes\n" : "write /data   no\n";
    text = "PS4LM probe done\n\n" + text + "\nO to exit";
    puts(text.c_str());

    auto scene = new Scene2D(1920, 1080, 4);
    if (!scene->Init(0xC000000, 2))
        return 1;
    scene->SetActiveFrameBuffer(0); /* the sample never sets it: a garbage index faults on the first draw */
    FT_Face font;
    if (!scene->InitFont(&font, "/app0/assets/fonts/font.ttf", 40))
        return 1;
    auto pad = new Controller();
    pad->Init(-1);

    Color bg = {0, 0, 0}, fg = {255, 255, 255};
    for (int frame = 0;; frame++) {
        scene->FrameBufferFill(bg);
        scene->DrawText((char *)text.c_str(), font, 120, 120, bg, fg);
        scene->SubmitFlip(frame);
        scene->FrameWait(frame);
        scene->FrameBufferSwap();
        if (pad->CirclePressed())
            break;
    }
    return 0;
}
