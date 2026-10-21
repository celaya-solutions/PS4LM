/* PS4LM app: a 1920x1080 screen with text and a QR code, over the OpenOrbis
 * graphics sample (samples/_common/graphics.cpp). Console only.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "screen.h"

#include <string.h>

#include "graphics.h"

extern "C" {
#include "qrcodegen.h"
}

#define FONT_PATH "/app0/assets/fonts/font.ttf"

static Scene2D *scene;
static Color bg;
static int frame;

/* One FreeType face per pixel size; the launcher uses a handful. */
static struct {
    int px;
    FT_Face face;
} faces[8];

static Color color(uint32_t rgb) {
    return Color{(uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb};
}

static FT_Face face_for(int px) {
    for (auto &f : faces)
        if (f.px == px)
            return f.face;
    for (auto &f : faces)
        if (f.px == 0 && scene->InitFont(&f.face, FONT_PATH, px)) {
            f.px = px;
            return f.face;
        }
    return NULL;
}

int screen_init(void) {
    scene = new Scene2D(1920, 1080, 4);
    if (!scene->Init(0xC000000, 2))
        return -1;
    scene->SetActiveFrameBuffer(0); /* the sample never sets it: a garbage index faults on the first draw */
    return face_for(40) ? 0 : -1;
}

void screen_clear(uint32_t rgb) {
    bg = color(rgb);
    scene->FrameBufferFill(bg);
}

void screen_text(int x, int y, int px, uint32_t rgb, const char *s) {
    FT_Face face = face_for(px);
    if (face)
        scene->DrawText((char *)s, face, x, y, bg, color(rgb));
}

void screen_qr(int x, int y, int module_px, const char *text) {
    static uint8_t qr[qrcodegen_BUFFER_LEN_MAX], tmp[qrcodegen_BUFFER_LEN_MAX];
    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
                              qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true))
        return;
    int n = qrcodegen_getSize(qr), quiet = 4;
    Color white = {255, 255, 255}, black = {0, 0, 0};
    scene->DrawRectangle(x, y, (n + 2 * quiet) * module_px, (n + 2 * quiet) * module_px, white);
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            if (qrcodegen_getModule(qr, c, r))
                scene->DrawRectangle(x + (c + quiet) * module_px, y + (r + quiet) * module_px, module_px,
                                     module_px, black);
}

void screen_present(void) {
    scene->SubmitFlip(frame);
    scene->FrameWait(frame);
    scene->FrameBufferSwap();
    frame++;
}
