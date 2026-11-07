#!/usr/bin/env python3
"""Draws the Steam library artwork from the game's own drawing code, at twelve times the size.

Outputs, in docs/steam/:
  cover_600x900.png      vertical cover
  hero_3840x1240.png     background banner, kept free of text because Steam lays the logo over it
  header_920x430.png     wide banner, the one shown on hover
  logo_transparent.png   logo with a transparent background, trimmed to its edges
  icon_512.png, icon.ico square icon

Requires Pillow and numpy. Output is deterministic.
"""
import math
import os
import random
import sys

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "docs", "steam")
sys.path.insert(0, os.path.dirname(__file__))
import gen_assets as g  # noqa: E402
import gen_backgrounds as b  # noqa: E402

g.SS = 12
g.shrink = lambda img: img
JUICE = {
    "melon": (232, 64, 84), "orange": (255, 150, 40), "lemon": (250, 216, 70), "kiwi": (150, 196, 70),
    "apple": (235, 80, 70), "plum": (150, 70, 170), "peach": (255, 160, 110), "pear": (190, 205, 90),
}
FRUIT = {spec[0]: g.fruit_images(spec) for spec in g.FRUITS}
BOMB = g.spr_bomb()
LOGO = g.spr_logo()
SPLATS = [g.spr_splat(s) for s in (1, 2, 3)]


def trimmed(img):
    return img.crop(img.getchannel("A").getbbox())


def board(w, h, k):
    """The Picnic Blanket board, the game's default, at the size asked for."""
    b.set_board_size(w // k + 1, h // k + 1, k)
    img = b.board_blanket()
    if not isinstance(img, Image.Image):
        img = Image.fromarray(img)
    return img.convert("RGBA").crop((0, 0, w, h))


def place(base, img, cx, cy, size, rot=0, trim=True):
    if trim:
        img = trimmed(img)
    k = size / max(img.size)
    img = img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)
    if rot:
        img = img.rotate(rot, Image.BICUBIC, expand=True)
    base.alpha_composite(img, (round(cx - img.width / 2), round(cy - img.height / 2)))


def splat(base, cx, cy, size, color, seed, alpha=235):
    mask = trimmed(SPLATS[seed % 3]).getchannel("A")
    k = size / max(mask.size)
    mask = mask.resize((round(mask.width * k), round(mask.height * k)), Image.LANCZOS).rotate(seed * 67 % 360, Image.BICUBIC, expand=True)
    base.alpha_composite(g.recolor(mask, color + (alpha,)), (round(cx - mask.width / 2), round(cy - mask.height / 2)))


def blade(base, x0, y0, x1, y1, width):
    """A blade stroke, thin at its tail and widest near its tip, in the game's Ember colours."""
    dx, dy = x1 - x0, y1 - y0
    n = math.hypot(dx, dy)
    nx, ny = -dy / n, dx / n
    def stroke(w):
        pts_a, pts_b = [], []
        for i in range(61):
            t = i / 60
            half = w * 0.5 * (t ** 1.3) * min(1, (1 - t) * 14)
            pts_a.append((x0 + dx * t + nx * half, y0 + dy * t + ny * half))
            pts_b.append((x0 + dx * t - nx * half, y0 + dy * t - ny * half))
        mask = Image.new("L", base.size, 0)
        ImageDraw.Draw(mask).polygon(pts_a + pts_b[::-1], fill=255)
        return mask
    base.alpha_composite(g.recolor(stroke(width * 2.6).filter(ImageFilter.GaussianBlur(width * 0.7)), (255, 110, 40, 190)))
    base.alpha_composite(g.recolor(stroke(width * 1.5).filter(ImageFilter.GaussianBlur(width * 0.12)), (255, 196, 90, 255)))
    base.alpha_composite(g.recolor(stroke(width * 0.8), (255, 250, 236, 255)))


def sliced(base, name, cx, cy, size, x0, y0, x1, y1, gap=0.17):
    """A fruit cut along the blade's line, with its halves drifting apart."""
    alpha = math.atan2(-(y1 - y0), x1 - x0)
    phi = alpha - math.pi / 2
    ox, oy = math.cos(math.pi + phi) * size * gap, -math.sin(math.pi + phi) * size * gap
    _, left, right = FRUIT[name]
    # Untrimmed, so each half keeps its place in the fruit. The fruit fills about 80% of its cell.
    cell = size * 1.25
    place(base, left, cx + ox, cy + oy, cell, math.degrees(phi), trim=False)
    place(base, right, cx - ox, cy - oy, cell, math.degrees(phi), trim=False)


def drops(base, cx, cy, spread, color, count, seed, scale):
    rng = random.Random(seed)
    d = ImageDraw.Draw(base)
    for _ in range(count):
        a, q = rng.uniform(0, 6.283), rng.uniform(0.35, 1) * spread
        r = rng.uniform(0.3, 1) * scale
        x, y = cx + math.cos(a) * q, cy + math.sin(a) * q
        d.ellipse((x - r, y - r, x + r, y + r), fill=color + (255,))


def pool_of_light(base, cx, cy, rx, ry, strength):
    """Lifts the blanket behind the subject and sinks the corners, so the bright art stands out."""
    w, h = base.size
    small = Image.new("L", (w // 8, h // 8), 0)
    ImageDraw.Draw(small).ellipse(((cx - rx) / 8, (cy - ry) / 8, (cx + rx) / 8, (cy + ry) / 8), fill=255)
    mask = small.filter(ImageFilter.GaussianBlur(min(rx, ry) / 16)).resize((w, h), Image.BICUBIC)
    dark = ImageEnhance.Brightness(base).enhance(1 - strength)
    return Image.composite(base, dark, mask)


def save(img, name, size=None, opaque=True):
    os.makedirs(OUT, exist_ok=True)
    if size:
        img = img.resize(size, Image.LANCZOS)
    if opaque:
        img = img.convert("RGB")
    img.save(os.path.join(OUT, name), optimize=True)


def make_cover():
    W, H = 1200, 1800
    img = pool_of_light(board(W, H, 6), W / 2, H * 0.56, 820, 1000, 0.42)
    x0, y0, x1, y1 = 60, 1610, 1150, 690
    splat(img, 330, 1450, 520, JUICE["orange"], 2)
    splat(img, 700, 1120, 640, JUICE["melon"], 4)
    drops(img, 700, 1130, 440, JUICE["melon"], 18, 3, 16)
    drops(img, 370, 1400, 300, JUICE["orange"], 12, 7, 12)
    place(img, FRUIT["lemon"][0], 950, 1560, 300, -20)
    place(img, FRUIT["apple"][0], 225, 860, 290, 16)
    place(img, BOMB, 1085, 945, 205, -14)
    blade(img, x0, y0, x1, y1, 46)
    sliced(img, "orange", 370, 1400, 340, x0, y0, x1, y1, 0.2)
    sliced(img, "melon", 680, 1140, 520, x0, y0, x1, y1, 0.2)
    place(img, LOGO, W / 2, 400, 1090)
    save(img, "cover_600x900.png", (600, 900))


def make_hero():
    W, H = 3840, 1240
    img = pool_of_light(board(W, H, 6), W * 0.58, H * 0.5, 1900, 900, 0.46)
    x0, y0, x1, y1 = 520, 1150, 3560, 150
    splat(img, 1320, 810, 620, JUICE["orange"], 2)
    splat(img, 2230, 510, 760, JUICE["melon"], 4)
    splat(img, 3060, 360, 500, JUICE["kiwi"], 6)
    drops(img, 2260, 550, 560, JUICE["melon"], 24, 3, 20)
    drops(img, 1370, 830, 360, JUICE["orange"], 14, 7, 14)
    drops(img, 3030, 340, 300, JUICE["kiwi"], 12, 9, 12)
    place(img, FRUIT["plum"][0], 1020, 280, 290, 14)
    place(img, FRUIT["lemon"][0], 1720, 250, 330, -24)
    place(img, FRUIT["apple"][0], 2830, 930, 410, 18)
    place(img, FRUIT["pear"][0], 1740, 1050, 290, -12)
    place(img, BOMB, 3370, 800, 300, -14)
    blade(img, x0, y0, x1, y1, 60)
    sliced(img, "orange", 1370, 830, 410, x0, y0, x1, y1, 0.2)
    sliced(img, "kiwi", 3030, 340, 320, x0, y0, x1, y1, 0.2)
    sliced(img, "melon", 2240, 560, 630, x0, y0, x1, y1, 0.2)
    save(img, "hero_3840x1240.png")


def make_header():
    W, H = 1840, 860
    img = pool_of_light(board(W, H, 4), W * 0.4, H / 2, 1250, 680, 0.4)
    x0, y0, x1, y1 = 1030, 820, 1810, 60
    splat(img, 1430, 420, 520, JUICE["melon"], 4)
    drops(img, 1430, 430, 360, JUICE["melon"], 14, 3, 13)
    place(img, FRUIT["orange"][0], 1295, 765, 165, 12)
    place(img, FRUIT["lemon"][0], 1170, 130, 190, -20)
    blade(img, x0, y0, x1, y1, 40)
    sliced(img, "melon", 1420, 440, 430, x0, y0, x1, y1, 0.2)
    place(img, LOGO, 590, H / 2, 1030)
    save(img, "header_920x430.png", (920, 430))


def make_logo():
    logo = trimmed(LOGO)
    k = 1280 / logo.width
    save(logo, "logo_transparent.png", (1280, round(logo.height * k)), opaque=False)


def make_icon():
    S = 1024
    tile = pool_of_light(board(S, S, 8), S / 2, S / 2, 620, 620, 0.3)
    x0, y0, x1, y1 = 90, 930, 950, 80
    blade(tile, x0, y0, x1, y1, 58)
    sliced(tile, "orange", 512, 512, 640, x0, y0, x1, y1, 0.24)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), 190, fill=255)
    tile.putalpha(mask)
    save(tile, "icon_512.png", (512, 512), opaque=False)
    os.makedirs(OUT, exist_ok=True)
    tile.resize((256, 256), Image.LANCZOS).save(
        os.path.join(OUT, "icon.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    )


if __name__ == "__main__":
    make_logo()
    make_icon()
    make_header()
    make_cover()
    make_hero()
