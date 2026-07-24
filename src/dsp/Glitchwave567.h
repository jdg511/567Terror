// ============================================================================
//  Glitchwave567.h — behavioral circuit simulation of the Glitchwave 567 pedal
//
//  Portable C++ (no JUCE dependency) so the same code runs in the plugin and
//  in the offline test harness. All voltages are stored as "delta volts"
//  around the +4.5V mid-rail reference (VREF). See docs/SIM_NOTES.md for the
//  full circuit-to-code mapping.
//
//  Signal path:
//    jack -> input network/buffer (U1.1) ->  x214.6 gain stage (U1.3, clips)
//         -> LM567 PLL (U2, no loop/output filter caps => audio-rate chatter)
//         -> Q open-collector node (R16 100k pull-up)
//         -> Sallen-Key LPF "FIZZ" (U1.4)
//         -> inverting mixer DRY/VOL (U1.2) -> output DC block -> jack
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace glitchwave
{

// ----------------------------------------------------------------------------
// Tunable "component tolerance" constants. These capture behaviors the
// schematic doesn't pin down exactly (stray capacitance, comparator
// thresholds inside the LM567). Tweak while A/B-ing against the real pedal.
// ----------------------------------------------------------------------------
struct Tunables
{
    float vcoPullRange   = 0.07f;   // VCO max deviation, fraction of f0 (datasheet: 14% total BW)
    float loopStrayHz    = 400000.f;// loop-filter pole with no LFIL cap (stray only)
    float ofilStrayHz    = 400000.f;// output-filter pole with no OFIL cap (stray only)
    float detOnLevel     = 0.35f;   // Schmitt comparator: Q turns ON (low) above this
    float detOffLevel    = 0.15f;   // Schmitt comparator: Q turns OFF below this
    float inHysteresisV  = 0.005f;  // input limiter comparator hysteresis (volts)
    float qNodeStrayC    = 220e-12f;// stray capacitance at the Q/pull-up node
    float qFallTauSec    = 2e-6f;   // Q transistor saturating (fast fall)
    float noiseFloorV    = 1.5e-4f; // input-referred noise at the 567 input (idle bleed)
    float jackVoltsPerFS = 2.0f;    // 1.0 full-scale sample == this many volts at the jack
    float fixedTrimDb    = 15.0f;   // v0.8: fixed drive into the 567 branch (knob removed)
};

// ----------------------------------------------------------------------------
// Small building blocks
// ----------------------------------------------------------------------------
namespace detail
{
    constexpr float kPi = 3.14159265358979323846f;

    inline float clampConst (float v, float lo, float hi) noexcept
    { return v < lo ? lo : (v > hi ? hi : v); }

    // TL074 on 9V single supply: output can swing ~±3.1V around VREF.
    inline float opampClip (float v) noexcept
    {
        constexpr float rail = 3.1f;
        // smooth bounded saturator, hard-ish knee (k = 8)
        const float x = v / rail;
        const float x2 = x * x;
        const float x4 = x2 * x2;
        const float x8 = x4 * x4;
        return rail * x / std::pow (1.0f + x8, 0.125f);
    }

    // A100k audio-taper pot: ~10% output at half rotation. a = (81^x - 1) / 80
    inline float audioTaper (float x) noexcept
    {
        x = std::clamp (x, 0.0f, 1.0f);
        return (std::exp2 (x * 6.33985f) - 1.0f) * (1.0f / 80.0f); // 81^x == 2^(x*log2(81))
    }

    struct OnePoleLP
    {
        float c = 0.f, y = 0.f;
        void setCutoff (float hz, float fs) noexcept
        {
            hz = std::min (hz, 0.45f * fs);
            c  = 1.0f - std::exp (-2.0f * kPi * hz / fs);
        }
        float process (float x) noexcept { y += c * (x - y); return y; }
        void reset() noexcept { y = 0.f; }
    };

    struct OnePoleHP
    {
        float c = 0.f, x1 = 0.f, y = 0.f;
        void setCutoff (float hz, float fs) noexcept
        {
            c = std::exp (-2.0f * kPi * hz / fs);
        }
        float process (float x) noexcept
        {
            y  = c * (y + x - x1);
            x1 = x;
            return y;
        }
        void reset() noexcept { x1 = y = 0.f; }
    };

    // RBJ biquad (LP / BP / HP), transposed direct form II
    struct BiquadLP
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;

        void setup (float fc, float q, float fs) noexcept { setup (0, fc, q, fs); }

        void setup (int mode, float fc, float q, float fs) noexcept
        {
            fc = std::clamp (fc, 10.0f, 0.45f * fs);
            const float w0    = 2.0f * kPi * fc / fs;
            const float cosw  = std::cos (w0);
            const float alpha = std::sin (w0) / (2.0f * q);
            const float a0    = 1.0f + alpha;
            const float inv   = 1.0f / a0;
            switch (mode)
            {
                default:
                case 0: // low-pass
                    b0 = 0.5f * (1.0f - cosw) * inv;
                    b1 = (1.0f - cosw) * inv;
                    b2 = b0;
                    break;
                case 1: // band-pass (constant 0 dB peak)
                    b0 = alpha * inv;
                    b1 = 0.0f;
                    b2 = -b0;
                    break;
                case 2: // high-pass
                    b0 = 0.5f * (1.0f + cosw) * inv;
                    b1 = -(1.0f + cosw) * inv;
                    b2 = b0;
                    break;
                case 3: // notch
                    b0 = inv;
                    b1 = -2.0f * cosw * inv;
                    b2 = inv;
                    break;
            }
            a1 = -2.0f * cosw * inv;
            a2 = (1.0f - alpha) * inv;
        }

        // RBJ peaking EQ (bell)
        void setupPeak (float fc, float q, float gainDb, float fs) noexcept
        {
            fc = std::clamp (fc, 10.0f, 0.45f * fs);
            const float A     = std::pow (10.0f, gainDb / 40.0f);
            const float w0    = 2.0f * kPi * fc / fs;
            const float cosw  = std::cos (w0);
            const float alpha = std::sin (w0) / (2.0f * q);
            const float a0    = 1.0f + alpha / A;
            const float inv   = 1.0f / a0;
            b0 = (1.0f + alpha * A) * inv;
            b1 = -2.0f * cosw * inv;
            b2 = (1.0f - alpha * A) * inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * inv;
        }
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() noexcept { z1 = z2 = 0.f; }
    };

    // tiny xorshift RNG for the noise floor (deterministic, allocation-free)
    struct Rng
    {
        uint32_t s = 0x9E3779B9u;
        float white() noexcept // roughly uniform in [-1, 1]
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (int32_t) s * (1.0f / 2147483648.0f);
        }
    };
} // namespace detail

// ----------------------------------------------------------------------------
// The pedal
// ----------------------------------------------------------------------------
class Glitchwave567
{
public:
    struct Params
    {
        // v0.8 signal path: In -> (trim +15dB -> 567) + (dry) -> MIX
        //                      -> envelope filter -> out (gate lives in the plugin)
        float freq        = 0.5f;  // 0..1 -> 0.1 Hz .. 18 kHz (log)
        float fizz        = 0.65f; // filter cutoff 0..1 within the selected RANGE
                                   // (v0.8: Lo 20 Hz..4 kHz, Hi 44 Hz..8.8 kHz)
        float lpfQ        = 0.4f;  // 0..1 -> Q 0.25 .. 8 (log)
        int   lpfMode     = 1;     // 0 = Off (bypass), 1 = LP, 2 = BP, 3 = HP, 4 = Notch
        int   lpfRangeHi  = 0;     // 0 = Lo range, 1 = Hi range
        float dry         = 0.5f;  // MIX crossfade — 0 = dry only, 0.5 = both, 1 = FX only
        float vol         = 0.5f;  // VOL1 pot position 0..1 (A100k), master level
        float gain        = 0.0f;  // v0.9: dirt GAIN 0..1 -> x2 (slightly dirty) .. x300 (fuzz wall)
        int   dirtType    = 0;     // v0.9: 0 Electra, 1 Fuzz Face Ge, 2 Bazz Fuss,
                                   //       3 Op-Amp OD, 4 Octave Fuzz (always on, dry path only)
        // v0.21 power modelling
        float supplyV     = 9.0f;  // 9 or 18 (centre-negative adapter voltage)
        float starve      = 0.0f;  // 0..1 secret starve: rail sags toward 5 V
        // v0.32 — Jason's FINAL output stage (audition over): two internal
        // switches. JFET stage on/off (ships ON) feeding an asymmetric
        // -3/-6 ladder on/off (ships OFF). Both off = the bare op-amp rail.
        bool  jfetOn      = true;
        bool  ladder36    = false;
        // v0.23: the +6 dB output boost is now switchable (1.0 or 2.0)
        float boost6Gain  = 2.0f;
    };

    Tunables tune; // exposed so the harness / future mods can poke at it

    void prepare (double sampleRate) noexcept
    {
        fs = (float) sampleRate;

        inputDCBlock.setCutoff (0.33f, fs);           // C1 + R3 (2.2M)
        gainShelfHP.setCutoff (15.4f, fs);            // C2 (4.7u) + R5 (2.2k)
        dirtDC.setCutoff (15.0f, fs);                 // dirt DC blocker
        odPreHP.setCutoff (250.0f, fs);               // Op-Amp OD tight-bass input
        octHP.setCutoff (30.0f, fs);                  // octave rectifier AC coupling

        // v0.10 fixed voicing filters (always on)
        inHP1.setup (2, 40.0f, 0.5412f, fs);          // 24 dB/oct Butterworth HP @ 40 Hz
        inHP2.setup (2, 40.0f, 1.3066f, fs);          //   (two cascaded 2nd-order sections)
        outHP.setup (2, 60.0f, 0.7071f, fs);          // 12 dB/oct HP @ 60 Hz
        outPeak.setupPeak (800.0f, 0.5f, 3.0f, fs);   // +3 dB broad bell @ 800 Hz
        sagCoeff = 1.0f - std::exp (-1.0f / (0.030f * fs));  // Ge fuzz bias sag ~30 ms
        in567HP.setCutoff (36.0f, fs);                // C3 (220n) into ~20k pin impedance
        loopFilter.setCutoff (tune.loopStrayHz, fs);  // no LFIL cap -> stray only
        ofilFilter.setCutoff (tune.ofilStrayHz, fs);  // no OFIL cap -> stray only
        outDCBlock.setCutoff (7.23f, fs);             // C8 (220n) + R12 (100k)
        jfetDC.setCutoff (10.0f, fs);                 // v0.22 JFET output cap

        // pot smoothing ~5 ms (tight enough that LFO/CV modulation stays audible;
        // the mod system's slew knobs own any additional smoothing)
        potSmoothCoeff = 1.0f - std::exp (-1.0f / (0.005f * fs));

        coeffCounter = 0;
        updateDerived (true);
        reset();
    }

    void reset() noexcept
    {
        inputDCBlock.reset(); gainShelfHP.reset(); in567HP.reset();
        loopFilter.reset();   ofilFilter.reset();  outDCBlock.reset();
        fizzLPF.reset();      dirtDC.reset();      odPreHP.reset();
        dirtLP.reset();       octHP.reset();
        inHP1.reset(); inHP2.reset(); outHP.reset(); outPeak.reset();
        jfetDC.reset();
        sagEnv = dirtPrevY = 0.0f;
        vcoPhase = 0.0; sIn = 1.0f; detected = false; vQ = 0.0f;
        updateDerived (true);
    }

    void setParams (const Params& p) noexcept { target = p; }

    float processSample (float inFS) noexcept
    {
        // --- parameter smoothing (per sample, cheap one-poles on pot positions)
        smoothed.freq += potSmoothCoeff * (target.freq - smoothed.freq);
        smoothed.fizz += potSmoothCoeff * (target.fizz - smoothed.fizz);
        smoothed.lpfQ += potSmoothCoeff * (target.lpfQ - smoothed.lpfQ);
        smoothed.dry  += potSmoothCoeff * (target.dry  - smoothed.dry);
        smoothed.vol  += potSmoothCoeff * (target.vol  - smoothed.vol);
        smoothed.starve += potSmoothCoeff * (target.starve - smoothed.starve);
        smoothed.boost6Gain += potSmoothCoeff * (target.boost6Gain - smoothed.boost6Gain);

        // v0.21: effective rail. 9 V is the reference design; 18 V doubles the
        // analogue headroom; the starve pot sags the DIRT/output rail toward
        // 5 V (never below — the digital 3.3/5 V rails are separately
        // regulated and never starved).
        const float vEff  = target.supplyV - smoothed.starve * (target.supplyV - 5.0f);
        railC             = vEff / 9.0f;               // clip ceiling re: 9 V FS
        starveA           = smoothed.starve;
        smoothed.gain += potSmoothCoeff * (target.gain - smoothed.gain);

        if (--coeffCounter <= 0)
        {
            coeffCounter = 16;      // recompute filter/VCO coefficients every 16 samples
            updateDerived (false);
        }

        // ==== Stage 1: jack -> input network -> buffer (U1.1) ================
        // DRY is tapped clean; the fixed +15 dB trim only feeds the 567 branch.
        float v = inFS * tune.jackVoltsPerFS;          // volts at the jack
        v = inputDCBlock.process (v);                  // C1/R3 DC block (R1/R2 ~ unity)
        v = inHP2.process (inHP1.process (v));         // v0.10: 24 dB/oct low-cut @ 40 Hz
        const float vDry = detail::opampClip (v);      // buffer output == DRY tap

        // ==== Stage 2: gain stage (U1.3), +46.6 dB, clips to the rails =======
        const float trim = std::exp2 (tune.fixedTrimDb * 0.166096f); // 10^(dB/20)
        const float v567src = vDry * trim;             // trim feeds only this branch
        float vGain = v567src + 213.6f * gainShelfHP.process (v567src);
        vGain = detail::opampClip (vGain);

        // ==== Stage 3: LM567 PLL =============================================
        // input pin: AC coupled + noise floor, then the input limiter/comparator
        const float v567 = in567HP.process (vGain) + tune.noiseFloorV * rng.white();
        if (sIn > 0.0f) { if (v567 < -tune.inHysteresisV) sIn = -1.0f; }
        else            { if (v567 >  tune.inHysteresisV) sIn =  1.0f; }

        // VCO (current-controlled oscillator) with loop-filter frequency pull
        const float loopV = std::clamp (loopFilter.y, -1.0f, 1.0f);
        const float fVco  = f0 * (1.0f + tune.vcoPullRange * loopV);
        vcoPhase += (double) (fVco / fs);
        if (vcoPhase >= 1.0) vcoPhase -= 1.0;
        const float sVco  = (vcoPhase < 0.5)                     ? 1.0f : -1.0f;
        const float sVcoQ = (vcoPhase < 0.25 || vcoPhase >= 0.75) ? 1.0f : -1.0f;

        // phase detectors (XOR-style on ±1 squares)
        loopFilter.process (sIn * sVco);              // -> pulls the VCO
        const float quad = ofilFilter.process (sIn * sVcoQ); // -> lock detector

        // output comparator with hysteresis; with no OFIL cap it chatters at
        // audio rate — this chatter is the pedal's voice
        if (!detected) { if (quad > tune.detOnLevel)  detected = true;  }
        else           { if (quad < tune.detOffLevel) detected = false; }

        // ==== Q open-collector node with R16 100k pull-up ====================
        const float qTargetV = detected ? qLowV : qHighV;
        vQ += (detected ? qFallCoeff : qRiseCoeff) * (qTargetV - vQ);

        // ==== Stage 3b: always-on dirt in the DRY path only (v0.9) ==========
        const float vDirt = processDirt (vDry);

        // ==== Stage 4: inverting mixer (U1.2) — raw 567 wet + dirty dry ======
        const float vMix = detail::opampClip (-(wetGain * vQ + dryGain * vDirt));

        // ==== Stage 5: envelope filter (single LP/BP/HP, Off = bypass) ======
        const float vOut = (target.lpfMode > 0)
                               ? detail::opampClip (fizzLPF.process (vMix))
                               : vMix;

        // ==== v0.24 output chain: voicing -> +6 dB boost -> rail soft clip ===
        // (Jason: the switchable +6 dB is the 2nd-to-last thing, feeding the
        // clip stage directly. The +15 dB pre-567 trim is untouched, wet only.)
        float o = outDCBlock.process (vOut);
        o = outHP.process (o);                         // 12 dB/oct low-cut @ 60 Hz
        o = outPeak.process (o);                       // +3 dB Q0.5 bell @ 800 Hz
        o *= smoothed.boost6Gain;                      // +6 dB boost (switchable)
        return clipStage (o / tune.jackVoltsPerFS);    // clip: the LAST thing
    }

    // ------------------------------------------------------------------------
    // v0.21..v0.24 output clip stage.
    // Ladder: Jason's ratio ladder (2:1 / 4:1 / 8:1 output-referred bands)
    // with quadratic knees, referenced to the effective rail. Three onsets,
    // named by where compression begins below the rail:
    //   -9 ladder: bands -9/-6/-3/0, 6 dB knees, unity below -15, rail at +33
    //   -6 ladder: bands -6/-4/-2/0, 4 dB knees, unity below  -8, rail at +22
    //   -3 ladder: bands -3/-2/-1/0, 2 dB knees, unity below  -4, rail at +11
    // JFET (D): a J201 square-law common-source stage biased at half
    // pinch-off (Fetzer-Valve style) — smooth curvature everywhere, the
    // cutoff side rounds to a perfect zero-slope stop at +0.5c, the ohmic
    // side runs hotter and corners at -1.5c. Big 2nd harmonic, asymmetric.
    // v0.24 asymmetric modes clip each polarity with a different ladder
    // (onset 0 = no ladder, just the bare rail as a hard stop) — in hardware:
    // different biased-diode-divider stacks per polarity of the feedback net.
    // Everything scales with the effective rail (18 V / starve).
    // ------------------------------------------------------------------------
    float ladderDb (float Li, int onset) const noexcept
    {
        if (onset == 6)
        {   // -6 ladder — output bands -6/-4/-2/0, 4 dB knees
            if      (Li <= -4.0f)  { const float d = Li + 8.0f; return -8.0f + d - d * d / 16.0f; }
            else if (Li <= 0.0f)   { const float d = Li + 4.0f; return -5.0f + 0.5f * d - d * d / 32.0f; }
            else if (Li <= 4.0f)   return -3.5f + 0.25f * Li;
            else if (Li <= 8.0f)   { const float d = Li - 4.0f; return -2.5f + 0.25f * d - d * d / 64.0f; }
            else if (Li <= 22.0f)  return -1.75f + 0.125f * (Li - 8.0f);
            return 0.0f;
        }
        if (onset == 3)
        {   // -3 ladder — output bands -3/-2/-1/0, 2 dB knees
            if      (Li <= -2.0f)  { const float d = Li + 4.0f; return -4.0f + d - d * d / 8.0f; }
            else if (Li <= 0.0f)   { const float d = Li + 2.0f; return -2.5f + 0.5f * d - d * d / 16.0f; }
            else if (Li <= 2.0f)   return -1.75f + 0.25f * Li;
            else if (Li <= 4.0f)   { const float d = Li - 2.0f; return -1.25f + 0.25f * d - d * d / 32.0f; }
            else if (Li <= 11.0f)  return -0.875f + 0.125f * (Li - 4.0f);
            return 0.0f;
        }
        // -9 ladder — output bands -9/-6/-3/0, 6 dB knees
        if      (Li <= -6.0f)  { const float d = Li + 12.0f; return -12.0f + d - d * d / 24.0f; }
        else if (Li <= 0.0f)   { const float d = Li + 6.0f;  return  -7.5f + 0.5f * d - d * d / 48.0f; }
        else if (Li <= 6.0f)   return -5.25f + 0.25f * Li;
        else if (Li <= 12.0f)  { const float d = Li - 6.0f;  return -3.75f + 0.25f * d - d * d / 96.0f; }
        else if (Li <= 33.0f)  return -2.625f + 0.125f * (Li - 12.0f);
        return 0.0f;
    }

    // below the first knee the signal passes untouched
    float ladderLo (int onset) const noexcept
    {
        return onset == 9 ? 0.2512f          // 10^(-12/20)
             : onset == 6 ? 0.3981f          // 10^( -8/20)
                          : 0.6310f;         // 10^( -4/20)
    }

    // one polarity through one ladder (ax >= 0). onset 0 = bare rail (hard).
    float halfClip (float ax, int onset) const noexcept
    {
        const float c = railC;
        if (onset == 0)
            return std::min (ax, c);
        const float a = ax / c;
        if (a < ladderLo (onset))
            return ax;
        return c * std::pow (10.0f, ladderDb (20.0f * std::log10 (a), onset) * 0.05f);
    }

    float ladderClip (float x, int onset) const noexcept
    {
        return x >= 0.0f ? halfClip (x, onset) : -halfClip (-x, onset);
    }

    // v0.24: different ladder per polarity (positive half first)
    float asymClip (float x, int posOnset, int negOnset) const noexcept
    {
        return x >= 0.0f ? halfClip (x, posOnset) : -halfClip (-x, negOnset);
    }

    float jfetStage (float x) noexcept
    {
        const float c = railC;
        const float u = x / c;                       // swing re: pinch-off
        float y;
        if      (u >=  1.0f) y =  0.5f;              // cutoff: zero-slope stop
        else if (u <= -1.0f) y = -1.5f;              // ohmic corner
        else                 y = u - 0.5f * u * u;   // square law
        return jfetDC.process (y * c);               // output cap: block the DC
    }

    float clipStage (float x) noexcept
    {
        // v0.32 internal switches: JFET (ships ON) -> -3/-6 ladder (ships
        // OFF). Both off = the bare op-amp rail as a hard stop.
        if (target.jfetOn)
        {
            const float y = jfetStage (x);
            return target.ladder36 ? asymClip (y, 3, 6) : y;
        }
        if (target.ladder36)
            return asymClip (x, 3, 6);
        return x >= 0.0f ? std::min (x, railC) : std::max (x, -railC);
    }

    // handy for UI / debugging
    float getVcoFrequency() const noexcept { return f0; }
    float getFizzCutoff()  const noexcept { return fizzFc; }

private:
    // ------------------------------------------------------------------------
    // v0.9 dirt models — each maps to a genuinely tiny hardware circuit:
    //   0 Electra:      1 Si transistor + 2 clipping diodes (hard-ish crunch)
    //   1 Fuzz Face Ge: 2 Ge transistors, bias sag -> woolly, sputtery
    //   2 Bazz Fuss:    1 transistor + 1 diode, gated "velcro" fuzz
    //   3 Op-Amp OD:    spare TL074 half + 2 feedback diodes (smooth, tight lows)
    //   4 Octave Fuzz:  Green Ringer style full-wave rectifier (octave-up ring)
    // ------------------------------------------------------------------------
    static inline float dirtClip (float v, float level) noexcept   // hard-ish knee
    {
        const float x = v / level;
        const float x2 = x * x, x4 = x2 * x2, x8 = x4 * x4;
        return level * x / std::pow (1.0f + x8, 0.125f);
    }

    float processDirt (float vIn) noexcept
    {
        float y;
        switch (target.dirtType)
        {
            default:
            case 0: // Electra: symmetric Si diode pair, crunchy and immediate
                y = dirtClip (dirtG * vIn, 0.55f);
                break;

            case 1: // Fuzz Face Ge: bias sags with level -> warm, compressed, sputters
            {
                sagEnv += sagCoeff * (std::fabs (dirtPrevY) - sagEnv);
                const float u = dirtG * vIn - 0.6f * sagEnv;
                y = u > 0.0f ? 0.42f * std::tanh (u / 0.42f)
                             : 0.27f * std::tanh (u / 0.27f);
                break;
            }

            case 2: // Bazz Fuss: dead zone (gated) + asymmetric hard clip -> velcro
            {       // v0.21: starving sags the bias, widens the dead zone,
                    // chokes the rails and gates the tails -> sputtery
                float u = dirtG * vIn + 0.15f * starveA;             // bias shift
                const float dz = 0.08f + 0.32f * starveA * starveA;  // gate grows
                u = u > dz ? u - dz : (u < -dz ? u + dz : 0.0f);
                y = u > 0.0f ? std::min (u * 1.2f,  0.5f  * railC)
                             : std::max (u * 1.6f, -0.32f * railC);
                const float gap = 0.15f * starveA * starveA;         // sputter gate
                if (gap > 0.0f)
                {
                    const float a = std::fabs (y);
                    if (a < gap) { const float t = a / gap; y *= t * t * t; }
                }
                break;
            }

            case 3: // Op-Amp OD: tight lows in, smooth symmetric soft clip
            {
                const float u = odPreHP.process (vIn) * dirtG * 0.6f;
                y = 0.5f * std::tanh (u / 0.5f);
                break;
            }

            case 4: // Octave Fuzz: full-wave rectifier (AC-coupled, like the
            {       // Green Ringer's output cap) folded in, then clipped
                const float u   = dirtG * vIn;
                const float oct = octHP.process (std::fabs (u));
                y = dirtClip (0.45f * u + 1.1f * oct, 0.5f);
                break;
            }
        }
        dirtPrevY = y;
        y = dirtDC.process (y);       // asymmetric clipping makes DC — block it
        return dirtLP.process (y);    // per-model top-end voicing
    }

    void updateDerived (bool force) noexcept
    {
        // ---- v0.3 extended FREQ: 0.1 Hz .. 18 kHz, log --------------------------
        // (the stock RT/CT network gave 304-1148 Hz; this is Jason's wishlist
        //  range — hardware will need switched timing caps to match)
        // v0.32: FREQ range 0.2 Hz .. 6 kHz (was 0.1 Hz .. 18 kHz)
        f0 = std::min (0.2f * std::pow (30000.0f, smoothed.freq), 0.4f * fs);

        // ---- v0.9 dirt: GAIN 0..1 -> x2 .. x300 (log), per-model voicing ------
        dirtG = 1.1f * std::pow (272.727f, smoothed.gain);   // v0.19: x1.1 .. x300
        dirtLP.setCutoff (target.dirtType == 1 ? 3500.0f
                        : target.dirtType == 3 ? 6000.0f : 9000.0f, fs);

        // ---- v0.8 filter: extended Mu-Tron sweep, LP/BP/HP, Q 0.25..8 log -----
        // Lo: 20 Hz .. 4 kHz; Hi: 44.2 Hz .. 8.84 kHz (Hi = Lo x2.21)
        fizzFc = (target.lpfRangeHi != 0 ? 44.2f : 20.0f) * std::pow (200.0f, smoothed.fizz);
        const float fizzQ = 0.25f * std::pow (32.0f, smoothed.lpfQ);
        if (target.lpfMode > 0)
            fizzLPF.setup (target.lpfMode - 1, fizzFc, fizzQ, fs);

        // ---- Q node levels: pull-up 100k to 9V vs. mixer load to 4.5V --------
        // v0.8: the filter no longer loads the Q node; the VOL pot + R9 (1M)
        // path is a much lighter load
        const float rLoad = 500000.0f;
        const float g16 = 1.0f / 100000.0f, gL = 1.0f / rLoad;
        qHighV = (9.0f * g16 + 4.5f * gL) / (g16 + gL) - 4.5f; // delta volts (+0.75..+2.4)
        qLowV  = 0.15f - 4.5f;                                  // hard saturation low

        const float tauRise = (100000.0f * rLoad / (100000.0f + rLoad)) * tune.qNodeStrayC;
        qRiseCoeff = 1.0f - std::exp (-1.0f / (std::max (tauRise, 1e-7f) * fs));
        qFallCoeff = 1.0f - std::exp (-1.0f / (std::max (tune.qFallTauSec, 1e-7f) * fs));

        // ---- mixer gains: MIX crossfade (v0.6) scaled by VOL master ----------
        // mix 0..0.5: dry stays 100%, FX rises 0->100%
        // mix 0.5..1: FX stays 100%, dry falls 100%->0
        const float mix    = smoothed.dry;
        const float dryMix = mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix);
        const float wetMix = mix >= 0.5f ? 1.0f : 2.0f * mix;

        const float aV  = detail::audioTaper (smoothed.vol);
        const float rwV = aV * (1.0f - aV) * 100000.0f; // VOL wiper source impedance
        wetGain = wetMix * aV * 100000.0f / (1000000.0f + rwV);  // R11 / (R9 + Rw)
        dryGain = dryMix * aV * 100000.0f / (100000.0f  + rwV);  // R11 / (R10 + Rw)

        if (force)
            smoothed = target;
    }

    float fs = 48000.0f;

    Params target, smoothed;
    float  potSmoothCoeff = 0.01f;
    int    coeffCounter   = 0;

    // stage filters
    detail::OnePoleHP inputDCBlock, gainShelfHP, in567HP, outDCBlock, dirtDC, odPreHP, octHP;
    detail::OnePoleHP jfetDC;   // v0.22: JFET stage output cap
    detail::OnePoleLP loopFilter, ofilFilter, dirtLP;
    detail::BiquadLP  fizzLPF;
    detail::BiquadLP  inHP1, inHP2, outHP, outPeak;   // v0.10 fixed voicing

    // dirt state (v0.9)
    float dirtG = 2.0f, sagEnv = 0.0f, sagCoeff = 0.001f, dirtPrevY = 0.0f;
    float railC = 1.0f, starveA = 0.0f;   // v0.21 effective rail / starve
    detail::Rng       rng;

    // PLL state
    double vcoPhase = 0.0;
    float  sIn      = 1.0f;
    bool   detected = false;

    // derived values
    float f0 = 700.0f, fizzFc = 3000.0f;
    float vQ = 0.0f, qHighV = 1.5f, qLowV = -4.35f;
    float qRiseCoeff = 1.0f, qFallCoeff = 1.0f;
    float wetGain = 0.05f, dryGain = 0.5f;
};

} // namespace glitchwave
