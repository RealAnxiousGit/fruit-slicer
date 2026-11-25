#!/usr/bin/env python3
"""Generates the 2.0.0 'Fruit Slicer: Orchard Blast' YouTube thumbnail (1920x1080 and 1280x720)
using gen_assets, gen_backgrounds, gen_steam_art, and gen_promo_logo at high resolution with crisp drop shadows."""
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)

import gen_assets as g  # noqa: E402
import gen_backgrounds as b  # noqa: E402
import gen_promo_logo as p  # noqa: E402

g.SS = 12
g.shrink = lambda img: img

ROOT = os.path.abspath(os.path.join(TOOLS_DIR, ".."))
REPO_ROOT = os.path.abspath(os.path.join(ROOT, ".."))
OUT_DIR = os.path.join(REPO_ROOT, "Fruit Slicer Trailer Footage")


def trimmed(img):
    box = img.getchannel("A").getbbox()
    return img.crop(box) if box else img


def drop_shadow(img, blur=24, dx=14, dy=18, opacity=0.58, color=(10, 6, 24)):
    """Returns a canvas-ready RGBA shadow layer and offset tuple."""
    pad = int(blur * 3)
    w, h = img.size
    a = Image.new("L", (w + pad * 2, h + pad * 2), 0)
    a.paste(img.getchannel("A"), (pad, pad))
    a = a.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: int(v * opacity))
    sh = Image.new("RGBA", a.size, color + (0,))
    sh.putalpha(a)
    return sh, pad, dx, dy


def composite_with_shadow(base, img, cx, cy, size=None, rot=0, trim=True, blur=24, dx=12, dy=18, opacity=0.58, glow=None):
    if trim:
        img = trimmed(img)
    if size is not None:
        k = size / max(img.size)
        img = img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)
    if rot:
        img = img.rotate(rot, Image.BICUBIC, expand=True)
    x = round(cx - img.width / 2)
    y = round(cy - img.height / 2)

    if glow is not None:
        gcol, gblur, gop = glow
        pad = int(gblur * 3)
        ga = Image.new("L", (img.width + pad * 2, img.height + pad * 2), 0)
        ga.paste(img.getchannel("A"), (pad, pad))
        ga = ga.filter(ImageFilter.GaussianBlur(gblur)).point(lambda v: int(v * gop))
        gl = Image.new("RGBA", ga.size, gcol + (0,))
        gl.putalpha(ga)
        base.alpha_composite(gl, (x - pad, y - pad))

    sh, pad, sdx, sdy = drop_shadow(img, blur=blur, dx=dx, dy=dy, opacity=opacity)
    base.alpha_composite(sh, (x - pad + sdx, y - pad + sdy))
    base.alpha_composite(img, (x, y))
    return img


def board(w, h, k=6):
    b.set_board_size(w // k + 1, h // k + 1, k)
    img = b.board_blanket()
    if not isinstance(img, Image.Image):
        img = Image.fromarray(img)
    return img.convert("RGBA").crop((0, 0, w, h))


def pool_of_light(base, cx, cy, rx, ry, strength=0.48):
    w, h = base.size
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2)
    mask = np.clip(1.0 - strength * np.clip((d - 0.55) / 0.85, 0.0, 1.0) ** 1.4, 1.0 - strength, 1.0)[..., None]
    arr = np.array(base, dtype=np.float32)
    arr[..., :3] *= mask
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")


def splat(base, img_splat, cx, cy, size, color, rot=0, alpha=230):
    mask = trimmed(img_splat).getchannel("A")
    k = size / max(mask.size)
    mask = mask.resize((round(mask.width * k), round(mask.height * k)), Image.LANCZOS).rotate(rot, Image.BICUBIC, expand=True)
    layer = g.recolor(mask, color + (alpha,))
    base.alpha_composite(layer, (round(cx - mask.width / 2), round(cy - mask.height / 2)))


def drops(base, cx, cy, spread, color, count, seed, scale):
    import random, math
    rng = random.Random(seed)
    d = ImageDraw.Draw(base)
    for _ in range(count):
        a, q = rng.uniform(0, 6.283), rng.uniform(0.35, 1.0) * spread
        r = rng.uniform(0.35, 1.0) * scale
        x, y = cx + math.cos(a) * q, cy + math.sin(a) * q
        d.ellipse((x - r, y - r, x + r, y + r), fill=color + (255,))


def blade_slash(base, x0, y0, x1, y1, width):
    import math
    dx, dy = x1 - x0, y1 - y0
    n = math.hypot(dx, dy)
    nx, ny = -dy / n, dx / n

    def stroke(w):
        pts_a, pts_b = [], []
        for i in range(65):
            t = i / 64.0
            half = w * 0.5 * (t ** 1.25) * min(1.0, (1.0 - t) * 14.0)
            pts_a.append((x0 + dx * t + nx * half, y0 + dy * t + ny * half))
            pts_b.append((x0 + dx * t - nx * half, y0 + dy * t - ny * half))
        mask = Image.new("L", base.size, 0)
        ImageDraw.Draw(mask).polygon(pts_a + pts_b[::-1], fill=255)
        return mask

    base.alpha_composite(g.recolor(stroke(width * 2.8).filter(ImageFilter.GaussianBlur(width * 0.72)), (255, 105, 35, 200)))
    base.alpha_composite(g.recolor(stroke(width * 1.55).filter(ImageFilter.GaussianBlur(width * 0.14)), (255, 204, 88, 255)))
    base.alpha_composite(g.recolor(stroke(width * 0.82), (255, 252, 238, 255)))


def make_upgrade_card(title, kind_label, bar_rgb, icon_index, w=280, h=360):
    """Draws a crisp roguelike Orchard Blast upgrade card in the game's blue wood-plank style."""
    card = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    # Subtle wood grain in slate-blue matching the in-game Orchard cards
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    grain = np.sin(yy * 0.16 + np.sin(xx * 0.04) * 2.2) * 0.5 + np.sin(yy * 0.42 + 1.0) * 0.25
    light = 1.0 + grain * 0.07 + 0.12 * (1.0 - yy / h) - 0.10 * (yy / h)
    arr = np.zeros((h, w, 4), np.uint8)
    base_rgb = (68.0, 104.0, 146.0)
    for c in range(3):
        arr[..., c] = np.clip(base_rgb[c] * light, 0, 255)
    arr[..., 3] = 255
    wood = Image.fromarray(arr, "RGBA")

    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([8, 8, w - 9, h - 9], 22, fill=255)
    wood.putalpha(mask)

    # Cream/ink outer sticker outline + glowing rarity bar
    d = ImageDraw.Draw(card)
    d.rounded_rectangle([0, 0, w - 1, h - 1], 28, fill=(255, 245, 224, 255))
    d.rounded_rectangle([5, 5, w - 6, h - 6], 24, fill=(32, 24, 44, 255))
    card.alpha_composite(wood)

    d = ImageDraw.Draw(card)
    d.rounded_rectangle([22, 20, w - 23, 32], 6, fill=bar_rgb + (255,))
    # Corner rivets
    for rx in (24, w - 25):
        for ry in (44, h - 24):
            d.ellipse([rx - 5, ry - 5, rx + 5, ry + 5], fill=(34, 26, 40, 255))
            d.ellipse([rx - 3, ry - 4, rx + 1, ry], fill=(170, 185, 205, 255))

    # Medallion icon
    icon = trimmed(g.spr_card_icon(icon_index)).resize((104, 104), Image.LANCZOS)
    card.alpha_composite(icon, ((w - 104) // 2, 56))

    # Crisp stroked text using gen_assets.stroke_text at local scale
    txt = Image.new("RGBA", (w * 2, h * 2), (0, 0, 0, 0))
    td = ImageDraw.Draw(txt)
    # Temporarily scale g.SS=2 for card text
    old_ss = g.SS
    g.SS = 2
    g.stroke_text(td, kind_label, w / 2, 198, 22, bar_rgb + (255,), 5.5)
    g.stroke_text(td, title, w / 2, 256, 36, (255, 250, 232, 255), 8.0)
    g.stroke_text(td, "ROGUELIKE", w / 2, 314, 19, (210, 228, 245, 255), 4.5)
    g.SS = old_ss
    txt = txt.resize((w, h), Image.LANCZOS)
    card.alpha_composite(txt)
    return card


def make_version_pill(text="VERSION 2.0", w=430, h=76):
    pill = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(pill)
    d.rounded_rectangle([0, 0, w - 1, h - 1], h // 2, fill=(255, 245, 224, 255))
    d.rounded_rectangle([6, 6, w - 7, h - 7], (h - 12) // 2, fill=(36, 26, 48, 255))
    txt = Image.new("RGBA", (w * 2, h * 2), (0, 0, 0, 0))
    td = ImageDraw.Draw(txt)
    old_ss = g.SS
    g.SS = 2
    g.stroke_text(td, text, w / 2, h / 2 - 2, 34, (255, 248, 226, 255), 5.0)
    g.SS = old_ss
    pill.alpha_composite(txt.resize((w, h), Image.LANCZOS))
    return pill


def make_boss_bar(name="THE BIG MELON", w=470, h=66):
    bar = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(bar)
    d.rounded_rectangle([0, 0, w - 1, h - 1], 18, fill=(255, 245, 224, 255))
    d.rounded_rectangle([4, 4, w - 5, h - 5], 15, fill=(34, 48, 72, 255))
    d.rounded_rectangle([9, 9, w - 10, h - 10], 11, fill=(56, 86, 122, 255))
    txt = Image.new("RGBA", (w * 2, h * 2), (0, 0, 0, 0))
    td = ImageDraw.Draw(txt)
    old_ss = g.SS
    g.SS = 2
    g.stroke_text(td, name, w / 2, 22, 20, (255, 218, 120, 255), 4.5)
    g.SS = old_ss
    bar.alpha_composite(txt.resize((w, h), Image.LANCZOS))
    d = ImageDraw.Draw(bar)
    # 7 glowing boss HP pips (3 depleted, 4 bright coral-red)
    for i in range(7):
        px0 = 42 + i * 56
        col = (60, 72, 92, 255) if i < 2 else (255, 96, 88, 255)
        d.rounded_rectangle([px0, 38, px0 + 44, 52], 4, fill=col)
    return bar


def make_crown(w=210, h=130):
    cr = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cr)
    pts = [(20, 110), (14, 34), (62, 68), (105, 16), (148, 68), (196, 34), (190, 110)]
    d.polygon(pts, fill=(255, 208, 58, 255), outline=(42, 26, 46, 255), width=8)
    for gx, gy, col in ((105, 68, (238, 64, 96, 255)), (56, 82, (90, 212, 255, 255)), (154, 82, (124, 240, 118, 255))):
        d.ellipse([gx - 10, gy - 10, gx + 10, gy + 10], fill=col, outline=(42, 26, 46, 255), width=3)
    return cr


def build_thumbnail():
    W, H = 1920, 1080
    img = pool_of_light(board(W, H, 6), W * 0.54, H * 0.48, 1050, 780, 0.50)

    # Rich juice splats behind the boss arena (right side)
    splats = [g.spr_splat(s) for s in (1, 2, 3)]
    splat(img, splats[1], 1430, 560, 760, (232, 62, 86), rot=14, alpha=230)
    splat(img, splats[0], 1160, 780, 420, (250, 208, 62), rot=-24, alpha=205)
    drops(img, 1440, 560, 490, (238, 68, 94), 24, 11, 18)
    drops(img, 1210, 760, 270, (255, 212, 66), 12, 19, 13)

    # Secondary Milestone bosses in depth (Dusty Durian top-right, Lord Lemon + floatie lower-right)
    durian = g.boss_body("durian", "guard")
    composite_with_shadow(img, durian, 1740, 290, size=310, rot=12, blur=20, dx=12, dy=16, opacity=0.52)

    lemon = g.boss_body("lemon", "guard")
    fl_back = g.spr_lem_floatie_back()
    fl_front = g.spr_lem_floatie_front()
    composite_with_shadow(img, fl_back, 1755, 865, size=290, rot=-10, blur=16, dx=10, dy=14, opacity=0.45)
    composite_with_shadow(img, lemon, 1755, 825, size=275, rot=-10, blur=20, dx=12, dy=16, opacity=0.52)
    composite_with_shadow(img, fl_front, 1755, 865, size=290, rot=-10, blur=10, dx=6, dy=10, opacity=0.35)

    # Hero Boss: The Big Melon with Royal Crown + deep drop shadow
    melon = g.boss_body("melon", "wind")
    composite_with_shadow(
        img, melon, 1410, 565, size=670, rot=-4, blur=32, dx=18, dy=24, opacity=0.65,
        glow=((255, 110, 90), 28, 0.28)
    )
    crown = make_crown()
    composite_with_shadow(img, crown, 1395, 225, size=220, rot=-9, blur=16, dx=10, dy=14, opacity=0.55)

    # Iconic glowing Ember blade slash cutting dramatically across the boss arena
    blade_slash(img, 860, 860, 1900, 280, 54)

    # Compact Boss HP bar above The Big Melon
    boss_bar = make_boss_bar("THE BIG MELON")
    composite_with_shadow(img, boss_bar, 1410, 115, size=460, blur=18, dx=8, dy=12, opacity=0.58)

    # Left side: Fanned Orchard Blast roguelike upgrade cards beneath the logo
    card_left = make_upgrade_card("BLADE", "RARE UPGRADE", (102, 210, 255), 3)       # Blade
    card_right = make_upgrade_card("CLEAVER", "LEGENDARY", (255, 206, 78), 24)      # Cleaver
    composite_with_shadow(img, card_left, 335, 835, size=330, rot=-10, blur=24, dx=12, dy=18, opacity=0.60)
    composite_with_shadow(img, card_right, 610, 845, size=330, rot=7, blur=24, dx=12, dy=18, opacity=0.62)

    # Hero Title: Transparent Fruit Slicer: Orchard Blast logo shining upper-left
    logo = p.build_orchard_blast_logo()
    composite_with_shadow(
        img, logo, 540, 345, size=940, blur=28, dx=14, dy=20, opacity=0.68,
        glow=((255, 228, 120), 26, 0.30)
    )

    # Version 2.0 Pill Badge below the logo
    pill = make_version_pill("VERSION 2.0")
    composite_with_shadow(img, pill, 535, 628, size=380, blur=18, dx=8, dy=12, opacity=0.60)

    os.makedirs(OUT_DIR, exist_ok=True)
    out_1080 = os.path.join(OUT_DIR, "thumbnail-2.0-1920x1080.png")
    out_720 = os.path.join(OUT_DIR, "thumbnail-2.0-1280x720.png")
    rgb = img.convert("RGB")
    rgb.save(out_1080, optimize=True)
    rgb.resize((1280, 720), Image.LANCZOS).save(out_720, optimize=True)
    print("Wrote", out_1080)
    print("Wrote", out_720)


if __name__ == "__main__":
    build_thumbnail()
