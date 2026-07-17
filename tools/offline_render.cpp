// ============================================================================
//  offline_render.cpp — cloud test harness for the Glitchwave 567 sim.
//  Generates guitar-like test signals, runs them through the circuit model at
//  4x oversampling (192 kHz), decimates to 48 kHz and writes 16-bit WAVs.
//  Not part of the plugin build — verification only.
// ============================================================================
#include "../src/dsp/Glitchwave567.h"
#include "../src/dsp/ModSystem.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>
#include <random>

static constexpr int   kFsOut = 48000;
static constexpr int   kOS    = 4;
static constexpr int   kFsOS  = kFsOut * kOS;

// ---------------------------------------------------------------- WAV writer
static bool writeWav16 (const std::string& path, const std::vector<float>& x, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (!f) return false;
    const uint32_t dataBytes = (uint32_t) x.size() * 2u;
    const uint32_t riffSize  = 36u + dataBytes;
    const uint16_t ch = 1, bits = 16, blockAlign = 2;
    const uint32_t byteRate = (uint32_t) fs * blockAlign;
    const uint16_t fmt = 1; const uint32_t fmtSize = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riffSize, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f); std::fwrite ("fmt ", 1, 4, f);
    std::fwrite (&fmtSize, 4, 1, f); std::fwrite (&fmt, 2, 1, f);
    std::fwrite (&ch, 2, 1, f);
    std::fwrite (&fs, 4, 1, f); std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&blockAlign, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (float v : x)
    {
        int s = (int) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
        int16_t s16 = (int16_t) s;
        std::fwrite (&s16, 2, 1, f);
    }
    std::fclose (f);
    return true;
}

// ------------------------------------------------- Karplus-Strong guitar pluck
struct Pluck
{
    std::vector<float> delay;
    size_t idx = 0;
    float damp = 0.996f;

    void trigger (float freq, int fs, unsigned seed)
    {
        size_t n = std::max<size_t> (2, (size_t) std::lround ((double) fs / freq));
        delay.assign (n, 0.0f);
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        // burst with a little lowpass so it's less "harpsichord"
        float lp = 0.0f;
        for (auto& v : delay) { lp += 0.55f * (d (rng) - lp); v = lp; }
        idx = 0;
    }
    float tick()
    {
        if (delay.size() < 2) return 0.0f;
        const size_t next = (idx + 1) % delay.size();
        const float out = delay[idx];
        delay[idx] = damp * 0.5f * (delay[idx] + delay[next]);
        idx = next;
        return out;
    }
};

// simple riff: note frequencies + start times (seconds)
struct NoteEvent { double t; float hz; };

static std::vector<float> makeRiff (double lengthSec)
{
    // A minor-ish phrase spanning the 567's tracking range
    const NoteEvent riff[] = {
        { 0.00, 110.00f }, { 0.55, 220.00f }, { 1.10, 261.63f },
        { 1.65, 329.63f }, { 2.20, 440.00f }, { 2.75, 392.00f },
        { 3.30, 329.63f }, { 3.85, 220.00f }, { 4.40, 440.00f },
        { 4.95, 523.25f }, { 5.50, 329.63f }, { 6.05, 110.00f },
    };
    std::vector<float> out ((size_t) (lengthSec * kFsOS), 0.0f);
    Pluck p;
    size_t next = 0;
    const size_t numNotes = sizeof (riff) / sizeof (riff[0]);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double t = (double) i / kFsOS;
        if (next < numNotes && t >= riff[next].t)
        {
            p.trigger (riff[next].hz, kFsOS, (unsigned) (1234 + next * 77));
            ++next;
        }
        out[i] = 0.16f * p.tick();   // ~0.32V peak at the jack: single-coil-ish
    }
    return out;
}

static std::vector<float> makeSineGlide (double lengthSec, float f1, float f2, float amp)
{
    std::vector<float> out ((size_t) (lengthSec * kFsOS), 0.0f);
    double phase = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double u = (double) i / (double) out.size();
        const double f = f1 * std::pow ((double) f2 / f1, u); // exponential glide
        phase += f / kFsOS;
        out[i] = amp * (float) std::sin (2.0 * M_PI * phase);
    }
    return out;
}

// --------------------------------------------------------- 4x decimator (LPF)
struct Decimator
{
    glitchwave::detail::BiquadLP lp1, lp2;
    void prepare()
    {
        lp1.setup (20000.0f, 0.541196f, (float) kFsOS); // 4th-order Butterworth
        lp2.setup (20000.0f, 1.306563f, (float) kFsOS);
    }
    float process (float x) { return lp2.process (lp1.process (x)); }
};

// --------------------------------------------------------------------- render
struct RenderResult { float peak; bool hasNaN; };

static RenderResult renderThroughPedal (const std::vector<float>& inOS,
                                        std::vector<float>& out48,
                                        glitchwave::Glitchwave567::Params params,
                                        bool sweepFreqKnob = false)
{
    glitchwave::Glitchwave567 pedal;
    pedal.prepare (kFsOS);
    pedal.setParams (params);

    Decimator dec;
    dec.prepare();

    out48.clear();
    out48.reserve (inOS.size() / kOS);

    RenderResult r { 0.0f, false };
    for (size_t i = 0; i < inOS.size(); ++i)
    {
        if (sweepFreqKnob && (i % 512 == 0))
        {
            params.freq = (float) i / (float) inOS.size(); // 0 -> 1 over the file
            pedal.setParams (params);
        }
        const float y = dec.process (pedal.processSample (inOS[i]));
        if (std::isnan (y) || std::isinf (y)) r.hasNaN = true;
        if ((i % kOS) == (size_t) (kOS - 1))
        {
            out48.push_back (y);
            r.peak = std::max (r.peak, std::fabs (y));
        }
    }
    return r;
}

static void saveNormalized (const std::string& path, std::vector<float> x,
                            float peak, float maxBoostDb = 30.0f)
{
    float g = 1.0f;
    if (peak > 1e-9f)
        g = std::min (std::pow (10.0f, maxBoostDb / 20.0f), 0.891f / peak); // -1 dBFS
    for (auto& v : x) v *= g;
    writeWav16 (path, x, kFsOut);
    std::printf ("  wrote %-38s peak %.4f  makeup %+.1f dB\n",
                 path.c_str(), peak, 20.0f * std::log10 (g));
}

int main()
{
    using P = glitchwave::Glitchwave567::Params;
    std::vector<float> out;

    std::printf ("Glitchwave 567 offline render — fs %d, oversample x%d\n", kFsOut, kOS);

    // reference dry riff
    auto riff = makeRiff (7.0);
    {
        std::vector<float> dry48;
        dry48.reserve (riff.size() / kOS);
        float pk = 0.f;
        for (size_t i = kOS - 1; i < riff.size(); i += kOS)
        { dry48.push_back (riff[i]); pk = std::max (pk, std::fabs (riff[i])); }
        saveNormalized ("01_dry_riff.wav", dry48, pk);
    }

    // pedal, FREQ mid
    { P p; p.freq = 0.5f; p.fizz = 0.35f; p.dry = 0.8f; p.vol = 0.85f;
      auto r = renderThroughPedal (riff, out, p);
      std::printf ("  [freq mid ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("02_riff_freq_mid.wav", out, r.peak); }

    // pedal, FREQ low (f0 ~304 Hz)
    { P p; p.freq = 1.0f; p.fizz = 0.35f; p.dry = 0.8f; p.vol = 0.85f;
      auto r = renderThroughPedal (riff, out, p);
      std::printf ("  [freq low ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("03_riff_freq_low.wav", out, r.peak); }

    // pedal, FREQ knob swept across the riff
    { P p; p.fizz = 0.35f; p.dry = 0.8f; p.vol = 0.85f;
      auto r = renderThroughPedal (riff, out, p, true);
      std::printf ("  [freq swp ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("04_riff_freq_knob_sweep.wav", out, r.peak); }

    // wet-only, dark fizz — raw glitch voice
    { P p; p.freq = 0.5f; p.fizz = 0.9f; p.dry = 0.0f; p.vol = 1.0f;
      auto r = renderThroughPedal (riff, out, p);
      std::printf ("  [wet only ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("05_riff_wet_only_dark.wav", out, r.peak); }

    // sine glide 150 -> 1400 Hz through the lock range: hear capture/chatter
    { auto glide = makeSineGlide (8.0, 150.0f, 1400.0f, 0.15f);
      P p; p.freq = 0.5f; p.fizz = 0.3f; p.dry = 0.25f; p.vol = 1.0f;
      auto r = renderThroughPedal (glide, out, p);
      std::printf ("  [glide    ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("06_sine_glide_lock_test.wav", out, r.peak); }

    // ---- v0.2 mod-system demo: LFO1(tri)->FREQ bent by LFO2, env->FREQ ------
    {
        glitchwave::Glitchwave567 pedal;
        pedal.prepare (kFsOS);
        glitchwave::ModSystem mod;
        mod.prepare (kFsOut);                 // mod runs at host rate

        glitchwave::ModSystem::Params mp;
        mp.lfo1RateHz = 3.0f;  mp.lfo1Depth = 0.7f;
        mp.lfo1Shape  = (int) glitchwave::LfoShape::Triangle;
        mp.lfo1Target = (int) glitchwave::ModTarget::Freq;
        mp.lfo2RateHz = 0.20f; mp.lfo2Depth = 0.6f;
        mp.envGain    = 4.0f;
        mp.envTarget  = (int) glitchwave::ModTarget::Freq;
        mod.setParams (mp);

        glitchwave::ModSystem::KnobSet baseKnobs;
        baseKnobs.freq = 0.5f; baseKnobs.fizz = 0.35f;
        baseKnobs.dry = 0.6f;  baseKnobs.vol = 0.9f;

        Decimator dec; dec.prepare();
        out.clear(); out.reserve (riff.size() / kOS);
        float pk = 0.f; bool nan = false;

        for (size_t i = 0; i + kOS <= riff.size(); i += kOS)
        {
            mod.tick (std::fabs (riff[i]), 0.0f, 0.0f);   // host-rate tick
            if ((i / kOS) % 32 == 0)
            {
                const auto k = mod.compute (baseKnobs);
                glitchwave::Glitchwave567::Params p;
                p.freq = k.freq; p.fizz = k.fizz; p.dry = k.dry; p.vol = k.vol;
                pedal.setParams (p);
            }
            float y = 0.f;
            for (int s = 0; s < kOS; ++s)
                y = dec.process (pedal.processSample (riff[i + (size_t) s]));
            if (std::isnan (y) || std::isinf (y)) nan = true;
            out.push_back (y);
            pk = std::max (pk, std::fabs (y));
        }
        std::printf ("  [mod demo ] NaN=%d\n", (int) nan);
        saveNormalized ("08_mod_lfo_env_freq.wav", out, pk);
    }

    // idle bleed: 3 s of silence, wet up (capped makeup so it stays honest)
    { std::vector<float> silence ((size_t) (3.0 * kFsOS), 0.0f);
      P p; p.freq = 0.5f; p.fizz = 0.3f; p.dry = 0.0f; p.vol = 1.0f;
      auto r = renderThroughPedal (silence, out, p);
      std::printf ("  [idle     ] NaN=%d\n", (int) r.hasNaN);
      saveNormalized ("07_idle_bleed.wav", out, r.peak, 0.0f); }

    std::printf ("done.\n");
    return 0;
}
