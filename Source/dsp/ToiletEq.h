// EQ sections for Toilet Vocals (no JUCE dependency).
//
//   Low cut   18 dB/oct high-pass, 20 Hz (off) to 250 Hz.
//   Low       one fader, three regions. Up boosts with a wide curve, down cuts with a narrow one.
//               Sub    shelf around 110 Hz
//               Lower  bell at 230 Hz
//               Upper  bell at 450 Hz
//   High      one fader, four models built from the published behaviour of classic units:
//               Bite   SSL 4000 E "black knob": parametric high-mid bell (Q 0.8-1.3, wider at
//                      low gain) at 3.8 kHz, with some of its 8 kHz high shelf.
//               Sheen  Neve 1073: its single high shelf, +/-16 dB "at 12 kHz", which is a very
//                      gradual first-order slope that starts lifting from the upper mids.
//               Silk   Pultec EQP-1A: broad resonant peak boost at 12 kHz; the cut direction is
//                      its separate high shelf attenuator.
//               Air    Maag EQ4 Air Band at 40 kHz: a very wide boost-only shelf whose slope
//                      reaches far down into the audible band. (The cut direction mirrors the
//                      boost; the hardware has no cut.)
//
// All filters are trapezoidal state-variable / one-pole forms: stable at low frequencies in
// single precision and safe to retune while audio runs.
#pragma once
#include <algorithm>
#include <cmath>

namespace tvox
{
constexpr int kNumLowModes = 3, kNumHighModels = 4;
constexpr float kLowCutOffHz = 20.0f, kLowCutMaxHz = 250.0f;

struct EqInfo { const char* name; const char* blurb; };
inline const EqInfo& lowModeInfo (int i)
{
    static const EqInfo m[kNumLowModes] = {
        { "Sub",   "Around 110 Hz. Up for chest and weight, down for boom." },
        { "Lower", "Around 230 Hz. Up for warmth, down for mud." },
        { "Upper", "Around 450 Hz. Up for thickness, down for boxiness." } };
    return m[std::min (kNumLowModes - 1, std::max (0, i))];
}
inline const EqInfo& highModelInfo (int i)
{
    static const EqInfo m[kNumHighModels] = {
        { "Bite",  "Console presence around 4 kHz. Pushes the vocal forward." },
        { "Sheen", "Gradual 12 kHz shelf. Thick and glossy." },
        { "Silk",  "Broad tube-EQ lift at 12 kHz. Smooth, hard to make harsh." },
        { "Air",   "Very wide 40 kHz shelf. Open, modern top." } };
    return m[std::min (kNumHighModels - 1, std::max (0, i))];
}

// Fader position (-1..+1) to dB: finer near the centre.
inline float eqLaw (float x, float maxBoostDb, float maxCutDb)
{
    const float a = std::pow (std::abs (x), 1.4f);
    return x >= 0.0f ? a * maxBoostDb : -a * maxCutDb;
}

// ------------------------------------------------- second-order sections
class Svf
{
public:
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }

    void setBell (float fs, float hz, float q, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f);
        coeffs (tanf_ (hz, fs), 1.0f / (q * A));
        m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f;
    }
    void setLowShelf (float fs, float hz, float q, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f);
        coeffs (tanf_ (hz, fs) / std::sqrt (A), 1.0f / q);
        m0 = 1.0f; m1 = k * (A - 1.0f); m2 = A * A - 1.0f;
    }
    void setHighShelf (float fs, float hz, float q, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f);
        coeffs (tanf_ (hz, fs) * std::sqrt (A), 1.0f / q);
        m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A;
    }
    void setHighpass (float fs, float hz, float q)
    {
        coeffs (tanf_ (hz, fs), 1.0f / q);
        m0 = 1.0f; m1 = -k; m2 = -1.0f;
    }

    inline float tick (float v0, int ch)
    {
        const float v3 = v0 - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

private:
    static float tanf_ (float hz, float fs) { return std::tan (3.14159265f * std::min (hz, 0.45f * fs) / fs); }
    void coeffs (float g, float kk)
    {
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    float a1 = 1, a2 = 0, a3 = 0, k = 1, m0 = 1, m1 = 0, m2 = 0;
    float ic1[2] {}, ic2[2] {};
};

// -------------------------------------------------- first-order sections
// out = lowGain * lowpass + highGain * highpass around one pole.
class OnePole
{
public:
    void reset() { s[0] = s[1] = 0.0f; }

    // First-order high shelf: unity at the bottom, `gain` (linear) at the top, pole at poleHz.
    // Poles above 10 kHz are mapped so the curve is right across the audible band instead of
    // being squeezed against the top of the spectrum.
    void setHighShelf (float fs, float poleHz, float gain)
    {
        const float matchHz = std::min (poleHz, 10000.0f);
        const float g = (poleHz / matchHz) * std::tan (3.14159265f * std::min (matchHz, 0.45f * fs) / fs);
        G = g / (1.0f + g);
        lowGain = 1.0f; highGain = gain;
    }
    void setHighpass (float fs, float hz)
    {
        const float g = std::tan (3.14159265f * std::min (hz, 0.45f * fs) / fs);
        G = g / (1.0f + g);
        lowGain = 0.0f; highGain = 1.0f;
    }

    inline float tick (float x, int ch)
    {
        const float v = (x - s[ch]) * G;
        const float lp = v + s[ch];
        s[ch] = lp + v;
        return lowGain * lp + highGain * (x - lp);
    }

private:
    float G = 0, lowGain = 1, highGain = 1;
    float s[2] {};
};

// ---------------------------------------------------------------- low cut
class LowCut
{
public:
    void reset() { a.reset(); b.reset(); }
    void set (float fs, float hz)
    {
        on = hz > kLowCutOffHz + 0.5f;
        if (! on) { reset(); return; }
        a.setHighpass (fs, hz);          // 1st + 2nd order (Q 1) = 18 dB/oct Butterworth
        b.setHighpass (fs, hz, 1.0f);
    }
    inline void process (float& l, float& r)
    {
        if (! on) return;
        l = b.tick (a.tick (l, 0), 0);
        r = b.tick (a.tick (r, 1), 1);
    }

private:
    OnePole a; Svf b; bool on = false;
};

// -------------------------------------------------------------- low fader
class LowBand
{
public:
    void reset() { f.reset(); }
    // x: fader -1..+1. Positive = wide boost, negative = narrow cut.
    void set (float fs, int mode, float x)
    {
        on = std::abs (x) > 1.0e-4f;
        if (! on) { reset(); return; }
        const float db = eqLaw (x, 9.0f, 9.0f);
        const bool boost = x > 0.0f;
        switch (mode)
        {
            case 0:  f.setLowShelf (fs, boost ? 110.0f : 120.0f, 0.7f, db); break;
            default:
            case 1:  f.setBell (fs, 230.0f, boost ? 0.6f : 1.4f, db); break;
            case 2:  f.setBell (fs, 450.0f, boost ? 0.6f : 1.4f, db); break;
        }
    }
    inline void process (float& l, float& r)
    {
        if (! on) return;
        l = f.tick (l, 0); r = f.tick (r, 1);
    }

private:
    Svf f; bool on = false;
};

// ------------------------------------------------------------- high fader
class HighBand
{
public:
    void reset() { bell.reset(); shelf.reset(); pole.reset(); }
    void set (float fs, int model, float x)
    {
        on = std::abs (x) > 1.0e-4f;
        if (! on) { reset(); return; }
        const float ax = std::abs (x);
        useBell = useShelf = usePole = false;
        switch (model)
        {
            case 0: // Bite
            {
                const float db = eqLaw (x, 10.0f, 10.0f);
                bell.setBell (fs, 3800.0f, 0.8f + 0.5f * ax, db);
                shelf.setHighShelf (fs, 8000.0f, 0.7f, 0.4f * db);
                useBell = useShelf = true;
                break;
            }
            case 1: // Sheen: first-order shelf pivoting around 5 kHz; at full gain it is within
            {       // 3 dB of its plateau by 12-13 kHz.
                const float gain = std::pow (10.0f, eqLaw (x, 10.0f, 10.0f) / 20.0f);
                pole.setHighShelf (fs, 5000.0f * std::sqrt (gain), gain);
                usePole = true;
                break;
            }
            case 2: // Silk
                if (x > 0.0f) { bell.setBell (fs, 12000.0f, 0.7f, eqLaw (x, 12.0f, 0.0f)); useBell = true; }
                else
                {
                    const float gain = std::pow (10.0f, eqLaw (x, 0.0f, 10.0f) / 20.0f);
                    pole.setHighShelf (fs, 8000.0f * std::sqrt (gain), gain);
                    usePole = true;
                }
                break;
            default:
            case 3: // Air: pole well above the audible band, so what you hear is its long slope
            {
                const float gain = std::pow (10.0f, eqLaw (x, 12.0f, 8.0f) / 20.0f);
                pole.setHighShelf (fs, x > 0.0f ? 28000.0f : 28000.0f * gain, gain);
                usePole = true;
                break;
            }
        }
    }
    inline void process (float& l, float& r)
    {
        if (! on) return;
        if (useBell)  { l = bell.tick (l, 0);  r = bell.tick (r, 1); }
        if (useShelf) { l = shelf.tick (l, 0); r = shelf.tick (r, 1); }
        if (usePole)  { l = pole.tick (l, 0);  r = pole.tick (r, 1); }
    }

private:
    Svf bell, shelf; OnePole pole;
    bool on = false, useBell = false, useShelf = false, usePole = false;
};

// One fader with a mode switch: glides to new positions, and changes mode by
// easing to flat, switching, and easing back, so nothing clicks.
struct EqFader
{
    float amount = 0.0f; int mode = 0;       // what the filters are currently set to
    float targetAmount = 0.0f; int targetMode = 0;
    void snap() { amount = targetAmount; mode = targetMode; }
    // Returns true when the filters need retuning.
    bool advance (float coef)
    {
        const float goal = mode == targetMode ? targetAmount : 0.0f;
        if (mode != targetMode && std::abs (amount) < 0.004f) { amount = 0.0f; mode = targetMode; return true; }
        if (std::abs (goal - amount) < 1.0e-5f) return false;
        amount += (goal - amount) * coef;
        if (std::abs (goal - amount) < 1.0e-4f) amount = goal;
        return true;
    }
};
} // namespace tvox
