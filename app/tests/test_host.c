/* Mac tests for the app's plain-C parts: make -C app test.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/conf.h"
#include "../src/net.h"
#include "../src/state.h"

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

static void test_conf_format_round_trip(void) {
    ps4lm_conf c, back;
    conf_defaults(&c);
    strcpy(c.model, "a.gguf");
    c.ctx = 2048;
    strcpy(c.extra, "--no-jinja");
    char text[1024];
    assert(conf_format(text, sizeof text, &c) > 0);
    conf_defaults(&back);
    assert(conf_parse(text, &back) == 0);
    assert(memcmp(&c, &back, sizeof c) == 0);
    assert(conf_format(text, 8, &c) == -1);
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

static int http_port, close_port;
static pid_t fixture_pid;

static void start_fixture(const char *script) {
    char dir[] = "/tmp/ps4lm-test-XXXXXX", ports[64];
    assert(mkdtemp(dir));
    snprintf(ports, sizeof ports, "%s/ports", dir);
    fixture_pid = fork();
    if (fixture_pid == 0) {
        execlp("python3", "python3", "-I", script, ports, (char *)NULL);
        _exit(127);
    }
    for (int i = 0; i < 100; i++) {
        FILE *f = fopen(ports, "r");
        if (f) {
            assert(fscanf(f, "%d %d", &http_port, &close_port) == 2);
            fclose(f);
            unlink(ports);
            rmdir(dir);
            return;
        }
        usleep(50000);
    }
    assert(!"fixture did not start");
}

static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

/* A port nothing listens on: bind one, read its number, close it. */
static int closed_port(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    socklen_t len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(fd, (struct sockaddr *)&a, sizeof a) == 0);
    assert(getsockname(fd, (struct sockaddr *)&a, &len) == 0);
    close(fd);
    return ntohs(a.sin_port);
}

static void test_port_open(void) {
    assert(net_port_open("127.0.0.1", http_port, 1000) == 1);
    long t = now_ms();
    assert(net_port_open("127.0.0.1", closed_port(), 1000) == 0);
    assert(now_ms() - t < 1200);
    t = now_ms();
    assert(net_port_open("10.255.255.1", 9, 300) == 0); /* no answer: times out */
    assert(now_ms() - t < 1200);
    assert(net_port_open("", 80, 100) == 0);
    assert(net_port_open("not-an-ip", 80, 100) == 0);
}

static void test_http_get(void) {
    char body[64];
    assert(net_http_get("127.0.0.1", http_port, "/health", body, sizeof body, 2000) == 503);
    assert(net_http_get("127.0.0.1", http_port, "/health", body, sizeof body, 2000) == 200);
    assert(strcmp(body, "{\"status\":\"ok\"}") == 0);
    assert(net_http_get("127.0.0.1", http_port, "/health", NULL, 0, 2000) == 200); /* status only */
    char slots[256];
    assert(net_http_get("127.0.0.1", http_port, "/slots", slots, sizeof slots, 2000) == 200);
    assert(slots_busy(slots) == 1);
    char small[16];
    assert(net_http_get("127.0.0.1", http_port, "/big", small, sizeof small, 2000) == 200);
    assert(strlen(small) == 15 && strspn(small, "x") == 15);
    assert(net_http_get("127.0.0.1", closed_port(), "/health", body, sizeof body, 500) == -1);
    assert(net_http_get("127.0.0.1", close_port, "/health", body, sizeof body, 500) == -1);
}

static void test_send_line(void) {
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    socklen_t len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(ls, (struct sockaddr *)&a, sizeof a) == 0 && listen(ls, 1) == 0);
    assert(getsockname(ls, (struct sockaddr *)&a, &len) == 0);
    const char *line = "file:/data/PS4LM/bin/stop.elf?args=llama-server\n";
    assert(net_send_line("127.0.0.1", ntohs(a.sin_port), line, 1000) == 0);
    int c = accept(ls, NULL, NULL);
    char got[256];
    ssize_t n, total = 0;
    while ((n = read(c, got + total, sizeof got - 1 - (size_t)total)) > 0)
        total += n;
    got[total] = 0;
    assert(strcmp(got, line) == 0);
    close(c);
    close(ls);

    /* A peer that hangs up while we write must not kill us with SIGPIPE. */
    size_t big = 4 << 20;
    char *huge = malloc(big + 1);
    memset(huge, 'a', big);
    huge[big] = 0;
    int rc = net_send_line("127.0.0.1", close_port, huge, 1000);
    assert(rc == -1 || rc == 0);
    free(huge);
    assert(net_send_line("127.0.0.1", closed_port(), "x\n", 500) == -1);
}

static void test_slots_busy(void) {
    assert(slots_busy("[{\"id\":0,\"is_processing\":true},{\"id\":1,\"is_processing\":false}]") == 1);
    assert(slots_busy("[{\"is_processing\": true},{\"is_processing\":true}]") == 2);
    assert(slots_busy("") == 0);
    assert(slots_busy(NULL) == 0);
}

static app_seen seen(void) {
    app_seen s = {.have_ip = 1, .loader_open = 1, .model_index = 0, .server_open = 0,
                  .health = -1, .busy = 0, .launched_ms_ago = -1};
    return s;
}

static void test_state_no_loader(void) {
    app_seen s = seen();
    s.loader_open = 0;
    assert(state_next(&s) == ST_NO_LOADER);
}

static void test_state_server_already_up(void) {
    app_seen s = seen();
    s.loader_open = 0;
    s.server_open = 1;
    s.health = 200;
    assert(state_next(&s) == ST_READY);
}

static void test_state_busy(void) {
    app_seen s = seen();
    s.server_open = 1;
    s.health = 200;
    s.busy = 2;
    assert(state_next(&s) == ST_BUSY);
}

static void test_state_loading(void) {
    app_seen s = seen();
    s.server_open = 1;
    s.health = 503;
    assert(state_next(&s) == ST_LOADING);
    s = seen();
    s.launched_ms_ago = 5000;
    assert(state_next(&s) == ST_LOADING);
}

static void test_state_load_timeout(void) {
    app_seen s = seen();
    s.launched_ms_ago = 121000;
    assert(state_next(&s) == ST_FAILED);
}

static void test_state_no_model(void) {
    app_seen s = seen();
    s.model_index = -1;
    assert(state_next(&s) == ST_NO_MODEL);
    s = seen();
    assert(state_next(&s) == ST_STOPPED);
}

static void test_state_no_ip(void) {
    app_seen s = seen();
    s.have_ip = 0;
    s.server_open = 1;
    s.health = 200;
    assert(state_next(&s) == ST_NO_NETWORK);
}

static void test_state_text(void) {
    assert(strcmp(state_text(ST_STOPPED), "Ready to start. Press X.") == 0);
    assert(strcmp(state_text(ST_FAILED), "Server stopped. See /data/PS4LM/logs.") == 0);
    for (int st = ST_NO_NETWORK; st <= ST_FAILED; st++)
        assert(state_text((app_state)st)[0]);
}

int main(int argc, char **argv) {
    load_vectors(argc > 1 ? argv[1] : "vectors.txt");
    start_fixture(argc > 2 ? argv[2] : "tests/fixture.py");
    test_conf_defaults_and_parse();
    test_conf_format_round_trip();
    test_pick_model();
    test_pick_model_missing();
    test_elfldr_line_matches_run_sh();
    test_server_line();
    test_chat_url_no_ip();
    test_port_open();
    test_http_get();
    test_send_line();
    test_slots_busy();
    test_state_no_loader();
    test_state_server_already_up();
    test_state_busy();
    test_state_loading();
    test_state_load_timeout();
    test_state_no_model();
    test_state_no_ip();
    test_state_text();
    kill(fixture_pid, SIGTERM);
    waitpid(fixture_pid, NULL, 0);
    puts("test_host: all PASS");
    return 0;
}
