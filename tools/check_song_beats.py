"""Listens to the finished Shop songs and checks that they sit on a steady 4/4 grid starting at 0:00.

usage: check_song_beats.py selftest
       check_song_beats.py <universal repo> [music_name ...]

Beat Slice counts beats from the very start of each file, so for every song this reports:
  tempo   the tempo found by listening, with nothing to go on but the audio, against the tempo in SONGS
  offset  how far the real beats sit from the grid, overall and at worst across twelve parts of the song
  4/4     whether the song repeats more strongly every four beats than every three
  kicks   how the low end is shared across beats 1 to 4 of the bar

selftest runs the same measurements on a made-up click track whose beats are known exactly.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert_shop_music import HOP, KICK_SURE, LOW_BINS, ONSET_LAG, RATE, SONGS, load, onsets  # noqa: E402

GOOD_OFFSET_MS = 25.0
GOOD_WORST_MS = 60.0   # Beat Slice counts 120 ms either side of a beat as on it
GOOD_TEMPO = 0.25
PARTS = 12
CLEAR_PART = 0.25


def autocorrelation(env):
    return np.fft.irfft(np.abs(np.fft.rfft(env, len(env) * 2)) ** 2)[:len(env)]


def heard_tempo(ac):
    """The best comb of beat multiples, from 70 to 200 BPM."""
    best, best_score = 120.0, -1.0
    for bpm in np.arange(70, 200, 0.01):
        frames = RATE / HOP * 60 / bpm
        score = 0.0
        for multiple, weight in ((1, 1.0), (2, 0.8), (4, 0.9), (8, 0.7), (16, 0.6), (32, 0.5)):
            lag = frames * multiple
            low = int(lag)
            if low + 1 < len(ac):
                score += weight * (ac[low] * (1 - lag + low) + ac[low + 1] * (lag - low))
        if score > best_score:
            best, best_score = float(bpm), score
    return best


def same_tempo(heard, bpm):
    """A beat tracker can land on half or double the tempo of a song and still be right about it."""
    return min(abs(heard - bpm), abs(heard * 2 - bpm), abs(heard / 2 - bpm))


def offsets_ms(env, bpm):
    samples_per_beat = RATE * 60.0 / bpm
    where = (np.arange(len(env)) * HOP + ONSET_LAG) / samples_per_beat

    def offset(mask_from, mask_to):
        part, spot = env[mask_from:mask_to], where[mask_from:mask_to]
        if part.sum() <= 0:
            return None, 0.0
        vector = (part * np.exp(2j * np.pi * spot)).sum()
        return np.angle(vector) / (2 * np.pi) * samples_per_beat / RATE * 1000, abs(vector) / part.sum()

    overall, sure = offset(0, len(env))
    size = len(env) // PARTS
    # Only the parts with a clear beat say anything: a breakdown or a held chord has no beat to measure.
    parts = [offset(p * size, (p + 1) * size) for p in range(PARTS)]
    return overall, sure, [v for v, clear in parts if v is not None and clear >= CLEAR_PART]


def bar_shape(env, low, bpm):
    ac = autocorrelation(env)
    frames = RATE / HOP * 60 / bpm
    def peak(beats):
        at = int(round(frames * beats))
        return ac[at - 2:at + 3].max()

    three, four = peak(3), peak(4)
    samples_per_beat = RATE * 60.0 / bpm
    beat = (np.arange(len(low)) * HOP + ONSET_LAG) / samples_per_beat
    nearest = np.round(beat)
    close = np.abs(beat - nearest) < 0.2
    shares = np.zeros(4)
    for position in range(4):
        shares[position] = low[close & ((nearest.astype(np.int64) % 4) == position)].sum()
    return four / (three or 1), shares / (shares.sum() or 1)


def report(name, x, bpm):
    mono = x.mean(axis=1)
    env = onsets(mono)
    low = onsets(mono, LOW_BINS)
    heard = heard_tempo(autocorrelation(env))
    overall, sure, parts = offsets_ms(low, bpm)
    if sure < KICK_SURE:
        overall, sure, parts = offsets_ms(env, bpm)
    worst = max((abs(v) for v in parts), default=0.0)
    ratio, shares = bar_shape(env, low, bpm)
    miss = same_tempo(heard, bpm)
    # A four-on-the-floor beat repeats just as strongly every three beats as every four, so near 1 is fine.
    good = miss <= GOOD_TEMPO and abs(overall) <= GOOD_OFFSET_MS and worst <= GOOD_WORST_MS and ratio > 0.75
    print(f"{'PASS' if good else 'LOOK'} {name}: said {bpm}, heard {heard:.2f}; offset {overall:+.1f} ms, "
          f"worst of {len(parts)} clear parts {worst:.1f} ms, sure {sure:.2f}; four beats against three {ratio:.2f}; kicks on 1-4 "
          + " ".join(f"{s * 100:.0f}%" for s in shares))
    return overall, good


def click_track(bpm, seconds, first):
    n = int(RATE * seconds)
    out = np.zeros((n, 2))
    beat_at = first
    index = 0
    burst_len = int(RATE * 0.06)
    decay = np.exp(-np.arange(burst_len) / (RATE * 0.012))
    rng = np.random.default_rng(1)
    while beat_at + burst_len < n:
        at = int(round(beat_at))
        tone = np.sin(2 * np.pi * (70 if index % 2 == 0 else 200) * np.arange(burst_len) / RATE)
        noise = rng.normal(0, 0.35, burst_len)
        strength = 1.0 if index % 4 == 0 else 0.7
        out[at:at + burst_len] += ((tone + noise) * decay * strength)[:, None]
        beat_at += RATE * 60.0 / bpm
        index += 1
    return out


def selftest():
    for bpm in (128, 150, 175):
        print(f"clicks at {bpm} BPM, first beat at 0.000 s (the offset here should be close to 0):")
        report("clicks", click_track(bpm, 45, 0), bpm)
    print("clicks at 150 BPM, first beat at 0.030 s (the offset here should be close to +30):")
    report("clicks", click_track(150, 45, RATE * 0.03), 150)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "selftest":
        selftest()
        return
    universal = sys.argv[1]
    only = sys.argv[2:]
    all_good = True
    for _, name, bpm in SONGS:
        if only and name not in only:
            continue
        _, good = report(name, load(os.path.join(universal, "romfs", "audio", name + ".ogg")), bpm)
        all_good = all_good and good
    print("ALL PASS" if all_good else "SOME NEED A LOOK")


if __name__ == "__main__":
    main()
