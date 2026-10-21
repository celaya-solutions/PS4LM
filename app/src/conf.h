/* PS4LM app: server.conf, model choice, elfldr launch lines and the chat URL.
 * Plain C with no console calls, so app/tests/test_host.c runs it on the Mac.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PS4LM_CONF_H
#define PS4LM_CONF_H

#include <stddef.h>

#define PS4LM_DIR    "/data/PS4LM"
#define PS4LM_BIN    PS4LM_DIR "/bin"
#define PS4LM_MODELS PS4LM_DIR "/models"
#define PS4LM_CONF   PS4LM_DIR "/server.conf"
#define ELFLDR_MAX   1024 /* elfldr reads one line into a PATH_MAX buffer */

typedef struct {
    char model[256];
    int ctx;
    int threads;
    int port;
    char extra[256];
} ps4lm_conf;

void conf_defaults(ps4lm_conf *c);
int conf_parse(const char *text, ps4lm_conf *c);
int conf_pick_model(const ps4lm_conf *c, const char *const *files, int n);
int elfldr_line(char *out, size_t cap, const char *elf, const char *const *argv, int argc);
int server_line(char *out, size_t cap, const ps4lm_conf *c, const char *model_file);
int stop_line(char *out, size_t cap);
int chat_url(char *out, size_t cap, const char *ip, int port);

#endif
