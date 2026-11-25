"""12 new Orchard Blast passive charm icons (32x32 medallion badges) for 2.1.0."""
import math
from PIL import ImageDraw


def make_charm_icon21(idx, canvas, SS, el, poly, shade, shrink, sticker):
    # idx 0..2: Uncommon (Emerald), 3: Rare (Cyan), 4..7: Epic (Amethyst), 8..11: Mythic (Rainbow/Gold)
    if idx < 3:
        fill, edge, rim_hi = (52, 168, 92, 255), (26, 92, 48, 255), (150, 244, 178, 255)
    elif idx == 3:
        fill, edge, rim_hi = (54, 152, 224, 255), (24, 78, 128, 255), (160, 228, 255, 255)
    elif idx < 8:
        fill, edge, rim_hi = (146, 78, 212, 255), (76, 34, 124, 255), (218, 172, 255, 255)
    else:
        fill, edge, rim_hi = (224, 108, 168, 255), (116, 36, 92, 255), (255, 232, 132, 255)

    img = canvas(32)
    d = ImageDraw.Draw(img)
    el(d, 16, 16, 15.2, 15.2, edge)
    el(d, 16, 15.6, 13.4, 13.4, fill)
    img = shade(img, light=(255, 255, 240, 88), dark=(0, 0, 0, 72))
    d = ImageDraw.Draw(img)
    d.arc([3.2 * SS, 2.8 * SS, 28.8 * SS, 28.4 * SS], 195, 325, fill=rim_hi, width=int(1.6 * SS))
    d.arc([3.2 * SS, 3.4 * SS, 28.8 * SS, 29.0 * SS], 20, 160, fill=edge, width=int(1.4 * SS))

    w = (255, 252, 242, 255)
    gold = (255, 214, 78, 255)
    orange = (255, 148, 56, 255)
    pink = (255, 120, 178, 255)
    ice = (176, 236, 255, 255)
    dark = (40, 30, 52, 255)

    def ln(pts, width=2.2, c=w):
        d.line([(x * SS, y * SS) for x, y in pts], fill=c, width=int(width * SS), joint="curve")

    def star(cx, cy, r_out, r_in, n, c):
        pts = []
        for i in range(n * 2):
            a = i * math.pi / n - math.pi / 2
            r = r_out if i % 2 == 0 else r_in
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        poly(d, pts, c)

    if idx == 0:  # Citrus Shield
        poly(d, [(9, 9), (23, 9), (21, 19), (16, 25), (11, 19)], orange)
        poly(d, [(11, 11), (21, 11), (19.5, 18), (16, 22), (12.5, 18)], gold)
    elif idx == 1:  # Sweet Tooth
        el(d, 16, 17, 7.5, 7.5, pink)
        el(d, 16, 17, 2.8, 2.8, fill)
        el(d, 22, 10, 2.4, 2.4, w)
    elif idx == 2:  # Quick Draw
        el(d, 16, 17, 7.5, 7.5, w)
        el(d, 16, 17, 5.4, 5.4, dark)
        ln([(16, 17), (16, 13), (19.5, 17)], 1.8, gold)
    elif idx == 3:  # Juicy Jackpot
        el(d, 13, 17, 6.2, 6.2, gold)
        el(d, 21.5, 12, 3.2, 3.2, orange)
        ln([(18, 21), (23, 21)], 2.2, w)
    elif idx == 4:  # Frostbite Edge
        ln([(8, 24), (24, 8)], 3.0, ice)
        star(12, 12, 4.2, 1.5, 6, w)
    elif idx == 5:  # Chain Reaction
        d.ellipse([8 * SS, 8 * SS, 24 * SS, 24 * SS], outline=gold, width=int(2.0 * SS))
        star(16, 16, 6.2, 2.8, 6, w)
    elif idx == 6:  # Royal Tribute
        poly(d, [(10, 10), (22, 10), (19, 18), (13, 18)], gold)
        poly(d, [(12, 22), (20, 22), (21, 25), (11, 25)], gold)
        ln([(16, 18), (16, 22)], 2.4, gold)
    elif idx == 7:  # Phantom Parry
        el(d, 16, 17, 6.5, 6.5, dark)
        ln([(7, 23), (25, 9)], 2.6, gold)
        star(22, 10, 3.6, 1.3, 4, w)
    elif idx == 8:  # Orchard Crown (Mythic)
        poly(d, [(8, 21), (9, 11), (13, 15), (16, 8), (19, 15), (23, 11), (24, 21)], gold)
        el(d, 16, 17, 2.2, 2.2, ice)
    elif idx == 9:  # Twin Harvest (Mythic)
        poly(d, [(7, 10), (16, 10), (16, 23), (7, 23)], w)
        poly(d, [(15, 8), (24, 8), (24, 21), (15, 21)], gold)
    elif idx == 10:  # Solar Blade (Mythic)
        star(16, 16, 10, 4.8, 8, gold)
        ln([(7, 24), (25, 8)], 2.8, w)
    else:  # 11: Prismatic Core (Mythic)
        poly(d, [(16, 6), (25, 16), (16, 26), (7, 16)], ice)
        poly(d, [(16, 9), (21, 16), (16, 23), (11, 16)], gold)
        el(d, 16, 16, 2.6, 2.6, w)

    return shrink(sticker(img, 1.4))
