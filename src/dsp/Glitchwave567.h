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
    // v0.39 - the two DNP pads on the real board, now switchable in the sim.
    // C41 sits on LM567 pin 2 (loop filter), C42 on pin 1 (output filter).
    // Both pins present ~4.7k of internal resistance, so each pad is just a
    // single real pole at 1/(2*pi*R*C). Values are what a builder would
    // actually try in those 0603 pads. C41 = 1u is the datasheet's classic
    // narrow-band loop cap. C42 = 220n is deliberately NOT the datasheet's
    // "C3 >= 2 x C2": that much output filtering stops pin 8 moving at all
    // and the wet path goes silent everywhere, which is a mute switch, not a
    // mod. 220n averages over a few cycles instead, so the decoder only stops
    // chattering when a tone actually sits on f0 (or a harmonic of it).
    float pin12R         = 4700.f;  // internal resistance at LM567 pins 1 and 2
    float c41Farads      = 1.0e-6f; // C41 = LFIL, pin 2  -> 33.9 Hz loop pole
    float c42Farads      = 220e-9f; // C42 = OFIL, pin 1  -> 154 Hz output pole
    float detOnLevel     = 0.35f;   // Schmitt comparator: Q turns ON (low) above this
    float detOffLevel    = 0.15f;   // Schmitt comparator: Q turns OFF below this
    float inHysteresisV  = 0.005f;  // input limiter comparator hysteresis (volts)
    float qNodeStrayC    = 220e-12f;// stray capacitance at the Q/pull-up node
    float qFallTauSec    = 2e-6f;   // Q transistor saturating (fast fall)
    float noiseFloorV    = 1.5e-4f; // input-referred noise at the 567 input (idle bleed)
    float jackVoltsPerFS = 2.0f;    // 1.0 full-scale sample == this many volts at the jack
    float fixedTrimDb    = 15.0f;   // v0.8: fixed drive into the 567 branch (knob removed)

    // ---- v0.41, mirroring hardware rev 7 -----------------------------------
    // V567 is its own rail now: VA through D105 + D107 (two 1N4148W in
    // series), so 9.0 V minus roughly 1.5 V of diode drop. The old 8.7 V
    // target was above TI's 8.5 V recommended maximum for the LM567C, and a
    // resistive divider could not hold ANY target because the chip swings
    // 7-10 mA idle to 12-15 mA activated. Two diodes are about six times
    // stiffer over that swing (a diode's dynamic resistance at 12 mA is only
    // ~2.2 ohm). R16 pulls the Q node up to THIS rail, not to VA, so the
    // lower rail costs the wet path a slice of its swing -- that is the
    // audible price of getting inside the datasheet, and it is modelled.
    // STARVE does not reach here: rev 7 starves VDIRT, not VA.
    float v567RailV      = 7.5f;    // VA 9.0 V - D105 - D107
    // Rev 7 moves the J201 Fetzer Valve in FRONT of the Bazz Fuss and hangs
    // its drain on VDIRT, so STARVE reaches it. It runs at natural gain --
    // no pad, no degeneration trim -- which for a J201 Fetzer is roughly
    // x4 to x10 depending on the individual part's Idss. 6 is the middle.
    // Consequence, and it is the real one: a natural-gain JFET turns a
    // 100-300 mV guitar into 0.4-3 V, and the Bazz Fuss clips at a few
    // hundred mV, so with the JFET IN, GAIN at minimum is already full
    // fuzz. The hardware fix is a fixed pad BETWEEN the JFET and the fuss
    // (attenuating before the JFET just starves it of the level it needs to
    // curve). That pad is still open, so the sim runs it unpadded too.
    float jfetGain       = 6.0f;    // small-signal voltage gain at a full rail
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
        float gain        = 0.5f;  // v0.44: dirt GAIN 0..1 -> x0.1 (-20 dB) .. x10
                                   // (fuzz wall). 0.5 = unity, dead centre.
        int   dirtType    = 0;     // v0.9: 0 Electra, 1 Fuzz Face Ge, 2 Bazz Fuss,
                                   //       3 Op-Amp OD, 4 Octave Fuzz (always on, dry path only)
        // v0.21 power modelling. v0.41 / rev 7: 9 V is the only rail now.
        // The 12/15/18 V options are gone from the hardware, so they are gone
        // from the sim; STARVE still sags this one linearly to a 1 V floor.
        float supplyV     = 9.0f;  // centre-negative adapter, 9 V, fixed
        float starve      = 0.0f;  // 0..1 secret starve: rail sags LINEARLY
                                   // from 9 V down to a 1 V floor (v0.39)
        // v0.41 / rev 7: ONE internal audio switch left. The -3/-6 ladder and
        // the +6 dB boost are off the board. The JFET has moved from the
        // output chain to the front of the Bazz Fuss, on VDIRT, so STARVE
        // reaches it. It ships OUT.
        bool  jfetOn      = false;
        // v0.39: the two DNP filter pads at the LM567. Both ship OUT, which
        // is how the board is built: the decoder never settles, so pin 8
        // chatters at audio rate and that chatter is the pedal's voice.
        bool  c41LoopCap  = false;   // pin 2, LFIL
        bool  c42OutCap   = false;   // pin 1, OFIL
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
        // loop / output filter poles are owned by updateDerived(), because
        // C41 and C42 can be switched in and out while running (v0.39)
        loopHzCur = ofilHzCur = -1.0f;
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

        // v0.21/v0.39/v0.41: effective rail. Rev 7 settles on one adapter
        // voltage, 9 V, so there is nothing to select any more. Starve models
        // a real 9 V / 100 mA rated wall-wart or battery -- a genuinely
        // realistic rating for a single small stompbox. Per spec this is a
        // plain LINEAR sag: Starve travels a straight line from 9 V all the
        // way down to a hard 1 V floor (the digital 3.3/5 V rails and the
        // LM567's own diode-dropped rail are separate and never starved; in
        // rev 7 this sagging rail IS VDIRT, which is why the JFET and the
        // Bazz Fuss both die on it). We let it go all the
        // way to 1 V even though every real op-amp/JFET stage in this
        // circuit would already be dead and silent long before that -- the
        // point is to hear the whole death spiral, not stop short of it.
        constexpr float kFloorV = 1.0f;    // absolute rail floor, fully collapsed
        const float vEff  = target.supplyV - smoothed.starve * (target.supplyV - kFloorV);
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
        // Loop polarity: the phase detector feeds the VCO with a MINUS sign, so
        // the stable lock sits a quarter cycle the other way round and the
        // quadrature detector below reads +1 when locked, which is the real
        // chip's convention (lock pulls pin 8 low). With no loop cap the loop
        // never settles anyway, so this sign is inaudible until C41 goes in.
        const float fVco  = f0 * (1.0f - tune.vcoPullRange * loopV);
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
        // v0.41 / rev 7: SW1 puts the J201 Fetzer Valve IN FRONT of the dirt
        // instead of after the mixer. Its drain rail is VDIRT, the same rail
        // STARVE collapses, so starving it kills the JFET's gain and headroom
        // together and the fuss behind it goes quiet from the input side as
        // well as from its own bias. At natural gain this hits the Bazz Fuss
        // with several volts, so with SW1 in, GAIN minimum is already fuzz.
        const float vDirt = processDirt (target.jfetOn ? jfetStage (vDry) : vDry);

        // ==== Stage 4: inverting mixer (U1.2) — raw 567 wet + dirty dry ======
        const float vMix = detail::opampClip (-(wetGain * vQ + dryGain * vDirt));

        // ==== Stage 5: envelope filter (single LP/BP/HP, Off = bypass) ======
        const float vOut = (target.lpfMode > 0)
                               ? detail::opampClip (fizzLPF.process (vMix))
                               : vMix;

        // ==== v0.41 output chain: voicing -> rail clip =======================
        // Rev 7 deletes the +6 dB boost and the -3/-6 ladder from the board,
        // so the output stage is now just the fixed voicing into the op-amp
        // rail. The +15 dB pre-567 trim is untouched, wet only.
        float o = outDCBlock.process (vOut);
        o = outHP.process (o);                         // 12 dB/oct low-cut @ 60 Hz
        o = outPeak.process (o);                       // +3 dB Q0.5 bell @ 800 Hz
        return clipStage (o / tune.jackVoltsPerFS);    // clip: the LAST thing
    }

    // ------------------------------------------------------------------------
    // v0.41 output clip stage + JFET stage (hardware rev 7).
    //
    // The -3/-6 ratio ladder and the +6 dB boost are off the board, so the
    // last thing in the chain is simply the op-amp hitting the effective
    // rail. It still scales with STARVE through railC.
    //
    // The JFET did NOT get deleted, it MOVED: rev 7 puts the J201 Fetzer
    // Valve in FRONT of the Bazz Fuss (see processSample, stage 3b), with its
    // drain on VDIRT so STARVE reaches it. It is a square-law common-source
    // stage biased at half pinch-off: smooth curvature everywhere, the cutoff
    // side rounding to a zero-slope stop and the ohmic side cornering harder,
    // which is where the big second harmonic and the asymmetry come from.
    //
    // v0.41 also gives it its REAL voltage gain instead of running it at
    // unity. tune.jfetGain is the small-signal gain at a full rail (6 is the
    // middle of a J201 Fetzer's natural x4..x10 spread). Both the gain and
    // the usable input swing scale with the drain rail, because a starved
    // JFET loses gm and headroom at the same time -- crank STARVE and the
    // stage does not just get quieter, it stops curving.
    // ------------------------------------------------------------------------
    float jfetStage (float x) noexcept
    {
        // usable input swing, referred to half pinch-off (~0.4 V on a J201 at
        // a full rail), and the gm*Rd gain. Both collapse with VDIRT.
        const float k  = std::max (railC, 0.05f);
        const float vp = 0.4f * k;
        const float g  = tune.jfetGain * k;
        const float u  = x / vp;                     // swing re: half pinch-off
        float y;
        if      (u >=  1.0f) y =  0.5f;              // cutoff: zero-slope stop
        else if (u <= -1.0f) y = -1.5f;              // ohmic corner
        else                 y = u - 0.5f * u * u;   // square law
        return jfetDC.process (y * vp * g);          // output cap: block the DC
    }

    float clipStage (float x) noexcept
    {
        // rev 7: the bare op-amp rail, hard stop. Nothing else lives here.
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

        // ---- v0.39 LM567 filter pads: C41 (pin 2, LFIL) and C42 (pin 1, OFIL) --
        // Each pad works into the chip's internal ~4.7k, so fitting one moves a
        // single real pole from "stray only" down to 1/(2*pi*R*C). Empty pads
        // leave the loop and the lock detector wide open, which is why the as-
        // built board chatters instead of decoding. Recomputed here rather than
        // in prepare() so the switches work while audio is running.
        const float loopHz = target.c41LoopCap
                               ? 1.0f / (2.0f * detail::kPi * tune.pin12R * tune.c41Farads)
                               : tune.loopStrayHz;
        const float ofilHz = target.c42OutCap
                               ? 1.0f / (2.0f * detail::kPi * tune.pin12R * tune.c42Farads)
                               : tune.ofilStrayHz;
        if (force || loopHz != loopHzCur) { loopFilter.setCutoff (loopHz, fs); loopHzCur = loopHz; }
        if (force || ofilHz != ofilHzCur) { ofilFilter.setCutoff (ofilHz, fs); ofilHzCur = ofilHz; }

        // ---- v0.44 dirt: GAIN 0..1 -> x0.1 .. x10 (log), per-model voicing ----
        // Two decades, +/-20 dB about unity, and because the ends are
        // reciprocal (max = 1/min) UNITY lands exactly at the knob's centre:
        // 0.1 * 100^0.5 = 1.0. That is the rule for any log knob -- noon sits
        // at the geometric mean of the two ends.
        // The bottom half is real attenuation, which is what rev 7's
        // natural-gain JFET needs: with SW1 in, the fuss is being slammed, and
        // -20 dB is enough to walk it back and find the pad value by ear
        // before it gets soldered. The top is x10 rather than x100 or x300
        // because the fuss is long past its clip point by then; what those
        // extra decades really cost is resolution, since cramming 80 dB into
        // 300 degrees of rotation makes every small move a big jump.
        dirtG = 0.1f * std::pow (100.0f, smoothed.gain);
        dirtLP.setCutoff (target.dirtType == 1 ? 3500.0f
                        : target.dirtType == 3 ? 6000.0f : 9000.0f, fs);

        // ---- v0.8 filter: extended Mu-Tron sweep, LP/BP/HP, Q 0.25..8 log -----
        // Lo: 20 Hz .. 4 kHz; Hi: 44.2 Hz .. 8.84 kHz (Hi = Lo x2.21)
        fizzFc = (target.lpfRangeHi != 0 ? 44.2f : 20.0f) * std::pow (200.0f, smoothed.fizz);
        const float fizzQ = 0.25f * std::pow (32.0f, smoothed.lpfQ);
        if (target.lpfMode > 0)
            fizzLPF.setup (target.lpfMode - 1, fizzFc, fizzQ, fs);

        // ---- Q node levels: R16 100k up to V567 vs. mixer load to 4.5V -------
        // v0.8: the filter no longer loads the Q node; the VOL pot + R9 (1M)
        // path is a much lighter load.
        // v0.41 / rev 7: R16 pulls up to the LM567's OWN rail, not to VA. That
        // rail is now 7.5 V (VA through D105 + D107) instead of 9 V, because
        // TI's recommended maximum for the LM567C is 8.5 V. The cost lands
        // right here: the Q node's high level drops from about +3.75 V to
        // +2.50 V relative to the 4.5 V mixer reference, so the wet path loses
        // roughly 15% of its total swing. The MIX law absorbs it.
        const float rLoad = 500000.0f;
        const float g16 = 1.0f / 100000.0f, gL = 1.0f / rLoad;
        qHighV = (tune.v567RailV * g16 + 4.5f * gL) / (g16 + gL) - 4.5f;
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
    float loopHzCur = -1.0f, ofilHzCur = -1.0f;   // v0.39 C41/C42 pole cache
};

} // namespace glitchwave
