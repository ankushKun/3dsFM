#!/usr/bin/env python3
"""Synthesises the banner jingle the HOME Menu plays when 3dsFM is selected.

An original little tune, timed to the banner animation: a quick radio-dial
riff while the tonearm swings in, a soft thud with a touch of crackle when
the needle lands at 0.7 s, and three chimes as the notes pop out. About 1.7
seconds of 44.1 kHz 16-bit stereo; banner sound must be stereo and at most
3 seconds.

Usage: python3 jingle.py jingle.wav
"""

import math
import random
import struct
import sys
import wave

RATE = 44100
LENGTH = 1.7


def tone(buffer, start, duration, freq, volume, kind="triangle", end_freq=None, decay=6.0):
    """Adds one note: a pitch (optionally gliding) with a fast attack and an
    exponential decay."""
    first = int(start * RATE)
    count = int(duration * RATE)
    phase = 0.0
    for i in range(count):
        t = i / RATE
        f = freq if end_freq is None else freq * (end_freq / freq) ** (i / count)
        phase += 2 * math.pi * f / RATE
        if kind == "triangle":
            wave_value = 2 / math.pi * math.asin(math.sin(phase))
        elif kind == "bell":
            wave_value = math.sin(phase) + (0.3 * math.sin(phase * 2.76) if f * 2.76 < RATE * 0.45 else 0.0)
        else:
            wave_value = math.sin(phase)
        attack = min(1.0, t / 0.004)
        release = min(1.0, (count - i) / (0.01 * RATE))
        envelope = attack * release * math.exp(-decay * t)
        if first + i < len(buffer):
            buffer[first + i] += volume * envelope * wave_value


def crackle(buffer, start, duration, volume, seed=7):
    """A few soft clicks, like a needle settling into a groove."""
    rng = random.Random(seed)
    first = int(start * RATE)
    count = int(duration * RATE)
    for _ in range(14):
        at = first + rng.randrange(count)
        width = rng.randrange(6, 20)
        for i in range(width):
            if at + i < len(buffer):
                buffer[at + i] += volume * (1 - i / width) * rng.uniform(-1, 1)


def main(path):
    samples = [0.0] * int(LENGTH * RATE)
    # Tuning the dial while the arm swings in (E5, G5, A5, then C6 held).
    for k, freq in enumerate((659.3, 784.0, 880.0)):
        tone(samples, 0.08 + k * 0.09, 0.12, freq, 0.3, decay=10)
    tone(samples, 0.35, 0.3, 1046.5, 0.28, decay=7)
    # The needle lands: a low thud with a pitch drop, and a little crackle.
    tone(samples, 0.7, 0.18, 420.0, 0.5, kind="sine", end_freq=170.0, decay=13)
    crackle(samples, 0.72, 0.2, 0.12)
    # Notes pop out: three chimes climbing a major triad.
    for k, freq in enumerate((2093.0, 2637.0, 3136.0)):
        tone(samples, 0.95 + k * 0.1, 0.55, freq, 0.16 - k * 0.03, kind="bell", decay=6.5)

    peak = max(abs(s) for s in samples) or 1.0
    gain = 0.5 / peak
    with wave.open(path, "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        frames = bytearray()
        for s in samples:
            value = int(max(-1.0, min(1.0, s * gain)) * 32767)
            frames += struct.pack("<hh", value, value)
        out.writeframes(bytes(frames))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
