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

# master file, name in romfs/audio, tempo from the song's project
SONGS = [
    ("Ahead of the Curve.wav", "music_ahead", 175),
    ("SPARE FOR ME.wav", "music_spare", 180),
    ("My Own Fright.wav", "music_fright", 160),
    ("WORTHLESS ENEMY.wav", "music_worthless", 180),
    ("Lies.flac", "music_lies", 128),
    ("Lies VIP Remix.wav", "music_liesvip", 128),  # sold as "Lies (VIP Remix)"
    ("JUST STAY WITH ME.wav", "music_staywithme", 150),
]

RATE = 44100
OGG_QUALITY = "3"       # about 112 kbit/s: three minutes come to around 2.5 MB
MEAN_TARGET_DB = -20.5  # what the Shop's other recorded tracks measure
BEATS_PER_BAR = 4
LOW_BINS = 8            # up to about 150 Hz: the kick and bass
KICK_SURE = 0.15        # how clear the low end's beat must be before it decides where the grid sits
SAME_BEAT = 0.04        # seconds: sound starting this close to a beat is on that beat
FILE_START = 0.02       # seconds: sound this close to the top of the file counts as starting there
HOP = 512
ONSET_LAG = 1425        # samples from the start of an onset frame to the sound it found; check_song_beats.py selftest proves it


def load(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2", "-ar", str(RATE), "-"],
                         check=True, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, "<f4").reshape(-1, 2).astype(np.float64)


def onsets(mono, top_bins=None):
    """How much new sound starts in each frame; top_bins keeps only the low end."""
    n = (len(mono) - 2048) // HOP
    window = np.hanning(2048)
    env = np.zeros(n)
    previous = None
    for i in range(n):
        spec = np.log1p(np.abs(np.fft.rfft(mono[i * HOP:i * HOP + 2048] * window)[:top_bins]) * 20)
        if previous is not None:
            env[i] = np.maximum(spec - previous, 0).sum()
        previous = spec
    env -= np.convolve(env, np.ones(43) / 43, "same")
    return np.maximum(env, 0)


def grid_phase(env, frames_per_beat):
    """Where the beat falls, in beats from 0..1, and how sure that is (0 none, 1 perfect)."""
    positions = np.arange(len(env)) / frames_per_beat
    vector = (env * np.exp(2j * np.pi * positions)).sum()
    phase = (np.angle(vector) / (2 * np.pi)) % 1.0
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


def make_loop(x, name, bpm):
    level = np.abs(x).max(axis=1)
    sounding = np.nonzero(level > 10 ** (-45 / 20))[0]
    first, last = int(sounding[0]), int(sounding[-1]) + 1
    mono = x.mean(axis=1)
    env = onsets(mono)
    low = onsets(mono, LOW_BINS)
    frames_per_beat = RATE / HOP * 60 / bpm
    phase, sure = grid_phase(env, frames_per_beat)
    kick_phase, kick_sure = grid_phase(low, frames_per_beat)
    if kick_sure >= KICK_SURE:  # the kick is the beat people hear; off-beat hats and vocals pull the full mix around
        phase, sure = kick_phase, kick_sure
    wander = steadiness(env, frames_per_beat)
    samples_per_beat = RATE * 60.0 / bpm

    # Beat Slice counts beats from the very start of the file, so the song starts on one.
    grid0 = (phase * frames_per_beat * HOP + ONSET_LAG) % samples_per_beat
    k = round((first - grid0) / samples_per_beat)
    nearest = grid0 + k * samples_per_beat
    if first <= RATE * FILE_START and abs(nearest - first) <= RATE * SAME_BEAT:
        start = first  # sound from the top of the file: the export begins on the project's first beat
    else:
        start = int(round(nearest if nearest >= 0 else nearest + samples_per_beat))
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
    # Any sound just before the first beat leads into it, so it goes at the end of the loop.
    pickup = x[first:start]
    if len(pickup):
        song[-len(pickup):] += pickup

    gain = 10 ** ((MEAN_TARGET_DB - 20 * np.log10(np.sqrt((song ** 2).mean()) + 1e-9)) / 20)
    song *= gain
    peak = float(np.abs(song).max())
    if peak > 0.97:
        song *= 0.97 / peak
    tail_rms = 20 * np.log10(np.sqrt((tail ** 2).mean()) * gain + 1e-9) if len(tail) else -120
    print(f"{name}: {bpm} bpm, sure {sure:.2f}, wander {wander:.3f} beat; "
          f"starts {start / RATE:.3f}s (sound at {first / RATE:.3f}s), {bars} bars = {len(song) / RATE:.2f}s, "
          f"tail {len(tail) / RATE:.2f}s at {tail_rms:.1f} dB, gain {20 * np.log10(gain):.1f} dB")
    return song


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
    for master, name, bpm in SONGS:
        if only and name not in only:
            continue
        song = make_loop(load(os.path.join(masters, master)), name, bpm)
        encode_ogg(song, os.path.join(universal, "romfs", "audio", name + ".ogg"))
        encode_3ds(song, os.path.join(three_ds, "romfs", "audio", name + ".pcm"))
        print(f'  SHOP_TRACK("{os.path.splitext(master)[0]}", 300, "Artist. Genre.", "{name}", {bpm})')


if __name__ == "__main__":
    main()
