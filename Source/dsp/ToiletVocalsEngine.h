// Toilet Vocals engine (no JUCE dependency).
//
// Signal path:  auto input leveling -> input fader -> compressor 1 -> compressor 2
//               -> (input fader undone) -> output fader
//
// Each compressor stage has four models and ONE fader ("amount"). The bottom of
// the fader is that unit's gentlest usable setting, the top its most extreme:
//
//   #1 76  FET limiter, 1176 family.       4:1, 0-2 dB  ->  all-buttons, ~18 dB
//   LA #2A    tube optical leveler, LA-2A.    compress, 1-2 dB -> limit, ~11 dB
//   CL #1b    clean tube opto, CL 1B family.  2:1, 1-2 dB  ->  10:1, ~11 dB
//   IBSsor    Distressor family.              2:1, 1-2 dB  ->  Nuke, ~18 dB
//
// Gain-reduction figures are for a vocal at the reference level (-18 dBFS gated
// RMS), which is what the automatic input leveling delivers.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "MakeupTables.h"

namespace tvox
{
constexpr int kNumModels = 4;
constexpr float kRefRmsDb = -18.0f;   // level the compressors are tuned for
constexpr float kLearnSeconds = 8.0f; // seconds of singing needed to set the input level

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 8.6858896f * std::log (std::max (g, 1.0e-9f)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }
inline float smoothstep (float lo, float hi, float v)
{
    const float t = clamp01 ((v - lo) / (hi - lo));
    return t * t * (3.0f - 2.0f * t);
}

struct ModelInfo { const char* name; const char* blurb; };
inline const ModelInfo& modelInfo (int i)
{
    static const ModelInfo m[kNumModels] = {
        { "#1 76", "FET. Fast and aggressive, with bite." },
        { "LA #2A",   "Tube opto. Slow, thick and smooth." },
        { "CL #1b",   "Clean opto. Glossy, modern pop." },
        { "IBSsor",   "Firm control up to all-out grit." } };
    return m[std::min (kNumModels - 1, std::max (0, i))];
}

// Threshold (dBFS) at the bottom and top of the fader, per model. Set by
// `tools/render tune` so the gain reduction on a reference-level vocal hits the
// targets in the header comment.
struct Tuning { float thrLo, thrHi; };
inline std::array<Tuning, kNumModels>& tuning()
{
    static std::array<Tuning, kNumModels> t { { { -8.9f, -27.1f }, { -9.4f, -21.2f }, { -10.7f, -20.4f }, { -8.1f, -27.0f } } };
    return t;
}

// Everything one fader position means for one model.
struct Curve
{
    float thrDb = 0, slope = 0, kneeDb = 6;       // slope = 1 - 1/ratio
    float attMs = 1, relMs = 100, relSlowMs = 100; // release slows after sustained compression
    float memory = 0, memAttMs = 400, memRelMs = 2000; // opto "memory": share of GR that lingers
    float memDepth = 0;                            // ...and lingers longer the deeper it went
    float satIn = 0;                               // input-stage drive (there with or without compression)
    float satOdd = 0, satGr = 0;                   // odd harmonics: fixed part, and part that grows with GR
    float satEven = 0, evenGr = 0;                 // even harmonics: likewise
    float lfSat = 0;                               // transformer: low frequencies saturate first
    float scTilt = 0;                              // detector hears highs louder (optical cell)
};

// Piecewise-linear lookup over (amount, value) pairs.
template <size_t N>
inline float pts (const std::array<std::array<float, 2>, N>& p, float a)
{
    if (a <= p[0][0]) return p[0][1];
    for (size_t i = 1; i < N; ++i)
        if (a <= p[i][0]) return lerp (p[i - 1][1], p[i][1], (a - p[i - 1][0]) / (p[i][0] - p[i - 1][0]));
    return p[N - 1][1];
}
inline float slopeOf (float ratio) { return 1.0f - 1.0f / ratio; }
inline float expLerp (float a, float b, float t) { return a * std::pow (b / a, t); }

inline Curve curveFor (int model, float a)
{
    a = clamp01 (a);
    const auto& tn = tuning()[(size_t) std::min (kNumModels - 1, std::max (0, model))];
    Curve c;
    c.thrDb = lerp (tn.thrLo, tn.thrHi, a);
    using P = std::array<float, 2>;
    switch (model)
    {
        default:
        case 0: // #1 76: 4 -> 8 -> 12 -> 20 -> all buttons in
        {
            // Published: 20-800 us attack, 50 ms-1.1 s release, soft knee, all of it program
            // dependent. All-buttons sits between 12:1 and 20:1 but the curve turns into a
            // plateau, the attack lags on transients, and distortion rises sharply.
            const float all = smoothstep (0.78f, 1.0f, a);
            c.slope = pts<4> ({ P { 0.0f, slopeOf (4) }, P { 0.33f, slopeOf (8) }, P { 0.55f, slopeOf (12) }, P { 0.78f, slopeOf (20) } }, a);
            c.kneeDb = lerp (lerp (7.0f, 4.0f, std::min (1.0f, a / 0.78f)), 1.0f, all);
            c.attMs = lerp (expLerp (0.8f, 0.05f, std::min (1.0f, a / 0.78f)), 0.5f, all);
            // Vocal 1176 settings keep the release at or near its fastest: that quick recovery
            // (and the grit it brings) is the urgency people use it for.
            c.relMs = lerp (expLerp (110.0f, 55.0f, std::min (1.0f, a / 0.78f)), 50.0f, all);
            c.relSlowMs = lerp (expLerp (380.0f, 160.0f, std::min (1.0f, a / 0.78f)), 120.0f, all);
            c.satIn = lerp (0.4f, 0.75f, a);     // the early revision's input stage clips on its own
            c.satOdd = lerp (0.35f, 0.75f, a) + 0.9f * all;
            c.satGr = lerp (0.3f, 1.0f, a);
            c.satEven = lerp (0.04f, 0.10f, a);
            c.evenGr = 0.05f;
            c.lfSat = 1.0f;
            break;
        }
        case 1: // LA #2A: compress (~3:1) -> limit
            // Published: ~10 ms average attack (frequency dependent); release reaches 50% in
            // ~60 ms, the rest takes 1-15 s and is slower after long or heavy compression.
            c.slope = lerp (slopeOf (3), slopeOf (14), smoothstep (0.0f, 1.0f, a));
            c.kneeDb = lerp (14.0f, 8.0f, a);
            c.attMs = 4.0f;        // lands near 10-15 ms to full grab on real signals
            c.relMs = 70.0f;       // first half of the release
            c.relSlowMs = 70.0f;
            c.memory = 0.42f;      // ...the rest takes seconds
            c.memAttMs = 600.0f;
            c.memRelMs = 1500.0f;
            c.memDepth = 0.2f;     // +20% release time per dB the cell has been holding
            c.satOdd = lerp (0.15f, 0.35f, a);
            c.satGr = 0.2f;
            c.satEven = lerp (0.03f, 0.08f, a);
            c.lfSat = 1.0f;
            c.scTilt = 0.4f;
            break;
        case 2: // CL #1b: 2:1 -> 10:1, the cleanest of the four
            // Published: 2:1-10:1, 0.5-300 ms attack, 0.05-10 s release, fixed mode 1 ms / 50 ms,
            // 0.15% THD. Quick attack and release at low ratio is its classic vocal setting.
            c.slope = pts<3> ({ P { 0.0f, slopeOf (2) }, P { 0.5f, slopeOf (4) }, P { 1.0f, slopeOf (10) } }, a);
            c.kneeDb = lerp (12.0f, 6.0f, a);
            c.attMs = expLerp (10.0f, 1.0f, a);
            c.relMs = expLerp (250.0f, 50.0f, a);
            c.relSlowMs = expLerp (700.0f, 200.0f, a);
            c.memory = 0.2f;
            c.memAttMs = 500.0f;
            c.memRelMs = 1000.0f;
            c.satOdd = lerp (0.06f, 0.12f, a);
            c.satGr = 0.1f;
            c.satEven = lerp (0.012f, 0.03f, a);
            c.lfSat = 0.6f;
            break;
        case 3: // IBSsor: 2 -> 3 -> 4 -> 6 -> 10 -> 20 -> Nuke
            // Published: 50 us-30 ms attack, 50 ms-3.5 s release; 2:1 has a knee up to 30 dB
            // long, Nuke is a brick wall; distortion rises with faster times; Dist 2 is mostly
            // 2nd harmonic while compressing, Dist 3 adds 3rd and flattens peaks like tape.
            c.slope = pts<7> ({ P { 0.0f, slopeOf (2) }, P { 0.17f, slopeOf (3) }, P { 0.33f, slopeOf (4) },
                                P { 0.5f, slopeOf (6) }, P { 0.67f, slopeOf (10) }, P { 0.84f, slopeOf (20) },
                                P { 1.0f, slopeOf (100) } }, a);
            c.kneeDb = lerp (30.0f, 2.0f, a);
            // Vocal settings from its manual sit around the middle of the attack knob, a few
            // milliseconds, so consonants get through before it clamps (the 1176 never allows that).
            c.attMs = expLerp (10.0f, 1.5f, a);
            // Its release is logarithmic: lets go quickly at first, then slows.
            c.relMs = expLerp (140.0f, 70.0f, a);
            c.relSlowMs = c.relMs;
            c.memory = 0.35f;
            c.memAttMs = 60.0f;
            c.memRelMs = expLerp (900.0f, 350.0f, a);
            // Clean (it is a low-distortion VCA design) until the distortion modes come in.
            c.satOdd = lerp (0.02f, 0.05f, a) + 1.0f * smoothstep (0.82f, 1.0f, a); // Dist 3 at the very top
            c.satGr = lerp (0.05f, 0.5f, a);
            c.satEven = 0.005f;
            c.evenGr = 0.5f * smoothstep (0.6f, 0.9f, a);                           // Dist 2 over the upper third
            break; // transformerless: no low-frequency saturation
    }
    return c;
}

inline float makeupFor (int model, float amount, float inputOffsetDb)
{
    const float fa = clamp01 (amount) * (float) (kMakeupAmounts - 1);
    const float fo = std::min ((float) (kMakeupOffsets - 1), std::max (0.0f, (inputOffsetDb + 12.0f) / 6.0f));
    const int ia = std::min (kMakeupAmounts - 2, (int) fa), io = std::min (kMakeupOffsets - 2, (int) fo);
    const float ta = fa - (float) ia, to = fo - (float) io;
    const auto& t = kMakeupDb[std::min (kNumModels - 1, std::max (0, model))];
    return lerp (lerp (t[io][ia], t[io][ia + 1], ta), lerp (t[io + 1][ia], t[io + 1][ia + 1], ta), to);
}

// ============================================================ one compressor
class Stage
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        dcR = 1.0f - 6.2831853f * 8.0f / fs;
        cMix = coef (15.0f);
        cMake = coef (30.0f);
        cLf = 1.0f - std::exp (-6.2831853f * 120.0f / fs);   // transformer low-frequency corner
        cSc = 1.0f - std::exp (-6.2831853f * 1500.0f / fs);  // detector tilt corner
        reset();
    }

    void reset()
    {
        env = mem = 0.0f;
        for (int ch = 0; ch < 2; ++ch) dcX[ch] = dcY[ch] = lfA[ch] = lfB[ch] = scLp[ch] = 0.0f;
        mix = on ? 1.0f : 0.0f;
        makeDb = makeTarget;
        grNow = 0.0f;
    }

    void set (int model, float amount, bool isOn, float inputOffsetDb, bool autoMakeup)
    {
        cv = curveFor (model, amount);
        on = isOn;
        makeTarget = autoMakeup ? makeupFor (model, amount, inputOffsetDb) : 0.0f;
        aAtt = coef (cv.attMs);
        aRel = coef (cv.relMs);
        aRelSlow = coef (cv.relSlowMs);
        aMemAtt = coef (cv.memAttMs);
        aMemRel = coef (cv.memRelMs);
    }

    float gainReductionDb() const { return grNow; }

    inline void process (float& l, float& r)
    {
        mix += ((on ? 1.0f : 0.0f) - mix) * cMix;
        if (! on && mix < 1.0e-4f)
        {
            mix = 0.0f; env = mem = grNow = 0.0f;
            return;
        }

        // --- input stage: clips a little on its own, compression or not
        float wl = l, wr = r;
        if (cv.satIn > 1.0e-3f)
        {
            wl = std::tanh (cv.satIn * wl) / cv.satIn;
            wr = std::tanh (cv.satIn * wr) / cv.satIn;
        }

        // --- detector (optionally hearing highs louder) + static curve with a soft knee
        float dl = wl, dr = wr;
        if (cv.scTilt > 0.0f)
        {
            scLp[0] += (wl - scLp[0]) * cSc; scLp[1] += (wr - scLp[1]) * cSc;
            dl += cv.scTilt * (wl - scLp[0]); dr += cv.scTilt * (wr - scLp[1]);
        }
        const float levelDb = gainToDb (std::max (std::abs (dl), std::abs (dr)));
        const float over = levelDb - cv.thrDb;
        const float w = cv.kneeDb;
        float target = 0.0f;
        if (2.0f * over >= w) target = cv.slope * over;
        else if (2.0f * over > -w) target = cv.slope * (over + 0.5f * w) * (over + 0.5f * w) / (2.0f * w);

        // --- ballistics. The release slows down after sustained compression and
        //     stays quick after a lone transient (program dependence).
        if (target > env) env += (target - env) * aAtt;
        else
        {
            const float sustained = clamp01 (mem / std::max (env, 0.25f));
            env += (target - env) * lerp (aRel, aRelSlow, sustained);
        }
        mem += (env - mem) * (env > mem ? aMemAtt : aMemRel / (1.0f + cv.memDepth * mem));
        const float gr = std::max (env, cv.memory * mem);
        grNow = gr;

        const float g = dbToGain (-gr);
        // The colour stage sees the signal with half the gain reduction undone, so
        // heavy compression still drives it (as it does the real gain cells).
        const float k = 1.0f / std::sqrt (g);
        const float grN = std::min (1.0f, gr * 0.05f);
        const float odd = cv.satOdd + cv.satGr * grN;
        const float even = cv.satEven + cv.evenGr * grN;
        makeDb += (makeTarget - makeDb) * cMake;
        const float out = dbToGain (makeDb);

        wl = dc (colour (wl * g * k, odd, even, 0) / k, 0) * out;
        wr = dc (colour (wr * g * k, odd, even, 1) / k, 1) * out;
        l += (wl - l) * mix;
        r += (wr - r) * mix;
    }

private:
    float coef (float ms) const { return 1.0f - std::exp (-1000.0f / (std::max (0.01f, ms) * fs)); }

    // Waveshaper between a low-shelf boost and its exact inverse, so lows hit
    // the nonlinearity harder (transformer cores saturate at low frequencies
    // first) while the small-signal response stays flat.
    inline float colour (float x, float odd, float even, int ch)
    {
        const float a = cv.lfSat;
        lfA[ch] += (x - lfA[ch]) * cLf;
        float v = x + a * lfA[ch];
        v += even * v * v;
        if (odd > 1.0e-3f) v = std::tanh (odd * v) / odd;
        const float y = (v - a * (1.0f - cLf) * lfB[ch]) / (1.0f + a * cLf);
        lfB[ch] += (y - lfB[ch]) * cLf;
        return y;
    }
    inline float dc (float x, int ch)
    {
        const float y = x - dcX[ch] + dcR * dcY[ch];
        dcX[ch] = x; dcY[ch] = y;
        return y;
    }

    Curve cv;
    float fs = 48000.0f;
    bool on = true;
    float aAtt = 1, aRel = 1, aRelSlow = 1, aMemAtt = 1, aMemRel = 1, cMix = 1, cMake = 1, cLf = 0, cSc = 0;
    float env = 0, mem = 0, mix = 1, makeDb = 0, makeTarget = 0, grNow = 0;
    float dcX[2] {}, dcY[2] {}, dcR = 0.999f, lfA[2] {}, lfB[2] {}, scLp[2] {};
};

// =========================================================== 2x oversampling
// Linear-phase half-band FIR up/down sampler. The compressors run at twice the
// sample rate so fast gain changes and the saturation don't alias back down.
class Halfband2x
{
public:
    static constexpr int kTaps = 47, kHalf = 24;  // even-index taps: 24. Of the odd ones only the centre is non-zero.
    static constexpr int kLatency = 23;           // samples at the base rate, up + down

    Halfband2x()
    {
        const double beta = 8.0, pi = 3.14159265358979;
        auto bessel = [] (double x) { double sum = 1.0, t = 1.0; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += t; } return sum; };
        double h[kTaps], total = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const int mi = n - (kTaps - 1) / 2;
            const double m = mi, r = 2.0 * n / (kTaps - 1) - 1.0;
            const double sinc = mi == 0 ? 1.0 : std::sin (pi * m / 2.0) / (pi * m / 2.0);
            h[n] = 0.5 * sinc * bessel (beta * std::sqrt (1.0 - r * r)) / bessel (beta);
            total += h[n];
        }
        for (int j = 0; j < kHalf; ++j) even[j] = (float) (h[2 * j] / total);
        centre = (float) (h[(kTaps - 1) / 2] / total);
        reset();
    }

    void reset() { for (auto& c : ch) c = Ch(); }

    inline void up (int c, float in, float& y0, float& y1)
    {
        auto& s = ch[c];
        s.xi = (s.xi + 1) % kHalf;
        s.x[s.xi] = in;
        float acc = 0.0f;
        for (int j = 0, i = s.xi; j < kHalf; ++j) { acc += even[j] * s.x[i]; i = i == 0 ? kHalf - 1 : i - 1; }
        y0 = 2.0f * acc;
        y1 = 2.0f * centre * s.x[(s.xi + kHalf - kCentre) % kHalf];
    }

    inline float down (int c, float a, float b)
    {
        auto& s = ch[c];
        s.di = (s.di + 1) % kHalf;
        s.a[s.di] = a; s.b[s.di] = b;
        float acc = 0.0f;
        for (int j = 0, i = s.di; j < kHalf; ++j) { acc += even[j] * s.a[i]; i = i == 0 ? kHalf - 1 : i - 1; }
        return acc + centre * s.b[(s.di + kHalf - kCentre - 1) % kHalf]; // this phase makes the total delay a whole number of samples
    }

private:
    static constexpr int kCentre = 11; // the centre tap is odd-index tap number 11
    struct Ch { float x[kHalf] {}, a[kHalf] {}, b[kHalf] {}; int xi = 0, di = 0; };
    float even[kHalf] {}, centre = 0.5f;
    Ch ch[2];
};

// ================================================================ the plugin
struct StageParams { int model = 0; float amount = 0.35f; bool on = true; };
struct Params
{
    float inputDb = 0.0f;   // offset on top of the automatic level, -12..+12
    StageParams comp1 { 0, 0.35f, true };
    StageParams comp2 { 1, 0.35f, true };
    float outputDb = 0.0f;
    bool autoLevel = true;  // false = input fader acts on the raw track level
    bool autoMakeup = true;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        oversample = sampleRate < 80000.0; // at 88.2k and up there is already room above the audio band
        blockLen = std::max (1, (int) (0.1f * fs));
        cGain = 1.0f - std::exp (-1.0f / (0.15f * fs));
        cFast = 1.0f - std::exp (-1.0f / (0.02f * fs));
        cMeter = 1.0f - std::exp (-1.0f / (0.3f * fs));
        s1.prepare (sampleRate * (oversample ? 2.0 : 1.0));
        s2.prepare (sampleRate * (oversample ? 2.0 : 1.0));
        reset();
    }

    void reset()
    {
        s1.reset(); s2.reset(); os.reset();
        blockSum = 0.0; blockCount = 0;
        normDb = learned || activeBlocks >= 3 ? normTarget() : 0.0f;
        preDb = normDb + p.inputDb;
        postDb = p.outputDb - p.inputDb;
        meterMs = 0.0f;
    }

    int latencySamples() const { return oversample ? Halfband2x::kLatency : 0; }

    void setParams (const Params& np)
    {
        p = np;
        s1.set (p.comp1.model, p.comp1.amount, p.comp1.on, p.inputDb, p.autoMakeup);
        s2.set (p.comp2.model, p.comp2.amount, p.comp2.on, p.inputDb, p.autoMakeup);
    }

    // ---- automatic input level
    void relearn() { learned = false; sumMs = 0.0; activeBlocks = 0; }
    void restoreLearn (bool isLearned, float gainDb)
    {
        learned = isLearned;
        learnedDb = gainDb;
        if (! isLearned) { sumMs = 0.0; activeBlocks = 0; }
    }
    bool isLearned() const { return learned; }
    float learnProgress() const { return learned ? 1.0f : std::min (1.0f, (float) activeBlocks * 0.1f / kLearnSeconds); }
    float normGainDb() const { return normTarget(); } // gain the auto level is applying / will hold

    // ---- meters (read after process)
    float inputLevelDb() const { return 10.0f * std::log10 (std::max (meterMs, 1.0e-10f)); } // level into the compressors
    float gr1Db() const { return gr1; }
    float gr2Db() const { return gr2; }

    void process (float* L, float* R, int n)
    {
        float g1 = 0.0f, g2 = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            analyse (0.5f * (l * l + r * r));

            normDb += ((p.autoLevel ? normTarget() : 0.0f) - normDb) * cGain;
            preDb += (normDb + p.inputDb - preDb) * cFast;
            postDb += (p.outputDb - p.inputDb - postDb) * cFast;
            const float pre = dbToGain (preDb);
            l *= pre; r *= pre;
            meterMs += (0.5f * (l * l + r * r) - meterMs) * cMeter;

            if (oversample)
            {
                float l0, l1, r0, r1;
                os.up (0, l, l0, l1); os.up (1, r, r0, r1);
                s1.process (l0, r0); g1 = std::max (g1, s1.gainReductionDb());
                s2.process (l0, r0); g2 = std::max (g2, s2.gainReductionDb());
                s1.process (l1, r1); s2.process (l1, r1);
                l = os.down (0, l0, l1); r = os.down (1, r0, r1);
            }
            else
            {
                s1.process (l, r); g1 = std::max (g1, s1.gainReductionDb());
                s2.process (l, r); g2 = std::max (g2, s2.gainReductionDb());
            }

            const float post = dbToGain (postDb);
            L[i] = l * post; R[i] = r * post;
        }
        gr1 = g1; gr2 = g2;
    }

private:
    float normTarget() const
    {
        if (learned) return learnedDb;
        if (activeBlocks < 3) return 0.0f; // not enough singing heard yet
        const float rmsDb = 10.0f * std::log10 ((float) (sumMs / activeBlocks) + 1.0e-12f);
        return std::min (30.0f, std::max (-24.0f, kRefRmsDb - rmsDb));
    }

    // Gated loudness of the raw input in 100 ms blocks. Silence, breaths and
    // bleed (anything far below the singing heard so far) are left out, and the
    // level locks once enough singing has gone by so bounces repeat exactly.
    inline void analyse (float ms)
    {
        if (learned) return;
        blockSum += ms;
        if (++blockCount < blockLen) return;
        const float m = (float) (blockSum / blockLen);
        blockSum = 0.0; blockCount = 0;
        const float db = 10.0f * std::log10 (m + 1.0e-12f);
        const float soFar = activeBlocks > 0 ? 10.0f * std::log10 ((float) (sumMs / activeBlocks) + 1.0e-12f) : -120.0f;
        if (db > -50.0f && db > soFar - 12.0f)
        {
            sumMs += m;
            if (++activeBlocks >= (int) (kLearnSeconds * 10.0f))
            {
                learnedDb = std::min (30.0f, std::max (-24.0f, kRefRmsDb - 10.0f * std::log10 ((float) (sumMs / activeBlocks) + 1.0e-12f)));
                learned = true;
            }
        }
    }

    Params p;
    Stage s1, s2;
    Halfband2x os;
    bool oversample = true;
    float fs = 48000.0f;
    int blockLen = 4800, blockCount = 0, activeBlocks = 0;
    double blockSum = 0.0, sumMs = 0.0;
    bool learned = false;
    float learnedDb = 0.0f;
    float normDb = 0, preDb = 0, postDb = 0, cGain = 1, cFast = 1, cMeter = 1, meterMs = 0;
    float gr1 = 0, gr2 = 0;
};
} // namespace tvox
