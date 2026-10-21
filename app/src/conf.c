/* PS4LM app: server.conf, model choice, elfldr launch lines and the chat URL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void conf_defaults(ps4lm_conf *c) {
    memset(c, 0, sizeof *c);
    c->ctx = 1024;
    c->threads = 2;
    c->port = 8080;
}

static int parse_int(const char *s, int lo, int hi, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < lo || v > hi)
        return -1;
    *out = (int)v;
    return 0;
}

static int copy_str(char *dst, size_t cap, const char *s) {
    if (strlen(s) >= cap)
        return -1;
    strcpy(dst, s);
    return 0;
}

/* One key=value line, already trimmed. Unknown keys and comments are ignored. */
static int parse_line(char *line, ps4lm_conf *c) {
    char *eq = strchr(line, '=');
    if (line[0] == '#' || !eq)
        return 0;
    *eq = '\0';
    const char *key = line, *val = eq + 1;
    if (strcmp(key, "model") == 0) return copy_str(c->model, sizeof c->model, val);
    if (strcmp(key, "extra") == 0) return copy_str(c->extra, sizeof c->extra, val);
    if (strcmp(key, "ctx") == 0) return parse_int(val, 256, 32768, &c->ctx);
    if (strcmp(key, "threads") == 0) return parse_int(val, 1, 2, &c->threads);
    if (strcmp(key, "port") == 0) return parse_int(val, 1, 65535, &c->port);
    return 0;
}

int conf_parse(const char *text, ps4lm_conf *c) {
    char line[512];
    while (*text) {
        size_t n = strcspn(text, "\n");
        if (n >= sizeof line)
            return -1;
        memcpy(line, text, n);
        line[n] = '\0';
        text += n + (text[n] == '\n');
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
            line[--n] = '\0';
        if (parse_line(line + strspn(line, " \t"), c) != 0)
            return -1;
    }
    return 0;
}

/* server.conf text that conf_parse reads back to the same values. */
int conf_format(char *out, size_t cap, const ps4lm_conf *c) {
    int n = snprintf(out, cap, "model=%s\nctx=%d\nthreads=%d\nport=%d\nextra=%s\n", c->model, c->ctx,
                     c->threads, c->port, c->extra);
    return n < 0 || (size_t)n >= cap ? -1 : n;
}

static int is_model_file(const char *name) {
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".gguf") == 0 && name[0] != '.' &&
           strncmp(name, "mmproj-", 7) != 0;
}

int conf_pick_model(const ps4lm_conf *c, const char *const *files, int n) {
    for (int i = 0; c->model[0] && i < n; i++)
        if (strcmp(files[i], c->model) == 0)
            return i;
    for (int i = 0; i < n; i++)
        if (is_model_file(files[i]))
            return i;
    return -1;
}

/* Same encoding as scripts/elfldr-args.py: escape backslash and space, join
 * on spaces, percent-encode % & # and control bytes, and keep a %XX off the
 * last three characters with a trailing backslash. */
int elfldr_line(char *out, size_t cap, const char *elf, const char *const *argv, int argc) {
    char raw[ELFLDR_MAX * 2], enc[ELFLDR_MAX * 3];
    size_t r = 0, e = 0;
    for (int i = 0; i < argc; i++) {
        if (i > 0 && r < sizeof raw) raw[r++] = ' ';
        for (const char *p = argv[i]; *p && r + 2 < sizeof raw; p++) {
            if (*p == '\\' || *p == ' ') raw[r++] = '\\';
            raw[r++] = *p;
        }
        if (r + 2 >= sizeof raw)
            return -1;
    }
    for (size_t i = 0; i < r; i++) {
        unsigned char ch = (unsigned char)raw[i];
        if (e + 4 >= sizeof enc)
            return -1;
        if (ch == '%' || ch == '&' || ch == '#' || ch < 32)
            e += (size_t)snprintf(enc + e, sizeof enc - e, "%%%02X", ch);
        else
            enc[e++] = (char)ch;
    }
    if (e >= 3 && enc[e - 3] == '%')
        enc[e++] = '\\';
    enc[e] = '\0';
    int n = snprintf(out, cap, "file:%s?args=%s\n", elf, enc);
    if (n < 0 || (size_t)n >= cap || n > ELFLDR_MAX)
        return -1;
    return n;
}

int server_line(char *out, size_t cap, const ps4lm_conf *c, const char *model_file) {
    char model[512], port[16], threads[16], ctx[16], extra[sizeof c->extra];
    const char *argv[64];
    int argc = 0;
    snprintf(model, sizeof model, "%s/%s", PS4LM_MODELS, model_file);
    snprintf(port, sizeof port, "%d", c->port);
    snprintf(threads, sizeof threads, "%d", c->threads);
    snprintf(ctx, sizeof ctx, "%d", c->ctx);
    const char *fixed[] = {"-m", model, "--host", "0.0.0.0", "--port", port, "-t", threads,
                           "--prio", "-1", "-lm", "none", "-c", ctx};
    for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++)
        argv[argc++] = fixed[i];
    strcpy(extra, c->extra);
    for (char *tok = strtok(extra, " "); tok && argc < 64; tok = strtok(NULL, " "))
        argv[argc++] = tok;
    return elfldr_line(out, cap, PS4LM_BIN "/llama-server.elf", argv, argc);
}

int stop_line(char *out, size_t cap) {
    const char *argv[] = {"llama-server"};
    return elfldr_line(out, cap, PS4LM_BIN "/stop.elf", argv, 1);
}

int chat_url(char *out, size_t cap, const char *ip, int port) {
    if (!ip || !ip[0] || strcmp(ip, "0.0.0.0") == 0)
        return -1;
    int n = snprintf(out, cap, "http://%s:%d", ip, port);
    return n < 0 || (size_t)n >= cap ? -1 : n;
}
