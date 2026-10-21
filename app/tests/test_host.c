/* Mac tests for the app's plain-C parts: make -C app test.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/conf.h"

static char vectors[16][2048];
static int nvectors;

static void load_vectors(const char *path) {
    FILE *f = fopen(path, "r");
    assert(f && "run app/tests/vectors.sh first (make -C app test does)");
    while (nvectors < 16 && fgets(vectors[nvectors], sizeof vectors[0], f))
        nvectors++;
    fclose(f);
}

static void test_conf_defaults_and_parse(void) {
    ps4lm_conf c;
    conf_defaults(&c);
    assert(strcmp(c.model, "") == 0 && c.ctx == 1024 && c.threads == 2 && c.port == 8080);
    assert(strcmp(c.extra, "") == 0);
    assert(conf_parse("model=a.gguf\nctx=2048\n# x\nfoo=1\n", &c) == 0);
    assert(strcmp(c.model, "a.gguf") == 0 && c.ctx == 2048 && c.threads == 2 && c.port == 8080);
    conf_defaults(&c);
    assert(conf_parse("ctx=abc", &c) == -1);
    conf_defaults(&c);
    assert(conf_parse("threads=3", &c) == -1);
    conf_defaults(&c);
    assert(conf_parse("ctx=100\n", &c) == -1);
    conf_defaults(&c);
    assert(conf_parse("port=70000\n", &c) == -1);
    conf_defaults(&c);
    assert(conf_parse("extra=--no-jinja -n 64\r\nport=9000", &c) == 0);
    assert(strcmp(c.extra, "--no-jinja -n 64") == 0 && c.port == 9000);
}

static void test_pick_model(void) {
    const char *files[] = {"mmproj-F16.gguf", "b.gguf", "a.gguf"};
    ps4lm_conf c;
    conf_defaults(&c);
    assert(conf_pick_model(&c, files, 3) == 1);
    strcpy(c.model, "a.gguf");
    assert(conf_pick_model(&c, files, 3) == 2);
}

static void test_pick_model_missing(void) {
    ps4lm_conf c;
    conf_defaults(&c);
    strcpy(c.model, "gone.gguf");
    assert(conf_pick_model(&c, NULL, 0) == -1);
    const char *files[] = {".x.gguf", "notes.txt"};
    assert(conf_pick_model(&c, files, 2) == -1);
}

static void check_line(int i, const char *elf, const char *const *argv, int argc) {
    char out[2048];
    int n = elfldr_line(out, sizeof out, elf, argv, argc);
    if (n < 0 || strcmp(out, vectors[i]) != 0) {
        fprintf(stderr, "case %d: got [%s] want [%s]\n", i, n < 0 ? "-1" : out, vectors[i]);
        abort();
    }
    assert(n == (int)strlen(out));
}

static void test_elfldr_line_matches_run_sh(void) {
    const char *hello = "/data/PS4LM/bin/hello.elf";
    const char *cases[] = {"hello", "two words", "a%b", "x&y#z", "back\\slash", "end%"};
    for (int i = 0; i < 6; i++)
        check_line(i, hello, &cases[i], 1);

    char big[1101];
    memset(big, 'a', 1100);
    big[1100] = 0;
    const char *bigv[] = {big};
    char out[4096];
    assert(elfldr_line(out, sizeof out, hello, bigv, 1) == -1);
    assert(elfldr_line(out, 10, hello, cases, 1) == -1);
}

static void test_server_line(void) {
    ps4lm_conf c;
    conf_defaults(&c);
    strcpy(c.extra, "--no-jinja");
    char out[2048];
    assert(server_line(out, sizeof out, &c, "Qwen3.5-0.8B-Q4_K_M.gguf") > 0);
    assert(strcmp(out, vectors[6]) == 0);
    assert(stop_line(out, sizeof out) > 0);
    assert(strcmp(out, vectors[7]) == 0);
}

static void test_chat_url_no_ip(void) {
    char out[64];
    assert(chat_url(out, sizeof out, "", 8080) == -1);
    assert(chat_url(out, sizeof out, "0.0.0.0", 8080) == -1);
    assert(chat_url(out, sizeof out, NULL, 8080) == -1);
    assert(chat_url(out, sizeof out, "192.168.50.2", 8080) > 0);
    assert(strcmp(out, "http://192.168.50.2:8080") == 0);
}

int main(int argc, char **argv) {
    load_vectors(argc > 1 ? argv[1] : "vectors.txt");
    test_conf_defaults_and_parse();
    test_pick_model();
    test_pick_model_missing();
    test_elfldr_line_matches_run_sh();
    test_server_line();
    test_chat_url_no_ip();
    puts("test_host: all PASS");
    return 0;
}
