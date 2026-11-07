"""Procedural backgrounds: top-screen scenes and bottom-screen slicing boards."""
import math
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

BOARD_W, BOARD_H = 320, 240


def smooth_noise(w, h, scale_x, scale_y, seed):
    """Value noise: random grid resized with bicubic smoothing, range 0..1."""
    rng = np.random.default_rng(seed)
    grid = rng.random((max(2, int(h / scale_y)) + 2, max(2, int(w / scale_x)) + 2))
    img = Image.fromarray((grid * 255).astype(np.uint8), "L").resize((w, h), Image.BICUBIC)
    return np.asarray(img, np.float32) / 255.0


def fractal(w, h, scale, seed, octaves=4, stretch=1.0):
    total = np.zeros((h, w), np.float32)
    amp = 1.0
    norm = 0.0
    for o in range(octaves):
        total += smooth_noise(w, h, max(1, scale * stretch), max(1, scale), seed + o) * amp
        norm += amp
        amp *= 0.5
        scale /= 2
    return total / norm


def vignette(arr, strength=0.85, floor=0.35):
    y, x = np.mgrid[0:arr.shape[0], 0:arr.shape[1]]
    d = np.sqrt(((x - arr.shape[1] / 2) / (arr.shape[1] * 0.72)) ** 2 + ((y - arr.shape[0] / 2) / (arr.shape[0] * 0.72)) ** 2)
    return arr * np.clip(1.05 - d * d * strength, floor, 1.05)[..., None]


def finish(arr):
    rgb = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, np.full(rgb.shape[:2], 255, np.uint8)]), "RGBA")


def mix(a, b, t):
    return a[None, None, :] * (1 - t[..., None]) + b[None, None, :] * t[..., None]


def board_blanket():
    W, H = BOARD_W, BOARD_H
    y, x = np.mgrid[0:H, 0:W]
    size = 32
    a, b = (x // size) % 2, (y // size) % 2
    arr = np.zeros((H, W, 3), np.float32)
    arr[:] = np.array([38, 62, 92], np.float32)
    arr[(a == 1) ^ (b == 1)] = np.array([58, 92, 122], np.float32)
    arr[(a == 1) & (b == 1)] = np.array([84, 124, 142], np.float32)
    arr += (((x + y) % 3 == 0) * 5.0 - ((x - y) % 4 == 0) * 4.0)[..., None]
    arr += np.random.default_rng(11).normal(0, 1.8, (H, W, 1))
    return finish(vignette(arr))


def board_wood():
    """Kitchen cutting board: one slab with curved end grain, knife marks and a juice groove."""
    W, H = BOARD_W, BOARD_H
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    warp = fractal(W, H, 90, 41, 3, stretch=3.0)
    rings = np.sin((y + warp * 70) * 0.42 + fractal(W, H, 40, 44, 3, stretch=4.0) * 5)
    t = np.clip(0.5 + rings * 0.38 + (fractal(W, H, 8, 47, 2, stretch=9.0) - 0.5) * 0.7, 0, 1)
    arr = mix(np.array([118, 72, 40], np.float32), np.array([176, 120, 72], np.float32), t)
    img = finish(arr)
    marks = Image.new("RGBA", (W * 2, H * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(marks)
    rng = random.Random(5)
    for _ in range(46):
        px, py = rng.uniform(30, W * 2 - 30), rng.uniform(30, H * 2 - 30)
        a, ln = rng.uniform(-0.9, 0.9), rng.uniform(30, 120)
        d.line([px, py, px + math.cos(a) * ln, py + math.sin(a) * ln], fill=(60, 32, 16, 70), width=1)
        d.line([px, py + 1.5, px + math.cos(a) * ln, py + 1.5 + math.sin(a) * ln], fill=(235, 190, 140, 34), width=1)
    d.rounded_rectangle([20, 20, W * 2 - 20, H * 2 - 20], 34, outline=(70, 38, 18, 170), width=6)
    d.rounded_rectangle([24, 25, W * 2 - 24, H * 2 - 18], 30, outline=(226, 170, 116, 60), width=2)
    img.alpha_composite(marks.resize((W, H), Image.LANCZOS))
    return finish(vignette(np.asarray(img.convert("RGB"), np.float32), 0.55, 0.55))


def board_plate():
    """Ceramic plate on a soft linen tablecloth."""
    W, H = BOARD_W, BOARD_H
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    linen = 150 + 8 * np.sin(x * 2.1) + 8 * np.sin(y * 2.1) + (fractal(W, H, 30, 61, 3) - 0.5) * 22
    arr = np.dstack([linen * 0.88, linen * 0.66, linen * 0.54]).astype(np.float32)
    r = np.sqrt((x - W / 2) ** 2 + (y - H / 2) ** 2) / 116.0
    shadow = np.clip(1 - np.sqrt((x - W / 2 - 2) ** 2 + (y - H / 2 - 7) ** 2) / 124.0, 0, 1)
    arr *= (1 - np.clip(shadow * 5, 0, 1) * 0.36)[..., None]
    edge = np.clip((1.04 - r) * 36, 0, 1)[..., None]
    plate = np.full((H, W, 3), 0, np.float32)
    plate[:] = np.array([242, 240, 232], np.float32)
    # Raised rim, then a softly shaded well.
    shade = 1.0 - 0.13 * np.exp(-(((r - 0.70) / 0.05) ** 2)) + 0.05 * np.exp(-(((r - 0.76) / 0.05) ** 2))
    shade -= 0.05 * np.clip(0.68 - r, 0, 1)
    plate *= shade[..., None]
    ring = np.exp(-(((r - 0.93) / 0.012) ** 2))[..., None]
    plate = plate * (1 - ring) + np.array([58, 110, 164], np.float32) * ring
    ring2 = np.exp(-(((r - 0.88) / 0.005) ** 2))[..., None]
    plate = plate * (1 - ring2) + np.array([58, 110, 164], np.float32) * ring2
    arr = arr * (1 - edge) + plate * edge
    return finish(vignette(arr, 0.35, 0.7))


def board_marble():
    W, H = BOARD_W, BOARD_H
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    f = fractal(W, H, 110, 71, 5)
    veins = np.abs(np.sin((x * 0.007 + y * 0.012 + f * 2.6) * math.pi))
    v = np.clip(1 - veins * 7, 0, 1) ** 1.5
    v = np.asarray(Image.fromarray((v * 255).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(0.8)), np.float32) / 255
    f2 = fractal(W, H, 70, 77, 4)
    veins2 = np.abs(np.sin((x * 0.012 - y * 0.006 + f2 * 3.2) * math.pi))
    v2 = np.clip(1 - veins2 * 12, 0, 1) ** 1.5
    base = 214 + (f - 0.5) * 34
    arr = np.dstack([base, base * 1.0, base * 1.03]).astype(np.float32)
    arr = arr * (1 - v[..., None] * 0.55) + np.array([88, 92, 108], np.float32) * v[..., None] * 0.55
    arr = arr * (1 - v2[..., None] * 0.3) + np.array([190, 160, 110], np.float32) * v2[..., None] * 0.3
    return finish(vignette(arr, 0.5, 0.55))


def board_sand():
    """Sandy beach with ripple lines and a little foam at the edge."""
    W, H = BOARD_W, BOARD_H
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    f = fractal(W, H, 70, 81, 4)
    ripples = np.sin((y + f * 80) * 0.5) * 0.5 + 0.5
    arr = mix(np.array([206, 170, 116], np.float32), np.array([236, 210, 160], np.float32), ripples * 0.55 + f * 0.45)
    arr += np.random.default_rng(83).normal(0, 4.5, (H, W, 1))
    shore = 28 + 12 * np.sin(x * 0.03) + 8 * np.sin(x * 0.09 + 1)
    depth = np.clip((shore - y) / 22, 0, 1)[..., None]
    arr = arr * (1 - depth) + (np.array([86, 176, 186], np.float32) * (1 - depth * 0.4)) * depth
    foam = np.exp(-(((y - shore) / 3.2) ** 2))[..., None]
    arr = arr * (1 - foam * 0.85) + np.array([250, 250, 244], np.float32) * foam * 0.85
    return finish(vignette(arr, 0.45, 0.6))


def board_starlight():
    """Deep night sky: nebula haze and stars, for a bright, high-contrast field."""
    W, H = BOARD_W, BOARD_H
    f = fractal(W, H, 120, 91, 5)
    g = fractal(W, H, 80, 95, 4)
    arr = np.zeros((H, W, 3), np.float32)
    arr[:] = np.array([14, 14, 40], np.float32)
    arr += np.clip(f - 0.46, 0, 1)[..., None] * np.array([190, 70, 200], np.float32)
    arr += np.clip(g - 0.5, 0, 1)[..., None] * np.array([40, 130, 220], np.float32)
    img = finish(arr)
    d = ImageDraw.Draw(img)
    rng = random.Random(97)
    for _ in range(170):
        px, py, r = rng.uniform(0, W), rng.uniform(0, H), rng.choice((0.4, 0.5, 0.7, 1.0))
        d.ellipse([px - r, py - r, px + r, py + r], fill=(255, 250, 235, rng.randint(120, 255)))
    return img


def bg_boards():
    return [board_blanket(), board_wood(), board_plate(), board_marble(), board_sand(), board_starlight()]


def add_stars(img, rng, n, limit):
    d = ImageDraw.Draw(img)
    for _ in range(n):
        x, y = rng.uniform(0, img.width), rng.uniform(0, limit)
        r = rng.choice((0.5, 0.8, 1.1))
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 244, 220, 220))


def add_glow(img, spr_soft, cx, cy, size, disc, disc_color, glow_color, power=1.3):
    glow = np.array(spr_soft(size, power))
    glow[..., 0], glow[..., 1], glow[..., 2] = glow_color
    img.alpha_composite(Image.fromarray(glow, "RGBA"), (int(cx - size / 2), int(cy - size / 2)))
    ImageDraw.Draw(img).ellipse([cx - disc, cy - disc, cx + disc, cy + disc], fill=disc_color)


def bg_top(scene, hill_layer, v_gradient, spr_soft):
    W, H = 424, 240
    rng = random.Random(21 + scene)
    if scene == 0:  # Dusk Orchard
        sky = v_gradient(W, H, [(0, (44, 34, 92)), (0.42, (150, 66, 128)), (0.72, (246, 136, 96)), (1, (255, 206, 130))])
        add_stars(sky, rng, 46, 96)
        add_glow(sky, spr_soft, W // 2 + 30, 140, 190, 30, (255, 238, 186, 255), (255, 226, 150))
        far = hill_layer(W, 120, 42, 16, 0.016, (120, 62, 124, 255), 3)
        mid = hill_layer(W, 110, 42, 14, 0.021, (76, 46, 104, 255), 5, trees=11)
        near = hill_layer(W, 70, 30, 9, 0.028, (42, 30, 74, 255), 8, trees=5)
    elif scene == 1:  # Morning Meadow
        sky = v_gradient(W, H, [(0, (96, 170, 232)), (0.55, (176, 222, 244)), (1, (252, 240, 206))])
        add_glow(sky, spr_soft, 96, 66, 170, 22, (255, 252, 226, 255), (255, 246, 196))
        d = ImageDraw.Draw(sky)
        cloud = Image.new("RGBA", sky.size, (255, 255, 255, 0))
        cd = ImageDraw.Draw(cloud)
        for _ in range(7):
            cx, cy = rng.uniform(0, W), rng.uniform(16, 96)
            for _ in range(7):
                ox, oy, r = rng.uniform(-30, 30), rng.uniform(-5, 6), rng.uniform(9, 17)
                cd.ellipse([cx + ox - r * 1.3, cy + oy - r * 0.7, cx + ox + r * 1.3, cy + oy + r * 0.7], fill=(255, 255, 255, 210))
        sky.alpha_composite(cloud.filter(ImageFilter.GaussianBlur(1.6)))
        del d
        far = hill_layer(W, 120, 44, 15, 0.017, (138, 196, 150, 255), 13)
        mid = hill_layer(W, 110, 44, 13, 0.022, (102, 174, 96, 255), 15, trees=9, tree_color=(62, 136, 84, 255), dots=(232, 70, 70, 255))
        near = hill_layer(W, 70, 30, 8, 0.03, (64, 138, 72, 255), 18, trees=4, tree_color=(38, 104, 62, 255), dots=(252, 186, 60, 255))
    else:  # Moonlit Bay
        sky = v_gradient(W, H, [(0, (10, 14, 44)), (0.55, (24, 42, 92)), (1, (66, 92, 148))])
        add_stars(sky, rng, 110, 130)
        add_glow(sky, spr_soft, W - 110, 62, 150, 24, (240, 240, 218, 255), (190, 206, 255), 1.7)
        d = ImageDraw.Draw(sky)
        for cx, cy, r in ((W - 118, 56, 4), (W - 102, 68, 3), (W - 108, 52, 2)):
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(208, 208, 192, 255))
        far = hill_layer(W, 120, 40, 12, 0.019, (34, 48, 96, 255), 23)
        # The middle layer is water with the moon's reflection.
        mid = Image.new("RGBA", (W, 110), (0, 0, 0, 0))
        md = ImageDraw.Draw(mid)
        md.rectangle([0, 30, W, 110], fill=(20, 38, 84, 255))
        for i in range(34):
            y = 32 + i * 2.2
            half = 8 + i * 0.9 + rng.uniform(-3, 3)
            x = W - 110 + rng.uniform(-4, 4)
            md.line([x - half, y, x + half, y], fill=(190, 206, 240, max(30, 190 - i * 5)), width=1)
        for _ in range(60):
            x, y = rng.uniform(0, W), rng.uniform(34, 108)
            md.line([x - rng.uniform(4, 16), y, x + rng.uniform(4, 16), y], fill=(70, 100, 160, 110), width=1)
        near = hill_layer(W, 70, 34, 10, 0.03, (10, 16, 40, 255), 28, trees=6, tree_color=(8, 12, 32, 255), dots=(250, 214, 110, 255))
    return sky, far, mid, near
