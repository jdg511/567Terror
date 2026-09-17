#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <DemoData.h>   // v0.40: the embedded demo clips (GwDemos target)

namespace
{
    constexpr int kOversampleFactorLog2 = 2; // 4x

    juce::String hzToText (float hz)
    {
        if (hz < 1.0f)     return juce::String (hz, 2) + " Hz";
        if (hz < 1000.0f)  return juce::String (juce::roundToInt (hz)) + " Hz";
        return juce::String (hz / 1000.0f, 2) + " kHz";
    }

    juce::String freqToText (float pos)   // v0.32: 0.2 Hz .. 6 kHz log
    {
        return hzToText (0.2f * std::pow (30000.0f, pos));
    }

    juce::String lpfToText (float pos)    // v0.8 ranges, shows Lo / Hi
    {
        const float lo = 20.0f * std::pow (200.0f, pos);
        return hzToText (lo) + " / " + hzToText (lo * 2.21f);
    }

    juce::String lpfQToText (float pos)   // Q 0.25 .. 8 log
    {
        return "Q " + juce::String (0.25f * std::pow (32.0f, pos), 2);
    }

    // v0.21 target lists (Jason's routing): LFO2 loses Gain; LFO1 gains the
    // env follower's Gain and output Level; the env follower gains LFO1
    // Rate/Depth. All three lists are 8 entries (one solid LED colour each).
    const juce::StringArray kLfo1Targets { "Off", "Freq", "LPF", "Res", "Mix", "Gain",
                                           "Env Gain", "Env Level" };
    const juce::StringArray kLfo2Targets { "Off", "Freq", "LPF", "Res", "Mix",
                                           "LFO1 Rate", "LFO1 Depth", "Env Gain" };
    const juce::StringArray kEnvTargets  { "Off", "Freq", "LPF", "Res", "Mix", "Gain",
                                           "LFO1 Rate", "LFO1 Depth" };
    // v0.18: Jason's two-bank plan — Bank A classics, Bank B fun stuff
    // (White/Pink Noise hold Bank B's two open slots until auditioned).
    const juce::StringArray kLfoShapes   { "Ramp Up", "Ramp Dn", "Square", "Triangle",
                                           "Sine", "Sweep", "Rand Slope", "S&H",
                                           "Lorenz", "Rossler", "Drunk Walk", "Perlin",
                                           "Wobble", "Glitch", "White Noise", "Pink Noise" };

    // list index -> enum maps
    using MT = wtf::ModTarget;
    using LS = wtf::LfoShape;
    constexpr MT kLfo1Map[] = { MT::Off, MT::Freq, MT::Fizz, MT::LpfQ, MT::Dry, MT::Gain,
                                MT::EnvAmount, MT::EnvLevel };
    constexpr MT kLfo2Map[] = { MT::Off, MT::Freq, MT::Fizz, MT::LpfQ, MT::Dry,
                                MT::Lfo1Rate, MT::Lfo1Depth, MT::EnvAmount };
    constexpr MT kEnvMap[]  = { MT::Off, MT::Freq, MT::Fizz, MT::LpfQ, MT::Dry, MT::Gain,
                                MT::Lfo1Rate, MT::Lfo1Depth };
    constexpr LS kShapeMap[] = { LS::RampUp, LS::RampDown, LS::Square, LS::Triangle,
                                 LS::Sine, LS::Sweep, LS::RandSlopes, LS::SampleHold,
                                 LS::Lorenz, LS::Rossler, LS::DrunkWalk, LS::PerlinDrift,
                                 LS::Wobble, LS::Glitch, LS::WhiteNoise, LS::PinkNoise };

    template <size_t N>
    int mapTarget (const MT (&table)[N], float rawIndex)
    {
        const auto i = juce::jlimit (0, (int) N - 1, (int) rawIndex);
        return (int) table[i];
    }

    inline void atomicMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }
}

WtfAudioProcessor::WtfAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    raw.freq     = apvts.getRawParameterValue ("freq");
    raw.fizz     = apvts.getRawParameterValue ("fizz");
    raw.lpfq     = apvts.getRawParameterValue ("lpfq");
    raw.dry      = apvts.getRawParameterValue ("dry");
    raw.vol      = apvts.getRawParameterValue ("vol");
    raw.dirtgain = apvts.getRawParameterValue ("dirtgain");
    raw.lfo1rate   = apvts.getRawParameterValue ("lfo1rate");
    raw.lfo1depth  = apvts.getRawParameterValue ("lfo1depth");
    raw.lfo1shape  = apvts.getRawParameterValue ("lfo1shape5");
    raw.lfo1target = apvts.getRawParameterValue ("lfo1target5");
    raw.lfo2rate   = apvts.getRawParameterValue ("lfo2rate");
    raw.lfo2depth  = apvts.getRawParameterValue ("lfo2depth");
    raw.lfo2shape  = apvts.getRawParameterValue ("lfo2shape4");
    raw.lfo2target = apvts.getRawParameterValue ("lfo2target4");
    raw.envtarget  = apvts.getRawParameterValue ("envtarget5");
    raw.envgain    = apvts.getRawParameterValue ("envgain");
    raw.envdrive   = apvts.getRawParameterValue ("envdrive");
    raw.envratio   = apvts.getRawParameterValue ("envratio");
    raw.envshape   = apvts.getRawParameterValue ("envshape");
    raw.envthresh  = apvts.getRawParameterValue ("envthresh");
    raw.lpfmode    = apvts.getRawParameterValue ("lpfmode3");
    raw.lpfrange   = apvts.getRawParameterValue ("lpfrange");
    raw.gatethresh  = apvts.getRawParameterValue ("gatethresh");
    raw.gatehold    = apvts.getRawParameterValue ("gatehold");
    raw.gatefade    = apvts.getRawParameterValue ("gatefade");
    raw.bypass      = apvts.getRawParameterValue ("bypass");
    raw.starve      = apvts.getRawParameterValue ("starve");
    raw.jfeton      = apvts.getRawParameterValue ("jfeton");
    raw.c41cap      = apvts.getRawParameterValue ("c41cap");
    raw.c42cap      = apvts.getRawParameterValue ("c42cap");
    raw.envattack   = apvts.getRawParameterValue ("envattack");
    raw.envdecay    = apvts.getRawParameterValue ("envdecay");
    raw.fuzzon      = apvts.getRawParameterValue ("fuzzon");
    raw.dec567on    = apvts.getRawParameterValue ("dec567on");
    raw.envfilton   = apvts.getRawParameterValue ("envfilton");
    raw.democlip    = apvts.getRawParameterValue ("democlip");
    raw.demovol     = apvts.getRawParameterValue ("demovol");

    // v0.40 demo player: Ogg comes in with registerBasicFormats(), so the
    // embedded clips decode with nothing extra enabled.
    demoFormats.registerBasicFormats();
    apvts.addParameterListener ("democlip", this);
    loadDemoClip ((int) raw.democlip->load());

    // v0.45: the pedal boots on Preset A. Nothing is saved yet at this point,
    // so A is the factory state: every knob at noon, all three circuits on.
    // If the host restores a session, setStateInformation runs after this and
    // overwrites it, which is the correct order.
    loadFactoryPresetA();
}

// ---------------------------------------------------------------------------
// v0.45  PRESETS
//
// Three slots. A slot is a whole APVTS snapshot, so it carries everything --
// the six knobs, the layers, the under-the-cover switches, the circuit kills.
// Nothing is excluded, because a preset that only recalls half the pedal is
// the fastest way to make presets feel broken.
//
// The pedal boots on slot A. Until you save something over it, slot A is the
// factory state below: every continuous control at 50 percent, and the fuzz,
// the 567 and the envelope filter all switched on.
// ---------------------------------------------------------------------------
void WtfAudioProcessor::savePreset (int slot)
{
    if (slot < 0 || slot > 2) return;
    presetState[slot] = apvts.copyState().createCopy();
    presetSaved[slot] = true;
}

void WtfAudioProcessor::recallPreset (int slot)
{
    if (slot < 0 || slot > 2) return;
    if (! presetSaved[slot])
    {
        // Nothing written there yet. Slot A falls back to the factory state;
        // B and C simply do nothing rather than silently loading A.
        if (slot == 0) loadFactoryPresetA();
        return;
    }
    apvts.replaceState (presetState[slot].createCopy());
}

void WtfAudioProcessor::loadFactoryPresetA()
{
    // "All settings at 50%" applies to the CONTINUOUS controls -- every knob
    // sits at noon. It deliberately does NOT reset the selectors (filter mode,
    // LFO shapes, mod targets): normalised 0.5 on a 5-way choice lands on
    // whatever happens to be third in the list, which is not a neutral state,
    // it is an arbitrary one. Those keep their designed defaults.
    //
    // The rest is exactly as specified: fuzz, 567 and envelope filter all on,
    // pedal engaged, and the demo player left alone because it is not part of
    // the sound.
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const juce::String id = rp->paramID;
        if (id == "democlip" || id == "demovol" || id == "preset3")
            continue;

        float target;
        if (id == "fuzzon" || id == "dec567on" || id == "envfilton")
            target = 1.0f;                              // all three circuits on
        else if (id == "bypass")
            target = 0.0f;                              // engaged, not bypassed
        // ---- v0.46: Preset A's named values ---------------------------------
        // These are Jason's, and between them they set the pedal up as a
        // Mu-Tron III sitting in front of the fuzz rather than a neutral grid
        // of 50 percents.
        else if (id == "envthresh")
            target = rp->convertTo0to1 (-96.0f);        // no gate = Mu-Tron
        else if (id == "envgain")
            target = rp->convertTo0to1 (4.0f);          // x4
        else if (id == "envratio")
            target = std::log (4.0f) / std::log (20.0f);   // 2:1 on the new range
        else if (id == "lfo2depth")
            target = 0.20f;                             // 20 %
        else if (id == "lfo2rate")
            target = rp->convertTo0to1 (0.5f);          // 0.5 Hz
        // v0.49: STARVE is the one knob whose noon is NOT a neutral value.
        // It is a brown-out control: 0 % leaves the rail at the full 9 V and
        // 100 % sags it to 1 V, so "every knob at noon" was booting the
        // factory preset on a half-dead battery. Preset A is a healthy pedal.
        else if (id == "starve")
            target = 0.0f;                              // 9.0 V, no sag
        else if (dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr)
            target = 0.5f;                              // the 50% rule: noon
        else
            continue;                                   // selectors keep theirs

        rp->beginChangeGesture();
        rp->setValueNotifyingHost (target);
        rp->endChangeGesture();
    }
}

WtfAudioProcessor::~WtfAudioProcessor()
{
    apvts.removeParameterListener ("democlip", this);
    cancelPendingUpdate();
}

// ---- v0.40 demo player ----------------------------------------------------------
// Display names, in the same order as the SOURCES list in CMakeLists.txt.
// The index is what gets saved in the session, so only ever append.
juce::StringArray WtfAudioProcessor::demoClipNames()
{
    return {
        "Arpeggio - Quick Clean",       "Arpeggio - Deluxe Clean",
        "Arpeggio - Iconic Clean-ish",  "Arpeggio - Warm Crunch",
        "Arpeggio - Dirty Punk",
        "Rhythm - Fender Clean",        "Rhythm - Suhr Clean",
        "Rhythm - AC30 Crunch",
        "Power Chords - Iconic Clean-ish", "Power Chords - Plexi",
        "Power Chords - Punk Rock",     "Power Chords - Classic Hi-Gain",
        "Solo - Mesa",                  "Solo - Diezel",
        "Solo - Fortin",
        "Metal - 5150",                 "Metal - Blackstar",
        "Metal - Dual Rec",
        "Pick Bass - Clean Bright",     "Pick Bass - Bite",
        "Pick Bass - Growl",            "Pick Bass - Rock Classic",
        "Pick Bass - Metalcore",
        "Finger Bass - Clean Bright",   "Finger Bass - Nice Warm",
        "Finger Bass - Bite",           "Finger Bass - Growl"
    };
}

void WtfAudioProcessor::setDemoPlaying (bool shouldPlay) noexcept
{
    if (shouldPlay)
        demoPlayer.restart();
    demoPlaying.store (shouldPlay, std::memory_order_relaxed);
}

// may arrive on the audio thread, so it only flags and bounces to the message
// thread; decoding an Ogg is never done under the audio callback
void WtfAudioProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "democlip")
    {
        pendingDemoClip.store ((int) newValue, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
}

void WtfAudioProcessor::handleAsyncUpdate()
{
    loadDemoClip (pendingDemoClip.load (std::memory_order_relaxed));
}

void WtfAudioProcessor::loadDemoClip (int index)
{
    index = juce::jlimit (0, DemoData::namedResourceListSize - 1, index);
    if (index == loadedDemoClip)
        return;

    int size = 0;
    if (const char* data = DemoData::getNamedResource (DemoData::namedResourceList[index], size))
        if (demoPlayer.loadFromMemory (data, size, demoFormats,
                                       DemoData::originalFilenames[index]))
            loadedDemoClip = index;
}

juce::AudioProcessorValueTreeState::ParameterLayout
WtfAudioProcessor::createParameterLayout()
{
    using PF  = juce::AudioParameterFloat;
    using PC  = juce::AudioParameterChoice;
    using Att = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    // ---- the pedal itself ----------------------------------------------------
    // v0.32 ship defaults (Jason's spec): FREQ 0.5 Hz, LPF 200 Hz (Hi range),
    // Q 4, Mix D100/FX25, Vol 90 %
    layout.add (std::make_unique<PF> (juce::ParameterID { "freq", 1 }, "Freq",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.088883f,   // 0.5 Hz
        Att().withStringFromValueFunction ([] (float v, int) { return freqToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "fizz", 1 }, "LPF",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.28494f,    // 200 Hz @ Hi
        Att().withStringFromValueFunction ([] (float v, int) { return lpfToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "lpfq", 1 }, "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.8f,        // Q 4
        Att().withStringFromValueFunction ([] (float v, int) { return lpfQToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "dry", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.125f,      // D100 / FX25
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const int d = juce::roundToInt ((v <= 0.5f ? 1.0f : 2.0f * (1.0f - v)) * 100.0f);
                const int w = juce::roundToInt ((v >= 0.5f ? 1.0f : 2.0f * v) * 100.0f);
                return "D" + juce::String (d) + " / FX" + juce::String (w);
            })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "vol", 1 }, "Vol",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.9f, pct));   // 90 %

    // ---- always-on Bazz Fuss dirt in the dry path (v0.12: hardwired) --------------
    // v0.44: the GAIN range is x0.1 .. x10, two decades, +/-20 dB about unity.
    // The ends are reciprocal, which is what puts UNITY at the knob's exact
    // centre (noon on a log knob is the geometric mean of the two ends), and
    // two decades keeps the resolution usable instead of cramming 80 dB into
    // 300 degrees of rotation. Default 0.5 = x1.00.
    layout.add (std::make_unique<PF> (juce::ParameterID { "dirtgain", 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const float g = 0.1f * std::pow (100.0f, v);   // x0.1 .. x10
                return "x" + juce::String (g, 2);
            })));

    // ---- LFO stack ---------------------------------------------------------------
    auto hz2 = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (v, 2) + " Hz"; });
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo1rate", 1 }, "LFO1 Rate",
        juce::NormalisableRange<float> (0.2f, 20.0f, 0.0f, 0.35f), 0.5f, hz2));   // 0.5 Hz
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo1depth", 1 }, "LFO1 Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f, pct));           // 50 %
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo1shape5", 1 }, "LFO1 Shape",
        kLfoShapes, 4));   // default: Sine
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo1target5", 1 }, "LFO1 Target",
        kLfo1Targets, 1)); // default: Freq (LFO1 is always unipolar-up)
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo2rate", 1 }, "LFO2 Rate",
        juce::NormalisableRange<float> (0.2f, 20.0f, 0.0f, 0.35f), 0.2f, hz2));   // 0.2 Hz
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo2depth", 1 }, "LFO2 Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.15f, pct));          // 15 %
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo2shape4", 1 }, "LFO2 Shape",
        kLfoShapes, 4));   // default: Sine
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo2target4", 1 }, "LFO2 Target",
        kLfo2Targets, 5)); // default: LFO1 Rate

    // ---- envelope follower + filter switches ------------------------------------------
    layout.add (std::make_unique<PC> (juce::ParameterID { "envtarget5", 1 }, "Env Target",
        kEnvTargets, 2)); // default: LPF
    // v0.48: top of the range raised from x40 to x100. The skew is set so that
    // x4 -- the Mu-Tron-ish setting and Preset A's value -- sits at NOON:
    //   skew = log(0.5) / log((4 - 0.125) / (100 - 0.125)) = 0.2133
    layout.add (std::make_unique<PF> (juce::ParameterID { "envgain", 1 }, "Env Gain",
        juce::NormalisableRange<float> (0.125f, 100.0f, 0.0f, 0.2133f), 4.0f,
        Att().withStringFromValueFunction ([] (float v, int)
            { return "x" + juce::String (v, v < 2.0f ? 2 : (v < 20.0f ? 1 : 0)); })));
    layout.add (std::make_unique<PC> (juce::ParameterID { "envdrive", 1 }, "Env Drive",
        juce::StringArray { "Drive Up", "Drive Down" }, 0));
    // v0.38 secret Layer-A envelope shaping: Ratio (LPF knob), Shape (Freq
    // knob), Threshold (Gain knob). Bare numbers only, on purpose -- these
    // stay off the books like Starve always has.
    layout.add (std::make_unique<PF> (juce::ParameterID { "envratio", 1 }, "Env Ratio",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                // v0.48: the range is 1:2 .. 10:1 now, log, so r = 0.5 .. 10.
                // Standard compressor notation: N:1 is compression, 1:N is
                // expansion. Unity lands at v = log(2)/log(20) = 0.231, and
                // Preset A's 2:1 at v = log(4)/log(20) = 0.463.
                const float r = 0.5f * std::pow (20.0f, v);            // 0.5 .. 10
                return r >= 1.0f ? (juce::String (r, r < 10.0f ? 2 : 1) + ":1")
                                 : ("1:" + juce::String (1.0f / r, 2));
            })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "envshape", 1 }, "Env Shape",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            { return juce::String (std::pow (2.0f, 4.0f * (v - 0.5f)), 2); })));
    // v0.46: THRESHOLD is a real dB control now, -96 to -12 dB, not a bare
    // 0..1 number. -96 dB is effectively no gate at all, which is what the
    // Mu-Tron III itself does: its precision rectifier is built to detect
    // "even very small signals", and there is no threshold control anywhere on
    // the pedal. So -96 dB IS the Mu-Tron setting, and it is Preset A's value.
    layout.add (std::make_unique<PF> (juce::ParameterID { "envthresh", 1 }, "Env Threshold",
        juce::NormalisableRange<float> (-96.0f, -12.0f, 0.1f), -96.0f,
        Att().withStringFromValueFunction ([] (float v, int)
            { return juce::String (v, 1) + " dB"; })));

    // ---- v0.46 Mu-Tron III ballistics on the Layer Z rate knobs -------------
    // Stored 0..1 and mapped logarithmically so NOON is exactly the stock
    // Musitronics value. v0.46 widens the span from two decades to THREE
    // (1000:1, so half that either side of noon), which is what "extended but
    // reasonable" buys you:
    //     ATTACK  0.049 ms .. 1.551 ms .. 49 ms
    //     DECAY   5.0 ms   .. 158.7 ms .. 5.0 s
    // Log, not linear, on purpose: these are RC time constants, and the whole
    // point of item 9 is that the Mu-Tron's response must not be linearised.
    layout.add (std::make_unique<PF> (juce::ParameterID { "envattack", 1 }, "Env Attack",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const float ms = 1.551f * std::pow (1000.0f, v - 0.5f);
                return juce::String (ms, ms < 1.0f ? 3 : (ms < 10.0f ? 2 : 1)) + " ms";
            })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "envdecay", 1 }, "Env Decay",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const float ms = 158.7f * std::pow (1000.0f, v - 0.5f);
                return ms < 1000.0f ? (juce::String (ms, ms < 100.0f ? 1 : 0) + " ms")
                                    : (juce::String (ms * 0.001f, 2) + " s");
            })));

    // ---- v0.45 per-circuit kills, one per stomp ----------------------------
    // A kills the fuzz, B kills the 567, C kills the envelope filter. All
    // three ship ON, and Preset A's factory state turns all three on.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "fuzzon", 1 },   "Fuzz Circuit", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "dec567on", 1 }, "567 Circuit", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "envfilton", 1 }, "Env Filter Circuit", true));

    // ---- v0.45 preset slot ---------------------------------------------------
    // Which of the three slots is live. Boots on A.
    layout.add (std::make_unique<PC> (juce::ParameterID { "preset3", 1 }, "Preset",
        juce::StringArray { "A", "B", "C" }, 0));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lpfmode3", 1 }, "Filter Mode",
        juce::StringArray { "Off", "Mode LP", "Mode BP", "Mode HP", "Mode Notch" }, 1));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lpfrange", 1 }, "Filter Range",
        juce::StringArray { "Range Lo", "Range Hi" }, 1));   // default: Hi (Up/Hi)

    // v0.13: CV 1/2 are hardwired VCAs on LFO 1/2 depth — no CV parameters.

    // ---- output gate --------------------------------------------------------------------
    auto db1 = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (v, 1) + " dB"; });
    auto sec = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (v, v < 1.0f ? 2 : 1) + " s"; });
    layout.add (std::make_unique<PF> (juce::ParameterID { "gatethresh", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-96.0f, 0.0f, 0.1f), -48.0f, db1));
    layout.add (std::make_unique<PF> (juce::ParameterID { "gatehold", 1 }, "Gate Hold",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.0f, 0.4f), 1.0f, sec));
    layout.add (std::make_unique<PF> (juce::ParameterID { "gatefade", 1 }, "Gate Fade",
        juce::NormalisableRange<float> (0.1f, 60.0f, 0.0f, 0.4f), 30.0f, sec));

    // ---- v0.21 power + bypass ----------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
    // v0.41 / hardware rev 7: the -3/-6 ladder, the +6 dB boost and the
    // 12/15/18 V supply options are off the board, so they are gone from the
    // sim too. One audio switch is left under the cover, and it now sits in
    // front of the Bazz Fuss instead of after the mixer. It still ships OFF.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "jfeton", 1 }, "JFET Stage", false));
    // v0.39: the two DNP filter pads at the LM567 (C41 = pin 2 loop filter,
    // C42 = pin 1 output filter). Both ship OUT, matching the built board.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "c41cap", 1 }, "C41 Loop Cap", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "c42cap", 1 }, "C42 Output Cap", false));

    // ---- v0.40 demo player ---------------------------------------------------------
    // Clip choice and level are parameters so they save with the session and
    // the UI can use the normal attachments. Play/stop is transport, not a
    // parameter, and lives on the processor as a plain atomic.
    layout.add (std::make_unique<PC> (juce::ParameterID { "democlip", 1 }, "Demo Clip",
        demoClipNames(), 0));
    layout.add (std::make_unique<PF> (juce::ParameterID { "demovol", 1 }, "Demo Level",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db1));
    layout.add (std::make_unique<PF> (juce::ParameterID { "starve", 1 }, "Starve",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                // secret: rail sags LINEARLY from the supply down to 1 V (v0.39)
                // v0.49: read it out in VOLTS. Rev 7 is a single 9 V rail, so
                // the number on the panel is the actual VDIRT the fuzz is
                // running on: 9.0 V at 0 %, 1.0 V fully starved. "50 %" told
                // you how far the knob had turned; "5.0 V" tells you what the
                // circuit is living on, which is the thing you are listening
                // for.
                return juce::String (9.0f - 8.0f * v, 1) + " V";
            })));

    return layout;
}

void WtfAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, kOversampleFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    circuit.prepare (sampleRate * (1 << kOversampleFactorLog2));
    mod.prepare (sampleRate);

    monoBuffer.setSize (2, samplesPerBlock);
    cvBuffer.setSize (2, samplesPerBlock);

    // v0.40 demo player
    demoBuffer.setSize (1, samplesPerBlock);
    demoPlayer.prepare (sampleRate);
    demoGainCur = juce::Decibels::decibelsToGain (raw.demovol->load());
    demoEnv     = 0.0f;

    hostRate     = sampleRate;
    gateEnvCoeff = 1.0f - std::exp (-1.0f / (0.010f * (float) sampleRate));
    gateEnv      = 0.0f;
    gateAttenDb  = 0.0f;
    gateBelowSec = 0.0f;
    lastOutGain  = 1.0f;

    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
}

bool WtfAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled())
        return false;
    if (! ((in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo())
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())))
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! (sc.isDisabled() || sc == juce::AudioChannelSet::mono()
                               || sc == juce::AudioChannelSet::stereo()))
            return false;
    }
    return true;
}

void WtfAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || oversampling == nullptr)
        return;

    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int numIn  = mainIn.getNumChannels();
    const int numOut = mainOut.getNumChannels();

    // ---- live input mono sum ---------------------------------------------------
    float* mono = monoBuffer.getWritePointer (0);
    if (numIn > 0)
    {
        const float inScale = 1.0f / (float) numIn;
        juce::FloatVectorOperations::copyWithMultiply (mono, mainIn.getReadPointer (0),
                                                       inScale, numSamples);
        for (int ch = 1; ch < numIn; ++ch)
            juce::FloatVectorOperations::addWithMultiply (mono, mainIn.getReadPointer (ch),
                                                          inScale, numSamples);
    }
    else
        juce::FloatVectorOperations::clear (mono, numSamples);

    // ---- v0.40 demo player -----------------------------------------------------
    // Summed into the pedal's input ahead of everything else, so the clip runs
    // through the gate, the meters and the whole circuit exactly like a guitar
    // would. The gain ramps across the block, so moving the level knob and
    // hitting start/stop never clicks.
    {
        const bool  want      = demoPlaying.load (std::memory_order_relaxed);
        const float envTarget = want ? 1.0f : 0.0f;

        if (want || demoEnv > 0.0f)
        {
            const int nd = juce::jmin (numSamples, demoBuffer.getNumSamples());
            float* d = demoBuffer.getWritePointer (0);
            demoPlayer.process (d, nd, true, true);

            const float gTarget = juce::Decibels::decibelsToGain (raw.demovol->load());
            demoBuffer.applyGainRamp (0, 0, nd, demoGainCur * demoEnv, gTarget * envTarget);
            juce::FloatVectorOperations::add (mono, d, nd);

            demoGainCur = gTarget;
            demoEnv     = envTarget;
        }
    }

    // raw copy for the gate threshold + input meter
    monoBuffer.copyFrom (1, 0, mono, numSamples);
    atomicMax (meterPeaks[0], monoBuffer.getMagnitude (0, 0, numSamples));

    // ---- CV sources: sidechain L / R --------------------------------------------
    cvBuffer.clear();
    if (getBusCount (true) > 1)
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
            cvBuffer.copyFrom (0, 0, sc, 0, 0, numSamples);
        if (sc.getNumChannels() > 1)
            cvBuffer.copyFrom (1, 0, sc, 1, 0, numSamples);
        else if (sc.getNumChannels() == 1)
            cvBuffer.copyFrom (1, 0, sc, 0, 0, numSamples);
    }
    const float* cv1 = cvBuffer.getReadPointer (0);
    const float* cv2 = cvBuffer.getReadPointer (1);
    const float* liveIn = monoBuffer.getReadPointer (1);

    // ---- modulation setup ----------------------------------------------------------
    // v0.13: CVs are hardwired VCAs on their LFO's depth inside ModSystem —
    // the LFOs always stay on; the sidechain just breathes their depth.
    // v0.45: stomp C's single tap kills the envelope filter circuit outright,
    // which is the same thing the Mode=Off selector already did, so route both
    // through one flag rather than inventing a second path.
    const bool envFilterOn = raw.envfilton->load() >= 0.5f;
    const int  lpfModeIdx  = envFilterOn ? (int) raw.lpfmode->load() : 0;

    wtf::ModSystem::Params mp;
    mp.lfo1RateHz  = raw.lfo1rate->load();
    mp.lfo1Depth   = raw.lfo1depth->load();
    mp.lfo1Shape   = (int) kShapeMap[juce::jlimit (0, 15, (int) raw.lfo1shape->load())];
    mp.lfo1Target  = mapTarget (kLfo1Map, raw.lfo1target->load());
    mp.lfo2RateHz  = raw.lfo2rate->load();
    mp.lfo2Depth   = raw.lfo2depth->load();
    mp.lfo2Shape   = (int) kShapeMap[juce::jlimit (0, 15, (int) raw.lfo2shape->load())];
    mp.lfo2Target  = mapTarget (kLfo2Map, raw.lfo2target->load());
    mp.envTarget   = mapTarget (kEnvMap, raw.envtarget->load());
    // filter Mode Off also disables the envelope follower section
    mp.envGain     = lpfModeIdx == 0 ? 0.0f : raw.envgain->load();
    mp.envDriveUp  = raw.envdrive->load() < 0.5f;
    mp.envRatio    = raw.envratio->load();
    mp.envShape    = raw.envshape->load();
    // v0.46: THRESHOLD is in dB on the panel; the follower compares against a
    // linear level, so convert here. -96 dB lands at 1.6e-5, which is below
    // anything a guitar makes, i.e. no gate -- the Mu-Tron's own behaviour.
    mp.envThresh   = juce::Decibels::decibelsToGain (raw.envthresh->load());
    // v0.46 Mu-Tron ballistics, three decades with noon on the stock value.
    mp.envAttackMs = 1.551f * std::pow (1000.0f, raw.envattack->load() - 0.5f);
    mp.envDecayMs  = 158.7f * std::pow (1000.0f, raw.envdecay->load()  - 0.5f);
    if (lfo2Retrig.exchange (false, std::memory_order_relaxed))
        mod.retriggerLfo2();     // tempo tap re-seeds chaos/drift generators
    if (lfo1Retrig.exchange (false, std::memory_order_relaxed))
        mod.retriggerLfo1();     // v0.24: LFO1 taps re-seed LFO1 the same way

    mod.setParams (mp);

    wtf::ModSystem::KnobSet base;
    base.freq = raw.freq->load();
    base.fizz = raw.fizz->load();
    base.lpfQ = raw.lpfq->load();
    base.dry  = raw.dry->load();
    base.vol  = raw.vol->load();
    base.gain = raw.dirtgain->load();

    // ---- gate settings ------------------------------------------------------------
    const float gateThreshDb = raw.gatethresh->load();
    const float gateHoldSec  = raw.gatehold->load();
    const float gateFadeSec  = raw.gatefade->load();

    // ---- chunked processing ----------------------------------------------------------
    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = juce::jmin (kModChunk, numSamples - offset);

        for (int i = offset; i < offset + chunk; ++i)
        {
            gateEnv += gateEnvCoeff * (std::fabs (liveIn[i]) - gateEnv);
            mod.tick (std::fabs (mono[i]), std::fabs (cv1[i]), std::fabs (cv2[i]));
        }

        // gate: volume fades after HOLD below THRESH; FREQ/LPF dragged down with
        // it and restored instantly when the input crosses THRESH again
        const float dt    = (float) chunk / (float) hostRate;
        const float envDb = juce::Decibels::gainToDecibels (gateEnv, -100.0f);
        const bool  belowThresh = envDb < gateThreshDb;
        gateBelowSec = belowThresh ? gateBelowSec + dt : 0.0f;

        if (gateBelowSec > gateHoldSec)
            gateAttenDb -= 96.0f * dt / juce::jmax (0.1f, gateFadeSec);
        else
            gateAttenDb += 96.0f * dt / 0.25f;
        gateAttenDb = juce::jlimit (-96.0f, 0.0f, gateAttenDb);

        const float gatePull = belowThresh ? (gateAttenDb / -96.0f) : 0.0f;

        auto k = mod.compute (base);
        k.freq *= (1.0f - gatePull);
        k.fizz *= (1.0f - gatePull);

        wtf::Wtf567::Params cp;
        cp.freq       = k.freq;
        cp.fizz       = k.fizz;
        cp.lpfQ       = k.lpfQ;
        cp.lpfMode    = lpfModeIdx;
        cp.lpfRangeHi = (int) raw.lpfrange->load();
        cp.dry        = k.dry;
        cp.vol        = k.vol;
        cp.gain       = k.gain;
        cp.dirtType   = 2;   // v0.12: Bazz Fuss, hardwired (Jason's PCB pick)
        cp.supplyV    = 9.0f;   // v0.41 / rev 7: one adapter voltage, 9 V
        cp.starve     = raw.starve->load();
        cp.jfetOn     = raw.jfeton->load()   >= 0.5f;   // SW1, now pre-fuss (ships OFF)
        cp.c41LoopCap = raw.c41cap->load()   >= 0.5f;   // LM567 pin 2 pad (ships OUT)
        cp.c42OutCap  = raw.c42cap->load()   >= 0.5f;   // LM567 pin 1 pad (ships OUT)
        cp.fuzzOn     = raw.fuzzon->load()   >= 0.5f;   // v0.45 stomp A
        cp.decoderOn  = raw.dec567on->load() >= 0.5f;   // v0.45 stomp B
        circuit.setParams (cp);

        float* chans[] = { mono + offset };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) chunk);
        auto osBlock = oversampling->processSamplesUp (block);

        float* os = osBlock.getChannelPointer (0);
        const size_t osSamples = osBlock.getNumSamples();
        for (size_t i = 0; i < osSamples; ++i)
            os[i] = circuit.processSample (os[i]);

        oversampling->processSamplesDown (block);

        {
            float g = juce::Decibels::decibelsToGain (gateAttenDb, -100.0f);
            if (gateAttenDb <= -95.0f)
                g = 0.0f;
            monoBuffer.applyGainRamp (0, offset, chunk, lastOutGain, g);
            lastOutGain = g;
        }

        offset += chunk;
    }

    atomicMax (meterPeaks[1], monoBuffer.getMagnitude (0, 0, numSamples));

    // LED visualization values for the editor
    visVals[0].store (mod.getLfo1Vis(),  std::memory_order_relaxed);
    visVals[1].store (mod.getLfo2Vis(),  std::memory_order_relaxed);
    visVals[2].store (mod.getEnvSigVis(), std::memory_order_relaxed);
    visVals[3].store (mod.getCv1Env(),   std::memory_order_relaxed);
    visVals[4].store (mod.getCv2Env(),   std::memory_order_relaxed);
    visVals[5].store (gateAttenDb,       std::memory_order_relaxed);

    // ---- v0.21 buffered bypass: crossfade to the raw input (10 ms) -------------
    {
        const float bt = raw.bypass->load() >= 0.5f ? 1.0f : 0.0f;
        const float bc = 1.0f - std::exp (-1.0f / (0.010f * (float) hostRate));
        const float* rawIn = monoBuffer.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            bypassMix += bc * (bt - bypassMix);
            mono[i] = mono[i] * (1.0f - bypassMix) + rawIn[i] * bypassMix;
        }
    }

    for (int ch = 0; ch < numOut; ++ch)
        mainOut.copyFrom (ch, 0, mono, numSamples);
}

juce::AudioProcessorEditor* WtfAudioProcessor::createEditor()
{
    return new WtfAudioProcessorEditor (*this);
}

void WtfAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void WtfAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // v0.32 (Jason): the STANDALONE pedal powers up on the ship defaults
    // every time, like flipping on a real pedal. DAWs still restore their
    // saved session state as plugins must.
    if (wrapperType == wrapperType_Standalone)
        return;
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WtfAudioProcessor();
}
