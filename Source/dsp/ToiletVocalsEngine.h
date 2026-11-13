// Toilet Vocals engine (no JUCE dependency).
//
// Signal path:  auto input leveling -> input fader -> compressor 1 -> compressor 2
//               -> (input fader undone) -> output fader
//
// Each compressor stage has four models and ONE fader ("amount"). The bottom of
// the fader is that unit's gentlest usable setting, the top its most extreme:
//
//   Plunger  FET limiter, 1176 family.       4:1, 0-2 dB  ->  all-buttons, ~18 dB
//   Float    tube optical leveler, LA-2A.    compress, 1-2 dB -> limit, ~11 dB
//   Bidet    clean tube opto, CL 1B family.  2:1, 1-2 dB  ->  10:1, ~11 dB
//   Snake    Distressor family.              2:1, 1-2 dB  ->  Nuke, ~18 dB
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
        { "Plunger", "FET. Fast and aggressive, with bite." },
        { "Float",   "Tube opto. Slow, thick and smooth." },
        { "Bidet",   "Clean opto. Glossy, modern pop." },
        { "Snake",   "Firm control up to all-out grit." } };
    return m[std::min (kNumModels - 1, std::max (0, i))];
}

// Threshold (dBFS) at the bottom and top of the fader, per model. Set by
// `tools/render tune` so the gain reduction on a reference-level vocal hits the
// targets in the header comment.
struct Tuning { float thrLo, thrHi; };
inline std::array<Tuning, kNumModels>& tuning()
{
    static std::array<Tuning, kNumModels> t { { { -8.3f, -25.7f }, { -9.8f, -21.6f }, { -11.8f, -20.5f }, { -8.4f, -25.7f } } };
    return t;
}

// Everything one fader position means for one model.
struct Curve
{
    float thrDb = 0, slope = 0, kneeDb = 6;       // slope = 1 - 1/ratio
    float attMs = 1, relMs = 100, relSlowMs = 100; // release slows after sustained compression
    float memory = 0, memAttMs = 400, memRelMs = 2000; // opto "memory": share of GR that lingers
    float satOdd = 0, satEven = 0, satGr = 0;      // harmonic colour (and how much more under GR)
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
        case 0: // Plunger: 4 -> 8 -> 12 -> 20 -> all buttons in
            c.slope = pts<5> ({ P { 0.0f, slopeOf (4) }, P { 0.33f, slopeOf (8) }, P { 0.55f, slopeOf (12) },
                                P { 0.78f, slopeOf (20) }, P { 1.0f, slopeOf (40) } }, a);
            c.kneeDb = lerp (7.0f, 3.0f, a);
            c.attMs = expLerp (0.8f, 0.05f, a);
            c.relMs = expLerp (220.0f, 55.0f, a);
            c.relSlowMs = expLerp (600.0f, 160.0f, a);
            c.satOdd = lerp (0.30f, 0.75f, a) + 0.9f * smoothstep (0.78f, 1.0f, a); // all-buttons grit
            c.satEven = lerp (0.05f, 0.14f, a);
            c.satGr = lerp (0.3f, 1.0f, a);
            break;
        case 1: // Float: compress (~3:1) -> limit
            c.slope = lerp (slopeOf (3), slopeOf (14), smoothstep (0.0f, 1.0f, a));
            c.kneeDb = lerp (14.0f, 8.0f, a);
            c.attMs = 4.0f;        // lands near 10-15 ms to full grab on real signals
            c.relMs = 70.0f;       // first half of the release
            c.relSlowMs = 70.0f;
            c.memory = 0.42f;      // ...the rest takes seconds, longer the harder it worked
            c.memAttMs = 600.0f;
            c.memRelMs = lerp (1500.0f, 4000.0f, a);
            c.satOdd = lerp (0.20f, 0.50f, a);
            c.satEven = lerp (0.05f, 0.14f, a);
            c.satGr = 0.2f;
            break;
        case 2: // Bidet: 2:1 -> 10:1, the cleanest of the four
            c.slope = pts<3> ({ P { 0.0f, slopeOf (2) }, P { 0.5f, slopeOf (4) }, P { 1.0f, slopeOf (10) } }, a);
            c.kneeDb = lerp (12.0f, 6.0f, a);
            c.attMs = expLerp (30.0f, 2.0f, a);
            c.relMs = expLerp (380.0f, 90.0f, a);
            c.relSlowMs = expLerp (900.0f, 300.0f, a);
            c.memory = 0.25f;
            c.memAttMs = 500.0f;
            c.memRelMs = 1200.0f;
            c.satOdd = lerp (0.08f, 0.18f, a);
            c.satEven = lerp (0.03f, 0.08f, a);
            c.satGr = 0.1f;
            break;
        case 3: // Snake: 2 -> 3 -> 4 -> 6 -> 10 -> 20 -> Nuke
            c.slope = pts<7> ({ P { 0.0f, slopeOf (2) }, P { 0.17f, slopeOf (3) }, P { 0.33f, slopeOf (4) },
                                P { 0.5f, slopeOf (6) }, P { 0.67f, slopeOf (10) }, P { 0.84f, slopeOf (20) },
                                P { 1.0f, slopeOf (100) } }, a);
            c.kneeDb = lerp (24.0f, 2.0f, a); // very soft at 2:1, brick wall at Nuke
            c.attMs = expLerp (8.0f, 0.25f, a);
            c.relMs = expLerp (240.0f, 60.0f, a);
            c.relSlowMs = expLerp (500.0f, 140.0f, a);
            c.satOdd = lerp (0.12f, 0.40f, a) + 1.1f * smoothstep (0.82f, 1.0f, a); // odd-order drive at the very top
            c.satEven = lerp (0.02f, 0.06f, a) + 0.26f * smoothstep (0.6f, 0.9f, a); // even-order warmth over the upper third
            c.satGr = lerp (0.2f, 0.8f, a);
            break;
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
        reset();
    }

    void reset()
    {
        env = mem = 0.0f;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
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

        // --- detector + static curve (soft knee)
        const float levelDb = gainToDb (std::max (std::abs (l), std::abs (r)));
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
        mem += (env - mem) * (env > mem ? aMemAtt : aMemRel);
        const float gr = std::max (env, cv.memory * mem);
        grNow = gr;

        const float g = dbToGain (-gr);
        // Colour stage sees the signal with half the gain reduction undone, so
        // heavy compression still drives it (as it does the real gain cells).
        const float k = 1.0f / std::sqrt (g);
        const float odd = cv.satOdd + cv.satGr * std::min (1.0f, gr * 0.05f);
        makeDb += (makeTarget - makeDb) * cMake;
        const float out = dbToGain (makeDb);

        float wl = shape (l * g * k, odd) / k;
        float wr = shape (r * g * k, odd) / k;
        wl = dc (wl, 0) * out;
        wr = dc (wr, 1) * out;
        l += (wl - l) * mix;
        r += (wr - r) * mix;
    }

private:
    float coef (float ms) const { return 1.0f - std::exp (-1000.0f / (std::max (0.01f, ms) * fs)); }
    inline float shape (float x, float odd) const
    {
        const float v = x + cv.satEven * x * x;
        return odd > 1.0e-3f ? std::tanh (odd * v) / odd : v;
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
    float aAtt = 1, aRel = 1, aRelSlow = 1, aMemAtt = 1, aMemRel = 1, cMix = 1, cMake = 1;
    float env = 0, mem = 0, mix = 1, makeDb = 0, makeTarget = 0, grNow = 0;
    float dcX[2] {}, dcY[2] {}, dcR = 0.999f;
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
        blockLen = std::max (1, (int) (0.1f * fs));
        cGain = 1.0f - std::exp (-1.0f / (0.15f * fs));
        cFast = 1.0f - std::exp (-1.0f / (0.02f * fs));
        cMeter = 1.0f - std::exp (-1.0f / (0.3f * fs));
        s1.prepare (sampleRate);
        s2.prepare (sampleRate);
        reset();
    }

    void reset()
    {
        s1.reset(); s2.reset();
        blockSum = 0.0; blockCount = 0;
        normDb = learned || activeBlocks >= 3 ? normTarget() : 0.0f;
        preDb = normDb + p.inputDb;
        postDb = p.outputDb - p.inputDb;
        meterMs = 0.0f;
    }

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

            s1.process (l, r);
            g1 = std::max (g1, s1.gainReductionDb());
            s2.process (l, r);
            g2 = std::max (g2, s2.gainReductionDb());

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
    float fs = 48000.0f;
    int blockLen = 4800, blockCount = 0, activeBlocks = 0;
    double blockSum = 0.0, sumMs = 0.0;
    bool learned = false;
    float learnedDb = 0.0f;
    float normDb = 0, preDb = 0, postDb = 0, cGain = 1, cFast = 1, cMeter = 1, meterMs = 0;
    float gr1 = 0, gr2 = 0;
};
} // namespace tvox
