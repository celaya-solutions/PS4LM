/* PS4LM app: the few network calls the launcher needs (POSIX sockets, IPv4).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PS4LM_NET_H
#define PS4LM_NET_H

#include <stddef.h>

int net_port_open(const char *ip, int port, int timeout_ms);
int net_send_line(const char *ip, int port, const char *line, int timeout_ms);
int net_http_get(const char *ip, int port, const char *path, char *body, size_t cap, int timeout_ms);
int slots_busy(const char *json);

#endif
