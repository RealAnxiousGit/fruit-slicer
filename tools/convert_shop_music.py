"""Turns finished songs into Shop music that loops cleanly on every port.

usage: convert_shop_music.py <folder with the masters> <universal repo> <3DS repo>

To add a song: put a line in SONGS, run this, then add the SHOP_TRACK line it prints to
source/shop_items.inc. Everything else (the tile, the price, scrolling, saving) follows from that line.

Windows, Linux and Android get stereo Ogg Vorbis. The 3DS has no Ogg decoder and holds the whole track
in memory, so it gets mono at 16 kHz, packed 4:1 with IMA ADPCM.
"""
import os
import struct
import subprocess
import sys

import numpy as np

# master file, name in romfs/audio
SONGS = [
    ("Ahead of the Curve.wav", "music_ahead"),
    ("SPARE FOR ME.wav", "music_spare"),
    ("My Own Fright.wav", "music_fright"),
    ("WORTHLESS ENEMY.wav", "music_worthless"),
    ("Lies.flac", "music_lies"),
    ("Lies VIP Remix.wav", "music_liesvip"),  # sold as "Lies (VIP Remix)"
    ("JUST STAY WITH ME.wav", "music_staywithme"),
]

RATE = 44100
OGG_QUALITY = "3"       # about 112 kbit/s: three minutes come to around 2.5 MB
MEAN_TARGET_DB = -20.5  # what the Shop's other recorded tracks measure
BEATS_PER_BAR = 4
HOP = 512


def load(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2", "-ar", str(RATE), "-"],
                         check=True, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, "<f4").reshape(-1, 2).astype(np.float64)


def onsets(mono):
    n = (len(mono) - 2048) // HOP
    window = np.hanning(2048)
    env = np.zeros(n)
    previous = None
    for i in range(n):
        spec = np.log1p(np.abs(np.fft.rfft(mono[i * HOP:i * HOP + 2048] * window)) * 20)
        if previous is not None:
            env[i] = np.maximum(spec - previous, 0).sum()
        previous = spec
    env -= np.convolve(env, np.ones(43) / 43, "same")
    return np.maximum(env, 0)


def comb_score(ac, frames_per_beat):
    total = 0.0
    for multiple, weight in ((1, 1.0), (2, 0.8), (4, 0.9), (8, 0.7), (16, 0.6)):
        lag = int(round(frames_per_beat * multiple))
        if lag < len(ac):
            total += weight * ac[lag]
    return total


def tempo(env):
    spectrum = np.fft.rfft(env, len(env) * 2)
    ac = np.fft.irfft(np.abs(spectrum) ** 2)[:len(env)]
    best, best_score = 120.0, -1.0
    for bpm in np.arange(80, 200, 0.02):
        score = comb_score(ac, RATE / HOP * 60 / bpm)
        if score > best_score:
            best, best_score = float(bpm), score
    return round(best) if abs(best - round(best)) <= 0.12 else round(best, 2)


def grid_phase(env, frames_per_beat):
    """Where the beat falls, in beats from 0..1, and how sure that is (0 none, 1 perfect)."""
    positions = np.arange(len(env)) / frames_per_beat
    vector = (env * np.exp(2j * np.pi * positions)).sum()
    phase = (-np.angle(vector) / (2 * np.pi)) % 1.0
    return phase, abs(vector) / (env.sum() or 1)


def steadiness(env, frames_per_beat):
    """How far the beat wanders across the song, in beats; small means Beat Slice can follow it."""
    pieces = 10
    size = len(env) // pieces
    vectors = []
    for p in range(pieces):
        part = env[p * size:(p + 1) * size]
        if part.sum() <= 0:
            continue
        where = (np.arange(len(part)) + p * size) / frames_per_beat
        vectors.append((part * np.exp(2j * np.pi * where)).sum() / part.sum())
    angles = np.angle(np.array(vectors))
    mean = np.angle(np.exp(1j * angles).mean())
    wander = np.angle(np.exp(1j * (angles - mean))) / (2 * np.pi)
    return float(np.abs(wander).max())


def make_loop(x, name):
    level = np.abs(x).max(axis=1)
    sounding = np.nonzero(level > 10 ** (-45 / 20))[0]
    first, last = int(sounding[0]), int(sounding[-1]) + 1
    mono = x.mean(axis=1)
    env = onsets(mono)
    bpm = tempo(env)
    frames_per_beat = RATE / HOP * 60 / bpm
    phase, sure = grid_phase(env, frames_per_beat)
    wander = steadiness(env, frames_per_beat)
    samples_per_beat = RATE * 60.0 / bpm
    steady = wander < 0.12 and sure > 0.08

    start = first
    if steady:
        # The window of the onset analysis sits about half of it ahead of the sound.
        grid0 = (phase * frames_per_beat * HOP + 1024) % samples_per_beat
        k = round((first - grid0) / samples_per_beat)
        start = max(0, int(round(grid0 + k * samples_per_beat)))
    samples_per_bar = samples_per_beat * BEATS_PER_BAR
    bars = int((last - start) / samples_per_bar)
    end = start + int(round(bars * samples_per_bar))
    tail = x[end:last].copy()
    song = x[start:end].copy()

    # Whatever rings on past the last whole bar is laid over the first bars, so the loop point is seamless.
    if len(tail):
        tail = tail[:len(song)]
        fade = np.linspace(1, 0, len(tail)) ** 1.5
        song[:len(tail)] += tail * fade[:, None]

    gain = 10 ** ((MEAN_TARGET_DB - 20 * np.log10(np.sqrt((song ** 2).mean()) + 1e-9)) / 20)
    song *= gain
    peak = float(np.abs(song).max())
    if peak > 0.97:
        song *= 0.97 / peak
    tail_rms = 20 * np.log10(np.sqrt((tail ** 2).mean()) * gain + 1e-9) if len(tail) else -120
    print(f"{name}: {bpm} bpm, sure {sure:.2f}, wander {wander:.3f} beat, {'steady' if steady else 'NOT steady'}; "
          f"starts {start / RATE:.3f}s (sound at {first / RATE:.3f}s), {bars} bars = {len(song) / RATE:.2f}s, "
          f"tail {len(tail) / RATE:.2f}s at {tail_rms:.1f} dB, gain {20 * np.log10(gain):.1f} dB")
    return song, bpm if steady else 0


def encode_ogg(song, out):
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-f", "f32le", "-ar", str(RATE), "-ac", "2", "-i", "-", "-c:a", "libvorbis",
                    "-q:a", OGG_QUALITY, out], input=song.astype("<f4").tobytes(), check=True)
    decoded = load(out)
    if len(decoded) != len(song):
        print(f"  decoded length {len(decoded)} against {len(song)}")
    join = np.abs(decoded[0] - decoded[-1]).max()
    typical = np.median(np.abs(np.diff(decoded, axis=0)).max(axis=1))
    print(f"  ogg {os.path.getsize(out) / 1048576:.2f} MB, step at the loop {join:.4f} (typical step {typical:.4f})")


IMA_STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
             130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
             1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
             7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def ima_step(sample, predicted, index):
    step = IMA_STEPS[index]
    diff = sample - predicted
    code = 8 if diff < 0 else 0
    diff = abs(diff)
    delta = step >> 3
    if diff >= step:
        code |= 4
        diff -= step
        delta += step
    if diff >= step >> 1:
        code |= 2
        diff -= step >> 1
        delta += step >> 1
    if diff >= step >> 2:
        code |= 1
        delta += step >> 2
    predicted += -delta if code & 8 else delta
    predicted = max(-32768, min(32767, predicted))
    index = max(0, min(88, index + IMA_INDEX[code & 7]))
    return code, predicted, index


def encode_3ds(song, out):
    wanted = int(round(len(song) * 16000 / RATE))
    # Three copies resampled together, keeping the middle one, so the filter sees the music on both sides of the loop.
    raw = subprocess.run(["ffmpeg", "-v", "error", "-f", "f32le", "-ar", str(RATE), "-ac", "2", "-i", "-", "-af",
                          "pan=mono|c0=0.5*c0+0.5*c1,aresample=16000", "-f", "s16le", "-"],
                         input=np.concatenate((song, song, song)).astype("<f4").tobytes(), check=True, stdout=subprocess.PIPE).stdout
    all_pcm = np.frombuffer(raw, "<i2")
    pcm = all_pcm[wanted:wanted * 2].tolist()
    # Run over the end of the song first, so the packed data picks up already settled at the loop point.
    predicted, index = 0, 0
    for sample in pcm[-4096:]:
        _, predicted, index = ima_step(sample, predicted, index)
    head_predicted, head_index = predicted, index
    packed = bytearray((len(pcm) + 1) // 2)
    for i, sample in enumerate(pcm):
        code, predicted, index = ima_step(sample, predicted, index)
        packed[i >> 1] |= code << 4 if i & 1 else code
    with open(out, "wb") as f:
        f.write(b"FSA2" + struct.pack("<II", len(pcm), 16000) + struct.pack("<hH", head_predicted, head_index) + bytes(packed))
    print(f"  3ds {os.path.getsize(out) / 1048576:.2f} MB, {len(pcm) * 2 / 1048576:.1f} MB in memory")


def main():
    masters, universal, three_ds = sys.argv[1:4]
    only = sys.argv[4:]
    for master, name in SONGS:
        if only and name not in only:
            continue
        song, bpm = make_loop(load(os.path.join(masters, master)), name)
        encode_ogg(song, os.path.join(universal, "romfs", "audio", name + ".ogg"))
        encode_3ds(song, os.path.join(three_ds, "romfs", "audio", name + ".pcm"))
        print(f'  SHOP_TRACK("{os.path.splitext(master)[0]}", 300, "Artist. Genre.", "{name}", {bpm})')


if __name__ == "__main__":
    main()
