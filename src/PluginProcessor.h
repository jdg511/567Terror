#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/Glitchwave567.h"
#include "dsp/ModSystem.h"

class GlitchwaveAudioProcessor : public juce::AudioProcessor
{
public:
    GlitchwaveAudioProcessor();
    ~GlitchwaveAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                              { return true; }

    const juce::String getName() const override                  { return JucePlugin_Name; }
    bool acceptsMidi() const override                            { return false; }
    bool producesMidi() const override                           { return false; }
    bool isMidiEffect() const override                           { return false; }
    double getTailLengthSeconds() const override                 { return 0.0; }

    int getNumPrograms() override                                { return 1; }
    int getCurrentProgram() override                             { return 0; }
    void setCurrentProgram (int) override                        {}
    const juce::String getProgramName (int) override             { return "Default"; }
    void changeProgramName (int, const juce::String&) override   {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- PPM meters: 0 = input, 1 = output ---------------------------------------
    float readMeterPeak (int id) noexcept
    {
        if (id < 0 || id > 1) return 0.0f;
        return meterPeaks[id].exchange (0.0f, std::memory_order_relaxed);
    }

    // ---- LED visualization values (v0.12) -----------------------------------------
    // 0 = LFO1 (-1..1), 1 = LFO2 (-1..1), 2 = env (0..1), 3 = CV1, 4 = CV2,
    // 5 = gate attenuation in dB (0 = open .. -96 = closed)
    float readVis (int id) const noexcept
    {
        return (id >= 0 && id < 6) ? visVals[id].load (std::memory_order_relaxed) : 0.0f;
    }

    // v0.18: a tempo tap on the LFO2 button re-seeds the non-periodic
    // generators (chaos/drift) so the tap "syncs" them. Editor -> audio thread.
    void requestLfo2Retrigger() noexcept
    {
        lfo2Retrig.store (true, std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int kModChunk = 32;  // host samples between mod updates

    glitchwave::Glitchwave567 circuit;
    glitchwave::ModSystem     mod;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    juce::AudioBuffer<float> monoBuffer;  // ch0 = processing, ch1 = raw live copy (gate)
    juce::AudioBuffer<float> cvBuffer;    // ch0 = CV1 (SC L), ch1 = CV2 (SC R)

    std::atomic<float> meterPeaks[2] { { 0.f }, { 0.f } };
    std::atomic<float> visVals[6] { { 0.f }, { 0.f }, { 0.f }, { 0.f }, { 0.f }, { 0.f } };
    std::atomic<bool>  lfo2Retrig { false };

    struct RawParams
    {
        std::atomic<float>* freq{};  std::atomic<float>* fizz{};
        std::atomic<float>* lpfq{};
        std::atomic<float>* dry{};   std::atomic<float>* vol{};
        std::atomic<float>* dirtgain{};
        std::atomic<float>* lfo1rate{}; std::atomic<float>* lfo1depth{};
        std::atomic<float>* lfo1shape{}; std::atomic<float>* lfo1target{};
        std::atomic<float>* lfo2rate{}; std::atomic<float>* lfo2depth{};
        std::atomic<float>* lfo2shape{}; std::atomic<float>* lfo2target{};
        std::atomic<float>* envtarget{}; std::atomic<float>* envgain{};
        std::atomic<float>* envdrive{};
        std::atomic<float>* lpfmode{}; std::atomic<float>* lpfrange{};
        std::atomic<float>* gatethresh{};
        std::atomic<float>* gatehold{}; std::atomic<float>* gatefade{};
    } raw;

    // ---- output gate state ------------------------------------------------------
    double hostRate     = 48000.0;
    float  gateEnv      = 0.0f;   // ~10 ms follower on the raw live input
    float  gateEnvCoeff = 0.01f;
    float  gateAttenDb  = 0.0f;   // 0 (open) .. -96 (fully faded)
    float  gateBelowSec = 0.0f;
    float  lastOutGain  = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchwaveAudioProcessor)
};
