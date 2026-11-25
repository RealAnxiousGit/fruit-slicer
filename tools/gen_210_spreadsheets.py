#!/usr/bin/env python3
"""Generates the 3 Fruit Slicer 2.1.0 tracking workbooks in the Other Mods root:
  - Fruit Slicer Achievements.xlsx
  - Fruit Slicer Shop Items.xlsx
  - Fruit Slicer Orchard Blast Charms.xlsx
Uses Excel-2007-compatible formulas (=COUNTA, =SUM, =COUNTIF, =SUMIF) and clean Arial styling.
"""
import os
import re
import openpyxl
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.abspath(os.path.join(TOOLS_DIR, ".."))
OUT_ROOT = os.path.abspath(os.path.join(REPO_DIR, ".."))

FONT_HDR = Font(name="Arial", size=11, bold=True, color="FFFFFF")
FONT_BODY = Font(name="Arial", size=10, color="1E2430")
FONT_BOLD = Font(name="Arial", size=10, bold=True, color="1E2430")
FONT_TITLE = Font(name="Arial", size=13, bold=True, color="1F3552")

FILL_HDR = PatternFill("solid", fgColor="1F3552")
FILL_SUBHDR = PatternFill("solid", fgColor="2F4E75")
FILL_EVEN = PatternFill("solid", fgColor="F5F8FC")
FILL_ODD = PatternFill("solid", fgColor="FFFFFF")
FILL_TOTAL = PatternFill("solid", fgColor="E4ECF7")

THIN_GRAY = Side(style="thin", color="D0D8E4")
BORDER_CELL = Border(left=THIN_GRAY, right=THIN_GRAY, top=THIN_GRAY, bottom=THIN_GRAY)


def style_sheet(ws, headers, rows, widths, col_aligns=None):
    ws.views.sheetView[0].showGridLines = True
    ws.freeze_panes = "A2"
    for c_idx, h in enumerate(headers, 1):
        cell = ws.cell(row=1, column=c_idx, value=h)
        cell.font = FONT_HDR
        cell.fill = FILL_HDR
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
        cell.border = BORDER_CELL
    ws.row_dimensions[1].height = 24

    for r_idx, row in enumerate(rows, 2):
        fill = FILL_EVEN if r_idx % 2 == 0 else FILL_ODD
        for c_idx, val in enumerate(row, 1):
            cell = ws.cell(row=r_idx, column=c_idx, value=val)
            cell.font = FONT_BODY
            cell.fill = fill
            align = (col_aligns[c_idx - 1] if col_aligns else "left")
            cell.alignment = Alignment(horizontal=align, vertical="center", wrap_text=True)
            cell.border = BORDER_CELL
        ws.row_dimensions[r_idx].height = 20

    for c_idx, w in enumerate(widths, 1):
        ws.column_dimensions[get_column_letter(c_idx)].width = w


def add_summary_table(ws, title, headers, rows, widths):
    ws.views.sheetView[0].showGridLines = True
    ws.cell(row=1, column=1, value=title).font = FONT_TITLE
    ws.row_dimensions[1].height = 24
    for c_idx, h in enumerate(headers, 1):
        cell = ws.cell(row=3, column=c_idx, value=h)
        cell.font = FONT_HDR
        cell.fill = FILL_SUBHDR
        cell.alignment = Alignment(horizontal="center", vertical="center")
        cell.border = BORDER_CELL
    ws.row_dimensions[3].height = 22
    for r_idx, row in enumerate(rows, 4):
        is_total = str(row[0]).startswith("Total")
        fill = FILL_TOTAL if is_total else (FILL_EVEN if r_idx % 2 == 0 else FILL_ODD)
        font = FONT_BOLD if is_total else FONT_BODY
        for c_idx, val in enumerate(row, 1):
            cell = ws.cell(row=r_idx, column=c_idx, value=val)
            cell.font = font
            cell.fill = fill
            cell.alignment = Alignment(horizontal="right" if c_idx > 1 else "left", vertical="center")
            cell.border = BORDER_CELL
        ws.row_dimensions[r_idx].height = 20
    for c_idx, w in enumerate(widths, 1):
        ws.column_dimensions[get_column_letter(c_idx)].width = w


def build_achievements_xlsx():
    achievements = [
        (1, "Ooh, Shiny!", "Collect your first coin.", 15, "Tier 1", "All", "Gleaming gold coin with sparkle", "Core"),
        (2, "Transaction Complete!", "Buy an item in the Shop.", 15, "Tier 1", "All", "Wooden shop bag with checkmark", "Core"),
        (3, "Fruit Juice", "Slice 100 fruits in total.", 15, "Tier 1", "All", "Cup filled with juice & straw", "Core"),
        (4, "A New Coat of Paint", "Enable your first mod.", 15, "Tier 1", "Universal", "Paint roller with rainbow streak", "Core"),
        (5, "Let's Party!", "Click the Main Menu logo.", 15, "Tier 1", "Universal", "Rainbow-lit sliced logo + confetti", "Core"),
        (6, "Mix It Up", "Play a run with the Mixer on.", 15, "Tier 1", "All", "Swirling coconut icon + sparkle", "2.1 Addition (Mixer OK)"),
        (7, "Piñata Party", "Burst a rare Piñata fruit.", 20, "Tier 1", "All", "Festive striped piñata spilling candy", "2.1 Addition"),
        (8, "A New Foe!", "Challenge a StreetPass player.", 25, "Tier 1", "3DS", "Custom green/white StreetPass emblem", "Core"),
        (9, "Reroll Addict", "Reroll 5 times in one Orchard run.", 25, "Tier 1", "All", "Spinning golden die with sparkles", "2.1 Addition"),
        (10, "Close Call", "Survive via Second Wind once.", 25, "Tier 1", "All", "Rising phoenix feather & heart", "2.1 Addition"),
        (11, "What a Mess!", "Slice 1,000 fruits in total.", 35, "Tier 2", "All", "Huge multicolored juice splatter", "Core"),
        (12, "Golden Harvest", "Slice 25 Golden Star Fruit.", 35, "Tier 2", "All", "Radiant 5-point golden star fruit", "2.1 Addition"),
        (13, "Master Chef", "Serve 5 orders in one Recipe run.", 35, "Tier 2", "All", "Chef hat over glowing smoothie cup", "2.1 Addition"),
        (14, "In the Groove", "Hit a 15-streak in Beat Slice.", 40, "Tier 2", "All", "Glowing eighth-note with pulse ring", "2.1 Addition"),
        (15, "Daily Ritual", "Reach a 7-day Daily Fruit streak.", 45, "Tier 2", "All", "Calendar page with golden flame 7", "2.1 Addition"),
        (16, "Card Shark", "Hold 8 passive charms in one run.", 45, "Tier 2", "All", "Fanned hand of glowing upgrade cards", "2.1 Addition"),
        (17, "Mixologist", "Finish with 3+ Mixer rules on.", 45, "Tier 2", "All", "Swirling coconut shaker", "2.1 Addition (Mixer OK)"),
        (18, "Quarter of a Milestone", "Reach Room 25 in Orchard Blast.", 50, "Tier 2", "All", "Stone milestone marker stamped 25", "Core"),
        (19, "Insane Juggler", "Keep 10 fruits airborne in Juggle.", 50, "Tier 2", "All", "Three colorful juggling balls", "Core"),
        (20, "Deadeye", "Score 50+ in Sharpshooter cleanly.", 50, "Tier 2", "All", "Glowing target bullseye crosshair", "2.1 Addition"),
        (21, "Royal Flush", "Beat all 4 regular Orchard bosses.", 50, "Tier 2", "All", "Four-gem royal crest", "2.1 Addition"),
        (22, "Legendary Cut", "Slice 10 fruits in one swipe.", 50, "Tier 2", "All", "Glowing katana slicing three stars", "2.1 Addition"),
        (23, "Glass Half Full", "Reach Room 15 with Glass Cannon.", 60, "Tier 2", "All", "Translucent glass blade & half-heart", "2.1 Addition"),
        (24, "Big Spender", "Spend 10,000 coins in the Shop.", 75, "Tier 3", "All", "Overflowing coin chest / gold stack", "Core"),
        (25, "Feelin' Lucky", "Win a Mythic charm from a boss.", 75, "Tier 3", "All", "Holographic Mythic card & gold clover", "2.1 Addition"),
        (26, "Milestone Breaker", "Beat a Room 25+ Milestone Boss.", 75, "Tier 3", "All", "Shattered spiked/citrus boss emblem", "2.1 Addition"),
        (27, "Boss Rush Champion", "Clear all 5 bosses in Boss Rush.", 75, "Tier 3", "All", "Gold stopwatch in victory ribbon", "2.1 Addition"),
        (28, "Untouchable", "Beat any boss without taking damage.", 85, "Tier 3", "All", "Gleaming crystal shield with glint", "2.1 Addition"),
        (29, "Sell Out", "Buy everything in the Shop.", 100, "Tier 3", "All", "Golden crown on a SOLD tag", "Core"),
        (30, "A Century Milestone!", "Reach Room 100 in Orchard Blast.", 100, "Tier 3", "All", "Gilded laurel trophy stamped 100", "Core"),
        (31, "Fruitocalypse", "Slice 10,000 fruits in total.", 100, "Tier 3", "All", "Crossed blades over exploding melon", "2.1 Addition"),
        (32, "Completionist", "Unlock every other achievement.", 100, "Tier 3", "All", "Holographic rainbow star medallion", "Core (Platform-aware)"),
    ]

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Achievements"
    style_sheet(
        ws,
        ["#", "Title", "Description", "Coins", "Tier", "Scope", "Bespoke Icon", "Notes"],
        achievements,
        [5, 24, 36, 9, 10, 12, 34, 24],
        ["center", "left", "left", "right", "center", "center", "left", "left"],
    )

    last_r = len(achievements) + 1
    ws_sum = wb.create_sheet("Summary")
    sum_rows = [
        ["Tier 1 (+15 to +25)", f'=COUNTIF(Achievements!E2:E{last_r},"Tier 1")', f'=SUMIF(Achievements!E2:E{last_r},"Tier 1",Achievements!D2:D{last_r})'],
        ["Tier 2 (+35 to +60)", f'=COUNTIF(Achievements!E2:E{last_r},"Tier 2")', f'=SUMIF(Achievements!E2:E{last_r},"Tier 2",Achievements!D2:D{last_r})'],
        ["Tier 3 (+75 to +100)", f'=COUNTIF(Achievements!E2:E{last_r},"Tier 3")', f'=SUMIF(Achievements!E2:E{last_r},"Tier 3",Achievements!D2:D{last_r})'],
        ["Scope: All Platforms", f'=COUNTIF(Achievements!F2:F{last_r},"All")', f'=SUMIF(Achievements!F2:F{last_r},"All",Achievements!D2:D{last_r})'],
        ["Scope: Universal Only", f'=COUNTIF(Achievements!F2:F{last_r},"Universal")', f'=SUMIF(Achievements!F2:F{last_r},"Universal",Achievements!D2:D{last_r})'],
        ["Scope: 3DS Only", f'=COUNTIF(Achievements!F2:F{last_r},"3DS")', f'=SUMIF(Achievements!F2:F{last_r},"3DS",Achievements!D2:D{last_r})'],
        ["Total Achievements", f'=COUNTA(Achievements!B2:B{last_r})', f'=SUM(Achievements!D2:D{last_r})'],
    ]
    add_summary_table(ws_sum, "Fruit Slicer 2.1.0 — Achievements Summary", ["Group", "Count", "Total Coin Payout"], sum_rows, [26, 14, 22])

    out_path = os.path.join(OUT_ROOT, "Fruit Slicer Achievements.xlsx")
    wb.save(out_path)
    print("Wrote", out_path)


def parse_shop_items():
    inc_path = os.path.join(REPO_DIR, "source", "shop_items.inc")
    txt = open(inc_path, "r", encoding="utf-8").read()
    boss_names = {
        "0": "Beat The Big Melon",
        "1": "Beat The Bomb King",
        "2": "Beat The Pineapple Guard",
        "3": "Beat The Inferno Pepper",
        "4": "Beat a Milestone Boss (Room 25+)",
    }
    rows = []
    idx = 1
    for line in txt.splitlines():
        s = line.strip()
        m = re.match(r'^SHOP_THEME\("([^"]+)",\s*(\d+),\s*"([^"]+)",', s)
        if m:
            rows.append((idx, "Board Theme", m.group(1), int(m.group(2)), m.group(3), "Unlocked at start (purchasable)"))
            idx += 1
            continue
        m = re.match(r'^SHOP_TRAIL\("([^"]+)",\s*(\d+),\s*"([^"]+)",\s*([A-Z0-9_]+),', s)
        if m:
            rows.append((idx, "Blade Trail", m.group(1), int(m.group(2)), m.group(3), f"Style: {m.group(4)}"))
            idx += 1
            continue
        m = re.match(r'^SHOP_TRACK\("([^"]+)",\s*(\d+),\s*"([^"]+)",\s*"([^"]+)",\s*(\d+)\)', s)
        if m:
            rows.append((idx, "Music Track", m.group(1), int(m.group(2)), m.group(3), f"{m.group(5)} BPM ({m.group(4)})"))
            idx += 1
            continue
        m = re.match(r'^BOSS_TRACK\("([^"]+)",\s*(\d+),\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*(\d+)\)', s)
        if m:
            b = boss_names.get(m.group(6), f"Boss {m.group(6)}")
            rows.append((idx, "Boss Track", m.group(1), int(m.group(2)), m.group(3), f"{m.group(5)} BPM | {b} ({m.group(4)})"))
            idx += 1
            continue
        m = re.match(r'^SHOP_SPLAT\("([^"]+)",\s*(\d+),\s*"([^"]+)",', s)
        if m:
            rows.append((idx, "Juice Splat", m.group(1), int(m.group(2)), m.group(3), "Custom splat colour"))
            idx += 1
            continue
        m = re.match(r'^SHOP_SKIN\("([^"]+)",\s*(\d+),\s*"([^"]+)",', s)
        if m:
            rows.append((idx, "Fruit Skin", m.group(1), int(m.group(2)), m.group(3), "3D/2D fruit material"))
            idx += 1
            continue
    return rows


def build_shop_xlsx():
    rows = parse_shop_items()
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Shop Items"
    style_sheet(
        ws,
        ["#", "Category", "Name", "Price (Coins)", "Description", "Details / Unlock"],
        rows,
        [5, 16, 28, 14, 48, 38],
        ["center", "center", "left", "right", "left", "left"],
    )

    last_r = len(rows) + 1
    cats = ["Board Theme", "Blade Trail", "Music Track", "Boss Track", "Juice Splat", "Fruit Skin"]
    ws_sum = wb.create_sheet("Summary")
    sum_rows = [
        [c, f'=COUNTIF(\'Shop Items\'!B2:B{last_r},"{c}")', f'=SUMIF(\'Shop Items\'!B2:B{last_r},"{c}",\'Shop Items\'!D2:D{last_r})']
        for c in cats
    ]
    sum_rows.append(["Total Shop Items", f"=COUNTA('Shop Items'!C2:C{last_r})", f"=SUM('Shop Items'!D2:D{last_r})"])
    add_summary_table(ws_sum, "Fruit Slicer — Shop Items Summary", ["Category", "Item Count", "Total Coins"], sum_rows, [22, 14, 16])

    out_path = os.path.join(OUT_ROOT, "Fruit Slicer Shop Items.xlsx")
    wb.save(out_path)
    print("Wrote", out_path)


def parse_existing_charms():
    src = open(os.path.join(REPO_DIR, "source", "orchard.cpp"), "r", encoding="utf-8").read()
    m = re.search(r"const CardInfo kCards\[CARD_COUNT\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise RuntimeError("Could not find kCards in orchard.cpp")
    body = m.group(1)
    kind_map = {
        "KIND_UPGRADE": "Upgrade",
        "KIND_PASSIVE": "Passive Charm",
        "KIND_REUSABLE": "Reusable Charm",
        "KIND_SINGLE": "Single-Use Charm",
    }
    rare_map = {
        "RARE_COMMON": "Common",
        "RARE_RARE": "Rare",
        "RARE_LEGEND": "Legendary",
    }
    rows = []
    idx = 1
    for line in body.splitlines():
        em = re.search(
            r'\{\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*([A-Z_]+)\s*,\s*([A-Z_]+)\s*,\s*(\d+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\}',
            line,
        )
        if not em:
            continue
        name, l1, l2, kind, rare, max_lv, lasts, cd = em.groups()
        desc = f"{l1} {l2}".strip()
        timing = "Passive / Stackable" if kind in ("KIND_UPGRADE", "KIND_PASSIVE") else (
            f"Lasts {lasts}s / CD {cd}s" if float(lasts) > 0 else (f"Instant / CD {cd}s" if float(cd) > 0 else "Single-use")
        )
        rows.append((idx, name, kind_map.get(kind, kind), rare_map.get(rare, rare), desc, timing, "Level-up draft", "2.0.0"))
        idx += 1
    return rows


def build_charms_xlsx():
    rows = parse_existing_charms()
    idx = len(rows) + 1
    new_210 = [
        ("Citrus Shield", "Passive Charm", "Uncommon", "Gain +5 temp shield HP entering each boss room.", "Passive", "Level-up draft", "2.1.0"),
        ("Sweet Tooth", "Passive Charm", "Uncommon", "Every 10th fruit sliced spawns a +1 HP droplet.", "Passive", "Level-up draft", "2.1.0"),
        ("Quick Draw", "Passive Charm", "Uncommon", "Reusable active charms recharge 25% faster.", "Passive", "Level-up draft", "2.1.0"),
        ("Juicy Jackpot", "Passive Charm", "Rare", "Coins grant +4 XP and drift toward your blade.", "Passive", "Level-up draft", "2.1.0"),
        ("Frostbite Edge", "Passive Charm", "Epic", "Every 6th swipe chills nearby fruit & shots (1.2s).", "Passive", "Level-up draft", "2.1.0"),
        ("Chain Reaction", "Passive Charm", "Epic", "4+ combos emit a shockwave popping nearby fruit.", "Passive", "Level-up draft", "2.1.0"),
        ("Royal Tribute", "Passive Charm", "Epic", "Beating a boss heals 40% HP + 1 Reroll + coins.", "Passive", "Level-up draft", "2.1.0"),
        ("Phantom Parry", "Passive Charm", "Epic", "Bombs sliced in Frenzy grant +10 XP, no damage.", "Passive", "Level-up draft", "2.1.0"),
        ("Orchard Crown", "Passive Charm", "Mythic", "Every 5 rooms: +1 Armor or +3 Max HP & better odds.", "Passive", "Boss drop (1/8 reg, 1/4 milestone) -> future drafts", "2.1.0"),
        ("Twin Harvest", "Passive Charm", "Mythic", "25% chance on level-up to pick 2 cards instead of 1.", "Passive", "Boss drop (1/8 reg, 1/4 milestone) -> future drafts", "2.1.0"),
        ("Solar Blade", "Passive Charm", "Mythic", "Every 3rd swipe fires a golden sunbeam wave slicing fruit & shots.", "Passive", "Boss drop (1/8 reg, 1/4 milestone) -> future drafts", "2.1.0"),
        ("Prismatic Core", "Passive Charm", "Mythic", "Doubles the strength of all other passive charm triggers.", "Passive", "Boss drop (1/8 reg, 1/4 milestone) -> future drafts", "2.1.0"),
    ]
    for name, kind, rare, desc, timing, unlock, ver in new_210:
        rows.append((idx, name, kind, rare, desc, timing, unlock, ver))
        idx += 1

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "Charms & Upgrades"
    style_sheet(
        ws,
        ["#", "Name", "Kind", "Rarity", "Effect / Description", "Timing / Cooldown", "Unlock Source", "Version"],
        rows,
        [5, 22, 18, 12, 54, 20, 44, 10],
        ["center", "left", "center", "center", "left", "center", "left", "center"],
    )

    last_r = len(rows) + 1
    ws_sum = wb.create_sheet("Summary")
    sum_rows = [
        ["Upgrade", f'=COUNTIF(\'Charms & Upgrades\'!C2:C{last_r},"Upgrade")', ""],
        ["Passive Charm", f'=COUNTIF(\'Charms & Upgrades\'!C2:C{last_r},"Passive Charm")', ""],
        ["Reusable Charm", f'=COUNTIF(\'Charms & Upgrades\'!C2:C{last_r},"Reusable Charm")', ""],
        ["Single-Use Charm", f'=COUNTIF(\'Charms & Upgrades\'!C2:C{last_r},"Single-Use Charm")', ""],
        ["Rarity: Common", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Common")'],
        ["Rarity: Uncommon", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Uncommon")'],
        ["Rarity: Rare", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Rare")'],
        ["Rarity: Epic", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Epic")'],
        ["Rarity: Legendary", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Legendary")'],
        ["Rarity: Mythic", "", f'=COUNTIF(\'Charms & Upgrades\'!D2:D{last_r},"Mythic")'],
        ["Total Cards & Charms", f"=COUNTA('Charms & Upgrades'!B2:B{last_r})", f"=COUNTA('Charms & Upgrades'!B2:B{last_r})"],
    ]
    add_summary_table(ws_sum, "Fruit Slicer — Orchard Blast Charms Summary", ["Group", "By Kind", "By Rarity"], sum_rows, [24, 14, 14])

    out_path = os.path.join(OUT_ROOT, "Fruit Slicer Orchard Blast Charms.xlsx")
    wb.save(out_path)
    print("Wrote", out_path)


if __name__ == "__main__":
    build_achievements_xlsx()
    build_shop_xlsx()
    build_charms_xlsx()
