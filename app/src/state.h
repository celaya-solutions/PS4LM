/* PS4LM app: what the launcher shows, from what it saw this poll.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PS4LM_STATE_H
#define PS4LM_STATE_H

#define LOAD_TIMEOUT_MS 120000

typedef enum { ST_NO_NETWORK, ST_NO_LOADER, ST_NO_MODEL, ST_STOPPED, ST_LOADING, ST_READY, ST_BUSY, ST_FAILED } app_state;

typedef struct {
    int have_ip;
    int loader_open;
    int model_index;
    int server_open;
    int health;          /* HTTP status of /health, or -1 */
    int busy;            /* slots processing a request */
    int launched_ms_ago; /* -1 when this app has not launched a server */
    int seen_open;       /* the server port was open at some poll since the launch */
} app_seen;

app_state state_next(const app_seen *s);
const char *state_text(app_state st);

#endif
