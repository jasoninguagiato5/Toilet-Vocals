# Matching the compressor models to reference plugins

The reference plugins are closed, so the match comes from measuring them from the outside: play a known test signal through each one, bounce the result, and compare it with the same signal through our model.

## 1. Make the test signal

```bash
pip3 install numpy scipy
cd tools/match
python3 make_test_signal.py        # writes test_signal.wav (63 s, mono, 48 kHz)
```

It holds a staircase of 1 kHz tones from -48 to 0 dBFS (the compression curve), two loud bursts over a quiet bed (attack and release), and four steady tones (harmonics).

## 2. Bounce it through each reference plugin

In a 48 kHz session, put `test_signal.wav` on a mono audio track starting at bar 1, with the reference plugin as the only insert.

For each plugin, make three bounces: the gentlest setting you'd use on a vocal, a medium one, and the most extreme one. These become the bottom, middle and top of our fader. Set them up by ear on a real vocal first, then bounce the test signal through those settings without touching anything, and write the knob positions down.

| Our model | Reference | Bottom of fader | Top of fader |
|---|---|---|---|
| Plunger | 1176 Rev A | ratio 4, slow attack, 0-2 dB on the meter | all buttons in, fast attack and release |
| Float | LA-2A | Compress, 1-2 dB | Limit, 10 dB or more |
| Bidet | CL 1B | 2:1, slow attack, 1-2 dB | 10:1, fast attack and release |
| Snake | Distressor | 2:1, 1-2 dB | Nuke, Dist 3 |

Name the files so the plugin and position are obvious, e.g. `1176_low.wav`, `1176_mid.wav`, `1176_high.wav`. Bounce at 48 kHz, mono, 24-bit or 32-bit float, from bar 1, with everything else on the track bypassed.

## 3. Compare

```bash
c++ -std=c++20 -O2 -o render ../render.cpp
./render file test_signal.wav ours.wav m1=0 a1=0.5 on2=0 auto=0   # model 0 = Plunger, fader at 50%, stage 2 off, auto level off
python3 analyze.py 1176_mid.wav ours.wav
```

Models: 0 Plunger, 1 Float, 2 Bidet, 3 Snake. The analyzer prints, for each file, the threshold and ratio, gain reduction at every input level, attack time, release time after a long and a short burst, and 2nd/3rd harmonic levels, then the differences. Timing is resolved to about a millisecond, so attacks faster than that read as 1.5 ms.

The differences are tuned out in `curveFor()` in `Source/dsp/ToiletVocalsEngine.h`.
