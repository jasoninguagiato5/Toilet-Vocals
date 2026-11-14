// Offline harness for the Toilet Vocals engine (no JUCE needed).
//   render stress                              random-parameter stability test
//   render measure                             gain reduction / loudness / distortion per model and fader position
//   render tune                                solve the per-model thresholds for the gain-reduction targets
//   render calib                               print Source/dsp/MakeupTables.h (auto makeup gain)
//   render diff                                how different the models sound from each other
//   render eq                                  frequency response of the low cut, Low and High sections
//   render deess                               what the de-esser does to esses and to everything else
//   render vocal <out.wav>                     the synthetic test vocal used for tuning
//   render file <in.wav> <out.wav> [key=value ...]
//       keys: in out m1 a1 on1 m2 a2 on2 auto makeup     (m = model 0..3, a = fader 0..1)
//             lowcut (Hz) lowmode (0..2) low (-1..1) highmodel (0..3) high (-1..1) deess (0..1) deesshigh (0/1)
#include "../Source/dsp/ToiletVocalsEngine.h"
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

using Buf = std::vector<float>;

static void writeWav (const char* path, const Buf& L, const Buf& R, int fs)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) { std::perror (path); std::exit (1); }
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 2 * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (3); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 8); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { std::fwrite (&L[i], 4, 1, f); std::fwrite (&R[i], 4, 1, f); }
    std::fclose (f);
}

// minimal reader: 16/24/32-bit PCM or 32-bit float, mono or stereo
static bool readWav (const char* path, Buf& L, Buf& R, int& fs)
{
    FILE* f = std::fopen (path, "rb");
    if (! f) return false;
    char id[4]; uint32_t sz; uint16_t fmt = 1, ch = 2, bits = 16; uint32_t rate = 48000;
    if (std::fread (id, 1, 4, f) != 4 || std::fread (&sz, 4, 1, f) != 1 || std::fread (id, 1, 4, f) != 4) { std::fclose (f); return false; }
    while (std::fread (id, 1, 4, f) == 4 && std::fread (&sz, 4, 1, f) == 1)
    {
        if (! std::memcmp (id, "fmt ", 4))
        {
            std::vector<uint8_t> b (sz);
            if (std::fread (b.data(), 1, sz, f) != sz || sz < 16) break;
            fmt = (uint16_t) (b[0] | b[1] << 8); ch = (uint16_t) (b[2] | b[3] << 8);
            rate = (uint32_t) (b[4] | b[5] << 8 | b[6] << 16 | b[7] << 24); bits = (uint16_t) (b[14] | b[15] << 8);
            if (fmt == 0xFFFE && sz >= 26) fmt = (uint16_t) (b[24] | b[25] << 8);
        }
        else if (! std::memcmp (id, "data", 4))
        {
            const int bps = bits / 8; if (bps == 0 || ch == 0) break;
            std::vector<uint8_t> b (sz);
            const size_t got = std::fread (b.data(), 1, sz, f);
            const size_t frames = got / (size_t) (bps * ch);
            L.resize (frames); R.resize (frames);
            for (size_t i = 0; i < frames; ++i)
                for (int c = 0; c < std::min<int> (ch, 2); ++c)
                {
                    const uint8_t* q = &b[(i * ch + (size_t) c) * (size_t) bps];
                    float v = 0;
                    if (fmt == 3 && bits == 32) std::memcpy (&v, q, 4);
                    else if (bits == 16) v = (float) (int16_t) (q[0] | q[1] << 8) / 32768.0f;
                    else if (bits == 24) v = (float) ((int32_t) ((uint32_t) q[0] << 8 | (uint32_t) q[1] << 16 | (uint32_t) q[2] << 24) >> 8) / 8388608.0f;
                    else if (bits == 32) v = (float) (int32_t) ((uint32_t) q[0] | (uint32_t) q[1] << 8 | (uint32_t) q[2] << 16 | (uint32_t) q[3] << 24) / 2147483648.0f;
                    (c == 0 ? L : R)[i] = v;
                }
            if (ch == 1) R = L;
            fs = (int) rate; std::fclose (f); return true;
        }
        else std::fseek (f, (long) (sz + (sz & 1)), SEEK_CUR);
    }
    std::fclose (f); return false;
}

// ---------------------------------------------------------------- measuring
// Gated RMS in dB: 100 ms blocks, ignoring silence and anything 20 dB under the rest.
static float loudness (const Buf& x, int fs)
{
    const int bl = fs / 10; std::vector<double> ms;
    for (size_t i = 0; i + (size_t) bl <= x.size(); i += (size_t) bl)
    {
        double s = 0; for (int j = 0; j < bl; ++j) s += (double) x[i + (size_t) j] * x[i + (size_t) j];
        ms.push_back (s / bl);
    }
    double sum = 0; int n = 0;
    for (double m : ms) if (m > 1e-5) { sum += m; ++n; }
    if (n == 0) return -120.0f;
    const double gate = sum / n * 0.01; sum = 0; n = 0;
    for (double m : ms) if (m > gate && m > 1e-5) { sum += m; ++n; }
    return (float) (10.0 * std::log10 (sum / std::max (1, n) + 1e-12));
}
static float peakDb (const Buf& x) { float p = 0; for (float v : x) p = std::max (p, std::abs (v)); return tvox::gainToDb (p); }

// Synthetic "vocal": pitched pulse train through three formants, syllable and
// phrase dynamics, breaths of silence and sibilant bursts. Normalised to the
// reference level. Deterministic.
// `essBoost` > 1 makes the sibilants as loud as real ones get after compression (for the de-esser test);
// the default keeps the signal the compressor calibration was done on.
static Buf makeVocal (int fs, float seconds, float essBoost = 1.0f)
{
    std::mt19937 rng (1234);
    auto uni = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
    const size_t n = (size_t) (seconds * (float) fs);
    Buf x (n, 0.0f);
    struct BP { float b0, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                void set (float f, float q, float fs_) { const float w = 6.2831853f * f / fs_, al = std::sin (w) / (2 * q), a0 = 1 + al;
                    b0 = al / a0; a1 = -2 * std::cos (w) / a0; a2 = (1 - al) / a0; }
                float run (float in) { const float y = b0 * (in - x2) - a1 * y1 - a2 * y2; x2 = x1; x1 = in; y2 = y1; y1 = y; return y; } };
    BP f1, f2, f3, sib; sib.set (6500.0f, 1.2f, (float) fs);
    size_t i = 0; float phase = 0.0f, vib = 0.0f, g1 = 0.0f, g2 = 0.0f; // g1/g2: glottal low-pass (-12 dB/oct tilt)
    const float gc = 1.0f - std::exp (-6.2831853f * 500.0f / (float) fs);
    while (i < n)
    {
        // one phrase: several syllables at a phrase level, then a gap
        const float phraseDb = uni (-3.0f, 3.0f);
        const int syl = 3 + (int) uni (0, 6);
        for (int s = 0; s < syl && i < n; ++s)
        {
            const float f0 = uni (150.0f, 330.0f), lvl = tvox::dbToGain (phraseDb + uni (-2.5f, 2.5f));
            const size_t len = (size_t) (uni (0.14f, 0.45f) * (float) fs), att = (size_t) (uni (0.008f, 0.03f) * (float) fs);
            f1.set (uni (400, 800), 6, (float) fs); f2.set (uni (1000, 2000), 8, (float) fs); f3.set (uni (2500, 3200), 8, (float) fs);
            const bool ess = uni (0, 1) < 0.25f; const size_t essLen = (size_t) (uni (0.05f, 0.1f) * (float) fs);
            for (size_t k = 0; k < len && i < n; ++k, ++i)
            {
                vib += 6.2831853f * 5.5f / (float) fs;
                phase += f0 * (1.0f + 0.012f * std::sin (vib)) / (float) fs;
                float src = 0.0f; if (phase >= 1.0f) { phase -= 1.0f; src = 1.0f; }
                g1 += (src - g1) * gc; g2 += (g1 - g2) * gc; src = g2 * 40.0f + uni (-0.004f, 0.004f);
                float v = 9.0f * f1.run (src) + 5.0f * f2.run (src) + 3.0f * f3.run (src);
                const float t = (float) k / (float) len;
                const float e = std::min (1.0f, (float) k / (float) att) * std::pow (1.0f - t, 0.6f) * (0.7f + 0.3f * std::sin (3.14159f * t));
                v *= e * lvl;
                if (ess && k < essLen) v = v * 0.2f / essBoost + 0.5f * essBoost * lvl * sib.run (uni (-1, 1)) * std::sin (3.14159f * (float) k / (float) essLen);
                x[i] = v;
            }
            i += (size_t) (uni (0.0f, 0.06f) * (float) fs);
        }
        i += (size_t) (uni (0.3f, 0.9f) * (float) fs);
    }
    const float g = tvox::dbToGain (tvox::kRefRmsDb - loudness (x, fs));
    for (auto& v : x) v *= g;
    return x;
}

struct Run { Buf out; float grP95 = 0, grMean = 0; };
// Runs mono audio through the engine; gain-reduction stats are for stage 1 while the signal is active.
static Run run (const tvox::Params& p, const Buf& in, int fs, bool learn = false)
{
    tvox::Engine e; e.prepare (fs);
    if (! learn) e.restoreLearn (true, 0.0f);
    e.setParams (p); e.reset();
    Run r; r.out = in; Buf R = in; std::vector<float> grs;
    const int block = 64;
    for (size_t i = 0; i < in.size(); i += block)
    {
        const int n = (int) std::min<size_t> (block, in.size() - i);
        double s = 0; for (int j = 0; j < n; ++j) s += (double) in[i + (size_t) j] * in[i + (size_t) j];
        e.setParams (p);
        e.process (r.out.data() + i, R.data() + i, n);
        if (s / n > 1e-4) grs.push_back (e.gr1Db());
    }
    if (! grs.empty())
    {
        std::sort (grs.begin(), grs.end());
        r.grP95 = grs[(size_t) ((float) (grs.size() - 1) * 0.95f)];
        double m = 0; for (float g : grs) m += g; r.grMean = (float) (m / (double) grs.size());
    }
    return r;
}

static float harmonicDb (const Buf& x, int fs, float f, size_t from)
{
    double re = 0, im = 0; const size_t n = x.size() - from;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (6.283185307 * (double) i / (double) n), ph = 6.283185307 * f * (double) i / fs;
        re += w * x[from + i] * std::cos (ph); im += w * x[from + i] * std::sin (ph);
    }
    return (float) (20.0 * std::log10 (4.0 * std::sqrt (re * re + im * im) / (double) n + 1e-12));
}

static tvox::Params solo (int model, float amount, float inputDb = 0.0f, bool makeup = true)
{
    tvox::Params p; p.autoLevel = false; p.autoMakeup = makeup; p.inputDb = inputDb;
    p.comp1 = { model, amount, true }; p.comp2.on = false;
    return p;
}

static const float kTargetLo[4] = { 1.5f, 1.5f, 1.5f, 1.5f };
static const float kTargetHi[4] = { 18.0f, 11.0f, 11.0f, 18.0f };

static int cmdTune()
{
    const int fs = 48000; const Buf v = makeVocal (fs, 14.0f);
    std::printf ("static std::array<Tuning, kNumModels> t { {");
    for (int m = 0; m < 4; ++m)
    {
        float res[2];
        for (int end = 0; end < 2; ++end)
        {
            float lo = -70.0f, hi = 0.0f; // lower threshold = more gain reduction
            for (int it = 0; it < 22; ++it)
            {
                const float mid = 0.5f * (lo + hi);
                (end ? tvox::tuning()[(size_t) m].thrHi : tvox::tuning()[(size_t) m].thrLo) = mid;
                const float gr = run (solo (m, (float) end, 0.0f, false), v, fs).grP95;
                if (gr > (end ? kTargetHi[m] : kTargetLo[m])) lo = mid; else hi = mid;
            }
            res[end] = 0.5f * (lo + hi);
        }
        std::printf (" { %.1ff, %.1ff }%s", res[0], res[1], m < 3 ? "," : "");
    }
    std::printf (" } };\n");
    return 0;
}

static int cmdCalib()
{
    const int fs = 48000; const Buf v = makeVocal (fs, 14.0f); const float ref = loudness (v, fs);
    std::printf ("// GENERATED by `tools/render calib` -- do not edit by hand.\n"
                 "// Auto makeup gain in dB per model: [input offset -12,-6,0,+6,+12 dB][amount 0..1 in steps of 1/8].\n"
                 "#pragma once\nnamespace tvox {\nconstexpr int kMakeupOffsets = 5;\nconstexpr int kMakeupAmounts = 9;\n"
                 "inline constexpr float kMakeupDb[4][kMakeupOffsets][kMakeupAmounts] = {\n");
    for (int m = 0; m < 4; ++m)
    {
        std::printf ("  { // %s\n", tvox::modelInfo (m).name);
        for (int o = 0; o < 5; ++o)
        {
            std::printf ("    {");
            for (int a = 0; a < 9; ++a)
                std::printf (" %.2ff%s", ref - loudness (run (solo (m, (float) a / 8.0f, -12.0f + 6.0f * (float) o, false), v, fs).out, fs), a < 8 ? "," : "");
            std::printf (" },\n");
        }
        std::printf ("  },\n");
    }
    std::printf ("};\n}\n");
    return 0;
}

static int cmdMeasure()
{
    const int fs = 48000; const Buf v = makeVocal (fs, 14.0f); const float ref = loudness (v, fs);
    Buf sine ((size_t) fs * 2); for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.251f * std::sin (6.2831853f * 1000.0f * (float) i / (float) fs);
    std::printf ("test vocal: loudness %.1f dB, peak %.1f dB\n", ref, peakDb (v));
    for (int rate : { 44100, 48000 })
    {   // oversampler: latency and passband with both compressors out
        tvox::Params p; p.autoLevel = false; p.comp1.on = p.comp2.on = false;
        Buf imp ((size_t) rate / 10, 0.0f); imp[100] = 1.0f;
        const Buf o = run (p, imp, rate).out; size_t pk = 0; for (size_t i = 0; i < o.size(); ++i) if (std::abs (o[i]) > std::abs (o[pk])) pk = i;
        tvox::Engine e; e.prepare (rate);
        std::printf ("oversampler at %d: impulse lands %d samples late (reported %d);", rate, (int) pk - 100, e.latencySamples());
        for (float f : { 1000.0f, 10000.0f, 16000.0f, 18000.0f, 20000.0f })
        {
            Buf t ((size_t) rate / 2); for (size_t i = 0; i < t.size(); ++i) t[i] = 0.25f * std::sin (6.2831853f * f * (float) i / (float) rate);
            std::printf (" %g Hz %+.2f dB", f, harmonicDb (run (p, t, rate).out, rate, f, (size_t) rate / 4) - harmonicDb (t, rate, f, (size_t) rate / 4));
        }
        std::printf ("\n");
    }
    for (int m = 0; m < 4; ++m)
    {
        std::printf ("\n%-8s fader   GR p95  GR mean   loudness  peak   crest   1k@-12: GR    H2     H3\n", tvox::modelInfo (m).name);
        for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto r = run (solo (m, a), v, fs);
            const auto s = run (solo (m, a), sine, fs);
            const float h1 = harmonicDb (s.out, fs, 1000.0f, (size_t) fs);
            std::printf ("         %4.2f   %6.1f  %6.1f    %+6.2f   %+6.1f  %5.1f         %5.1f  %5.1f  %5.1f\n", a, r.grP95, r.grMean,
                         loudness (r.out, fs) - ref, peakDb (r.out), peakDb (r.out) - loudness (r.out, fs), s.grP95,
                         harmonicDb (s.out, fs, 2000.0f, (size_t) fs) - h1, harmonicDb (s.out, fs, 3000.0f, (size_t) fs) - h1);
        }
    }
    // both stages, input fader, auto level
    std::printf ("\nchains (loudness change vs. input):\n");
    for (float in : { -12.0f, 0.0f, 12.0f })
        for (float a : { 0.0f, 0.5f, 1.0f })
        {
            tvox::Params p; p.autoLevel = false; p.inputDb = in; p.comp1 = { 0, a, true }; p.comp2 = { 1, a, true };
            std::printf ("  #1 76>LA #2A  input %+3.0f  faders %.1f : %+5.2f dB\n", in, a, loudness (run (p, v, fs).out, fs) - ref);
            p.comp1.model = 3; p.comp2.model = 2;
            std::printf ("  IBSsor>CL #1b    input %+3.0f  faders %.1f : %+5.2f dB\n", in, a, loudness (run (p, v, fs).out, fs) - ref);
        }
    std::printf ("\nauto level (track gain -> loudness into the compressors once learned):\n");
    for (float g : { -24.0f, -10.0f, 0.0f, 10.0f })
    {
        Buf q = v; for (auto& s : q) s *= tvox::dbToGain (g);
        tvox::Engine e; e.prepare (fs); tvox::Params p; p.comp1.on = p.comp2.on = false; e.setParams (p); e.reset();
        Buf L = q, R = q; e.process (L.data(), R.data(), (int) L.size());
        L = q; R = q; e.process (L.data(), R.data(), (int) L.size()); // second pass: level is locked
        std::printf ("  track at %+5.1f dB: learned=%d gain %+5.1f dB -> %.1f dB\n", ref + g, (int) e.isLearned(), e.normGainDb(), loudness (L, fs));
    }
    return 0;
}

static int cmdStress()
{
    std::mt19937 rng (99); auto uni = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
    int runs = 0;
    for (int fs : { 44100, 48000, 96000 })
    {
        const Buf v = makeVocal (fs, 4.0f);
        for (int t = 0; t < 40; ++t, ++runs)
        {
            tvox::Engine e; e.prepare (fs);
            Buf L = v, R = v;
            const int kind = t % 5; const float lvl = tvox::dbToGain (uni (-50.0f, 24.0f));
            for (size_t i = 0; i < L.size(); ++i)
            {
                float s = L[i] * lvl;
                if (kind == 1) s = uni (-1, 1) * lvl;                          // noise
                if (kind == 2) s = (i % 9000 == 0) ? 4.0f : 0.0f;              // clicks over silence
                if (kind == 3) s = (i / 3000) % 2 ? 1.0f : -1.0f;              // full-scale square
                if (kind == 4) s = 0.0f;                                       // silence
                L[i] = s; R[i] = kind == 1 ? uni (-1, 1) * lvl : s;
            }
            size_t i = 0; float peak = 0;
            while (i < L.size())
            {
                const int n = (int) std::min<size_t> ((size_t) uni (1, 700), L.size() - i);
                tvox::Params p; p.inputDb = uni (-12, 12); p.outputDb = uni (-24, 12); p.autoLevel = uni (0, 1) < 0.7f;
                p.comp1 = { (int) uni (0, 3.99f), uni (0, 1), uni (0, 1) < 0.8f };
                p.comp2 = { (int) uni (0, 3.99f), uni (0, 1), uni (0, 1) < 0.8f };
                p.lowCutHz = uni (10, 300); p.lowMode = (int) uni (0, 2.99f); p.low = uni (-1.2f, 1.2f);
                p.highModel = (int) uni (0, 3.99f); p.high = uni (-1.2f, 1.2f);
                p.deEss = uni (-0.2f, 1.2f); p.deEssHigh = uni (0, 1) < 0.5f;
                if (uni (0, 1) < 0.02f) e.relearn();
                e.setParams (p); e.process (L.data() + i, R.data() + i, n);
                for (int j = 0; j < n; ++j)
                {
                    const float a = std::max (std::abs (L[i + (size_t) j]), std::abs (R[i + (size_t) j]));
                    if (! std::isfinite (a)) { std::printf ("FAIL: non-finite output (fs %d run %d)\n", fs, t); return 1; }
                    peak = std::max (peak, a);
                }
                i += (size_t) n;
            }
            if (peak > 4000.0f) { std::printf ("FAIL: runaway level %.1f (fs %d run %d kind %d)\n", peak, fs, t, kind); return 1; }
        }
    }
    std::printf ("stress: %d runs OK\n", runs);
    return 0;
}

// How different two models sound on the test vocal at the same fader position:
// the level of (A - B) relative to A. 0 dB = unrelated, very negative = nearly identical.
static int cmdDiff()
{
    const int fs = 48000; const Buf v = makeVocal (fs, 14.0f);
    std::printf ("difference between models at the same fader position (dB relative to the signal; closer to 0 = more different)\n       ");
    for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b) std::printf (" %d vs %d ", a, b);
    std::printf ("\n");
    for (float amt : { 0.25f, 0.5f, 0.75f, 1.0f })
    {
        Buf o[4]; for (int m = 0; m < 4; ++m) o[m] = run (solo (m, amt), v, fs).out;
        std::printf ("  %.2f ", amt);
        for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b)
        {
            Buf d = o[a]; for (size_t i = 0; i < d.size(); ++i) d[i] -= o[b][i];
            std::printf (" %6.1f ", loudness (d, fs) - loudness (o[a], fs));
        }
        std::printf ("\n");
    }
    return 0;
}

// Magnitude response of the engine with both compressors out, from its impulse response.
static std::vector<float> responseDb (tvox::Params p, int fs, const std::vector<float>& freqs)
{
    p.autoLevel = false; p.comp1.on = p.comp2.on = false;
    Buf imp ((size_t) fs / 2, 0.0f); imp[0] = 0.25f;
    const Buf h = run (p, imp, fs).out;
    std::vector<float> out;
    for (float f : freqs)
    {
        std::complex<double> acc = 0.0;
        for (size_t n = 0; n < h.size(); ++n) acc += (double) h[n] * std::polar (1.0, -6.283185307179586 * f * (double) n / fs);
        out.push_back ((float) (20.0 * std::log10 (std::abs (acc) / 0.25 + 1e-12)));
    }
    return out;
}

static int cmdEq()
{
    const std::vector<float> lf { 30, 50, 80, 110, 160, 230, 320, 450, 700, 1000, 2000 };
    const std::vector<float> hf { 500, 1000, 2000, 3000, 4000, 5000, 6500, 8000, 10000, 12000, 14000, 16000, 18000, 20000 };
    auto row = [] (const char* label, const std::vector<float>& v) { std::printf ("  %-22s", label); for (float d : v) std::printf (" %6.1f", d); std::printf ("\n"); };
    auto head = [] (const std::vector<float>& f) { std::printf ("  %-22s", "Hz"); for (float v : f) std::printf (" %6.0f", v); std::printf ("\n"); };
    for (int fs : { 44100, 48000, 96000 })
    {
        std::printf ("\n===== %d Hz =====\nlow cut\n", fs); head (lf);
        for (float hz : { 20.0f, 60.0f, 100.0f, 160.0f, 250.0f })
        { tvox::Params p; p.lowCutHz = hz; char b[40]; std::snprintf (b, sizeof b, "%g Hz", hz); row (b, responseDb (p, fs, lf)); }
        std::printf ("Low fader\n"); head (lf);
        for (int m = 0; m < tvox::kNumLowModes; ++m)
            for (float x : { 1.0f, 0.5f, -0.5f, -1.0f })
            { tvox::Params p; p.lowMode = m; p.low = x; char b[40]; std::snprintf (b, sizeof b, "%s %+.1f", tvox::lowModeInfo (m).name, x); row (b, responseDb (p, fs, lf)); }
        std::printf ("High fader\n"); head (hf);
        for (int m = 0; m < tvox::kNumHighModels; ++m)
            for (float x : { 1.0f, 0.5f, -0.5f, -1.0f })
            { tvox::Params p; p.highModel = m; p.high = x; char b[40]; std::snprintf (b, sizeof b, "%s %+.1f", tvox::highModelInfo (m).name, x); row (b, responseDb (p, fs, hf)); }

        // How closely the digital filters follow the analogue curves they stand for, fader at +1.
        std::printf ("High models vs. their analogue prototype at full boost (error in dB)\n"); head (hf);
        using C = std::complex<double>;
        auto bellA = [] (double f, double f0, double q, double db) { const double A = std::pow (10.0, db / 40.0); const C s (0.0, f / f0); return std::abs ((s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0)); };
        auto shelf2A = [] (double f, double f0, double q, double db) { const double A = std::pow (10.0, db / 40.0); const C s (0.0, f / f0); return std::abs (A * (A * s * s + s * (std::sqrt (A) / q) + 1.0) / (s * s + s * (std::sqrt (A) / q) + A)); };
        auto shelf1A = [] (double f, double fp, double g) { return std::abs (C (1.0, f * g / fp) / C (1.0, f / fp)); };
        for (int m = 0; m < tvox::kNumHighModels; ++m)
        {
            tvox::Params p; p.highModel = m; p.high = 1.0f;
            auto meas = responseDb (p, fs, hf);
            for (size_t i = 0; i < hf.size(); ++i)
            {
                const double f = hf[i]; double t = 1.0;
                if (m == 0) t = bellA (f, 3800.0, 1.3, 10.0) * shelf2A (f, 8000.0, 0.7, 4.0);
                if (m == 1) t = shelf1A (f, 5000.0 * std::sqrt (std::pow (10.0, 0.5)), std::pow (10.0, 0.5));
                if (m == 2) t = bellA (f, 12000.0, 0.7, 12.0);
                if (m == 3) t = shelf1A (f, 28000.0, std::pow (10.0, 0.6));
                meas[i] -= (float) (20.0 * std::log10 (t));
            }
            row (tvox::highModelInfo (m).name, meas);
        }
    }
    return 0;
}

// De-esser check on the test vocal: how much it takes off the esses, and how much it touches the rest.
static int cmdDeess()
{
    const int fs = 48000; const Buf v = makeVocal (fs, 14.0f, 5.0f);
    // mark each 10 ms frame of the input: 1 = ess (a third or more of its energy above 4 kHz), 0 = clearly not, -1 = ignore
    const int frame = fs / 100; std::vector<int> ess;
    {
        tvox::Svf hp; hp.setHighpass ((float) fs, 4000.0f, 0.7071f);
        for (size_t i = 0; i + (size_t) frame <= v.size(); i += (size_t) frame)
        {
            double eh = 0, ea = 0;
            for (int j = 0; j < frame; ++j) { const float x = v[i + (size_t) j], h = hp.tick (x, 0); eh += h * h; ea += x * x; }
            ess.push_back (ea < 1e-6 ? -1 : (eh > 0.33 * ea ? 1 : (eh < 0.03 * ea ? 0 : -1)));
        }
    }
    auto bandDb = [&] (const Buf& x, int want, bool highBand)
    {
        tvox::Svf hp; hp.setHighpass ((float) fs, 4000.0f, 0.7071f); double e = 0; size_t n = 0;
        for (size_t f = 0; f < ess.size(); ++f)
            for (int j = 0; j < frame; ++j)
            {
                const float s = x[f * (size_t) frame + (size_t) j], h = hp.tick (s, 0);
                if (ess[f] == want) { e += highBand ? h * h : (s - h) * (s - h); ++n; }
            }
        return (float) (10.0 * std::log10 (e / (double) std::max<size_t> (1, n) + 1e-12));
    };
    int nEss = 0; for (int e : ess) nEss += e == 1;
    std::printf ("test vocal: %d of %zu frames are esses\n", nEss, ess.size());
    std::printf ("%-28s  esses: highs   rest: highs  rest: lows   overall loudness\n", "");
    for (float high : { 0.0f, 1.0f })
        for (int range = 0; range < 2; ++range)
            for (float amt : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                tvox::Params p; p.autoLevel = false; p.comp1.amount = 0.4f; p.comp2.amount = 0.3f; p.highModel = 3; p.high = high;
                tvox::Params q = p; q.deEss = amt; q.deEssHigh = range == 1;
                Buf ref = run (p, v, fs).out, out = run (q, v, fs).out;
                const size_t lat = 39; // line the frames up with the input markers
                ref.erase (ref.begin(), ref.begin() + lat); out.erase (out.begin(), out.begin() + lat);
                ref.resize (v.size()); out.resize (v.size());
                char label[64]; std::snprintf (label, sizeof label, "Air %+.0f, %s range, amount %.1f", high * 10.0f, range ? "high" : "low", amt);
                std::printf ("%-28s   %+6.1f dB    %+6.1f dB   %+6.2f dB     %+6.2f dB\n", label,
                             bandDb (out, 1, true) - bandDb (ref, 1, true), bandDb (out, 0, true) - bandDb (ref, 0, true),
                             bandDb (out, 0, false) - bandDb (ref, 0, false), loudness (out, fs) - loudness (ref, fs));
            }
    return 0;
}

int main (int argc, char** argv)
{
    const std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "stress") return cmdStress();
    if (cmd == "measure") return cmdMeasure();
    if (cmd == "tune") return cmdTune();
    if (cmd == "calib") return cmdCalib();
    if (cmd == "diff") return cmdDiff();
    if (cmd == "eq") return cmdEq();
    if (cmd == "deess") return cmdDeess();
    if (cmd == "vocal" && argc > 2) { const Buf v = makeVocal (48000, 20.0f); writeWav (argv[2], v, v, 48000); return 0; }
    if (cmd == "file" && argc > 3)
    {
        Buf L, R; int fs = 48000;
        if (! readWav (argv[2], L, R, fs)) { std::fprintf (stderr, "can't read %s\n", argv[2]); return 1; }
        tvox::Params p; bool learn = true;
        for (int i = 4; i < argc; ++i)
        {
            const std::string kv = argv[i]; const auto eq = kv.find ('=');
            if (eq == std::string::npos) { std::fprintf (stderr, "expected key=value: %s\n", argv[i]); return 2; }
            const std::string k = kv.substr (0, eq); const float val = std::stof (kv.substr (eq + 1));
            if (k == "in") p.inputDb = val; else if (k == "out") p.outputDb = val;
            else if (k == "m1") p.comp1.model = (int) val; else if (k == "a1") p.comp1.amount = val; else if (k == "on1") p.comp1.on = val > 0.5f;
            else if (k == "m2") p.comp2.model = (int) val; else if (k == "a2") p.comp2.amount = val; else if (k == "on2") p.comp2.on = val > 0.5f;
            else if (k == "auto") { p.autoLevel = val > 0.5f; learn = p.autoLevel; }
            else if (k == "makeup") p.autoMakeup = val > 0.5f;
            else if (k == "lowcut") p.lowCutHz = val; else if (k == "lowmode") p.lowMode = (int) val; else if (k == "low") p.low = val;
            else if (k == "highmodel") p.highModel = (int) val; else if (k == "high") p.high = val;
            else if (k == "deess") p.deEss = val; else if (k == "deesshigh") p.deEssHigh = val > 0.5f;
            else { std::fprintf (stderr, "unknown key %s\n", k.c_str()); return 2; }
        }
        tvox::Engine e; e.prepare (fs); if (! learn) e.restoreLearn (true, 0.0f);
        e.setParams (p); e.reset();
        for (size_t i = 0; i < L.size(); i += 64)
        {
            const int n = (int) std::min<size_t> (64, L.size() - i);
            e.process (L.data() + i, R.data() + i, n);
        }
        writeWav (argv[3], L, R, fs);
        return 0;
    }
    std::fprintf (stderr, "usage: render stress | measure | tune | calib | diff | eq | deess | vocal <out.wav> | file <in.wav> <out.wav> [key=value ...]\n");
    return 2;
}
