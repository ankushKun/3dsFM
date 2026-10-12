#!/usr/bin/env python3
"""Generates the placeholder launcher icon, HOME Menu banner and jingle.

Pure standard library so it runs anywhere. The real artwork will replace
these once the design system lands; keeping the generator means a clean
checkout can always build.

    python3 scripts/make-art.py
"""
import math
import struct
import wave
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "resources"

ACCENT = (0x1C, 0xA7, 0xF2)
DEEP = (0x0F, 0x7F, 0xC4)
PAPER = (0xF2, 0xF4, 0xF7)
INK = (0x24, 0x28, 0x2E)


def png(path, width, height, pixel, alpha=True):
    channels = 4 if alpha else 3
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            row.extend(pixel(x, y)[:channels])
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6 if alpha else 2, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.write_bytes(data)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def rounded(x, y, w, h, r):
    cx = min(max(x, r), w - 1 - r)
    cy = min(max(y, r), h - 1 - r)
    return math.hypot(x - cx, y - cy) <= r


def icon_pixel(x, y):
    size = 48
    if not rounded(x, y, size, size, 10):
        return (0, 0, 0, 0)
    base = lerp(ACCENT, DEEP, y / size)
    cx, cy = size / 2, size / 2
    d = math.hypot(x - cx, y - cy)
    if d <= 15:
        ring = int(d) % 3 == 0
        shade = (0x20, 0x24, 0x2A) if ring else INK
        if d <= 5:
            shade = (0xFF, 0xD2, 0x4A)
        if d <= 1.5:
            shade = INK
        return shade + (255,)
    return base + (255,)


def banner_pixel(x, y):
    w, h = 256, 128
    base = lerp(PAPER, (0xDC, 0xEA, 0xF7), y / h)
    cx, cy = 64, 64
    d = math.hypot(x - cx, y - cy)
    if d <= 44:
        if d <= 14:
            return (0xFF, 0xD2, 0x4A)
        return INK if int(d) % 4 else (0x30, 0x34, 0x3A)
    if 130 <= x < 236 and 52 <= y < 60:
        t = (x - 130) / 106
        return lerp(ACCENT, DEEP, t)
    if 130 <= x < 236 and 70 <= y < 76 and (x // 8) % 2 == 0:
        return (0xB9, 0xC2, 0xCC)
    return base


def jingle(path):
    rate = 32000
    notes = [(440.0, 0.0, 0.18), (554.37, 0.14, 0.18), (659.25, 0.28, 0.32)]
    length = 0.7
    frames = int(rate * length)
    samples = []
    for i in range(frames):
        t = i / rate
        value = 0.0
        for freq, start, dur in notes:
            if start <= t < start + dur:
                local = t - start
                env = min(1.0, local / 0.01) * (1.0 - local / dur)
                value += math.sin(2 * math.pi * freq * local) * env
        samples.append(int(max(-1.0, min(1.0, value * 0.35)) * 32767))
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(b"".join(struct.pack("<h", s) for s in samples))


def main():
    RES.mkdir(exist_ok=True)
    (RES / "banner").mkdir(exist_ok=True)
    png(RES / "icon-3dsfm.png", 48, 48, icon_pixel, alpha=True)
    png(RES / "banner-3dsfm.png", 256, 128, banner_pixel, alpha=False)
    jingle(RES / "banner" / "jingle.wav")
    print("wrote icon-3dsfm.png, banner-3dsfm.png, banner/jingle.wav")


if __name__ == "__main__":
    main()
