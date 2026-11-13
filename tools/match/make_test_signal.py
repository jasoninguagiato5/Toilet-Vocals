#!/usr/bin/env python3
"""Writes test_signal.wav: tones for measuring a compressor's curve, timing and harmonics.

    python3 tools/match/make_test_signal.py [out.wav]
"""
import sys
import numpy as np
from scipy.io import wavfile
from layout import FS, TONE, sections

secs, total = sections()
n = int(total * FS)
t = np.arange(n) / FS
amp = np.zeros(n)
for _, start, dur, lv in secs:
    a, b = int(start * FS), int((start + dur) * FS)
    amp[a:b] = 10 ** (lv / 20)
x = (amp * np.sin(2 * np.pi * TONE * t)).astype(np.float32)
path = sys.argv[1] if len(sys.argv) > 1 else "test_signal.wav"
wavfile.write(path, FS, x)
print(f"wrote {path}: {total:.1f} s, mono, {FS} Hz, 32-bit float")
