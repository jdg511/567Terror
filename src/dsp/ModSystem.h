// ============================================================================
//  ModSystem.h — modulation layer for the Glitchwave 567 sim.
//
//  v0.13 — matches the hardware plan exactly:
//    * LFO 1: always UNIPOLAR-UP (0..+1), assignable target, blink LED.
//    * LFO 2: always BIPOLAR (±1), assignable target incl. LFO1 rate/depth
//      and the envelope follower's gain.
//    * CV 1 (sidechain L) is hardwired to LFO 1's DEPTH as a VCA; CV 2
//      (sidechain R) to LFO 2's DEPTH. No CV target dropdowns, no strength
//      knobs. Like a normalled jack: with no sidechain signal for ~3 s the
//      VCA opens fully and the LFO runs at its DEPTH knob.
//    * Mu-Tron III style envelope follower (fixed 4 ms / 150 ms), GAIN knob,
//      DRIVE up/down, assignable target.
//
//  Portable C++ (no JUCE). See docs/MODS.md.
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace glitchwave
{

// Target list enum. Values are stable; the processor maps dropdown/LED-selector
// indices onto these.
enum class ModTarget : int
{
    Off = 0,
    Freq, Fizz, Dry, Vol, Trim,     // Fizz == LPF cutoff, Dry == MIX (old names kept)
    Lfo1Rate, Lfo1Depth, Lfo2Rate, Lfo2Depth, EnvAmount,
    Cv1Strength, Cv1Slew,           // retired (kept so enum values stay stable)
    LpfQ, DryLpf,                   // DryLpf retired
    Gain                            // the dirt GAIN knob
};

// v0.17: the full Electric Druid TAPLFO 3D waveform set (both wave sets),
// plus the White/Pink noise LFOs Jason asked for in v0.4.
// Values are append-only so saved sessions stay stable.
enum class LfoShape : int { Sine = 0, Triangle, Square, SampleHold, WhiteNoise, PinkNoise,
                            RampUp, RampDown,
                            // TAPLFO 3D original set additions
                            Sweep, Lumps,
                            // TAPLFO 3D alternate set
                            RampOct, QuadRamp, QuadPulse, TriStep,
                            SineOct, Sine3rd, Sine4th, RandSlopes };

class ModSystem
{
public:
    struct Params
    {
        // LFO 1 (always unipolar-up). Noise shapes: rate = LPF cutoff on noise.
        float lfo1RateHz = 2.0f;
        float lfo1Depth  = 0.0f;
        int   lfo1Shape  = (int) LfoShape::Triangle;
        int   lfo1Target = (int) ModTarget::Freq;
        // LFO 2 (always bipolar)
        float lfo2RateHz = 0.25f;
        float lfo2Depth  = 0.0f;
        int   lfo2Shape  = (int) LfoShape::Sine;
        int   lfo2Target = (int) ModTarget::Lfo1Rate;
        // Envelope follower
        float envGain    = 4.0f;    // x0.125 .. x40
        bool  envDriveUp = true;
        int   envTarget  = (int) ModTarget::Fizz;
    };

    struct KnobSet
    {
        float freq = 0.5f, fizz = 0.65f, dry = 0.5f, vol = 0.7f;
        float lpfQ = 0.4f, gain = 0.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        envAtkCoeff = onePoleCoeff (4.0f);     // Mu-Tron-ish follower ballistics
        envRelCoeff = onePoleCoeff (150.0f);
        cvCoeff     = onePoleCoeff (15.0f);    // fixed CV smoothing
        reset();
    }

    void reset() noexcept
    {
        inEnv = cv1Env = cv2Env = 0.0f;
        cv1SilentSec = cv2SilentSec = 1000.0f;   // start "unplugged" -> VCAs open
        lfo1Phase = lfo2Phase = 0.0;
        shValue = shValue2 = shPrev = shPrev2 = 0.0f;
        noise1 = noise2 = {};
        rngState = 0x1234ABCDu;
        visLfo1 = visLfo2 = 0.0f;
        samplesSinceCompute = 0;
    }

    void setParams (const Params& p) noexcept { params = p; }

    // Call once per host-rate sample. Inputs are absolute-value levels (FS).
    inline void tick (float inputAbs, float cv1Abs, float cv2Abs) noexcept
    {
        inEnv  += (inputAbs > inEnv ? envAtkCoeff : envRelCoeff) * (inputAbs - inEnv);
        cv1Env += cvCoeff * (cv1Abs - cv1Env);
        cv2Env += cvCoeff * (cv2Abs - cv2Env);
        ++samplesSinceCompute;
    }

    // Call every N ticks; returns the modulated knob positions.
    KnobSet compute (const KnobSet& base) noexcept
    {
        const float dt = (float) samplesSinceCompute / fs;
        samplesSinceCompute = 0;

        KnobSet out = base;

        // ---- CV "jack detect" + depth VCAs (hardwired CV1->LFO1, CV2->LFO2) --
        if (cv1Env > 0.001f) cv1SilentSec = 0.0f; else cv1SilentSec += dt;
        if (cv2Env > 0.001f) cv2SilentSec = 0.0f; else cv2SilentSec += dt;
        const float cv1Vca = cv1SilentSec < 3.0f ? clampf (cv1Env * 2.0f, 0.0f, 1.0f) : 1.0f;
        const float cv2Vca = cv2SilentSec < 3.0f ? clampf (cv2Env * 2.0f, 0.0f, 1.0f) : 1.0f;

        float lfo1RateEff  = params.lfo1RateHz;
        float lfo1DepthEff = params.lfo1Depth;
        float envGainEff   = params.envGain;

        // ---- LFO 2 (always bipolar) -----------------------------------------
        float lfo2Raw;
        if ((LfoShape) params.lfo2Shape == LfoShape::WhiteNoise
         || (LfoShape) params.lfo2Shape == LfoShape::PinkNoise)
        {
            lfo2Raw = noiseSample (noise2, (LfoShape) params.lfo2Shape == LfoShape::PinkNoise,
                                   params.lfo2RateHz, dt);
        }
        else
        {
            const double prev2 = lfo2Phase;
            lfo2Phase = wrapPhase (lfo2Phase + (double) (params.lfo2RateHz * dt));
            if (lfo2Phase < prev2)
            {
                shPrev2  = shValue2;             // Random Slopes interpolates prev -> next
                shValue2 = whiteNoise();
            }
            lfo2Raw = lfoValue ((LfoShape) params.lfo2Shape, lfo2Phase, shValue2, shPrev2);
        }
        visLfo2 = lfo2Raw;                                     // bipolar, for the LED
        const float lfo2Sig = lfo2Raw * params.lfo2Depth * cv2Vca;

        float lfo1RateFactor = 1.0f;
        switch ((ModTarget) params.lfo2Target)
        {
            case ModTarget::Lfo1Rate:  lfo1RateFactor = std::exp2 (lfo2Sig * 2.0f); break;
            case ModTarget::Lfo1Depth: lfo1DepthEff = clampf (lfo1DepthEff + lfo2Sig, 0.0f, 1.0f); break;
            case ModTarget::EnvAmount: envGainEff = clampf (envGainEff * std::exp2 (lfo2Sig * 2.0f),
                                                            0.125f, 40.0f); break;
            default: applyToKnob (out, (ModTarget) params.lfo2Target, lfo2Sig * 0.5f); break;
        }

        // ---- LFO 1 (always unipolar-up; noise: rate = noise LPF cutoff) ------
        const float lfo1RateBent = clampf (lfo1RateEff * lfo1RateFactor, 0.01f, 0.45f / dt);
        float lfo1Raw;
        if ((LfoShape) params.lfo1Shape == LfoShape::WhiteNoise
         || (LfoShape) params.lfo1Shape == LfoShape::PinkNoise)
        {
            lfo1Raw = noiseSample (noise1, (LfoShape) params.lfo1Shape == LfoShape::PinkNoise,
                                   lfo1RateBent, dt);
        }
        else
        {
            const double prevPhase = lfo1Phase;
            lfo1Phase = wrapPhase (lfo1Phase + (double) (lfo1RateBent * dt));
            if (lfo1Phase < prevPhase)
            {
                shPrev  = shValue;
                shValue = whiteNoise();
            }
            lfo1Raw = lfoValue ((LfoShape) params.lfo1Shape, lfo1Phase, shValue, shPrev);
        }
        const float lfo1Out = 0.5f * (lfo1Raw + 1.0f);         // unipolar-up 0..1
        visLfo1 = lfo1Out;
        applyToKnob (out, (ModTarget) params.lfo1Target,
                     lfo1Out * lfo1DepthEff * cv1Vca * 0.5f);

        // ---- envelope follower -> assignable target --------------------------
        const float envSig = clampf (inEnv * envGainEff, 0.0f, 1.0f);
        applyToKnob (out, (ModTarget) params.envTarget,
                     params.envDriveUp ? envSig : -envSig);

        return out;
    }

    float getInputEnv() const noexcept { return inEnv; }
    float getCv1Env()   const noexcept { return cv1Env; }
    float getCv2Env()   const noexcept { return cv2Env; }
    float getLfo1Vis()  const noexcept { return visLfo1; }   // 0..1 (unipolar)
    float getLfo2Vis()  const noexcept { return visLfo2; }   // -1..1 (bipolar)

private:
    static inline float clampf (float v, float lo, float hi) noexcept
    { return v < lo ? lo : (v > hi ? hi : v); }

    inline float onePoleCoeff (float ms) noexcept
    {
        return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * fs));
    }

    static inline double wrapPhase (double p) noexcept { return p - std::floor (p); }

    static inline float sineOf (double phase) noexcept
    { return std::sin ((float) (phase * 6.283185307179586)); }

    inline float whiteNoise() noexcept
    {
        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
        return (float) (int32_t) rngState * (1.0f / 2147483648.0f);
    }

    static inline double fracd (double x) noexcept { return x - std::floor (x); }

    // Shapes traced from the TAPLFO 3D datasheet waveform diagram (p.3).
    static float lfoValue (LfoShape shape, double phase, float sh, float shPrev) noexcept
    {
        switch (shape)
        {
            case LfoShape::Sine:       return sineOf (phase);
            case LfoShape::Triangle:   return (float) (phase < 0.5 ? 4.0 * phase - 1.0
                                                                   : 3.0 - 4.0 * phase);
            case LfoShape::Square:     return phase < 0.5 ? 1.0f : -1.0f;   // "Pulse"
            case LfoShape::SampleHold: return sh;                           // "Random Levels"
            case LfoShape::RampUp:     return (float) (2.0 * phase - 1.0);
            case LfoShape::RampDown:   return (float) (1.0 - 2.0 * phase);

            case LfoShape::Sweep:      // smooth scoop: starts high, dips, returns
                return (float) std::cos (phase * 6.283185307179586);

            case LfoShape::Lumps:      // one smooth arch per cycle
                return (float) (2.0 * std::abs (std::sin (phase * 3.141592653589793)) - 1.0);

            case LfoShape::RampOct:    // ramp + ramp one octave up
                return (float) (((2.0 * phase - 1.0)
                               + 0.5 * (2.0 * fracd (phase * 2.0) - 1.0)) / 1.5);

            case LfoShape::QuadRamp:   // 4 quick ramp-down teeth, then rest
                return phase < 0.5 ? (float) (1.0 - 2.0 * fracd (phase * 8.0)) : -1.0f;

            case LfoShape::QuadPulse:  // 4 quick pulses, then rest
                return phase < 0.5 ? (fracd (phase * 8.0) < 0.5 ? 1.0f : -1.0f) : -1.0f;

            case LfoShape::TriStep:    // triangle quantised to 4 levels
            {
                const double t = phase < 0.5 ? 2.0 * phase : 2.0 - 2.0 * phase;   // 0..1..0
                const int    q = std::min (3, (int) (t * 4.0));
                return (float) (q * (2.0 / 3.0) - 1.0);
            }

            case LfoShape::SineOct:    // sine + 1/2 octave   (peak 1.2990 -> normalised)
                return (float) ((std::sin (phase * 6.283185307179586)
                               + 0.5 * std::sin (phase * 12.566370614359172)) / 1.2990381);

            case LfoShape::Sine3rd:    // sine + 1/3 third    (peak 0.9428 -> normalised)
                return (float) ((std::sin (phase * 6.283185307179586)
                               + (1.0 / 3.0) * std::sin (phase * 18.849555921538759)) / 0.9428090);

            case LfoShape::Sine4th:    // sine + 1/4 fourth   (peak 1.1888 -> normalised)
                return (float) ((std::sin (phase * 6.283185307179586)
                               + 0.25 * std::sin (phase * 25.132741228718345)) / 1.1888206);

            case LfoShape::RandSlopes: // line from previous random level to the next
                return shPrev + (sh - shPrev) * (float) phase;

            default:                   return sh;
        }
    }

    struct NoiseState { float lp = 0.0f, p0 = 0.0f, p1 = 0.0f, p2 = 0.0f; };

    float noiseSample (NoiseState& ns, bool pink, float cutoffHz, float dt) noexcept
    {
        const float w = whiteNoise();
        float x = w;
        if (pink)
        {
            ns.p0 = 0.99765f * ns.p0 + w * 0.0990460f;
            ns.p1 = 0.96300f * ns.p1 + w * 0.2965164f;
            ns.p2 = 0.57000f * ns.p2 + w * 1.0526913f;
            x = (ns.p0 + ns.p1 + ns.p2 + w * 0.1848f) * 0.25f;
        }
        const float fc = std::min (cutoffHz, 0.45f / dt);
        const float c  = 1.0f - std::exp (-6.2831853f * fc * dt);
        ns.lp += c * (x - ns.lp);
        const float norm = std::sqrt (std::max ((2.0f - c) / c, 1.0f));
        return clampf (ns.lp * norm * 0.7f, -1.0f, 1.0f);
    }

    static void applyToKnob (KnobSet& k, ModTarget t, float offset) noexcept
    {
        switch (t)
        {
            case ModTarget::Freq: k.freq = clampf (k.freq + offset, 0.0f, 1.0f); break;
            case ModTarget::Fizz: k.fizz = clampf (k.fizz + offset, 0.0f, 1.0f); break;
            case ModTarget::Dry:  k.dry  = clampf (k.dry  + offset, 0.0f, 1.0f); break;
            case ModTarget::Vol:  k.vol  = clampf (k.vol  + offset, 0.0f, 1.0f); break;
            case ModTarget::LpfQ: k.lpfQ = clampf (k.lpfQ + offset, 0.0f, 1.0f); break;
            case ModTarget::Gain: k.gain = clampf (k.gain + offset, 0.0f, 1.0f); break;
            default: break;
        }
    }

    Params params;
    float  fs = 48000.0f;

    float inEnv = 0.0f, cv1Env = 0.0f, cv2Env = 0.0f;
    float envAtkCoeff = 0.1f, envRelCoeff = 0.01f, cvCoeff = 0.05f;
    float cv1SilentSec = 1000.0f, cv2SilentSec = 1000.0f;

    double lfo1Phase = 0.0, lfo2Phase = 0.0;
    float  shValue = 0.0f, shValue2 = 0.0f;
    float  shPrev  = 0.0f, shPrev2  = 0.0f;   // for Random Slopes
    NoiseState noise1, noise2;
    uint32_t rngState = 0x1234ABCDu;

    int   samplesSinceCompute = 0;
    float visLfo1 = 0.0f, visLfo2 = 0.0f;
};

} // namespace glitchwave
