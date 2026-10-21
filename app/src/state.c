/* PS4LM app: what the launcher shows, from what it saw this poll.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "state.h"

app_state state_next(const app_seen *s) {
    if (!s->have_ip)
        return ST_NO_NETWORK;
    if (s->health == 200)
        return s->busy > 0 ? ST_BUSY : ST_READY;
    if (s->health == 503)
        return ST_LOADING;
    if (s->launched_ms_ago >= 0 && s->seen_open && !s->server_open)
        return ST_FAILED; /* it was up and the port closed: the server died */
    if (s->launched_ms_ago >= 0)
        return s->launched_ms_ago <= LOAD_TIMEOUT_MS ? ST_LOADING : ST_FAILED;
    if (!s->loader_open)
        return ST_NO_LOADER;
    if (s->model_index < 0)
        return ST_NO_MODEL;
    return ST_STOPPED;
}

const char *state_text(app_state st) {
    switch (st) {
    case ST_NO_NETWORK: return "No network. Connect the PS4 to the network.";
    case ST_NO_LOADER: return "Loader not running. Load GoldHEN, then run scripts/console-setup.sh on the Mac.";
    case ST_NO_MODEL: return "No model in /data/PS4LM/models. Run scripts/fetch-model.sh on the Mac.";
    case ST_STOPPED: return "Ready to start. Press X.";
    case ST_LOADING: return "Loading the model...";
    case ST_READY: return "Chat is open. Scan the code or type the address.";
    case ST_BUSY: return "Writing a reply...";
    case ST_FAILED: return "Server stopped. See /data/PS4LM/logs.";
    }
    return "";
}
