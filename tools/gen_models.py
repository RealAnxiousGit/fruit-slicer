#!/usr/bin/env python3
"""Generates the low-poly 3D fruit from code: no third-party models.

Writes romfs/models/fruit_hi.bin and fruit_lo.bin. Each mesh has three parts: whole, left half
and right half, the halves being the whole fruit cut by the plane x = 0 with a coloured cut face.

File format (little endian):
  "FSM1", u32 mesh count (always 3 parts per mesh)
  for every part: u32 vertex count, u32 index count, u32 body index count, u32 flesh RGBA, u32 rim RGBA
  then for every part: its vertices, then its u16 indices (padded to 4 bytes)
A vertex is 20 bytes: float x, y, z; s8 nx, ny, nz, 0; u8 r, g, b, a.
Units are screen pixels, +y is down the screen and +z points at the viewer.

Requires numpy. Output is deterministic.
"""
import math
import os
import struct

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LOD = {"hi": (14, 20), "lo": (8, 12)}


def lerp3(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def clamp01(v):
    return max(0.0, min(1.0, v))


class Mesh:
    """An indexed triangle list with per-vertex normal and colour."""

    def __init__(self):
        self.v = []
        self.i = []
        self.body = 0  # indices of the fruit itself, before stems and leaves

    def add_vertex(self, p, n, c):
        self.v.append((tuple(p), tuple(n), tuple(c)))
        return len(self.v) - 1

    def add_tri(self, a, b, c):
        self.i += [a, b, c]

    def add(self, other):
        base = len(self.v)
        self.v += other.v
        self.i += [k + base for k in other.i]


def surface(lat, lon, radius, color):
    """A closed, smooth-shaded surface. radius(theta, phi) gives the distance from the origin;
    theta runs 0 (top, -y) to pi (bottom) and phi runs round the vertical axis."""

    def point(th, ph):
        r = radius(th, ph)
        return np.array((math.sin(th) * math.cos(ph) * r, -math.cos(th) * r, math.sin(th) * math.sin(ph) * r))

    m = Mesh()
    eps = 0.012
    ids = {}
    for a in range(lat + 1):
        th = a / lat * math.pi
        for b in range(lon):
            if (a == 0 or a == lat) and b > 0:
                continue
            ph = b / lon * math.tau
            p = point(th, ph)
            tlo, thi = max(th - eps, 0.001), min(th + eps, math.pi - 0.001)
            n = np.cross(point(tlo, ph) - point(thi, ph), point(max(th, 0.02), ph + eps) - point(max(th, 0.02), ph - eps))
            ln = np.linalg.norm(n)
            n = n / ln if ln > 1e-9 else p / (np.linalg.norm(p) or 1)
            if np.dot(n, p) < 0:
                n = -n
            ids[(a, b)] = m.add_vertex(p, n, color(th, ph))

    def vid(a, b):
        return ids[(a, 0)] if a == 0 or a == lat else ids[(a, b % lon)]

    for a in range(lat):
        for b in range(lon):
            p, q, r, s = vid(a, b), vid(a, b + 1), vid(a + 1, b), vid(a + 1, b + 1)
            if a > 0:
                m.add_tri(p, q, r)
            if a < lat - 1:
                m.add_tri(q, s, r)
    m.body = len(m.i)
    return m


def prism(a, b, ra, rb, color, sides=5):
    """A tapered bar from a to b, used for stems, fuses and crowns."""
    a, b = np.array(a, float), np.array(b, float)
    d = b - a
    d /= np.linalg.norm(d) or 1
    u = np.cross(d, (0.3, 0.2, 1.0))
    u /= np.linalg.norm(u) or 1
    w = np.cross(d, u)
    m = Mesh()
    for k in range(sides):
        t = k / sides * math.tau
        o = u * math.cos(t) + w * math.sin(t)
        m.add_vertex(a + o * ra, o, color)
        m.add_vertex(b + o * rb, o, color)
    for k in range(sides):
        p, q, r, s = 2 * k, 2 * k + 1, (2 * k + 2) % (2 * sides), (2 * k + 3) % (2 * sides)
        m.add_tri(p, r, q)
        m.add_tri(q, r, s)
    tip = m.add_vertex(b, d, color)
    for k in range(sides):
        m.add_tri(2 * k + 1, (2 * k + 3) % (2 * sides), tip)
    return m


def leaf(base, angle, tilt, length, width, color):
    """A two-sided folded leaf."""
    base = np.array(base, float)
    d = np.array((math.cos(angle) * math.cos(tilt), -math.sin(tilt), math.sin(angle) * math.cos(tilt)))
    side = np.array((-math.sin(angle), 0, math.cos(angle)))
    up = np.cross(d, side)
    tip = base + d * length
    mid = base + d * length * 0.5 + up * length * 0.1
    dark = tuple(c * 0.8 for c in color)
    m = Mesh()
    for s in (1, -1):
        e = mid + side * width * s
        nrm = (up + side * s * 0.4)
        nrm = nrm / np.linalg.norm(nrm)
        a, b, c, f = m.add_vertex(base, nrm, dark), m.add_vertex(e, nrm, color), m.add_vertex(tip, nrm, color), m.add_vertex(mid, nrm, dark)
        m.add_tri(a, b, f)
        m.add_tri(f, b, c)
        m.add_tri(a, f, b)
        m.add_tri(f, c, b)
    return m


# ---------------------------------------------------------------- fruit
def tri_shade(colors, t):
    t = clamp01(t) * (len(colors) - 1)
    k = min(int(t), len(colors) - 2)
    return lerp3(colors[k], colors[k + 1], t - k)


def apple(lat, lon):
    def rad(th, ph):
        r = 22 + 2.0 * math.sin(th)
        return r * (1 - 0.22 * math.exp(-(th / 0.55) ** 2) - 0.10 * math.exp(-((math.pi - th) / 0.45) ** 2))

    def col(th, ph):
        blush = 0.5 + 0.5 * math.sin(ph + 0.6)
        return lerp3((226, 44, 48), (250, 120, 60), clamp01(blush * (th / math.pi) ** 0.7))

    m = surface(lat, lon, rad, col)
    m.add(prism((0, -15, 0), (2.5, -27, 0.5), 1.5, 1.0, (96, 62, 36)))
    m.add(leaf((1.5, -21, 0), 0.5, 0.45, 15, 4.2, (84, 170, 72)))
    return m


def orange(lat, lon):
    def rad(th, ph):
        return 22.5 + 0.35 * math.sin(th * 9) * math.sin(ph * 11) * math.sin(th)

    def col(th, ph):
        return lerp3((255, 160, 34), (240, 118, 20), th / math.pi)

    m = surface(lat, lon, rad, col)
    m.add(prism((0, -21.5, 0), (0.5, -24.5, 0), 1.5, 1.2, (96, 110, 50)))
    return m


def lemon(lat, lon):
    def rad(th, ph):
        # Stretched along the screen, with the classic nipple at both ends.
        s = math.sin(th)
        e = abs(math.cos(th))
        return 17.5 + 8.0 * e ** 2 + 5.5 * math.exp(-(s / 0.2) ** 2)

    def col(th, ph):
        return lerp3((255, 234, 76), (246, 206, 40), th / math.pi)

    return surface(lat, lon, rad, col)


def pear(lat, lon):
    def rad(th, ph):
        t = th / math.pi
        return 11.5 + 12.5 * (0.5 - 0.5 * math.cos(t * math.pi)) ** 1.2 + (2.5 if t > 0.3 else 2.5 * t / 0.3)

    def col(th, ph):
        return lerp3((186, 214, 90), (226, 216, 92), th / math.pi)

    m = surface(lat, lon, rad, col)
    m.add(prism((0, -13, 0), (1.5, -23, 0), 1.3, 0.9, (96, 62, 36)))
    m.add(leaf((1, -17.5, 0), 2.6, 0.35, 11, 3.2, (84, 170, 72)))
    return m


def plum(lat, lon):
    def rad(th, ph):
        return 21 * (1 + 0.12 * math.cos(th) ** 2) * (1 - 0.05 * math.exp(-((ph % math.tau) - 3.1) ** 2 * 4))

    def col(th, ph):
        return lerp3((126, 48, 148), (80, 28, 112), th / math.pi)

    return surface(lat, lon, rad, col)


def melon(lat, lon):
    def rad(th, ph):
        return 24.5 * (1 + 0.07 * math.cos(th) ** 2)

    def col(th, ph):
        # Dark stripes running pole to pole; hi-detail meshes resolve these best.
        stripe = 0.5 + 0.5 * math.cos(ph * 5 + math.sin(th * 4) * 0.5)
        return lerp3((58, 128, 62), (32, 88, 44), stripe ** 2)

    return surface(lat, lon, rad, col)


def kiwi(lat, lon):
    def rad(th, ph):
        return 20.5 * (1 + 0.1 * math.sin(th) ** 2)

    def col(th, ph):
        return lerp3((136, 98, 58), (112, 80, 46), 0.5 + 0.5 * math.sin(ph * 7 + th * 9))

    return surface(lat, lon, rad, col)


def berry(lat, lon):
    def rad(th, ph):
        t = th / math.pi
        return 13 + 11 * math.sin(min(t * 1.5, 1) * math.pi / 2) * (1.05 - t * 0.6)

    def col(th, ph):
        seed = math.sin(ph * 9 + (th * 7 // 1) * 1.6) * math.sin(th * 14)
        return (255, 222, 120) if seed > 0.82 else lerp3((236, 34, 58), (196, 22, 44), th / math.pi)

    m = surface(lat, lon, rad, col)
    for k in range(5):
        m.add(leaf((0, -13.5, 0), k / 5 * math.tau, 0.1, 10, 3.0, (84, 170, 72)))
    return m


def peach(lat, lon):
    def rad(th, ph):
        cleft = 0.09 * math.exp(-(math.sin((ph - 1.57) / 2) / 0.16) ** 2) * math.sin(th) ** 0.5
        return 22.5 * (1 - cleft) * (1 - 0.1 * math.exp(-(th / 0.42) ** 2))

    def col(th, ph):
        return lerp3((255, 178, 112), (242, 104, 88), clamp01(0.5 + 0.5 * math.sin(ph + 1.1) * math.sin(th)))

    m = surface(lat, lon, rad, col)
    m.add(leaf((0, -20, 0), 0.7, 0.45, 12, 3.6, (84, 170, 72)))
    return m


def pineapple(lat, lon):
    def rad(th, ph):
        t = th / math.pi
        bump = 0.55 * math.sin(ph * 8 + t * 22) * math.sin(ph * 8 - t * 22) * math.sin(th)
        return (19 + 3.0 * math.sin(th)) * (1 + 0.3 * math.cos(th) ** 2) + bump

    def col(th, ph):
        diamond = math.sin(ph * 8 + th * 7) * math.sin(ph * 8 - th * 7)
        return (190, 118, 30) if diamond > 0.75 else lerp3((244, 180, 50), (222, 148, 40), th / math.pi)

    m = surface(lat, lon, rad, col)
    for k in range(7):
        ang = k / 7 * math.tau
        m.add(leaf((0, -24, 0), ang, 1.0 + 0.2 * (k % 2), 15 + 3 * (k % 2), 3.2, (66, 156, 70)))
    return m


def coconut(lat, lon):
    def rad(th, ph):
        return 22 + 0.4 * math.sin(ph * 13) * math.sin(th * 11) * math.sin(th)

    def col(th, ph):
        fibre = 0.5 + 0.5 * math.sin(ph * 17 + th * 5)
        c = lerp3((100, 68, 44), (66, 42, 28), fibre)
        if th < 0.32:
            d = min(abs((ph % (math.tau / 3)) - math.pi / 3), 1.0)
            if d < 0.3:
                return (38, 24, 18)
        return c

    return surface(lat, lon, rad, col)


def gold(lat, lon):
    def rad(th, ph):
        return (19 + 7 * abs(math.cos(ph * 2.5))) * (0.72 + 0.28 * math.sin(th))

    def col(th, ph):
        return lerp3((255, 230, 96), (240, 170, 34), th / math.pi)

    return surface(lat, lon, rad, col)


def bomb(lat, lon):
    def col(th, ph):
        return (44, 40, 62)

    m = surface(lat, lon, lambda th, ph: 19.0, col)
    m.add(prism((0, -17, 0), (0, -22, 0), 5.5, 5.5, (86, 84, 102), 8))
    m.add(prism((0, -22, 0), (5, -29, 1), 1.2, 1.0, (190, 170, 130)))
    # Red warning stars on both faces, so that a bomb reads clearly at a glance.
    for s in (1, -1):
        cross = prism((-8, 0, 17.5 * s), (8, 0, 17.5 * s), 2.0, 2.0, (232, 56, 48), 4)
        m.add(cross)
        cross = prism((0, -8, 17.5 * s), (0, 8, 17.5 * s), 2.0, 2.0, (232, 56, 48), 4)
        m.add(cross)
    return m


def sunburst(lat, lon):
    def rad(th, ph):
        return (41 + 4.5 * math.cos(ph * 10) * math.sin(th)) * (0.82 + 0.18 * math.sin(th))

    def col(th, ph):
        ridge = 0.5 + 0.5 * math.cos(ph * 10)
        return lerp3((214, 62, 88), (255, 160, 62), clamp01(ridge * 0.9 + 0.3 * math.sin(th) ** 8))

    m = surface(lat + 4, lon + 8, rad, col)
    for k in range(6):
        m.add(leaf((0, -33, 0), k / 6 * math.tau, 0.7, 22, 5.5, (72, 170, 92)))
    return m


# name, builder, flesh, rim
FRUIT = [
    ("apple", apple, (255, 240, 190), (246, 216, 150)),
    ("orange", orange, (255, 176, 66), (255, 232, 180)),
    ("lemon", lemon, (252, 240, 130), (255, 250, 214)),
    ("pear", pear, (248, 246, 196), (226, 232, 150)),
    ("plum", plum, (240, 190, 90), (214, 110, 110)),
    ("melon", melon, (238, 66, 82), (244, 244, 190)),
    ("kiwi", kiwi, (150, 208, 80), (214, 232, 170)),
    ("berry", berry, (252, 120, 140), (255, 196, 200)),
    ("peach", peach, (255, 214, 130), (250, 150, 110)),
    ("pineapple", pineapple, (255, 232, 110), (255, 246, 190)),
    ("coconut", coconut, (250, 250, 244), (150, 110, 80)),
    ("gold", gold, (255, 236, 120), (255, 250, 200)),
    ("bomb", bomb, (60, 56, 80), (44, 40, 62)),
    ("sunburst", sunburst, (255, 196, 80), (250, 104, 70)),
]


# ---------------------------------------------------------------- pre-split halves
def split(mesh, side, flesh, rim):
    """Keeps the part of the mesh on one side of x = 0 and closes it with a cut face."""
    out = Mesh()
    cache = {}

    def emit(p, n, c):
        key = (round(p[0], 3), round(p[1], 3), round(p[2], 3), round(n[0], 2), round(n[1], 2), round(n[2], 2), c)
        if key not in cache:
            cache[key] = out.add_vertex(p, n, c)
        return cache[key]

    loop = []
    for t in range(0, len(mesh.i), 3):
        tri = [mesh.v[mesh.i[t + k]] for k in range(3)]
        d = [side * v[0][0] for v in tri]
        poly = []
        for k in range(3):
            a, b = tri[k], tri[(k + 1) % 3]
            da, db = d[k], d[(k + 1) % 3]
            if da >= 0:
                poly.append(a)
            if (da > 0) != (db > 0) and abs(da - db) > 1e-9:
                f = da / (da - db)
                p = tuple(a[0][j] + (b[0][j] - a[0][j]) * f for j in range(3))
                nv = tuple(a[1][j] + (b[1][j] - a[1][j]) * f for j in range(3))
                c = tuple(a[2][j] + (b[2][j] - a[2][j]) * f for j in range(3))
                poly.append((p, nv, c))
                if t < mesh.body and da > 0:  # each edge of the cut outline, once; stems stay out of it
                    loop.append(p)
        for k in range(1, len(poly) - 1):
            out.add_tri(*[emit(*poly[j]) for j in (0, k, k + 1)])

    if len(loop) >= 3:
        pts = np.array(loop)
        centre = pts.mean(axis=0)
        ang = np.arctan2(pts[:, 1] - centre[1], pts[:, 2] - centre[2])
        order = np.argsort(ang)
        ring = pts[order]
        keep = [ring[0]]
        for p in ring[1:]:
            if np.linalg.norm(p - keep[-1]) > 0.6:
                keep.append(p)
        ring = keep
        normal = (-side, 0.0, 0.0)
        mid = out.add_vertex(tuple(centre), normal, tuple(c * 1.0 + (255 - c) * 0.35 for c in flesh))
        count = len(ring)
        outer = [out.add_vertex(tuple(p), normal, rim) for p in ring]
        inner = [out.add_vertex(tuple(centre + (p - centre) * 0.86), normal, flesh) for p in ring]
        for k in range(count):
            n = (k + 1) % count
            out.add_tri(outer[k], inner[k], outer[n])
            out.add_tri(outer[n], inner[k], inner[n])
            out.add_tri(inner[k], mid, inner[n])
    return out


def rgba(c, a=255):
    return int(c[0]) | int(c[1]) << 8 | int(c[2]) << 16 | a << 24


def pack(mesh):
    b = bytearray()
    for p, n, c in mesh.v:
        ln = math.sqrt(sum(x * x for x in n)) or 1
        sn = [max(-127, min(127, int(round(x / ln * 127)))) for x in n]
        b += struct.pack("<3f4b4B", p[0], p[1], p[2], sn[0], sn[1], sn[2], 0, *[max(0, min(255, int(x))) for x in c], 255)
    data = struct.pack(f"<{len(mesh.i)}H", *mesh.i)
    if len(data) % 4:
        data += b"\0\0"
    return bytes(b) + data


def build_models():
    folder = os.path.join(ROOT, "romfs", "models")
    os.makedirs(folder, exist_ok=True)
    for lod, (lat, lon) in LOD.items():
        header = [b"FSM1", struct.pack("<I", len(FRUIT))]
        body = []
        tris = 0
        for name, fn, flesh, rim in FRUIT:
            whole = fn(lat, lon)
            tris += len(whole.i) // 3
            for part in (whole, split(whole, -1, flesh, rim), split(whole, 1, flesh, rim)):
                header.append(struct.pack("<5I", len(part.v), len(part.i), part.body, rgba(flesh), rgba(rim)))
                body.append(pack(part))
        with open(os.path.join(folder, f"fruit_{lod}.bin"), "wb") as f:
            f.write(b"".join(header) + b"".join(body))
        print(f"{lod}: about {tris // len(FRUIT)} triangles per fruit, {sum(map(len, body)) // 1024} KB")


if __name__ == "__main__":
    build_models()
