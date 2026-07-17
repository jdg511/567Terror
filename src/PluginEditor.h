#pragma once

#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Simple digital PPM: instant attack, timed fall, green/amber/red segments.
// ---------------------------------------------------------------------------
class PPMMeter : public juce::Component
{
public:
    void push (float linearPeak) noexcept
    {
        const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
        if (db > levelDb)
            levelDb = db;
    }

    void fall (float dbPerFrame) noexcept
    {
        levelDb = juce::jmax (-60.0f, levelDb - dbPerFrame);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff121418));
        g.fillRoundedRectangle (r, 2.0f);

        const float frac = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
        if (frac <= 0.001f)
            return;

        auto bar = r.reduced (2.0f);
        const float top = bar.getBottom() - frac * bar.getHeight();

        auto seg = [&] (float loDb, float hiDb, juce::Colour c)
        {
            const float yLo = bar.getBottom() - juce::jlimit (0.0f, 1.0f, (loDb + 60.0f) / 60.0f) * bar.getHeight();
            const float yHi = bar.getBottom() - juce::jlimit (0.0f, 1.0f, (hiDb + 60.0f) / 60.0f) * bar.getHeight();
            const float y0  = juce::jmax (yHi, top);
            if (y0 < yLo)
            {
                g.setColour (c);
                g.fillRect (juce::Rectangle<float> (bar.getX(), y0, bar.getWidth(), yLo - y0));
            }
        };
        seg (-60.0f, -12.0f, juce::Colour (0xff4caf6d));
        seg (-12.0f,  -4.0f, juce::Colour (0xffd9a441));
        seg ( -4.0f,   0.0f, juce::Colour (0xffd95050));
    }

private:
    float levelDb = -60.0f;
};

// ---------------------------------------------------------------------------
class GlitchwaveAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit GlitchwaveAudioProcessorEditor (GlitchwaveAudioProcessor&);
    ~GlitchwaveAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupKnob  (juce::Slider& s, juce::Label& l, const juce::String& name, bool big);
    void setupCombo (juce::ComboBox& box, const juce::String& paramID,
                     std::unique_ptr<ComboBoxAttachment>& att);

    GlitchwaveAudioProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    // pedal row
    juce::Slider freqKnob, lpfKnob, resKnob, mixKnob, volKnob, gainKnob;
    juce::Label  freqLabel, lpfLabel, resLabel, mixLabel, volLabel, gainLabel;
    juce::ComboBox dirtBox;
    std::unique_ptr<SliderAttachment> freqAtt, lpfAtt, resAtt, mixAtt, volAtt, gainAtt;
    std::unique_ptr<ComboBoxAttachment> dirtAtt;
    PPMMeter meterIn, meterOut;

    // LFO panels
    juce::Slider lfo1RateKnob, lfo1DepthKnob, lfo2RateKnob, lfo2DepthKnob;
    juce::Label  lfo1RateLabel, lfo1DepthLabel, lfo2RateLabel, lfo2DepthLabel;
    juce::ComboBox lfo1ShapeBox, lfo1ModeBox, lfo1TargetBox;
    juce::ComboBox lfo2ShapeBox, lfo2ModeBox, lfo2TargetBox;
    std::unique_ptr<SliderAttachment> lfo1RateAtt, lfo1DepthAtt, lfo2RateAtt, lfo2DepthAtt;
    std::unique_ptr<ComboBoxAttachment> lfo1ShapeAtt, lfo1ModeAtt, lfo1TargetAtt;
    std::unique_ptr<ComboBoxAttachment> lfo2ShapeAtt, lfo2ModeAtt, lfo2TargetAtt;

    // envelope follower panel
    juce::ComboBox envTargetBox, envDriveBox, lpfModeBox, lpfRangeBox;
    juce::Slider envGainKnob;
    juce::Label  envGainLabel;
    std::unique_ptr<ComboBoxAttachment> envTargetAtt, envDriveAtt, lpfModeAtt, lpfRangeAtt;
    std::unique_ptr<SliderAttachment> envGainAtt;

    // CV buses
    juce::ComboBox cv1TargetBox, cv2TargetBox;
    juce::Slider cv1StrKnob, cv2StrKnob;
    juce::Label  cv1StrLabel, cv2StrLabel;
    std::unique_ptr<ComboBoxAttachment> cv1TargetAtt, cv2TargetAtt;
    std::unique_ptr<SliderAttachment> cv1StrAtt, cv2StrAtt;

    // output gate
    juce::Slider threshKnob, holdKnob, fadeKnob;
    juce::Label  threshLabel, holdLabel, fadeLabel;
    std::unique_ptr<SliderAttachment> threshAtt, holdAtt, fadeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchwaveAudioProcessorEditor)
};
