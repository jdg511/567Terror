#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kOversampleFactorLog2 = 2; // 4x

    juce::String hzToText (float hz)
    {
        if (hz < 1.0f)     return juce::String (hz, 2) + " Hz";
        if (hz < 1000.0f)  return juce::String (juce::roundToInt (hz)) + " Hz";
        return juce::String (hz / 1000.0f, 2) + " kHz";
    }

    juce::String freqToText (float pos)   // 0.1 Hz .. 18 kHz log
    {
        return hzToText (0.1f * std::pow (180000.0f, pos));
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

    // Selector item lists (v0.13: CVs are hardwired to LFO depths — no CV lists;
    // polarity is fixed per LFO — no mode list)
    const juce::StringArray kModTargets  { "Off", "Freq", "LPF", "Res", "Mix", "Gain" };
    const juce::StringArray kLfo2Targets { "Off", "Freq", "LPF", "Res", "Mix", "Gain",
                                           "LFO1 Rate", "LFO1 Depth", "Env Gain" };
    // v0.17: the full Electric Druid TAPLFO 3D waveform set — original set (8),
    // alternate set (8) — in datasheet order, plus Jason's v0.4 noise LFOs.
    const juce::StringArray kLfoShapes   { "Ramp Up", "Ramp Dn", "Pulse", "Tri",
                                           "Sine", "Sweep", "Lumps", "Rand Lvls",
                                           "Ramp+Oct", "Quad Ramp", "Quad Pulse", "Tri Step",
                                           "Sine+Oct", "Sine+3rd", "Sine+4th", "Rand Slope",
                                           "White Noise", "Pink Noise" };

    // list index -> enum maps
    using MT = glitchwave::ModTarget;
    using LS = glitchwave::LfoShape;
    constexpr MT kModMap[]  = { MT::Off, MT::Freq, MT::Fizz, MT::LpfQ, MT::Dry, MT::Gain };
    constexpr MT kLfo2Map[] = { MT::Off, MT::Freq, MT::Fizz, MT::LpfQ, MT::Dry, MT::Gain,
                                MT::Lfo1Rate, MT::Lfo1Depth, MT::EnvAmount };
    constexpr LS kShapeMap[] = { LS::RampUp, LS::RampDown, LS::Square, LS::Triangle,
                                 LS::Sine, LS::Sweep, LS::Lumps, LS::SampleHold,
                                 LS::RampOct, LS::QuadRamp, LS::QuadPulse, LS::TriStep,
                                 LS::SineOct, LS::Sine3rd, LS::Sine4th, LS::RandSlopes,
                                 LS::WhiteNoise, LS::PinkNoise };

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

GlitchwaveAudioProcessor::GlitchwaveAudioProcessor()
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
    raw.lfo1shape  = apvts.getRawParameterValue ("lfo1shape4");
    raw.lfo1target = apvts.getRawParameterValue ("lfo1target4");
    raw.lfo2rate   = apvts.getRawParameterValue ("lfo2rate");
    raw.lfo2depth  = apvts.getRawParameterValue ("lfo2depth");
    raw.lfo2shape  = apvts.getRawParameterValue ("lfo2shape3");
    raw.lfo2target = apvts.getRawParameterValue ("lfo2target3");
    raw.envtarget  = apvts.getRawParameterValue ("envtarget4");
    raw.envgain    = apvts.getRawParameterValue ("envgain");
    raw.envdrive   = apvts.getRawParameterValue ("envdrive");
    raw.lpfmode    = apvts.getRawParameterValue ("lpfmode3");
    raw.lpfrange   = apvts.getRawParameterValue ("lpfrange");
    raw.gatethresh  = apvts.getRawParameterValue ("gatethresh");
    raw.gatehold    = apvts.getRawParameterValue ("gatehold");
    raw.gatefade    = apvts.getRawParameterValue ("gatefade");
}

juce::AudioProcessorValueTreeState::ParameterLayout
GlitchwaveAudioProcessor::createParameterLayout()
{
    using PF  = juce::AudioParameterFloat;
    using PC  = juce::AudioParameterChoice;
    using Att = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    // ---- the pedal itself ----------------------------------------------------
    layout.add (std::make_unique<PF> (juce::ParameterID { "freq", 1 }, "Freq",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int) { return freqToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "fizz", 1 }, "LPF",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.65f,
        Att().withStringFromValueFunction ([] (float v, int) { return lpfToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "lpfq", 1 }, "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.4f,
        Att().withStringFromValueFunction ([] (float v, int) { return lpfQToText (v); })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "dry", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.5f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const int d = juce::roundToInt ((v <= 0.5f ? 1.0f : 2.0f * (1.0f - v)) * 100.0f);
                const int w = juce::roundToInt ((v >= 0.5f ? 1.0f : 2.0f * v) * 100.0f);
                return "D" + juce::String (d) + " / FX" + juce::String (w);
            })));
    layout.add (std::make_unique<PF> (juce::ParameterID { "vol", 1 }, "Vol",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.7f, pct));

    // ---- always-on Bazz Fuss dirt in the dry path (v0.12: hardwired) --------------
    layout.add (std::make_unique<PF> (juce::ParameterID { "dirtgain", 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f,
        Att().withStringFromValueFunction ([] (float v, int)
            {
                const float g = 2.0f * std::pow (150.0f, v);
                return "x" + juce::String (g, g < 10.0f ? 1 : 0);
            })));

    // ---- LFO stack ---------------------------------------------------------------
    auto hz2 = Att().withStringFromValueFunction ([] (float v, int)
                   { return juce::String (v, 2) + " Hz"; });
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo1rate", 1 }, "LFO1 Rate",
        juce::NormalisableRange<float> (0.05f, 20.0f, 0.0f, 0.35f), 2.0f, hz2));
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo1depth", 1 }, "LFO1 Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f, pct));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo1shape4", 1 }, "LFO1 Shape",
        kLfoShapes, 3));   // default: Tri
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo1target4", 1 }, "LFO1 Target",
        kModTargets, 1)); // default: Freq (LFO1 is always unipolar-up)
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo2rate", 1 }, "LFO2 Rate",
        juce::NormalisableRange<float> (0.02f, 10.0f, 0.0f, 0.35f), 0.25f, hz2));
    layout.add (std::make_unique<PF> (juce::ParameterID { "lfo2depth", 1 }, "LFO2 Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), 0.0f, pct));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo2shape3", 1 }, "LFO2 Shape",
        kLfoShapes, 4));   // default: Sine
    layout.add (std::make_unique<PC> (juce::ParameterID { "lfo2target3", 1 }, "LFO2 Target",
        kLfo2Targets, 6)); // default: LFO1 Rate (LFO2 is always bipolar)

    // ---- envelope follower + filter switches ------------------------------------------
    layout.add (std::make_unique<PC> (juce::ParameterID { "envtarget4", 1 }, "Env Target",
        kModTargets, 2)); // default: LPF
    layout.add (std::make_unique<PF> (juce::ParameterID { "envgain", 1 }, "Env Gain",
        juce::NormalisableRange<float> (0.125f, 40.0f, 0.0f, 0.3f), 4.0f,
        Att().withStringFromValueFunction ([] (float v, int)
            { return "x" + juce::String (v, v < 2.0f ? 2 : 1); })));
    layout.add (std::make_unique<PC> (juce::ParameterID { "envdrive", 1 }, "Env Drive",
        juce::StringArray { "Drive Up", "Drive Down" }, 0));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lpfmode3", 1 }, "Filter Mode",
        juce::StringArray { "Off", "Mode LP", "Mode BP", "Mode HP", "Mode Notch" }, 1));
    layout.add (std::make_unique<PC> (juce::ParameterID { "lpfrange", 1 }, "Filter Range",
        juce::StringArray { "Range Lo", "Range Hi" }, 0));

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

    return layout;
}

void GlitchwaveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, kOversampleFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    circuit.prepare (sampleRate * (1 << kOversampleFactorLog2));
    mod.prepare (sampleRate);

    monoBuffer.setSize (2, samplesPerBlock);
    cvBuffer.setSize (2, samplesPerBlock);

    hostRate     = sampleRate;
    gateEnvCoeff = 1.0f - std::exp (-1.0f / (0.010f * (float) sampleRate));
    gateEnv      = 0.0f;
    gateAttenDb  = 0.0f;
    gateBelowSec = 0.0f;
    lastOutGain  = 1.0f;

    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
}

bool GlitchwaveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GlitchwaveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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
    const int lpfModeIdx = (int) raw.lpfmode->load();

    glitchwave::ModSystem::Params mp;
    mp.lfo1RateHz  = raw.lfo1rate->load();
    mp.lfo1Depth   = raw.lfo1depth->load();
    mp.lfo1Shape   = (int) kShapeMap[juce::jlimit (0, 17, (int) raw.lfo1shape->load())];
    mp.lfo1Target  = mapTarget (kModMap, raw.lfo1target->load());
    mp.lfo2RateHz  = raw.lfo2rate->load();
    mp.lfo2Depth   = raw.lfo2depth->load();
    mp.lfo2Shape   = (int) kShapeMap[juce::jlimit (0, 17, (int) raw.lfo2shape->load())];
    mp.lfo2Target  = mapTarget (kLfo2Map, raw.lfo2target->load());
    mp.envTarget   = mapTarget (kModMap, raw.envtarget->load());
    // filter Mode Off also disables the envelope follower section
    mp.envGain     = lpfModeIdx == 0 ? 0.0f : raw.envgain->load();
    mp.envDriveUp  = raw.envdrive->load() < 0.5f;
    mod.setParams (mp);

    glitchwave::ModSystem::KnobSet base;
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

        glitchwave::Glitchwave567::Params cp;
        cp.freq       = k.freq;
        cp.fizz       = k.fizz;
        cp.lpfQ       = k.lpfQ;
        cp.lpfMode    = lpfModeIdx;
        cp.lpfRangeHi = (int) raw.lpfrange->load();
        cp.dry        = k.dry;
        cp.vol        = k.vol;
        cp.gain       = k.gain;
        cp.dirtType   = 2;   // v0.12: Bazz Fuss, hardwired (Jason's PCB pick)
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
    visVals[2].store (mod.getInputEnv(), std::memory_order_relaxed);
    visVals[3].store (mod.getCv1Env(),   std::memory_order_relaxed);
    visVals[4].store (mod.getCv2Env(),   std::memory_order_relaxed);
    visVals[5].store (gateAttenDb,       std::memory_order_relaxed);

    for (int ch = 0; ch < numOut; ++ch)
        mainOut.copyFrom (ch, 0, mono, numSamples);
}

juce::AudioProcessorEditor* GlitchwaveAudioProcessor::createEditor()
{
    return new GlitchwaveAudioProcessorEditor (*this);
}

void GlitchwaveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GlitchwaveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchwaveAudioProcessor();
}
