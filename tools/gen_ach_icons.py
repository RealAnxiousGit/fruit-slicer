"""32 bespoke achievement icons (32x32 supersampled) for Fruit Slicer 2.1.0."""
import math
from PIL import Image, ImageDraw


def make_ach_icon(index, canvas, SS, el, poly, shade, shrink, sticker):
    img = canvas(32)
    d = ImageDraw.Draw(img)

    # Distinct background badge shapes per tier/theme so locked silhouettes read cleanly too.
    gold = (255, 214, 72, 255)
    gold_d = (188, 124, 22, 255)
    cream = (255, 248, 232, 255)
    red = (238, 72, 76, 255)
    blue = (86, 176, 255, 255)
    green = (112, 216, 92, 255)
    purple = (182, 108, 248, 255)
    orange = (255, 148, 56, 255)
    pink = (255, 120, 178, 255)
    dark = (38, 32, 48, 255)

    def ln(pts, width=2.2, c=cream):
        d.line([(x * SS, y * SS) for x, y in pts], fill=c, width=int(width * SS), joint="curve")

    def star(cx, cy, r_out, r_in, pts_n, c, rot=0.0):
        pts = []
        for i in range(pts_n * 2):
            a = rot + i * math.pi / pts_n - math.pi / 2
            r = r_out if i % 2 == 0 else r_in
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        poly(d, pts, c)

    if index == 0:  # Ooh, Shiny! - Gold coin + sparkle
        el(d, 16, 16, 12, 12, gold_d)
        el(d, 16, 15.5, 10, 10, gold)
        el(d, 16, 15.5, 6.8, 6.8, (255, 236, 130, 255))
        star(24.5, 7.5, 5.2, 1.8, 4, cream)
    elif index == 1:  # Transaction Complete! - Shopping bag
        poly(d, [(7, 11), (25, 11), (27, 28), (5, 28)], orange)
        ln([(11, 11), (11, 6), (21, 6), (21, 11)], 2.4, gold)
        el(d, 16, 19.5, 4.5, 4.5, gold)
    elif index == 2:  # Fruit Juice - Juice cup + straw
        poly(d, [(9, 11), (23, 11), (21, 28), (11, 28)], red)
        poly(d, [(8, 9), (24, 9), (24, 12), (8, 12)], cream)
        ln([(18, 9), (20, 3), (24, 3)], 2.2, green)
    elif index == 3:  # What a Mess! - Overflowing fruit splash
        el(d, 16, 18, 11, 9, pink)
        star(16, 16, 13, 6, 6, orange, 0.3)
        el(d, 16, 17, 6.5, 6.5, red)
        el(d, 13, 12, 2.2, 2.2, green)
        el(d, 21, 11, 2.0, 2.0, gold)
    elif index == 4:  # A New Coat of Paint - Paint roller
        poly(d, [(6, 6), (24, 6), (24, 13), (6, 13)], blue)
        ln([(24, 9.5), (27, 9.5), (27, 16), (16, 18), (16, 27)], 2.2, cream)
        poly(d, [(13.5, 21), (18.5, 21), (18.5, 29), (13.5, 29)], orange)
    elif index == 5:  # A New Foe! - StreetPass green emblem
        el(d, 16, 16, 13, 13, (40, 160, 72, 255))
        el(d, 16, 15.5, 10.5, 10.5, green)
        el(d, 16, 11, 4.0, 4.0, cream)
        poly(d, [(9.5, 23), (11.5, 16.5), (20.5, 16.5), (22.5, 23)], cream)
    elif index == 6:  # Let's Party! - Rainbow celebration emblem
        star(16, 16, 13, 6.5, 5, pink)
        el(d, 16, 16, 6.5, 6.5, gold)
        el(d, 7, 7, 2.2, 2.2, blue)
        el(d, 25, 7, 2.2, 2.2, green)
        el(d, 25, 25, 2.2, 2.2, orange)
    elif index == 7:  # Quarter of a Milestone - Stone marker "25"
        poly(d, [(7, 7), (25, 7), (27, 27), (5, 27)], (132, 142, 162, 255))
        poly(d, [(9, 9), (23, 9), (24.5, 25), (7.5, 25)], (178, 188, 206, 255))
        ln([(10, 14), (14, 14), (14, 17), (10, 19), (10, 22), (14.5, 22)], 1.8, dark)
        ln([(22, 14), (17.5, 14), (17.5, 18), (22, 18), (22, 22), (17.5, 22)], 1.8, dark)
    elif index == 8:  # Insane Juggler - Three juggling balls mid-arc
        ln([(6, 22), (16, 6), (26, 22)], 1.8, cream)
        el(d, 8, 22, 4.5, 4.5, red)
        el(d, 16, 8.5, 4.5, 4.5, gold)
        el(d, 24, 22, 4.5, 4.5, blue)
    elif index == 9:  # Big Spender - Overflowing coin chest
        poly(d, [(5, 14), (27, 14), (26, 27), (6, 27)], (152, 92, 44, 255))
        poly(d, [(6, 9), (26, 9), (27, 14), (5, 14)], (196, 128, 62, 255))
        el(d, 11, 13, 3.2, 3.2, gold)
        el(d, 16, 11.5, 3.5, 3.5, gold)
        el(d, 21, 13, 3.2, 3.2, gold)
    elif index == 10:  # Sell Out - Golden crown + tag
        poly(d, [(5, 22), (6, 10), (11, 15), (16, 7), (21, 15), (26, 10), (27, 22)], gold)
        poly(d, [(5, 22), (27, 22), (27, 26), (5, 26)], gold_d)
        el(d, 16, 17, 2.5, 2.5, red)
    elif index == 11:  # A Century Milestone! - Golden "100" monument
        poly(d, [(5, 8), (27, 8), (29, 26), (3, 26)], gold_d)
        poly(d, [(7, 10), (25, 10), (26.5, 24), (5.5, 24)], gold)
        ln([(9, 13), (9, 21)], 2.0, dark)
        el(d, 15.5, 17, 3.2, 4.0, dark)
        el(d, 15.5, 17, 1.4, 2.2, gold)
        el(d, 22.5, 17, 3.2, 4.0, dark)
        el(d, 22.5, 17, 1.4, 2.2, gold)
    elif index == 12:  # Completionist - Rainbow star medallion
        el(d, 16, 16, 13.5, 13.5, gold)
        star(16, 16, 11, 5.2, 5, purple)
        star(16, 16, 7, 3.2, 5, cream)
    elif index == 13:  # Mix It Up - Swirling coconut
        el(d, 16, 16, 11.5, 11.5, (124, 78, 44, 255))
        el(d, 16, 16, 8.2, 8.2, cream)
        ln([(12, 16), (16, 12), (20, 16), (16, 20)], 2.0, orange)
        star(25, 7, 4.2, 1.5, 4, gold)
    elif index == 14:  # Pinata Party - Festive striped pinata
        poly(d, [(7, 13), (22, 9), (26, 20), (11, 24)], pink)
        poly(d, [(12, 12), (17, 10.5), (20, 21.5), (15, 23)], gold)
        poly(d, [(7, 13), (11, 24), (7, 27), (5, 20)], blue)
    elif index == 15:  # Reroll Addict - Golden die
        poly(d, [(7, 7), (25, 7), (25, 25), (7, 25)], gold)
        for px, py in ((11.5, 11.5), (20.5, 11.5), (16, 16), (11.5, 20.5), (20.5, 20.5)):
            el(d, px, py, 2.0, 2.0, dark)
    elif index == 16:  # Close Call - Phoenix feather & heart
        el(d, 16, 18, 9, 8, red)
        poly(d, [(16, 4), (24, 15), (16, 26), (8, 15)], orange)
        el(d, 16, 17, 4.5, 4.5, gold)
    elif index == 17:  # Combo Master - Slashed fruit + sparks
        el(d, 15, 17, 10, 10, green)
        ln([(5, 25), (27, 7)], 2.8, cream)
        star(25, 8, 4.5, 1.5, 4, gold)
    elif index == 18:  # Classic Legend - Classic golden trophy
        poly(d, [(9, 7), (23, 7), (20, 18), (12, 18)], gold)
        poly(d, [(14, 18), (18, 18), (18, 24), (14, 24)], gold_d)
        poly(d, [(10, 24), (22, 24), (23, 28), (9, 28)], gold)
    elif index == 19:  # Arcade Ace - Lightning bolt
        poly(d, [(18, 4), (8, 17), (15, 17), (13, 28), (24, 14), (17, 14)], blue)
        poly(d, [(17, 7), (11, 16), (16, 16), (15, 24), (21, 15), (16, 15)], cream)
    elif index == 20:  # Rhythm Master - Glowing music note
        el(d, 11, 22, 4.5, 3.8, purple)
        el(d, 22, 19, 4.5, 3.8, purple)
        ln([(14.5, 21), (14.5, 8), (25.5, 6), (25.5, 18)], 2.6, gold)
    elif index == 21:  # Zen Serenity - Crescent moon & lotus
        el(d, 16, 15, 11, 11, blue)
        el(d, 20, 13, 8.5, 8.5, dark)
        el(d, 16, 24, 8, 3.5, pink)
    elif index == 22:  # Master Chef - Chef hat + bowl
        el(d, 12, 11, 5.5, 5.5, cream)
        el(d, 20, 11, 5.5, 5.5, cream)
        el(d, 16, 9, 6.0, 6.0, cream)
        poly(d, [(9, 14), (23, 14), (22, 22), (10, 22)], cream)
        poly(d, [(8, 23), (24, 23), (21, 28), (11, 28)], orange)
    elif index == 23:  # Deadeye - Bullseye target
        el(d, 16, 16, 12.5, 12.5, red)
        el(d, 16, 16, 8.5, 8.5, cream)
        el(d, 16, 16, 4.5, 4.5, red)
        el(d, 16, 16, 1.8, 1.8, gold)
    elif index == 24:  # Orchard Survivor - Fresh orchard apple + leaf
        el(d, 16, 18, 10.5, 9.5, red)
        ln([(16, 9), (16, 5)], 2.0, (124, 78, 44, 255))
        poly(d, [(16, 7), (23, 4), (21, 10)], green)
    elif index == 25:  # Mixologist - Triple chaos badges
        el(d, 10, 20, 6.2, 6.2, red)
        el(d, 22, 20, 6.2, 6.2, blue)
        el(d, 16, 10, 6.2, 6.2, gold)
    elif index == 26:  # Dedicated Slicer - Golden 7-day calendar
        poly(d, [(6, 8), (26, 8), (26, 26), (6, 26)], cream)
        poly(d, [(6, 8), (26, 8), (26, 13), (6, 13)], red)
        ln([(12, 16), (20, 16), (14, 24)], 2.2, dark)
    elif index == 27:  # Feelin' Lucky - Holographic Mythic card + clover
        poly(d, [(7, 5), (25, 5), (25, 27), (7, 27)], purple)
        poly(d, [(9, 7), (23, 7), (23, 25), (9, 25)], pink)
        el(d, 13, 14, 3.2, 3.2, green)
        el(d, 19, 14, 3.2, 3.2, green)
        el(d, 16, 19, 3.2, 3.2, green)
    elif index == 28:  # Milestone Breaker - Shattered spiked boss emblem
        star(16, 16, 13, 8, 8, orange)
        el(d, 16, 16, 7, 7, gold)
        ln([(9, 23), (23, 9)], 2.4, dark)
    elif index == 29:  # Boss Rush Champion - Gold stopwatch + ribbon
        el(d, 16, 17, 10.5, 10.5, gold)
        el(d, 16, 17, 7.5, 7.5, cream)
        ln([(16, 17), (16, 11)], 2.0, red)
        ln([(16, 17), (20, 17)], 1.8, dark)
        poly(d, [(13, 4), (19, 4), (19, 7), (13, 7)], gold_d)
    elif index == 30:  # Untouchable - Gleaming crystal shield
        poly(d, [(6, 6), (26, 6), (24, 19), (16, 28), (8, 19)], blue)
        poly(d, [(9, 9), (23, 9), (21, 18), (16, 24), (11, 18)], (176, 236, 255, 255))
        star(12, 12, 3.8, 1.3, 4, cream)
    else:  # 31: Full Arsenal - Six-blade crest
        for k in range(6):
            a = k * math.pi / 3
            x2 = 16 + math.cos(a) * 12
            y2 = 16 + math.sin(a) * 12
            ln([(16, 16), (x2, y2)], 2.6, gold if k % 2 == 0 else blue)
        el(d, 16, 16, 4.5, 4.5, cream)

    return shrink(sticker(img, 1.25))
