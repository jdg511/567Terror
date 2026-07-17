#include "PluginEditor.h"

namespace
{
    const juce::Colour kBackground { 0xff1d1f24 };
    const juce::Colour kPanel      { 0xff26292f };
    const juce::Colour kPanelLine  { 0xff3a3f47 };
    const juce::Colour kAccent     { 0xffffd166 };
    const juce::Colour kText       { 0xffe8e8e8 };
    const juce::Colour kDim        { 0xff9aa0a6 };

    void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (kPanelLine);
        g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
        g.setColour (kDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 14,
                    juce::Justification::centredLeft);
    }
}

GlitchwaveAudioProcessorEditor::GlitchwaveAudioProcessorEditor (GlitchwaveAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, kPanelLine);
    lnf.setColour (juce::Slider::thumbColourId, kAccent);
    lnf.setColour (juce::Slider::textBoxTextColourId, kText);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e3340));
    lnf.setColour (juce::ComboBox::textColourId, kText);
    lnf.setColour (juce::ComboBox::outlineColourId, kPanelLine);
    lnf.setColour (juce::ComboBox::arrowColourId, kAccent);
    setLookAndFeel (&lnf);

    auto& apvts = processor.apvts;

    // ---- pedal row -----------------------------------------------------------
    setupKnob (freqKnob, freqLabel, "FREQ", true);
    setupKnob (gainKnob, gainLabel, "GAIN", true);   // dirt drive (dry path, always on)
    setupKnob (lpfKnob,  lpfLabel,  "LPF",  true);
    setupKnob (resKnob,  resLabel,  "RES",  true);
    setupKnob (mixKnob,  mixLabel,  "MIX",  true);
    setupKnob (volKnob,  volLabel,  "VOL",  true);
    freqAtt = std::make_unique<SliderAttachment> (apvts, "freq",     freqKnob);
    gainAtt = std::make_unique<SliderAttachment> (apvts, "dirtgain", gainKnob);
    lpfAtt  = std::make_unique<SliderAttachment> (apvts, "fizz",     lpfKnob);
    resAtt  = std::make_unique<SliderAttachment> (apvts, "lpfq",     resKnob);
    mixAtt  = std::make_unique<SliderAttachment> (apvts, "dry",      mixKnob);
    volAtt  = std::make_unique<SliderAttachment> (apvts, "vol",      volKnob);
    setupCombo (dirtBox, "dirttype", dirtAtt);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    // ---- LFO panels --------------------------------------------------------------
    setupKnob (lfo1RateKnob,  lfo1RateLabel,  "RATE",  false);
    setupKnob (lfo1DepthKnob, lfo1DepthLabel, "DEPTH", false);
    setupKnob (lfo2RateKnob,  lfo2RateLabel,  "RATE",  false);
    setupKnob (lfo2DepthKnob, lfo2DepthLabel, "DEPTH", false);
    lfo1RateAtt  = std::make_unique<SliderAttachment> (apvts, "lfo1rate",  lfo1RateKnob);
    lfo1DepthAtt = std::make_unique<SliderAttachment> (apvts, "lfo1depth", lfo1DepthKnob);
    lfo2RateAtt  = std::make_unique<SliderAttachment> (apvts, "lfo2rate",  lfo2RateKnob);
    lfo2DepthAtt = std::make_unique<SliderAttachment> (apvts, "lfo2depth", lfo2DepthKnob);
    setupCombo (lfo1ShapeBox,  "lfo1shape2",  lfo1ShapeAtt);
    setupCombo (lfo1ModeBox,   "lfo1mode",    lfo1ModeAtt);
    setupCombo (lfo1TargetBox, "lfo1target4", lfo1TargetAtt);
    setupCombo (lfo2ShapeBox,  "lfo2shape",   lfo2ShapeAtt);
    setupCombo (lfo2ModeBox,   "lfo2mode",    lfo2ModeAtt);
    setupCombo (lfo2TargetBox, "lfo2target3", lfo2TargetAtt);

    // ---- envelope follower panel --------------------------------------------------
    setupCombo (envTargetBox, "envtarget4", envTargetAtt);
    setupCombo (envDriveBox,  "envdrive",   envDriveAtt);
    setupCombo (lpfModeBox,   "lpfmode2",   lpfModeAtt);
    setupCombo (lpfRangeBox,  "lpfrange",   lpfRangeAtt);
    setupKnob (envGainKnob, envGainLabel, "GAIN", false);
    envGainAtt = std::make_unique<SliderAttachment> (apvts, "envgain", envGainKnob);

    // ---- CV buses --------------------------------------------------------------------
    setupCombo (cv1TargetBox, "cv1target4", cv1TargetAtt);
    setupCombo (cv2TargetBox, "cv2target4", cv2TargetAtt);
    setupKnob (cv1StrKnob, cv1StrLabel, "STRENGTH", false);
    setupKnob (cv2StrKnob, cv2StrLabel, "STRENGTH", false);
    cv1StrAtt = std::make_unique<SliderAttachment> (apvts, "cv1strength", cv1StrKnob);
    cv2StrAtt = std::make_unique<SliderAttachment> (apvts, "cv2strength", cv2StrKnob);

    // ---- output gate ---------------------------------------------------------------------
    setupKnob (threshKnob, threshLabel, "THRESH", false);
    setupKnob (holdKnob,   holdLabel,   "HOLD",   false);
    setupKnob (fadeKnob,   fadeLabel,   "FADE",   false);
    threshAtt = std::make_unique<SliderAttachment> (apvts, "gatethresh", threshKnob);
    holdAtt   = std::make_unique<SliderAttachment> (apvts, "gatehold",   holdKnob);
    fadeAtt   = std::make_unique<SliderAttachment> (apvts, "gatefade",   fadeKnob);

    startTimerHz (30);
    setSize (980, 600);
}

GlitchwaveAudioProcessorEditor::~GlitchwaveAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GlitchwaveAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 96 : 72, big ? 18 : 14);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (big ? 16.0f : 11.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, big ? kText : kDim);
    addAndMakeVisible (l);
}

void GlitchwaveAudioProcessorEditor::setupCombo (juce::ComboBox& box,
                                                 const juce::String& paramID,
                                                 std::unique_ptr<ComboBoxAttachment>& att)
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (
                       processor.apvts.getParameter (paramID)))
        box.addItemList (pc->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<ComboBoxAttachment> (processor.apvts, paramID, box);
}

void GlitchwaveAudioProcessorEditor::timerCallback()
{
    constexpr float fallPerFrame = 40.0f / 30.0f;
    meterIn.push  (processor.readMeterPeak (0));
    meterOut.push (processor.readMeterPeak (1));
    meterIn.fall  (fallPerFrame);
    meterOut.fall (fallPerFrame);

    // v0.8 grey-out rules -----------------------------------------------------
    // an active CV bus takes over from its LFO
    const bool lfo1Active = cv1TargetBox.getSelectedItemIndex() <= 0;
    const bool lfo2Active = cv2TargetBox.getSelectedItemIndex() <= 0;
    // filter Mode Off disables the whole envelope-filter block
    const bool filterOn   = lpfModeBox.getSelectedItemIndex() > 0;

    juce::Component* lfo1Parts[] = { &lfo1ShapeBox, &lfo1ModeBox, &lfo1TargetBox,
                                     &lfo1RateKnob, &lfo1DepthKnob,
                                     &lfo1RateLabel, &lfo1DepthLabel };
    for (auto* c : lfo1Parts) c->setEnabled (lfo1Active);

    juce::Component* lfo2Parts[] = { &lfo2ShapeBox, &lfo2ModeBox, &lfo2TargetBox,
                                     &lfo2RateKnob, &lfo2DepthKnob,
                                     &lfo2RateLabel, &lfo2DepthLabel };
    for (auto* c : lfo2Parts) c->setEnabled (lfo2Active);

    juce::Component* envParts[] = { &envTargetBox, &envDriveBox, &lpfRangeBox,
                                    &envGainKnob, &envGainLabel,
                                    &lpfKnob, &lpfLabel, &resKnob, &resLabel };
    for (auto* c : envParts) c->setEnabled (filterOn);
}

void GlitchwaveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kAccent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GLITCHWAVE 567", 20, 10, 400, 30, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("LM567 glitch pedal simulation — v0.10",
                20, 38, 500, 16, juce::Justification::centredLeft);

    drawSection (g, { 12,  60, 956, 206 }, "PEDAL");
    drawSection (g, { 12, 274, 340, 184 }, "LFO 1");
    drawSection (g, { 360, 274, 340, 184 }, "LFO 2");
    drawSection (g, { 708, 274, 260, 184 }, "ENVELOPE FOLLOWER");
    drawSection (g, { 12, 466, 296, 122 }, "CV 1  (SIDECHAIN L)");
    drawSection (g, { 316, 466, 330, 122 }, "CV 2  (SIDECHAIN R)");
    drawSection (g, { 654, 466, 314, 122 }, "OUTPUT GATE");

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("DIRT (DRY PATH)", 756, 94, 148, 12, juce::Justification::centredLeft);
    g.drawText ("IN",  912, 84, 24, 12, juce::Justification::centred);
    g.drawText ("OUT", 940, 84, 28, 12, juce::Justification::centred);
}

void GlitchwaveAudioProcessorEditor::resized()
{
    // ---- pedal row -------------------------------------------------------------
    {
        const int y = 82, knobW = 112, knobH = 140, labelH = 18;
        int x = 20;
        juce::Slider* ks[] = { &freqKnob, &gainKnob, &lpfKnob, &resKnob, &mixKnob, &volKnob };
        juce::Label*  ls[] = { &freqLabel, &gainLabel, &lpfLabel, &resLabel, &mixLabel, &volLabel };
        for (int i = 0; i < 6; ++i)
        {
            ls[i]->setBounds (x, y, knobW, labelH);
            ks[i]->setBounds (x, y + labelH, knobW, knobH);
            x += knobW + 10;
        }
        dirtBox.setBounds (756, 110, 148, 24);
        meterIn.setBounds  (916, 98, 16, 150);
        meterOut.setBounds (946, 98, 16, 150);
    }

    // ---- LFO 1 / LFO 2 panels ------------------------------------------------------
    {
        const int y = 296, kw = 94, kh = 118, lh = 14;

        lfo1ShapeBox.setBounds  (24, y, 112, 24);
        lfo1ModeBox.setBounds   (24, y + 30, 112, 24);
        lfo1TargetBox.setBounds (24, y + 60, 112, 24);
        lfo1RateLabel.setBounds  (150, y, kw, lh);
        lfo1RateKnob.setBounds   (150, y + lh, kw, kh);
        lfo1DepthLabel.setBounds (250, y, kw, lh);
        lfo1DepthKnob.setBounds  (250, y + lh, kw, kh);

        lfo2ShapeBox.setBounds  (372, y, 112, 24);
        lfo2ModeBox.setBounds   (372, y + 30, 112, 24);
        lfo2TargetBox.setBounds (372, y + 60, 112, 24);
        lfo2RateLabel.setBounds  (498, y, kw, lh);
        lfo2RateKnob.setBounds   (498, y + lh, kw, kh);
        lfo2DepthLabel.setBounds (598, y, kw, lh);
        lfo2DepthKnob.setBounds  (598, y + lh, kw, kh);
    }

    // ---- envelope follower panel ---------------------------------------------------
    {
        const int y = 296;
        envTargetBox.setBounds (720, y, 118, 24);
        envGainLabel.setBounds (720, y + 40, 80, 14);
        envGainKnob.setBounds  (720, y + 54, 80, 92);

        envDriveBox.setBounds  (848, y, 108, 24);
        lpfModeBox.setBounds   (848, y + 30, 108, 24);
        lpfRangeBox.setBounds  (848, y + 60, 108, 24);
    }

    // ---- CV buses + gate -----------------------------------------------------------------
    {
        const int y = 488, kw = 84, kh = 80, lh = 14;
        cv1TargetBox.setBounds (24, y + 14, 140, 24);
        cv1StrLabel.setBounds  (180, y, kw, lh);
        cv1StrKnob.setBounds   (180, y + lh, kw, kh);

        cv2TargetBox.setBounds (328, y + 14, 140, 24);
        cv2StrLabel.setBounds  (490, y, kw, lh);
        cv2StrKnob.setBounds   (490, y + lh, kw, kh);

        const int gx[3] = { 672, 772, 872 };
        juce::Slider* gk[] = { &threshKnob, &holdKnob, &fadeKnob };
        juce::Label*  gl[] = { &threshLabel, &holdLabel, &fadeLabel };
        for (int i = 0; i < 3; ++i)
        {
            gl[i]->setBounds (gx[i], y - 2, 80, 12);
            gk[i]->setBounds (gx[i], y + 10, 80, 84);
        }
    }
}
