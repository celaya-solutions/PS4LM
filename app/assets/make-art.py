#!/usr/bin/env python3
"""Draws the PS4LM home-screen art in the CSR house look: icon0.png (512x512)
and pic1.png (1920x1080). Motif: a game-controller outline whose face buttons
are chat bubbles. No console maker's logos, names or button glyphs.

    python3 -I app/assets/make-art.py      (needs cairosvg: pip install cairosvg;
                                            on macOS DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib)
"""
import os

import cairosvg

ACCENT, ORANGE, BG = "#2EC4B6", "#FF6B35", "#000000"
HERE = os.path.dirname(os.path.abspath(__file__))


def bubble(x, y, s):
    """A chat bubble centred on (x, y), s = half width, with a tail and dots."""
    h = s * 0.72
    dots = "".join(f'<circle cx="{x + d * s * 0.42:.1f}" cy="{y:.1f}" r="{s * 0.11:.1f}" fill="{ACCENT}"/>' for d in (-1, 0, 1))
    tail = f'M{x - s * 0.35:.1f},{y + h:.1f} L{x - s * 0.55:.1f},{y + h + s * 0.45:.1f} L{x - s * 0.05:.1f},{y + h:.1f}'
    return (f'<rect x="{x - s:.1f}" y="{y - h:.1f}" width="{2 * s:.1f}" height="{2 * h:.1f}" rx="{s * 0.45:.1f}" '
            f'fill="none" stroke="{ACCENT}" stroke-width="{s * 0.12:.1f}"/>'
            f'<path d="{tail}" fill="none" stroke="{ACCENT}" stroke-width="{s * 0.12:.1f}" stroke-linejoin="round"/>' + dots)


def controller(cx, cy, w):
    """Controller outline of width w centred on (cx, cy)."""
    k = w / 1000.0
    def p(x, y):
        return f"{cx + (x - 500) * k:.1f},{cy + (y - 300) * k:.1f}"
    body = (f'M{p(250, 120)} L{p(750, 120)} C{p(880, 120)} {p(960, 220)} {p(985, 380)} '
            f'C{p(1010, 540)} {p(960, 600)} {p(880, 560)} L{p(760, 440)} L{p(240, 440)} L{p(120, 560)} '
            f'C{p(40, 600)} {p(-10, 540)} {p(15, 380)} C{p(40, 220)} {p(120, 120)} {p(250, 120)} Z')
    sw = 14 * k
    out = f'<path d="{body}" fill="none" stroke="{ACCENT}" stroke-width="{sw:.1f}" stroke-linejoin="round"/>'
    for x in (370, 630):  # thumbsticks
        a, b = p(x, 360).split(",")
        out += f'<circle cx="{a}" cy="{b}" r="{48 * k:.1f}" fill="none" stroke="{ACCENT}" stroke-width="{sw * 0.8:.1f}"/>'
    a, b = map(float, p(230, 270).split(","))  # a plain plus, left
    out += (f'<path d="M{a - 55 * k:.1f},{b:.1f} H{a + 55 * k:.1f} M{a:.1f},{b - 55 * k:.1f} V{b + 55 * k:.1f}" '
            f'stroke="{ACCENT}" stroke-width="{26 * k:.1f}" stroke-linecap="round"/>')
    for dx, dy in ((0, -85), (85, 0), (0, 85), (-85, 0)):  # face buttons as bubbles, right
        a, b = map(float, p(770 + dx, 270 + dy).split(","))
        out += bubble(a, b, 36 * k)
    return out


def svg(w, h, motif, glow_cx):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
<defs><radialGradient id="g" cx="{glow_cx}" cy="48%" r="50%">
<stop offset="0%" stop-color="{ACCENT}" stop-opacity="0.14"/><stop offset="100%" stop-color="{BG}" stop-opacity="0"/>
</radialGradient></defs>
<rect width="{w}" height="{h}" fill="{BG}"/><rect width="{w}" height="{h}" fill="url(#g)"/>
{motif}
<text x="{w * 0.05:.0f}" y="{h * 0.07 + 8:.0f}" font-family="Menlo, monospace" font-size="{max(14, w // 46)}" font-weight="700"
 letter-spacing="3" fill="{ORANGE}" opacity="0.82">CSR</text>
<circle cx="{w - w * 0.05:.0f}" cy="{h - w * 0.05 if w == h else h - 50:.0f}" r="{max(5, w // 120)}" fill="{ACCENT}" opacity="0.72"/>
</svg>'''


def render(text, out):
    cairosvg.svg2png(bytestring=text.encode(), write_to=out)


icon = controller(256, 236, 400) + (
    f'<text x="256" y="440" text-anchor="middle" font-family="Helvetica Neue, Arial" font-weight="900" '
    f'font-size="64" letter-spacing="-2" fill="{ACCENT}">PS4LM</text>')
render(svg(512, 512, icon, "50%"), os.path.join(HERE, "icon0.png"))
render(svg(1920, 1080, controller(1340, 520, 900), "70%"), os.path.join(HERE, "pic1.png"))
