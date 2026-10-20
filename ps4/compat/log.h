/* ps4/compat/log.c: stdout and stderr to the Mac and to /data/PS4LM/logs.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Drains everything written so far to its destinations. Safe to call twice. */
void ps4lm_log_finish(void);

#ifdef __cplusplus
}
#endif
