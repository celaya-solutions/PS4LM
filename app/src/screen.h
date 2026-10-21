/* PS4LM app: a 1920x1080 screen with text and a QR code, over the OpenOrbis
 * graphics sample (samples/_common/graphics.cpp). Console only.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PS4LM_SCREEN_H
#define PS4LM_SCREEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int screen_init(void);
void screen_clear(uint32_t rgb);
void screen_text(int x, int y, int px, uint32_t rgb, const char *s);
void screen_qr(int x, int y, int module_px, const char *text);
void screen_present(void);

#ifdef __cplusplus
}
#endif

#endif
