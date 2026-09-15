// ============================================================================
//  ModSystem.h — modulation layer for the WTF sim.
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

namespace wtf
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
    Gain,                           // the dirt GAIN knob
    EnvLevel                        // v0.21: scales the env follower's output
};

// v0.18: Jason's two-bank waveform plan (from the signal-flow-diagram chat).
// Bank A classics: RampUp, RampDown, Square, Triangle, Sine, Sweep,
//                  RandSlopes, SampleHold (S&H).
// Bank B fun stuff: Lorenz, Rossler, DrunkWalk, PerlinDrift, Wobble, Glitch,
//                   + White/Pink noise holding the two open slots.
// Values are append-only so saved sessions stay stable (TAPLFO extras from
// v0.17 keep their values but are no longer listed in the UI).
enum class LfoShape : int { Sine = 0, Triangle, Square, SampleHold, WhiteNoise, PinkNoise,
                            RampUp, RampDown,
                            Sweep, Lumps,
                            RampOct, QuadRamp, QuadPulse, TriStep,
                            SineOct, Sine3rd, Sine4th, RandSlopes,
                            // v0.18 Bank B generators (stateful, non-periodic)
                            Lorenz, Rossler, DrunkWalk, PerlinDrift, Wobble, Glitch };

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
        // v0.38 secret Layer-A shaping (all three default to "no change"):
        float envThresh  = 0.0f;    // 0..1 -- below this, the follower reads 0
        float envRatio   = 0.5f;    // 0..1 -> 0.1:1 (expand) .. 1:1 .. 1:10 (compress)
        float envShape   = 0.5f;    // 0..1 -> log .. linear .. exponential curve
        // v0.45 Mu-Tron III ballistics, now on the Layer Z knobs. Defaults are
        // the REAL numbers off the Musitronics circuit -- see MuTron below.
        float envAttackMs = 1.551f;   // noon on the ATTACK knob
        float envDecayMs  = 158.7f;   // noon on the DECAY knob
    };

    // ------------------------------------------------------------------------
    // v0.45  MU-TRON III ENVELOPE, derived from the circuit rather than guessed
    //
    // Values are read off the GGG Mutron III schematic and confirmed against
    // the Aion Lumitron parts list (same circuit, different designators):
    //
    //   IC3b   precision HALF-WAVE rectifier, 1M feedback, two 1N914
    //   R14    330 ohm   feeds the envelope cap          (Lumitron R16)
    //   C9     4.7 uF    the envelope cap                (Lumitron C12)
    //   R15    47k       from the cap node to -9V        (Lumitron R17)
    //   R16    120k      from the cap node to the LED driver's virtual ground
    //
    // ATTACK  = R14 * C9            = 330 * 4.7u    = 1.551 ms
    // DECAY   = (R15 || R16) * C9   = 33.77k * 4.7u = 158.7 ms
    //
    // That is where the "4 ms / 150 ms" figure everyone quotes comes from: the
    // decay is the 159 ms time constant, and 4 ms is the 10-90% RISE time
    // (2.2 * 1.551 = 3.4 ms) once the vactrol's own lag is stacked on top.
    //
    // THE VACTROL IS THE SECOND HALF OF THE BALLISTICS. The Lumitron specifies
    // a VTL5C3, "fast on / fast off": 2.5 ms turn-on to 63%, 35 ms decay. That
    // lag cascades with the cap, and it is why a Mu-Tron does not sound like a
    // plain one-pole follower.
    //
    // AND THE VACTROL IS WHY NOTHING HERE IS LINEAR. From the Xvive datasheet:
    //     1 mA -> 30 kOhm,  10 mA -> 5 Ohm,  40 mA -> 1.5 Ohm, dark 10 MOhm
    // Fitting the 1-10 mA leg gives R proportional to I^-3.78. Nearly four
    // decades of resistance per decade of current. The filter runs at
    // f = 1/(2*pi*(Rldr || 220k)*C), so frequency follows I^3.78, which is a
    // STRAIGHT LINE in log-current versus log-frequency. In the pedal's real
    // operating window (about 0.35 to 1.1 mA through the LED) that works out
    // to roughly 460 Hz to 4.6 kHz on the high range and 206 Hz to 2.06 kHz on
    // the low range: a 10:1 sweep either way.
    //
    // Because the knob-to-frequency map in this plugin is already logarithmic,
    // the whole non-linearity collapses into one line: the envelope drives LED
    // CURRENT linearly, and knob position follows log(current). That is
    // kVacShape() below. Change ATTACK or DECAY all you like and the shape
    // survives, which is the point of item 9.
    // ------------------------------------------------------------------------
    struct MuTron
    {
        static constexpr float kAttackMs   = 1.551f;    // R14 * C9
        static constexpr float kDecayMs    = 158.7f;    // (R15||R16) * C9
        static constexpr float kVacOnMs    = 2.5f;      // VTL5C3 turn-on to 63%
        static constexpr float kVacOffMs   = 35.0f;     // VTL5C3 decay
        // LED current window, full/idle. 1.1 mA / 0.35 mA from the sweep above.
        static constexpr float kCurrentK   = 3.143f;
        // 10:1 filter sweep, expressed against this plugin's 200:1 FIZZ span.
        static constexpr float kSweepSpan  = 0.4343f;   // log10(10) / log10(200)
    };

    // The vactrol law, normalised. d is 0..1 of envelope; the return is 0..1 of
    // sweep. Compressive: a light touch already opens the filter most of the
    // way, then it crowds together at the top, exactly like the real thing.
    static inline float kVacShape (float d) noexcept
    {
        d = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
        return std::log (1.0f + d * (MuTron::kCurrentK - 1.0f))
             / std::log (MuTron::kCurrentK);
    }

    struct KnobSet
    {
        float freq = 0.5f, fizz = 0.65f, dry = 0.5f, vol = 0.7f;
        float lpfQ = 0.4f, gain = 0.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;
        // v0.45: real Mu-Tron numbers, and the vactrol's own lag behind them.
        envAtkCoeff = onePoleCoeff (MuTron::kAttackMs);
        envRelCoeff = onePoleCoeff (MuTron::kDecayMs);
        vacOnCoeff  = onePoleCoeff (MuTron::kVacOnMs);
        vacOffCoeff = onePoleCoeff (MuTron::kVacOffMs);
        atkMsCur = MuTron::kAttackMs;
        decMsCur = MuTron::kDecayMs;
        cvCoeff     = onePoleCoeff (15.0f);    // fixed CV smoothing
        reset();
    }

    void reset() noexcept
    {
        inEnv = cv1Env = cv2Env = vacEnv = 0.0f;
        cv1SilentSec = cv2SilentSec = 1000.0f;   // start "unplugged" -> VCAs open
        lfo1Phase = lfo2Phase = 0.0;
        shValue = shValue2 = shPrev = shPrev2 = 0.0f;
        noise1 = noise2 = {};
        state1 = state2 = {};
        rngState = 0x1234ABCDu;
        visLfo1 = visLfo2 = visEnvSig = 0.0f;
        samplesSinceCompute = 0;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        // v0.45: ATTACK and DECAY are live knobs now (Layer Z). Recompute the
        // coefficients only when they actually move -- exp() is not free.
        if (p.envAttackMs != atkMsCur)
        {
            atkMsCur    = p.envAttackMs;
            envAtkCoeff = onePoleCoeff (atkMsCur);
        }
        if (p.envDecayMs != decMsCur)
        {
            decMsCur    = p.envDecayMs;
            envRelCoeff = onePoleCoeff (decMsCur);
        }
    }

    // v0.18: a tap-tempo tap on LFO 2 resets the non-periodic generators'
    // state (the tap sets the time-scale; the reset makes it feel synced
    // under your foot). Harmless for periodic shapes — phase is untouched.
    void retriggerLfo2() noexcept
    {
        state2 = {};
        noise2 = {};
    }

    // v0.24: tap tempo commits now target either LFO — LFO1 taps re-seed
    // LFO1's chaos generators the same way.
    void retriggerLfo1() noexcept
    {
        state1 = {};
        noise1 = {};
    }

    // Call once per host-rate sample. Inputs are absolute-value levels (FS).
    inline void tick (float inputAbs, float cv1Abs, float cv2Abs) noexcept
    {
        // v0.45 two-stage Mu-Tron ballistics.
        //   stage 1: the 4.7uF envelope cap, charged through 330R and
        //            discharged through 47k||120k
        //   stage 2: the VTL5C3 itself, which has its own 2.5 ms / 35 ms lag
        // Cascading them is what makes the attack feel soft-but-quick instead
        // of clicky, and what puts the little tail on the end of the decay.
        inEnv  += (inputAbs > inEnv ? envAtkCoeff : envRelCoeff) * (inputAbs - inEnv);
        vacEnv += (inEnv > vacEnv ? vacOnCoeff : vacOffCoeff) * (inEnv - vacEnv);
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
        const float lfo2Raw = generate ((LfoShape) params.lfo2Shape, params.lfo2RateHz, dt,
                                        lfo2Phase, shValue2, shPrev2, noise2, state2);
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

        // ---- v0.38: secret Layer-A envelope shaping (Threshold/Ratio/Shape) --
        // Threshold (Gain knob): below it the follower reads flat zero.
        // Ratio (LPF knob): a compressor-style ratio applied to the level
        // above threshold, continuously from 0.1:1 (expand) through 1:1
        // (unity) to 1:10 (heavy compression).
        // Shape (Freq knob): the curve's knee -- log (concave) at one end,
        // straight line in the middle, exponential (convex) at the other.
        // All three default to "no change" (thresh 0, ratio/shape centred
        // at 0.5) so a stock patch behaves exactly as it did before v0.38.
        // v0.45: the follower now reads off vacEnv, which is the envelope cap
        // AND the vactrol, not the bare rectifier.
        const float envAboveThresh = clampf ((vacEnv - params.envThresh)
                                             / std::max (1.0f - params.envThresh, 0.0001f),
                                             0.0f, 1.0f);
        const float envShapeExp = std::pow (2.0f,  4.0f * (params.envShape - 0.5f)); // 0.25 (log) .. 4 (exp)
        const float envRatioExp = std::pow (10.0f, 2.0f * (params.envRatio - 0.5f)); // 0.1:1 .. 1:10
        // v0.45: the VTL5C3 law goes LAST, after Threshold/Ratio/Shape, because
        // in the real pedal those all live in front of the LED driver and the
        // vactrol is the final thing between the driver and the filter. Leaving
        // Ratio/Shape centred gives you the stock Mu-Tron curve.
        const float envShaped   = kVacShape (std::pow (envAboveThresh,
                                                       envShapeExp * envRatioExp));

        // ---- envelope pre-pass (v0.21: env can drive LFO1 rate/depth) --------
        const float envPre      = clampf (envShaped * envGainEff, 0.0f, 1.0f);
        const float envApplyPre = params.envDriveUp ? envPre : -envPre;
        switch ((ModTarget) params.envTarget)
        {
            case ModTarget::Lfo1Rate:
                lfo1RateFactor *= std::exp2 (envApplyPre * 2.0f); break;
            case ModTarget::Lfo1Depth:
                lfo1DepthEff = clampf (lfo1DepthEff + envApplyPre, 0.0f, 1.0f); break;
            default: break;
        }

        // ---- LFO 1 (always unipolar-up; noise: rate = noise LPF cutoff) ------
        const float lfo1RateBent = clampf (lfo1RateEff * lfo1RateFactor, 0.01f, 0.45f / dt);
        const float lfo1Raw = generate ((LfoShape) params.lfo1Shape, lfo1RateBent, dt,
                                        lfo1Phase, shValue, shPrev, noise1, state1);
        const float lfo1Out = 0.5f * (lfo1Raw + 1.0f);         // unipolar-up 0..1
        visLfo1 = lfo1Out;

        // v0.21: LFO1 can also drive the env follower's gain or output level
        float envLevelMul = 1.0f;
        const float lfo1Sig = lfo1Out * lfo1DepthEff * cv1Vca;
        switch ((ModTarget) params.lfo1Target)
        {
            case ModTarget::EnvAmount:
                envGainEff = clampf (envGainEff * std::exp2 (lfo1Sig * 2.0f),
                                     0.125f, 40.0f); break;
            case ModTarget::EnvLevel:
                envLevelMul = 1.0f + 2.0f * lfo1Sig; break;      // up to x3
            default:
                applyToKnob (out, (ModTarget) params.lfo1Target, lfo1Sig * 0.5f);
                break;
        }

        // ---- envelope follower -> its knob target ----------------------------
        // (skipped when it already spent itself on LFO1 rate/depth above)
        const float envSig = clampf (envShaped * envGainEff * envLevelMul, 0.0f, 1.0f);
        visEnvSig = envSig;
        const ModTarget et = (ModTarget) params.envTarget;
        if (et != ModTarget::Lfo1Rate && et != ModTarget::Lfo1Depth)
            applyToKnob (out, et, params.envDriveUp ? envSig : -envSig);

        return out;
    }

    float getInputEnv() const noexcept { return inEnv; }
    float getCv1Env()   const noexcept { return cv1Env; }
    float getCv2Env()   const noexcept { return cv2Env; }
    float getLfo1Vis()  const noexcept { return visLfo1; }   // 0..1 (unipolar)
    float getLfo2Vis()  const noexcept { return visLfo2; }   // -1..1 (bipolar)
    float getEnvSigVis() const noexcept { return visEnvSig; } // 0..1 post gain/level

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

    // ---- v0.18 Bank B state ------------------------------------------------
    struct LfoState
    {
        // Lorenz / Rossler attractor coordinates
        double ax = 0.0, ay = 0.0, az = 0.0;
        bool   seeded = false;
        // drunk walk (brownian with momentum)
        float  dv = 0.0f, dpos = 0.0f;
        // Perlin-style drift: 3 octaves of value noise
        double pph[3] = { 0.0, 0.0, 0.0 };
        float  pa[3]  = { 0.0f, 0.0f, 0.0f }, pb[3] = { 0.0f, 0.0f, 0.0f };
        // wobble envelope (random swell/fade on a sine)
        float  wEnv = 0.6f, wTgt = 0.6f;
        // glitch: calm wander + burst timer
        float  gCalm = 0.0f, gVal = 0.0f, gBurst = 0.0f;
    };

    inline float rand01() noexcept { return 0.5f * (whiteNoise() + 1.0f); }

    // One LFO output sample. Periodic shapes ride `phase`; Bank B shapes and
    // the noise LFOs are stateful. Rate = time-scale for the non-periodic ones
    // ("how fast the attractor moves" — Jason's plan).
    float generate (LfoShape shape, float rateHz, float dt,
                    double& phase, float& sh, float& shp,
                    NoiseState& ns, LfoState& ls) noexcept
    {
        switch (shape)
        {
            case LfoShape::WhiteNoise:
            case LfoShape::PinkNoise:
                return noiseSample (ns, shape == LfoShape::PinkNoise, rateHz, dt);

            case LfoShape::Lorenz:      // swoopy, orbit-like, never repeats
            {
                if (! ls.seeded)
                {
                    ls.ax = 0.1 + 0.05 * whiteNoise(); ls.ay = 0.0; ls.az = 25.0;
                    ls.seeded = true;
                }
                double h = 0.75 * rateHz * dt;          // ~1 orbit per 1/rate sec
                const int n = 1 + (int) (h / 0.01);
                h /= n;
                for (int i = 0; i < n; ++i)
                {
                    const double dx = 10.0 * (ls.ay - ls.ax);
                    const double dy = ls.ax * (28.0 - ls.az) - ls.ay;
                    const double dz = ls.ax * ls.ay - (8.0 / 3.0) * ls.az;
                    ls.ax += h * dx; ls.ay += h * dy; ls.az += h * dz;
                }
                if (! std::isfinite (ls.ax) || std::abs (ls.ax) > 1000.0)
                    ls.seeded = false;                  // re-seed next sample
                return clampf ((float) (ls.ax / 20.0), -1.0f, 1.0f);
            }

            case LfoShape::Rossler:     // smoother, spiral-y
            {
                if (! ls.seeded)
                {
                    ls.ax = 1.0 + 0.1 * whiteNoise(); ls.ay = 0.0; ls.az = 0.0;
                    ls.seeded = true;
                }
                double h = 5.9 * rateHz * dt;           // ~1 spiral per 1/rate sec
                const int n = 1 + (int) (h / 0.05);
                h /= n;
                for (int i = 0; i < n; ++i)
                {
                    const double dx = -ls.ay - ls.az;
                    const double dy = ls.ax + 0.2 * ls.ay;
                    const double dz = 0.2 + ls.az * (ls.ax - 5.7);
                    ls.ax += h * dx; ls.ay += h * dy; ls.az += h * dz;
                }
                if (! std::isfinite (ls.ax) || std::abs (ls.ax) > 1000.0)
                    ls.seeded = false;
                return clampf ((float) (ls.ax / 11.0), -1.0f, 1.0f);
            }

            case LfoShape::DrunkWalk:   // brownian wander with momentum
            {
                const float a = clampf (rateHz * dt * 4.0f, 0.0f, 0.5f);
                ls.dv  += whiteNoise() * a;
                ls.dv  *= 1.0f - 0.5f * a;              // gentle damping
                ls.dpos += ls.dv * clampf (rateHz * dt * 6.0f, 0.0f, 0.5f);
                if (ls.dpos >  1.0f) { ls.dpos =  2.0f - ls.dpos; ls.dv = -std::abs (ls.dv); }
                if (ls.dpos < -1.0f) { ls.dpos = -2.0f - ls.dpos; ls.dv =  std::abs (ls.dv); }
                return ls.dpos;
            }

            case LfoShape::PerlinDrift: // layered smooth randomness, organic
            {
                float sum = 0.0f, amp = 1.0f, tot = 0.0f, f = rateHz;
                for (int k = 0; k < 3; ++k)
                {
                    ls.pph[k] += (double) (f * dt);
                    if (ls.pph[k] >= 1.0)
                    {
                        ls.pph[k] -= std::floor (ls.pph[k]);
                        ls.pa[k] = ls.pb[k];
                        ls.pb[k] = whiteNoise();
                    }
                    const float t = (float) ls.pph[k];
                    const float s = 0.5f - 0.5f * std::cos (3.1415927f * t);
                    sum += (ls.pa[k] + (ls.pb[k] - ls.pa[k]) * s) * amp;
                    tot += amp;
                    f *= 2.1f; amp *= 0.5f;
                }
                return clampf (sum / (tot * 0.75f), -1.0f, 1.0f);
            }

            case LfoShape::Glitch:      // mostly calm, sudden brief chaotic flurries
            {
                if (ls.gBurst > 0.0f)
                {
                    ls.gBurst -= dt;
                    if (rand01() < dt * 250.0f)     // a hard jump every ~4 ms
                        ls.gVal = whiteNoise();     // full-range chaotic flurry
                }
                else
                {
                    if (rand01() < rateHz * 0.4f * dt)          // flurries ~0.4/cycle
                        ls.gBurst = 0.08f + 0.17f * rand01();
                    ls.gCalm += (whiteNoise() - ls.gCalm)
                              * clampf (dt * rateHz * 0.25f, 0.0f, 0.1f);
                    ls.gVal  += (0.1f * ls.gCalm - ls.gVal)
                              * clampf (dt * rateHz * 2.0f, 0.0f, 0.5f);
                }
                return clampf (ls.gVal, -1.0f, 1.0f);
            }

            default: break;             // periodic shapes fall through
        }

        // ---- phase-based shapes (incl. Wobble's sine carrier) ---------------
        const double prev = phase;
        phase = wrapPhase (phase + (double) (rateHz * dt));
        if (phase < prev)
        {
            shp = sh;                   // Random Slopes interpolates prev -> next
            sh  = whiteNoise();
            if (shape == LfoShape::Wobble)
                ls.wTgt = 0.15f + 0.85f * rand01();     // new swell each cycle
        }

        if (shape == LfoShape::Wobble)  // sine whose depth randomly swells/fades
        {
            ls.wEnv += (ls.wTgt - ls.wEnv) * clampf (rateHz * dt * 2.0f, 0.0f, 0.5f);
            return sineOf (phase) * ls.wEnv;
        }

        return lfoValue (shape, phase, sh, shp);
    }

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
    // v0.45 vactrol stage + the live ATTACK/DECAY cache
    float vacEnv = 0.0f, vacOnCoeff = 0.1f, vacOffCoeff = 0.01f;
    float atkMsCur = MuTron::kAttackMs, decMsCur = MuTron::kDecayMs;
    float cv1SilentSec = 1000.0f, cv2SilentSec = 1000.0f;

    double lfo1Phase = 0.0, lfo2Phase = 0.0;
    float  shValue = 0.0f, shValue2 = 0.0f;
    float  shPrev  = 0.0f, shPrev2  = 0.0f;   // for Random Slopes
    NoiseState noise1, noise2;
    LfoState   state1, state2;   // v0.18 Bank B generators
    uint32_t rngState = 0x1234ABCDu;

    int   samplesSinceCompute = 0;
    float visLfo1 = 0.0f, visLfo2 = 0.0f, visEnvSig = 0.0f;
};

} // namespace wtf
