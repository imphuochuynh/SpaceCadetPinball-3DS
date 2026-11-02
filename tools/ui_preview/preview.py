#!/usr/bin/env python3
"""Mock the themed bottom-screen menu / initials pages with the game's own
message font (pbmsg_ft) so layout can be checked off-device.
Usage: preview.py CADET.DAT out_dir"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import datdump as d
from PIL import Image, ImageDraw

g, _ = d.load(sys.argv[1]); out = sys.argv[2]; os.makedirs(out, exist_ok=True)
bg = g[d.find(g, "background")]
pal = next(b for t, b in bg if t == d.PALETTE)
P = []
for i in range(256):
    b_, g_, r, a = pal[i * 4:i * 4 + 4]; P += [r, g_, b_]
gi = d.find(g, "pbmsg_ft"); GAP = d.short_arrays(g[gi])[0][0]
glyph = {}
for c in range(32, 128):
    bm = [b for b in d.bitmaps(g[gi + c - 32]) if b["res"] == 0][0]
    w, h = bm["w"], bm["h"]; s = w if w % 4 == 0 else w - w % 4 + 4
    rows = [bm["pixels"][r * s:r * s + w] for r in range(h)]
    idx = Image.new("L", (w, h)); idx.putdata(b"".join(reversed(rows)))
    im = Image.new("P", (w, h)); im.putpalette(P); im.putdata(b"".join(reversed(rows)))
    rgba = im.convert("RGBA"); rgba.putalpha(idx.point(lambda v: 255 if v else 0))
    glyph[chr(c)] = rgba
H = 22

def width(s, sc=1):
    return int(sum(glyph[c].size[0] + GAP for c in s) * sc) - GAP * sc if s else 0

def text(img, s, x, y, sc=1, align="l", maxw=0, alpha=255):
    w = width(s, sc); sx = sc
    if maxw and w > maxw: sx = sc * maxw / w; w = maxw
    if align == "c": x -= w / 2
    if align == "r": x -= w
    for c in s:
        gl = glyph[c]
        gw = max(1, round(gl.size[0] * sx)); gh = round(H * sc)
        g2 = gl.resize((gw, gh), Image.NEAREST)
        if alpha < 255:
            a = g2.getchannel("A").point(lambda v: v * alpha // 255); g2.putalpha(a)
        img.alpha_composite(g2, (int(round(x)), int(y)))
        x += (gl.size[0] + GAP) * sx

BG=(16,20,24,255); BTN=(27,33,48,255); HOT=(36,48,73,255); ACC=(143,184,255,255); RULE=(44,50,64,255)
def frame(dr, x, y, w, h, col, t):
    dr.rectangle([x, y, x + w - 1, y + h - 1], outline=col, width=t)

def menu():
    im = Image.new("RGBA", (320, 240), BG); dr = ImageDraw.Draw(im)
    labels = ["Resume", "New Game", "Players: 1", "View: Follow", "Sound: On", "Music: On",
              "Controls", "Demo", "Reset Scores", "Quit"]
    m, gap, w, h = 8, 6, (320 - 16 - 6) / 2, 40
    for i, lab in enumerate(labels):
        x = m + (i % 2) * (w + gap); y = m + (i // 2) * (h + gap)
        dr.rectangle([x, y, x + w - 1, y + h - 1], fill=HOT if i == 0 else BTN)
        frame(dr, x, y, w, h, ACC if i == 0 else RULE, 2 if i == 0 else 1)
        text(im, lab, x + w / 2, y + (h - H) / 2, 1, "c", w - 12)
        print(f"{lab!r}: {width(lab)}px in {w - 12:.0f}")
    im.save(os.path.join(out, "menu.png"))

def initials():
    im = Image.new("RGBA", (320, 240), BG); dr = ImageDraw.Draw(im)
    text(im, "NEW HIGH SCORE", 160, 10, 1, "c")
    text(im, "PLAYER 1   RANK 2", 160, 38, 1, "c", alpha=150)
    text(im, "241,500", 160, 64, 1, "c")
    slotw, gap = 44, 18; x0 = 160 - (3 * slotw + 2 * gap) / 2; ly = 112
    for i, ch in enumerate("PHO"):
        sx = x0 + i * (slotw + gap); mid = sx + slotw / 2
        text(im, ch, mid, ly, 2, "c")
        dr.rectangle([sx, ly + 48, sx + slotw - 1, ly + 50], fill=ACC if i == 1 else RULE)
        if i == 1:
            dr.polygon([(mid, ly - 14), (mid - 8, ly - 5), (mid + 8, ly - 5)], fill=ACC)
            dr.polygon([(mid - 8, ly + 56), (mid + 8, ly + 56), (mid, ly + 65)], fill=ACC)
    text(im, "A: NEXT    B: BACK", 160, 206, 1, "c", alpha=150)
    im.save(os.path.join(out, "initials.png"))

def controls():
    im = Image.new("RGBA", (320, 240), BG); dr = ImageDraw.Draw(im)
    text(im, "CONTROLS", 12, 4, 1)
    text(im, "START: CANCEL", 308, 4, 1, "r", alpha=150)
    rows = [("Left Flipper", "L"), ("Right Flipper", "R"), ("Plunger", "A"),
            ("Nudge Left", "D-Pad Left"), ("Nudge Right", "D-Pad Right"), ("Nudge Up", "D-Pad Up")]
    for i, (a, k) in enumerate(rows):
        y = 30 + i * 28
        dr.rectangle([8, y, 311, y + 24], fill=HOT if i == 2 else BTN)
        if i == 2: frame(dr, 8, y, 304, 25, ACC, 1)
        text(im, a, 16, y + 2, 1)
        text(im, "Press a button" if i == 2 else k, 304, y + 2, 1, "r", alpha=255 if i == 2 else 150)
    for j, lab in enumerate(["Defaults", "Back"]):
        x = 8 + j * 155
        dr.rectangle([x, 202, x + 148, 233], fill=BTN); frame(dr, x, 202, 149, 32, RULE, 1)
        text(im, lab, x + 74, 207, 1, "c")
    im.save(os.path.join(out, "controls.png"))

menu(); initials(); controls()
sheet = Image.new("RGB", (3 * 330 + 10, 250), (60, 60, 60))
for i, n in enumerate(["menu", "initials", "controls"]):
    sheet.paste(Image.open(os.path.join(out, n + ".png")).convert("RGB"), (10 + i * 330, 5))
sheet.save(os.path.join(out, "sheet.png"))
