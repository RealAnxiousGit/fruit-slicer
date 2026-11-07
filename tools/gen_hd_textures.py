#!/usr/bin/env python3
"""HD detail maps for this port's 3D fruit: skin pores, fuzz, seeds and fibres, from code.

Each map multiplies over the colours the mesh already has, and tiles sideways around the fruit. At 1080p
a fruit's visible half covers about one map width, so one map pixel lands on about one screen pixel.
Writes romfs/tex/<mesh>.png. Requires Pillow and numpy. Output is deterministic.
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
W, H = 512, 256


def band(rng, lo, hi, sx=1.0, sy=1.0):
    """Tiling noise with features between lo and hi cycles across the map; sx, sy stretch it. Range about -1..1."""
    fy = np.fft.fftfreq(H)[:, None] * H * sy
    fx = np.fft.rfftfreq(W)[None, :] * W * sx
    f = np.sqrt(fx * fx + fy * fy)
    spec = np.fft.rfft2(rng.normal(0, 1, (H, W)))
    spec *= np.exp(-((f - (lo + hi) / 2) / ((hi - lo) / 2 + 1e-3)) ** 2)
    out = np.fft.irfft2(spec, s=(H, W))
    return out / (np.abs(out).max() + 1e-6)


def spots(rng, count, radius):
    """Tiling round dots at random places, 0..1."""
    grid = np.zeros((H, W))
    grid[rng.integers(0, H, count), rng.integers(0, W, count)] = 1
    fy = np.fft.fftfreq(H)[:, None]
    fx = np.fft.rfftfreq(W)[None, :]
    k = np.exp(-2 * (np.pi * radius) ** 2 * (fx * fx + fy * fy))
    out = np.fft.irfft2(np.fft.rfft2(grid) * k, s=(H, W))
    return np.clip(out / out.max() * 1.6, 0, 1)


def make(kind, seed):
    r = np.random.default_rng(seed)
    grain = band(r, 60, 130)
    if kind == "apple":
        return 0.96 + 0.08 * band(r, 4, 20, sx=0.12) + 0.02 * grain - 0.1 * spots(r, 320, 1.1)
    if kind in ("orange", "lemon"):
        return 0.98 + 0.05 * band(r, 45, 85) - 0.2 * spots(r, 2600, 1.0) + 0.03 * band(r, 3, 10)
    if kind == "pear":
        return 0.97 + 0.05 * band(r, 5, 20) + 0.02 * grain - 0.26 * spots(r, 800, 1.0)
    if kind == "plum":
        return 0.97 + 0.08 * band(r, 4, 15) + 0.02 * grain
    if kind == "melon":
        return 0.96 + 0.08 * band(r, 15, 40, sx=0.25) + 0.04 * band(r, 45, 90)
    if kind == "kiwi":
        return 0.92 + 0.16 * band(r, 65, 130, sx=0.2) + 0.06 * band(r, 10, 30)
    if kind == "berry":
        return 0.98 + 0.03 * grain - 0.42 * spots(r, 260, 2.0)
    if kind == "peach":
        return 0.96 + 0.05 * band(r, 85, 130) + 0.06 * band(r, 4, 12)
    if kind == "pineapple":
        x = np.arange(W)[None, :] / W * 2 * np.pi * 16
        y = np.arange(H)[:, None] / H * 2 * np.pi * 10
        return 0.86 + 0.14 * np.abs(np.sin(x + y)) * np.abs(np.sin(x - y)) ** 0.5 + 0.04 * grain
    if kind == "coconut":
        return 0.84 + 0.24 * band(r, 30, 100, sx=0.1) + 0.12 * band(r, 100, 130, sx=0.15)
    if kind == "gold":
        return 0.97 + 0.05 * band(r, 40, 120, sx=0.05)
    if kind == "bomb":
        return 0.94 + 0.06 * band(r, 15, 60) - 0.24 * spots(r, 50, 0.9)
    if kind == "banana":
        return 0.97 + 0.05 * band(r, 3, 12, sx=0.1) + 0.02 * grain - 0.3 * spots(r, 90, 1.3)
    if kind == "mango":
        return 0.97 + 0.04 * band(r, 4, 14) + 0.02 * grain - 0.12 * spots(r, 600, 0.9)
    if kind in ("cherry", "grapes", "grape"):
        return 0.97 + 0.06 * band(r, 3, 12) + 0.015 * grain
    if kind == "dragon":
        return 0.96 + 0.06 * band(r, 8, 30) + 0.03 * grain
    if kind == "spiky":
        return 0.9 + 0.1 * band(r, 20, 60) + 0.05 * grain - 0.2 * spots(r, 400, 1.2)
    if kind == "honey":
        return 0.95 + 0.06 * band(r, 25, 60) + 0.03 * grain
    if kind == "jelly":
        return 0.985 + 0.015 * band(r, 3, 10)
    if kind == "chili":
        return 0.96 + 0.06 * band(r, 3, 14, sy=0.15) + 0.015 * grain
    if kind == "coin":
        return 0.97 + 0.04 * band(r, 60, 130, sx=0.05)
    return 0.96 + 0.07 * band(r, 5, 30, sx=0.15) + 0.03 * grain  # sunburst


def main():
    out = os.path.join(ROOT, "romfs", "tex")
    os.makedirs(out, exist_ok=True)
    names = ["apple", "orange", "lemon", "pear", "plum", "melon", "kiwi", "berry", "peach", "pineapple", "coconut", "gold", "bomb", "sunburst",
             "banana", "mango", "cherry", "dragon", "spiky", "grapes", "grape", "honey", "jelly", "chili", "coin"]
    for i, name in enumerate(names):
        a = np.clip(make(name, 4000 + i), 0, 1)
        Image.fromarray((a * 255).astype(np.uint8), "L").convert("RGB").save(os.path.join(out, name + ".png"), optimize=True)
        print(name, os.path.getsize(os.path.join(out, name + ".png")) // 1024, "KB")


if __name__ == "__main__":
    main()
