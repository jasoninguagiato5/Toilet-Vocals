# Toilet Vocals

By **Porcelain Audio**. One plugin on the vocal track and it's done. AU, VST3 and Standalone for macOS (Apple Silicon + Intel). Built with JUCE 8.

Aimed at pop and rock vocals: in your face, with a crisp top end. This is an early build (v0.5): automatic input leveling, a low cut, the Low and High EQ faders, the two compressor stages and a de-esser. Saturation and effects come next.

## Interface

The faceplate shows the whole planned plugin: Input, Low, High, Comp 1, Comp 2, Reverb, Delay, Width, Dirt and Output, plus a Clean/Dirty switch. Reverb, Delay, Width, Dirt and the switch are drawn greyed out as placeholders.

- **Input:** fader, level meter (the green "18" is where the vocal should sit), a light that blinks amber while the level is being measured and turns green once it is set, and a red button under the meter that measures again.
- **Low / High:** lights to pick the region or EQ model, and a fader that is flat in the middle.
- **Comp 1 / Comp 2:** four lights to pick the model, a small In button above the slot, the amount fader, and a gain-reduction meter beside it.
- **Toilet roll (bottom left):** the low cut. Drag the paper down to cut more; double-click to switch it off.
- **Soap dispenser (bottom right):** the de-esser. Drag the soap level up for more; click the pump to switch range (up = high, down = low); double-click the bottle to switch it off. The small meter beside it shows how much it is taking off.
- Drag a fader to see its value; double-click to reset it. Hover over a light's name for a one-line description.

## Signal path

```
low cut -> auto level -> INPUT fader -> EQ cuts -> COMP 1 -> COMP 2 -> EQ boosts -> de-esser -> OUTPUT fader
```

The Low and High faders cut before the compressors, so mud and harshness don't drive them, and boost after them, so added lows and highs don't make them pump. The low cut is first, so rumble stays out of the level measurement too.

**Input.** The plugin listens to about 8 seconds of singing, works out the gain that puts the vocal at the level the compressors are tuned for (-18 dBFS RMS), and locks it. The lock is saved with the session, so bounces repeat exactly. The INPUT fader is an offset on top of that (±12 dB): up pushes the vocal harder into everything, down backs it off. The offset is undone at the output and the makeup gain follows it, so pushing the input changes the intensity, not the volume. The green "18" on the input meter is where the vocal should sit. The red button forgets the measured level and listens again; play the loudest section while it does.

**Comp 1 and Comp 2.** Two compressors in series. Each has all four models, an In switch and one fader. The bottom of the fader is that unit's gentlest usable setting, the top its most extreme, and it moves smoothly between them. Gain reduction below is for the loud parts of a vocal at the reference level.

| Model | Modelled on | Fader at bottom | Fader at top |
|---|---|---|---|
| **#1 76** | 1176-style FET | 4:1, 0-2 dB | all-buttons-in, about 18 dB, with grit |
| **LA #2A** | LA-2A-style tube opto | compress, 1-2 dB | limit, about 11 dB |
| **CL #1b** | CL 1B-style opto | 2:1, 1-2 dB | 10:1, about 11 dB, still the cleanest |
| **IBSsor** | Distressor-style | 2:1, 1-2 dB | Nuke, about 18 dB, with even then odd harmonics |

Makeup gain is automatic, so a fader changes how hard the stage works without changing the loudness (within about 1 dB).

What each model takes from the published behaviour of its hardware:

- **#1 76:** grabs instantly (0.8 ms down to 0.05 ms attack) and lets go fast, quicker after a lone peak and slower after sustained compression. Its input stage adds grit even with no compression, and it gets dirtier the harder it works. At the top of the fader it takes on the all-buttons traits: a plateau-shaped curve, a lagging attack that lets transients through, and much more distortion.
- **LA #2A:** about 10 ms attack, a release that recovers half way in about 60 ms and then takes seconds, longer the harder and longer it has been working, and a detector that reacts more to high frequencies.
- **CL #1b:** quick attack and release at low ratios (its classic vocal use), moving to 1 ms / 50 ms at the top, with very low distortion throughout.
- **IBSsor:** a slower attack (10 ms down to 1.5 ms) that lets consonants through before it clamps, a knee about 30 dB long at 2:1 tightening to a brick wall at Nuke, and a release that lets go quickly at first and then slows. It stays clean until the upper third of the fader, where second harmonic appears while it compresses (Dist 2), with third harmonic at the very top (Dist 3).

The three transformer-coupled models (#1 76, LA #2A and CL #1b) also saturate low frequencies first. The compressors run at twice the sample rate (below 88.2 kHz sessions) so the fast gain changes and harmonics don't alias; this adds 39 samples of latency, which the plugin reports to the host.

**Low cut.** 18 dB/octave, from off (20 Hz) to 250 Hz.

**Low.** Flat at the centre, up to ±9 dB at the ends. Up boosts with a wide curve, down cuts with a narrow one.

| Light | Region | Up | Down |
|---|---|---|---|
| **Upper** | bell at 450 Hz | thickness | boxiness |
| **Lower** | bell at 230 Hz | warmth and body | mud |
| **Sub** | shelf around 110 Hz | chest and weight | boom, proximity effect |

**High.** Flat at the centre. Each light is a different EQ, built from the published behaviour of the hardware.

| Light | Modelled on | Up | Down |
|---|---|---|---|
| **Air** | Maag EQ4 Air Band at 40 kHz | very wide boost-only shelf, up to +12 dB, heard as a long slope: about +4 dB at 10 kHz and +7 dB at 16 kHz at full | mirror of the boost (the hardware has no cut) |
| **Silk** | Pultec EQP-1A | broad peak boost at 12 kHz, up to +12 dB | its separate high shelf attenuator, down to -10 dB |
| **Sheen** | Neve 1073 | its single "12 kHz" shelf: a very gradual first-order slope that starts lifting in the upper mids, up to +10 dB | the same shelf, cutting |
| **Bite** | SSL 4000 E "black knob" | high-mid bell at 3.8 kHz (Q 0.8-1.3, up to +10 dB) plus some of its 8 kHz shelf | the same curves, cutting |

The high EQs run at twice the sample rate, so shelves above the audible band keep their analogue shape instead of being squeezed against the top of the spectrum; `./render eq` prints every curve and how closely each follows its analogue formula (within about 0.5 dB up to 16 kHz).

**De-esser.** After the dbx 902: it compares the level of the highs with the level of the whole signal, in dB, so it behaves the same on quiet and loud lines and needs no threshold. Only the highs are turned down, by up to 14 dB at full, and it lets go at the 902's 925 dB per second. It comes last, so it works on the esses as the compressors and the High fader left them. Pump up (high range) listens above 5.5 kHz and dips from about 5 kHz, for "s" and brighter voices; pump down (low range) listens above 3.2 kHz and dips from about 3 kHz, for "sh" and deeper voices. `./render deess` shows what it takes off the esses and that it leaves everything else alone.

**Output.** Final level, -24 to +12 dB.

## Build (macOS)

You need Xcode (or the Command Line Tools) and CMake 3.22 or newer (`brew install cmake`).

```bash
cmake -B build -G Xcode            # first run downloads JUCE 8.0.13
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD` installs the AU to `~/Library/Audio/Plug-Ins/Components` and the VST3 to `~/Library/Audio/Plug-Ins/VST3`. To check the AU: `auval -v aufx TlVc Tlta`.

Every push to `main` also builds a `.pkg` installer on GitHub Actions and attaches it to the Release for the current version.

## Source layout

```
Source/dsp/ToiletVocalsEngine.h  input leveling and the four compressor models (no JUCE dependency)
Source/dsp/ToiletEq.h            low cut, Low and High EQ sections, de-esser
Source/dsp/MakeupTables.h        auto makeup gain, generated by `render calib`
Source/PluginProcessor.*         JUCE processor, parameters, state
Source/PluginEditor.*            controls and meters
Source/ui/Artwork.*              all faceplate drawing (juce_graphics only)
tools/render.cpp                 offline harness: stress test, measurements, calibration
tools/preview.cpp                renders the faceplate to a PNG without a plugin host
tools/match/                     test signal + analyzer for matching the models to reference plugins
```

Offline harness (no JUCE needed):

```bash
c++ -std=c++20 -O2 -o render tools/render.cpp
./render stress                  # 120 randomized runs at 44.1/48/96 kHz
./render measure                 # gain reduction, loudness and harmonics per model and fader position
./render diff                    # how different the models sound from each other
./render eq                      # frequency response of the low cut, Low and High sections
./render deess                   # what the de-esser does to esses and to everything else
./render file in.wav out.wav m1=0 a1=0.5 m2=1 a2=0.3
```

After changing a model's curve, re-run `./render tune` (paste the line it prints into `tuning()` in the engine), then `./render calib > Source/dsp/MakeupTables.h`.

## Matching the reference plugins

The models are built from the published behaviour of the hardware and have not yet been matched against the plugins they're based on. `tools/match/README.md` explains how to bounce a test signal through those plugins so the curves, timing and harmonics can be tuned to them.
