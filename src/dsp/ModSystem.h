// ============================================================================
//  ModSystem.h — modulation layer for the Glitchwave 567 sim (step-2 mods).
//
//  Portable C++ (no JUCE). Sources: LFO1 (shape/target), LFO2 (hard-wired to
//  LFO1 rate), input envelope follower (hard-wired to FREQ), and two CV buses
//  fed by sidechain audio + file players. CV2 can additionally modulate CV1's
//  strength and slew ("modulation of modulation"). See docs/MODS.md.
//
//  Usage per host sample:   mod.tick (inputAbs, cv1In, cv2In);
//  Every N samples:         auto out = mod.compute (baseParams);
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace glitchwave
{

// Dropdown target lists — ORDER IS PART OF THE SAVED STATE, append only!
// LFO1 + CV1 use indices 0..10; CV2 additionally understands 11 and 12.
enum class ModTarget : int
{
    Off = 0,
    Freq, Fizz, Dry, Vol, Trim,     // Fizz == the "LPF" cutoff knob (old name kept for state compat)
    Lfo1Rate, Lfo1Depth, Lfo2Rate, Lfo2Depth, EnvAmount,
    Cv1Strength, Cv1Slew,           // CV2-only extras
    LpfQ, DryLpf,                   // v0.3 additions
    Gain                            // v0.9: the dirt Gain knob
};

enum class LfoShape : int { Sine = 0, Triangle, Square, SampleHold, WhiteNoise, PinkNoise };
enum class LfoMode  : int { UniUp = 0, Bipolar, UniDown };

class ModSystem
{
public:
    struct Params
    {
        // LFO 1 (assignable). Noise shapes: rate = LPF cutoff on the noise.
        float lfo1RateHz  = 2.0f;
        float lfo1Depth   = 0.0f;
        int   lfo1Shape   = (int) LfoShape::Triangle;
        int   lfo1Mode    = (int) LfoMode::Bipolar;
        int   lfo1Target  = (int) ModTarget::Freq;
        // LFO 2 (assignable: knobs, LFO1 rate/depth, or env amount)
        float lfo2RateHz  = 0.25f;
        float lfo2Depth   = 0.0f;
        int   lfo2Shape   = (int) LfoShape::Sine;
        int   lfo2Mode    = (int) LfoMode::Bipolar;
        int   lfo2Target  = (int) ModTarget::Lfo1Rate;
        // Mu-Tron III style envelope follower (v0.7), assignable target.
        // Fixed follower ballistics (4 ms attack / 150 ms release), GAIN sets
        // how hard playing dynamics drive the sweep, DRIVE flips direction.
        float envGain     = 4.0f;   // x0.125 .. x40 (Mu-Tron GAIN)
        bool  envDriveUp  = true;   // Drive switch: up = opens, down = closes
        int   envTarget   = (int) ModTarget::Fizz;
        // CV buses
        int   cv1Target   = (int) ModTarget::Off;
        float cv1Strength = 0.0f;   // -1..+1
        float cv1SlewMs   = 20.0f;  // 1..1000
        int   cv2Target   = (int) ModTarget::Off;
        float cv2Strength = 0.0f;
        float cv2SlewMs   = 20.0f;
    };

    // Base knob positions in, modulated positions out. All normalized 0..1
    // (trim is the 0..1 position of the -24..+24 dB range).
    struct KnobSet
    {
        float freq = 0.5f, fizz = 0.65f, dry = 0.5f, vol = 0.7f;
        float lpfQ = 0.4f, gain = 0.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        envAtkCoeff = onePoleCoeff (4.0f);    // Mu-Tron III-ish follower ballistics
        envRelCoeff = onePoleCoeff (150.0f);
        reset();
    }

    void reset() noexcept
    {
        inEnv = cv1Env = cv2Env = 0.0f;
        lfo1Phase = lfo2Phase = 0.0;
        shValue = shValue2 = 0.0f;
        noise1 = noise2 = {};
        rngState = 0x1234ABCDu;
    }

    void setParams (const Params& p) noexcept { params = p; }

    // Call once per host-rate sample. Inputs are absolute-value audio levels
    // in full-scale units (0..~1).
    inline void tick (float inputAbs, float cv1Abs, float cv2Abs) noexcept
    {
        // input envelope: fast attack, slower release
        inEnv += (inputAbs > inEnv ? envAtkCoeff : envRelCoeff) * (inputAbs - inEnv);

        // CV envelopes: single slew constant each (may be modulated by CV2,
        // so coefficients are refreshed in compute())
        cv1Env += cv1Coeff * (cv1Abs - cv1Env);
        cv2Env += cv2Coeff * (cv2Abs - cv2Env);

        ++samplesSinceCompute;
    }

    // Call every N ticks. Advances the LFOs by the elapsed time and returns
    // the modulated knob positions.
    KnobSet compute (const KnobSet& base) noexcept
    {
        const float dt = (float) samplesSinceCompute / fs;
        samplesSinceCompute = 0;

        // ---- 1) CV2 first: it may retune CV1 or any other mod control ------
        float cv1StrengthEff = params.cv1Strength;
        float cv1SlewMsEff   = params.cv1SlewMs;
        float lfo1RateEff    = params.lfo1RateHz;
        float lfo1DepthEff   = params.lfo1Depth;
        float lfo2RateEff    = params.lfo2RateHz;
        float lfo2DepthEff   = params.lfo2Depth;
        float envGainEff     = params.envGain;

        KnobSet out = base;

        // helper: route a CV signal into the mod controls; returns false if the
        // target is a plain knob (applied later so evaluation order stays clean)
        auto applyToModControl = [&] (ModTarget t, float sig) -> bool
        {
            switch (t)
            {
                case ModTarget::Cv1Strength: cv1StrengthEff = clampf (cv1StrengthEff + sig, -1.0f, 1.0f); return true;
                case ModTarget::Cv1Slew:     cv1SlewMsEff   = clampf (cv1SlewMsEff * std::exp2 (sig * 4.0f), 1.0f, 1000.0f); return true;
                case ModTarget::Lfo1Rate:    lfo1RateEff    = clampf (lfo1RateEff * std::exp2 (sig * 3.0f), 0.02f, 40.0f); return true;
                case ModTarget::Lfo1Depth:   lfo1DepthEff   = clampf (lfo1DepthEff + sig, 0.0f, 1.0f); return true;
                case ModTarget::Lfo2Rate:    lfo2RateEff    = clampf (lfo2RateEff * std::exp2 (sig * 3.0f), 0.01f, 20.0f); return true;
                case ModTarget::Lfo2Depth:   lfo2DepthEff   = clampf (lfo2DepthEff + sig, 0.0f, 1.0f); return true;
                case ModTarget::EnvAmount:   envGainEff     = clampf (envGainEff * std::exp2 (sig * 2.0f), 0.125f, 40.0f); return true;
                default: return false;
            }
        };

        // ---- 1) CV2 first (it may retune CV1's strength/slew or any control)
        const float cv2Sig = params.cv2Strength * cv2Env;   // -1..+1
        const bool cv2HitsKnob = ! applyToModControl ((ModTarget) params.cv2Target, cv2Sig);

        // ---- 2) CV1 next, using its (possibly CV2-modulated) strength -------
        const float cv1Sig = cv1StrengthEff * cv1Env;
        const bool cv1HitsKnob = ! applyToModControl ((ModTarget) params.cv1Target, cv1Sig);

        // refresh CV slew coefficients (cheap; every compute call)
        cv1Coeff = onePoleCoeff (cv1SlewMsEff);
        cv2Coeff = onePoleCoeff (params.cv2SlewMs);

        // ---- 3) LFO2 (assignable; can bend LFO1 or the ADSR amount) ---------
        float lfo2Raw;
        if ((LfoShape) params.lfo2Shape == LfoShape::WhiteNoise
         || (LfoShape) params.lfo2Shape == LfoShape::PinkNoise)
        {
            lfo2Raw = noiseSample (noise2, (LfoShape) params.lfo2Shape == LfoShape::PinkNoise,
                                   lfo2RateEff, dt);
        }
        else
        {
            const double prev2 = lfo2Phase;
            lfo2Phase = wrapPhase (lfo2Phase + (double) (lfo2RateEff * dt));
            if (lfo2Phase < prev2)
                shValue2 = whiteNoise();
            lfo2Raw = lfoValue ((LfoShape) params.lfo2Shape, lfo2Phase, shValue2);
        }
        const float lfo2Sig = modeShape ((LfoMode) params.lfo2Mode, lfo2Raw) * lfo2DepthEff;

        float lfo1RateFactor = 1.0f;
        switch ((ModTarget) params.lfo2Target)
        {
            case ModTarget::Lfo1Rate:  lfo1RateFactor = std::exp2 (lfo2Sig * 2.0f); break;
            case ModTarget::Lfo1Depth: lfo1DepthEff = clampf (lfo1DepthEff + lfo2Sig, 0.0f, 1.0f); break;
            case ModTarget::EnvAmount: envGainEff = clampf (envGainEff * std::exp2 (lfo2Sig * 2.0f), 0.125f, 40.0f); break;
            default: applyToKnob (out, (ModTarget) params.lfo2Target, lfo2Sig * 0.5f); break;
        }

        // ---- 4) LFO1 (noise shapes use the rate as the noise LPF cutoff) ----
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
            if (lfo1Phase < prevPhase)                 // wrapped -> new S&H value
                shValue = whiteNoise();
            lfo1Raw = lfoValue ((LfoShape) params.lfo1Shape, lfo1Phase, shValue);
        }
        const float lfo1Out = modeShape ((LfoMode) params.lfo1Mode, lfo1Raw);
        applyToKnob (out, (ModTarget) params.lfo1Target, lfo1Out * lfo1DepthEff * 0.5f);

        // ---- 4b) Mu-Tron III envelope follower -> assignable target ---------
        // follower level x GAIN, clamped to a full-range sweep; DRIVE flips it
        const float envSig = clampf (inEnv * envGainEff, 0.0f, 1.0f);
        applyToKnob (out, (ModTarget) params.envTarget,
                     params.envDriveUp ? envSig : -envSig);

        // ---- 5) CV knob targets last ----------------------------------------
        if (cv2HitsKnob) applyToKnob (out, (ModTarget) params.cv2Target, cv2Sig);
        if (cv1HitsKnob) applyToKnob (out, (ModTarget) params.cv1Target, cv1Sig);

        return out;
    }

    // handy for UI meters later
    float getInputEnv() const noexcept { return inEnv; }
    float getCv1Env()   const noexcept { return cv1Env; }
    float getCv2Env()   const noexcept { return cv2Env; }

private:
    static inline float clampf (float v, float lo, float hi) noexcept
    { return v < lo ? lo : (v > hi ? hi : v); }

    inline float onePoleCoeff (float ms) noexcept
    {
        return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * fs));
    }

    static inline double wrapPhase (double p) noexcept
    { return p - std::floor (p); }

    static inline float sineOf (double phase) noexcept
    { return std::sin ((float) (phase * 6.283185307179586)); }

    inline float whiteNoise() noexcept
    {
        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
        return (float) (int32_t) rngState * (1.0f / 2147483648.0f);
    }

    static float lfoValue (LfoShape shape, double phase, float sh) noexcept
    {
        switch (shape)
        {
            case LfoShape::Sine:       return sineOf (phase);
            case LfoShape::Triangle:   return (float) (phase < 0.5 ? 4.0 * phase - 1.0
                                                                   : 3.0 - 4.0 * phase);
            case LfoShape::Square:     return phase < 0.5 ? 1.0f : -1.0f;
            case LfoShape::SampleHold: return sh;
            default:                   return sh;   // noise shapes handled elsewhere
        }
    }

    // uni-up / bipolar / uni-down polarity switch
    static float modeShape (LfoMode m, float v) noexcept
    {
        switch (m)
        {
            case LfoMode::UniUp:   return  0.5f * (v + 1.0f);   //  0 .. +1
            case LfoMode::UniDown: return -0.5f * (v + 1.0f);   //  0 .. -1
            case LfoMode::Bipolar:
            default:               return v;                    // -1 .. +1
        }
    }

    // white/pink noise through a one-pole LPF whose cutoff = the LFO rate knob
    struct NoiseState { float lp = 0.0f, p0 = 0.0f, p1 = 0.0f, p2 = 0.0f; };

    float noiseSample (NoiseState& ns, bool pink, float cutoffHz, float dt) noexcept
    {
        const float w = whiteNoise();
        float x = w;
        if (pink)   // Paul Kellet's economy pink filter
        {
            ns.p0 = 0.99765f * ns.p0 + w * 0.0990460f;
            ns.p1 = 0.96300f * ns.p1 + w * 0.2965164f;
            ns.p2 = 0.57000f * ns.p2 + w * 1.0526913f;
            x = (ns.p0 + ns.p1 + ns.p2 + w * 0.1848f) * 0.25f;
        }
        const float fc = std::min (cutoffHz, 0.45f / dt);
        const float c  = 1.0f - std::exp (-6.2831853f * fc * dt);
        ns.lp += c * (x - ns.lp);
        // rough variance renormalization so slow (dark) settings stay audible
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
            default: break; // Off and non-knob targets: nothing to do here
        }
    }

    Params params;
    float  fs = 48000.0f;

    float inEnv = 0.0f, cv1Env = 0.0f, cv2Env = 0.0f;
    float envAtkCoeff = 0.1f, envRelCoeff = 0.01f;
    float cv1Coeff = 0.05f, cv2Coeff = 0.05f;

    double lfo1Phase = 0.0, lfo2Phase = 0.0;
    float  shValue = 0.0f, shValue2 = 0.0f;
    NoiseState noise1, noise2;
    uint32_t rngState = 0x1234ABCDu;

    int   samplesSinceCompute = 0;
};

} // namespace glitchwave
