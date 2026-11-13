#!/usr/bin/env python3
"""Measures compressors from bounces of test_signal.wav and compares them.

    python3 tools/match/analyze.py reference.wav [ours.wav]

For each file: static curve (threshold, ratio), attack and release times, and
2nd/3rd harmonic levels. With two files the differences are listed, which is
what the model tuning in Source/dsp/ToiletVocalsEngine.h is adjusted against.
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt, hilbert
from layout import FS, TONE, STAIR_LEVELS, STEP_LOW, STEP_HIGH, sections


def load(path):
    fs, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x / float(2 ** (8 * x.dtype.itemsize - 1))
    elif x.dtype.kind == "u":
        x = (x - 128) / 128.0
    x = np.asarray(x, dtype=np.float64)
    if x.ndim > 1:
        x = x[:, 0]
    if fs != FS:
        sys.exit(f"{path}: expected {FS} Hz, got {fs}. Bounce at {FS} Hz.")
    return x


def envelope_db(x):
    """Level of the 1 kHz tone over time."""
    sos = butter(2, [TONE / 2, TONE * 2], btype="band", fs=FS, output="sos")
    return 20 * np.log10(np.abs(hilbert(sosfiltfilt(sos, x))) + 1e-9)


def align(x):
    """Shifts the bounce so its first tone starts where the test signal's does."""
    secs, total = sections()
    first = int(secs[0][1] * FS)
    win = int(0.002 * FS)
    head = x[: first + 3 * FS]
    rms = 10 * np.log10(np.convolve(head ** 2, np.ones(win) / win, mode="same") + 1e-14)
    floor = np.median(rms[: first // 2])                         # the silent lead-in (may carry modelled hiss)
    tone = np.median(rms[first + int(0.4 * FS): first + FS])     # the first step, once settled
    above = np.nonzero(rms > floor + 0.5 * (tone - floor))[0]
    shift = (above[0] + win // 2 - first) if len(above) and tone > floor + 6 else 0
    if abs(shift) < int(0.003 * FS):
        shift = 0
    x = x[shift:] if shift > 0 else np.concatenate([np.zeros(-shift), x])
    need = int(total * FS)
    return np.concatenate([x, np.zeros(max(0, need - len(x)))])[:need], shift


SKIP = int(0.0015 * FS)  # the level jump itself smears over about a millisecond in the envelope


def harmonic_db(x, f):
    w = np.hanning(len(x))
    t = np.arange(len(x)) / FS
    return 20 * np.log10(4 * abs(np.sum(w * x * np.exp(-2j * np.pi * f * t))) / len(x) + 1e-12)


def measure(path):
    x, shift = align(load(path))
    env = envelope_db(x)
    secs, _ = sections()
    S = {name: (int(s * FS), int((s + d) * FS), lv) for name, s, d, lv in secs}
    r = {"path": path, "latency_ms": 1000 * shift / FS}

    # static curve: settled output level in the last third of each step
    outs = []
    for lv in STAIR_LEVELS:
        a, b, _ = S[f"stair{lv}"]
        outs.append(np.mean(env[a + 2 * (b - a) // 3: b - int(0.02 * FS)]))
    ins, outs = np.array(STAIR_LEVELS, float), np.array(outs)
    unity = np.median((outs - ins)[:4])            # gain below threshold (makeup)
    gr = (ins + unity) - outs                      # gain reduction at each step
    r["ins"], r["gr"], r["makeup"] = ins, gr, unity
    hit = np.nonzero(gr > 1.0)[0]
    r["threshold"] = float(np.interp(1.0, gr[max(0, hit[0] - 1): hit[0] + 1], ins[max(0, hit[0] - 1): hit[0] + 1])) if len(hit) and hit[0] > 0 else float("nan")
    top = slice(-4, None)                          # slope over the loudest steps
    slope = np.polyfit(ins[top], outs[top], 1)[0]
    r["ratio"] = 1.0 / max(slope, 1e-3)

    # attack: how quickly gain falls after the jump; release: how quickly it returns
    for tag in ("long", "short"):
        a, b, _ = S[f"{tag}_burst"]
        pa, pb, _ = S[f"{tag}_post"]
        gain_burst = env[a:b] - STEP_HIGH - unity
        gain_post = env[pa:pb] - STEP_LOW - unity
        settled = np.mean(gain_burst[-max(1, len(gain_burst) // 5):])
        r[f"{tag}_gr"] = -settled
        if tag == "long" and settled < -0.5:
            reach = np.nonzero(gain_burst[SKIP:] <= 0.9 * settled)[0]
            r["attack_ms"] = 1000 * (reach[0] + SKIP) / FS if len(reach) else float("nan")
        start = np.mean(gain_post[int(0.004 * FS): int(0.01 * FS)])
        if start < -0.5:
            for frac, key in ((0.5, "rel50_ms"), (0.1, "rel90_ms")):
                back = np.nonzero(gain_post[SKIP:] >= frac * start)[0]
                r[f"{tag}_{key}"] = 1000 * (back[0] + SKIP) / FS if len(back) else float("inf")

    r["thd"] = {}
    for name, (a, b, lv) in S.items():
        if name.startswith("thd"):
            seg = x[a + (b - a) // 2: b - int(0.05 * FS)]
            h1 = harmonic_db(seg, TONE)
            r["thd"][lv] = (harmonic_db(seg, 2 * TONE) - h1, harmonic_db(seg, 3 * TONE) - h1)
    return r


def fmt(v, unit=""):
    return "   n/a" if v is None or v != v else (f"{v:6.1f}{unit}" if abs(v) != float("inf") else "  >4 s")


def report(r):
    print(f"\n== {r['path']}  (latency {r['latency_ms']:.1f} ms, makeup {r['makeup']:+.1f} dB)")
    print(f"   threshold {fmt(r['threshold'], ' dBFS')}   ratio near the top {r['ratio']:.1f}:1")
    print("   in dBFS : " + " ".join(f"{v:5.0f}" for v in r["ins"]))
    print("   GR dB   : " + " ".join(f"{v:5.1f}" for v in r["gr"]))
    print(f"   attack to 90%            {fmt(r.get('attack_ms'), ' ms')}   (at {r['long_gr']:.1f} dB GR)")
    print(f"   release after 1.5 s burst: half {fmt(r.get('long_rel50_ms'), ' ms')}   90% {fmt(r.get('long_rel90_ms'), ' ms')}")
    print(f"   release after 50 ms burst: half {fmt(r.get('short_rel50_ms'), ' ms')}   90% {fmt(r.get('short_rel90_ms'), ' ms')}")
    for lv, (h2, h3) in r["thd"].items():
        print(f"   1 kHz at {lv:5.0f} dBFS: 2nd {h2:6.1f} dB   3rd {h3:6.1f} dB")


def compare(a, b):
    print(f"\n== difference (ours - reference)")
    print(f"   threshold {fmt(b['threshold'] - a['threshold'], ' dB')}   ratio {a['ratio']:.1f} vs {b['ratio']:.1f}")
    print("   GR dB   : " + " ".join(f"{v:+5.1f}" for v in b["gr"] - a["gr"]))
    for key, label in (("attack_ms", "attack"), ("long_rel50_ms", "release half (long)"), ("long_rel90_ms", "release 90% (long)"),
                       ("short_rel50_ms", "release half (short)"), ("short_rel90_ms", "release 90% (short)")):
        print(f"   {label:22s} ref {fmt(a.get(key), ' ms')}   ours {fmt(b.get(key), ' ms')}")
    for lv in a["thd"]:
        print(f"   harmonics at {lv:5.0f} dBFS: 2nd {b['thd'][lv][0] - a['thd'][lv][0]:+6.1f} dB   3rd {b['thd'][lv][1] - a['thd'][lv][1]:+6.1f} dB")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    results = [measure(p) for p in sys.argv[1:3]]
    for res in results:
        report(res)
    if len(results) == 2:
        compare(*results)
