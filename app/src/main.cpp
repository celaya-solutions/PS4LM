/* PS4LM app: the home-screen launcher. Starts and stops the llama-server
 * payload through elfldr on 127.0.0.1:9021 (the same line scripts/run.sh
 * sends), and shows its state, the chat address and a QR code on the TV.
 * The model itself runs in the payload, not in this app. Console only.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <sstream>

#include <orbis/NetCtl.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

extern "C" {
#include "conf.h"
#include "net.h"
#include "screen.h"
#include "state.h"
}

#define LOCAL       "127.0.0.1"
#define ELFLDR_PORT 9021
#define POLL_MS     500
#define MAX_MODELS  32

std::stringstream debugLogStream; /* the graphics and controller samples log here */

static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

/* The first user's controller, or -1. */
static int open_pad(void) {
    int32_t user = -1;
    if (scePadInit() != 0)
        return -1;
    OrbisUserServiceInitializeParams param = {};
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user);
    return scePadOpen(user, 0, 0, NULL);
}

static int by_name(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Model file names in /data/PS4LM/models, sorted; returns the count. */
static int list_models(char names[][256], const char *ptrs[]) {
    int n = 0;
    DIR *d = opendir(PS4LM_MODELS);
    if (d) {
        struct dirent *e;
        while (n < MAX_MODELS && (e = readdir(d)))
            if (strstr(e->d_name, ".gguf")) {
                snprintf(names[n], 256, "%s", e->d_name);
                ptrs[n] = names[n];
                n++;
            }
        closedir(d);
    }
    qsort(ptrs, (size_t)n, sizeof ptrs[0], by_name);
    return n;
}

/* 0, or -1 when server.conf exists but does not parse (c is then the
 * defaults, and the file must not be written over). */
static int read_conf(ps4lm_conf *c) {
    char text[2048] = "";
    conf_defaults(c);
    FILE *f = fopen(PS4LM_CONF, "r");
    if (!f)
        return 0;
    text[fread(text, 1, sizeof text - 1, f)] = '\0';
    fclose(f);
    if (conf_parse(text, c) == 0)
        return 0;
    conf_defaults(c);
    return -1;
}

static void write_conf(const ps4lm_conf *c) {
    char text[1024];
    FILE *f = conf_format(text, sizeof text, c) > 0 ? fopen(PS4LM_CONF, "w") : NULL;
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static int local_ip(char *ip, size_t cap) {
    OrbisNetCtlInfo info;
    memset(&info, 0, sizeof info);
    if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info) != 0)
        return -1;
    snprintf(ip, cap, "%s", info.ip_address);
    return ip[0] && strcmp(ip, "0.0.0.0") != 0 ? 0 : -1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (screen_init() != 0)
        return 1;
    int pad = open_pad();
    sceNetCtlInit();

    ps4lm_conf conf;
    static char names[MAX_MODELS][256];
    const char *models[MAX_MODELS];
    int nmodels = 0, sel = -1, prev_buttons = 0, conf_bad = 0;
    long last_poll = 0, launched_at = -1;
    char ip[32] = "", url[64] = "", note[128] = "";
    app_seen seen = {};
    app_state st = ST_NO_LOADER;

    for (;;) {
        long now = now_ms();
        if (now - last_poll >= POLL_MS) {
            last_poll = now;
            conf_bad = read_conf(&conf) != 0;
            const char *chosen = sel >= 0 && sel < nmodels ? models[sel] : NULL;
            char keep[256] = "";
            if (chosen)
                snprintf(keep, sizeof keep, "%s", chosen);
            nmodels = list_models(names, models);
            if (keep[0]) /* keep the controller's choice across rescans */
                snprintf(conf.model, sizeof conf.model, "%s", keep);
            sel = conf_pick_model(&conf, models, nmodels);

            seen.have_ip = local_ip(ip, sizeof ip) == 0;
            seen.loader_open = net_port_open(LOCAL, ELFLDR_PORT, 300);
            seen.model_index = sel;
            seen.server_open = net_port_open(LOCAL, conf.port, 300);
            seen.health = seen.server_open ? net_http_get(LOCAL, conf.port, "/health", NULL, 0, 1000) : -1;
            char slots[4096] = "";
            seen.busy = seen.health == 200 &&
                        net_http_get(LOCAL, conf.port, "/slots", slots, sizeof slots, 1000) == 200
                            ? slots_busy(slots) : 0;
            seen.launched_ms_ago = launched_at < 0 ? -1 : (int)(now - launched_at);
            seen.seen_open = launched_at >= 0 && (seen.seen_open || seen.server_open);
            app_state was = st;
            st = state_next(&seen);
            if (st != was)
                note[0] = '\0'; /* a note belongs to the state it was written in */
            if (st == ST_READY || st == ST_BUSY)
                launched_at = -1;
            if (chat_url(url, sizeof url, seen.have_ip ? ip : "", conf.port) < 0)
                url[0] = '\0';
        }

        /* Buttons act on the press, not while held. */
        OrbisPadData data = {};
        int buttons = pad >= 0 && scePadReadState(pad, &data) == 0 ? (int)data.buttons : 0;
        int pressed = buttons & ~prev_buttons;
        prev_buttons = buttons;
        int idle = st == ST_STOPPED || st == ST_FAILED;
        char line[ELFLDR_MAX + 1];
        if (idle && nmodels > 0 && (pressed & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN))) {
            int step = pressed & ORBIS_PAD_BUTTON_UP ? -1 : 1;
            for (int i = sel + step; i >= 0 && i < nmodels; i += step)
                if (conf_pick_model(&conf, &models[i], 1) == 0) { /* skip mmproj- and dot files */
                    sel = i;
                    break;
                }
        }
        if (idle && sel >= 0 && seen.loader_open && (pressed & ORBIS_PAD_BUTTON_CROSS)) {
            snprintf(conf.model, sizeof conf.model, "%s", models[sel]);
            if (!conf_bad)
                write_conf(&conf);
            if (server_line(line, sizeof line, &conf, models[sel]) > 0 &&
                net_send_line(LOCAL, ELFLDR_PORT, line, 1000) == 0) {
                launched_at = now;
                seen.seen_open = 0;
                st = ST_LOADING;
                if (conf_bad)
                    snprintf(note, sizeof note, "server.conf did not read; started with the defaults.");
                else
                    note[0] = '\0';
            } else {
                snprintf(note, sizeof note, "Could not reach the loader on port %d.", ELFLDR_PORT);
            }
        }
        if ((st == ST_LOADING || st == ST_READY || st == ST_BUSY) &&
            (pressed & ORBIS_PAD_BUTTON_CIRCLE)) {
            /* launched_at stays: until the port is seen closing, or the load
             * timeout passes, X stays off, so a stop that did not work cannot
             * lead to a second server. */
            if (stop_line(line, sizeof line) > 0 && net_send_line(LOCAL, ELFLDR_PORT, line, 1000) == 0)
                snprintf(note, sizeof note, "Stopping the server...");
            else
                snprintf(note, sizeof note, "Could not reach the loader on port %d.", ELFLDR_PORT);
        }
        if (pressed & ORBIS_PAD_BUTTON_OPTIONS)
            snprintf(note, sizeof note, "Close the app with the PS button. The server keeps running.");

        screen_clear(0x000000);
        screen_text(120, 140, 96, 0xFFFFFF, "PS4LM");
        screen_text(120, 240, 44, 0xFFFFFF, state_text(st));
        if (note[0])
            screen_text(120, 300, 36, 0xFFC857, note);
        if ((st == ST_READY || st == ST_BUSY) && url[0]) {
            screen_text(120, 420, 72, 0xFFFFFF, url);
            screen_qr(1260, 140, 12, url);
        }
        char info[160];
        snprintf(info, sizeof info, "ctx %d   threads %d   port %d", conf.ctx, conf.threads, conf.port);
        screen_text(120, 560, 32, 0xA0A0A0, info);
        for (int i = 0; i < nmodels && i < 10; i++) {
            char row[300];
            snprintf(row, sizeof row, "%s %s", i == sel ? ">" : " ", models[i]);
            screen_text(120, 630 + i * 40, 32, i == sel ? 0xFFFFFF : 0x808080, row);
        }
        screen_text(120, 1040, 30, 0xA0A0A0, "X start   O stop   Up/Down model   PS button closes");
        screen_present();
        /* No return from main: an installed app that exits that way dies
         * with SIGSYS (seen 2026-10-21). The PS button closes it. */
    }
}
