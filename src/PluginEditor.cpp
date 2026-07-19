#include "PluginEditor.h"

namespace
{
    const juce::Colour kBackground { 0xff1d1f24 };
    const juce::Colour kPanel      { 0xff26292f };
    const juce::Colour kPanelLine  { 0xff3a3f47 };
    const juce::Colour kAccent     { 0xffffd166 };
    const juce::Colour kText       { 0xffe8e8e8 };
    const juce::Colour kDim        { 0xff9aa0a6 };
    const juce::Colour kGreen      { 0xff4caf6d };
    const juce::Colour kAmber      { 0xffd9a441 };
    const juce::Colour kRed        { 0xffd95050 };

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
    lnf.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2e3340));
    lnf.setColour (juce::TextButton::textColourOffId, kText);
    setLookAndFeel (&lnf);

    auto& apvts = processor.apvts;

    // ---- pedal row -----------------------------------------------------------
    setupKnob (freqKnob, freqLabel, "FREQ", true);
    setupKnob (gainKnob, gainLabel, "GAIN", true);
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
    dirtLed.setColour (kRed);
    dirtLed.setLevel (1.0f);
    addAndMakeVisible (dirtLed);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    // ---- LFO panels ----------------------------------------------------------
    setupKnob (lfo1RateKnob,  lfo1RateLabel,  "RATE",  false);
    setupKnob (lfo1DepthKnob, lfo1DepthLabel, "DEPTH", false);
    lfo1RateAtt  = std::make_unique<SliderAttachment> (apvts, "lfo1rate",  lfo1RateKnob);
    lfo1DepthAtt = std::make_unique<SliderAttachment> (apvts, "lfo1depth", lfo1DepthKnob);

    // v0.17: LFO2 rate/depth = one button. Tap = tempo. Holding BOTH LFO2
    // buttons 750 ms (sim: CTRL + hold this button) = depth sweep; see
    // d2Frame() for the wave. Release freezes the depth.
    lfo2RateBtn.onPress = [this] { d2Press(); };
    addAndMakeVisible (lfo2RateBtn);

    lfo2ValueLabel.setJustificationType (juce::Justification::centred);
    lfo2ValueLabel.setFont (juce::FontOptions (12.0f));
    lfo2ValueLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (lfo2ValueLabel);
    lfo1Shape.bind  (apvts, "lfo1shape5");
    lfo1Target.bind (apvts, "lfo1target4");
    lfo2Shape.bind  (apvts, "lfo2shape4");
    lfo2Target.bind (apvts, "lfo2target3");
    lfo1Shape.setRowHeight (13);     // 16 waveforms (Bank A + Bank B)
    lfo2Shape.setRowHeight (13);
    for (auto* s : { &lfo1Shape, &lfo1Target, &lfo2Shape, &lfo2Target })
    {
        s->setInteractive (false);   // v0.14: LED columns are indicators only
        addAndMakeVisible (s);
    }
    lfo1Led.setColour (kAmber);
    lfo2Led.setColour (kAmber);
    addAndMakeVisible (lfo1Led);
    addAndMakeVisible (lfo2Led);

    // one button per LFO: tap = shape, hold = cycle target
    lfo1Btn.onTap      = [this] { cycleChoice ("lfo1shape5"); };
    lfo1Btn.onHoldTick = [this] { cycleChoice ("lfo1target4"); };
    lfo2Btn.onTap      = [this] { cycleChoice ("lfo2shape4"); };
    lfo2Btn.onHoldTick = [this] { cycleChoice ("lfo2target3"); };
    addAndMakeVisible (lfo1Btn);
    addAndMakeVisible (lfo2Btn);

    // ---- envelope follower panel --------------------------------------------
    setupKnob (envGainKnob, envGainLabel, "GAIN", false);
    envGainAtt = std::make_unique<SliderAttachment> (apvts, "envgain", envGainKnob);
    envTarget.bind (apvts, "envtarget4");
    envDrive.bind  (apvts, "envdrive");
    lpfMode.bind   (apvts, "lpfmode3");
    lpfRange.bind  (apvts, "lpfrange");
    for (auto* s : { &envDrive, &lpfMode, &lpfRange })
    {
        s->setInteractive (false);   // driven by the env button
        addAndMakeVisible (s);
    }
    addAndMakeVisible (envTarget);   // TARGET stays tappable (its own cycle)
    envLed.setColour (kAmber);
    addAndMakeVisible (envLed);

    // Jason's one-button scheme: tap = filter mode ("waveform");
    // hold 600 ms = cycle drive x range combos every 400 ms from current spot
    envBtn.onTap      = [this] { cycleChoice ("lpfmode3"); };
    envBtn.onHoldTick = [this] { advanceDriveRange(); };
    addAndMakeVisible (envBtn);

    // ---- CV jacks (hardwired to LFO depth VCAs) --------------------------------
    cv1Led.setColour (kGreen);
    cv2Led.setColour (kGreen);
    addAndMakeVisible (cv1Led);
    addAndMakeVisible (cv2Led);

    // ---- output gate (under the cover) ----------------------------------------
    setupKnob (threshKnob, threshLabel, "THRESH", false);
    setupKnob (holdKnob,   holdLabel,   "HOLD",   false);
    setupKnob (fadeKnob,   fadeLabel,   "FADE",   false);
    threshAtt = std::make_unique<SliderAttachment> (apvts, "gatethresh", threshKnob);
    holdAtt   = std::make_unique<SliderAttachment> (apvts, "gatehold",   holdKnob);
    fadeAtt   = std::make_unique<SliderAttachment> (apvts, "gatefade",   fadeKnob);
    gateLed.setColour (kGreen);
    addAndMakeVisible (gateLed);
    gateCover.onToggle = [this] { setGateOpen (true); };
    addAndMakeVisible (gateCover);
    gateCoverBtn.onClick = [this] { setGateOpen (false); };
    addAndMakeVisible (gateCoverBtn);
    setGateOpen (false);

    startTimerHz (60);   // smooth depth sweep + LED animation
    setSize (1060, 700);
}

GlitchwaveAudioProcessorEditor::~GlitchwaveAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GlitchwaveAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 96 : 74, big ? 18 : 14);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (big ? 16.0f : 11.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, big ? kText : kDim);
    addAndMakeVisible (l);
}

void GlitchwaveAudioProcessorEditor::cycleChoice (const juce::String& paramID)
{
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (
                       processor.apvts.getParameter (paramID)))
    {
        pc->beginChangeGesture();
        *pc = (pc->getIndex() + 1) % pc->choices.size();
        pc->endChangeGesture();
    }
}

void GlitchwaveAudioProcessorEditor::advanceDriveRange()
{
    // combo ring, in Jason's order: up&hi -> up&low -> down&hi -> down&low -> ...
    auto* d = dynamic_cast<juce::AudioParameterChoice*> (
                  processor.apvts.getParameter ("envdrive"));
    auto* r = dynamic_cast<juce::AudioParameterChoice*> (
                  processor.apvts.getParameter ("lpfrange"));
    if (d == nullptr || r == nullptr)
        return;

    // drive: 0 = Up, 1 = Down; range: 0 = Lo, 1 = Hi
    int combo = d->getIndex() * 2 + (r->getIndex() == 1 ? 0 : 1);
    combo = (combo + 1) % 4;

    d->beginChangeGesture(); *d = combo / 2;                 d->endChangeGesture();
    r->beginChangeGesture(); *r = (combo % 2 == 0) ? 1 : 0;  r->endChangeGesture();
}

// ---------------------------------------------------------------------------
// LFO2 one-button state machine (v0.16) — Jason's spec, state for state.
//
// "Double tap" = two presses <=100 ms apart. In Rate mode arming additionally
// requires no further press for 0.5 s (the full definition); inside the
// Armed / point-A windows the verdict falls at the 100 ms mark, exactly as
// specified ("pressed only once and not again within 100ms").
// ---------------------------------------------------------------------------
void GlitchwaveAudioProcessorEditor::d2SetRateFromInterval (double intervalMs)
{
    const float hz = juce::jlimit (0.02f, 10.0f, (float) (1000.0 / intervalMs));
    if (auto* p = processor.apvts.getParameter ("lfo2rate"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (hz));
        p->endChangeGesture();
    }
}

void GlitchwaveAudioProcessorEditor::d2SetDepth (float newDepth01)
{
    newDepth01 = juce::jlimit (0.0f, 1.0f, newDepth01);
    if (auto* p = processor.apvts.getParameter ("lfo2depth"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (newDepth01);
        p->endChangeGesture();
    }
}

float GlitchwaveAudioProcessorEditor::d2GetDepth() const
{
    if (auto* p = processor.apvts.getParameter ("lfo2depth"))
        return p->getValue();
    return 0.0f;
}

void GlitchwaveAudioProcessorEditor::d2Press()
{
    // With CTRL held this press is the start (or part) of the dual-button
    // depth gesture -- never a tempo tap. d2Frame() handles the 750 ms arming.
    if (juce::ModifierKeys::getCurrentModifiers().isCtrlDown())
    {
        d2LastTapMs = 0.0;
        return;
    }

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (d2LastTapMs > 0.0 && now - d2LastTapMs < 5000.0 && now - d2LastTapMs > 40.0)
        d2SetRateFromInterval (now - d2LastTapMs);
    processor.requestLfo2Retrigger();   // v0.18: tap re-seeds chaos/drift waves
    d2LastTapMs = now;
}

void GlitchwaveAudioProcessorEditor::d2Engage()
{
    d2Sweeping = true;
    d2Pausing  = false;

    // Resume the wave from the current depth %, in the remembered direction.
    float d = d2GetDepth();
    if (d2Rising && d >= 0.999f)      { d2Rising = false; d = 1.0f; }
    else if (! d2Rising && d <= 0.001f) { d2Rising = true;  d = 0.0f; }

    // rising: d = 0.5 - 0.5*cos(pi*u)   falling: d = 0.5 + 0.5*cos(pi*u)
    const double c = juce::jlimit (-1.0, 1.0, d2Rising ? 1.0 - 2.0 * d : 2.0 * d - 1.0);
    d2U = std::acos (c) / juce::MathConstants<double>::pi;
}

void GlitchwaveAudioProcessorEditor::d2Frame (double nowMs)
{
    const double dtMs = d2PrevFrameMs > 0.0 ? juce::jlimit (0.0, 100.0, nowMs - d2PrevFrameMs)
                                            : 1000.0 / 60.0;
    d2PrevFrameMs = nowMs;

    // "Both buttons held" -- in the sim that is CTRL + the rate/depth button.
    const bool bothDown = lfo2RateBtn.isDown()
                       && juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown();

    if (! bothDown)
    {
        d2BothSinceMs = 0.0;
        d2Sweeping    = false;   // releasing either button freezes the depth
        return;
    }

    if (d2BothSinceMs == 0.0)
        d2BothSinceMs = nowMs;

    if (! d2Sweeping)
    {
        if (nowMs - d2BothSinceMs >= 750.0)
            d2Engage();
        return;
    }

    // ---- advance the sine-like depth wave ---------------------------------
    if (d2Pausing)
    {
        if (nowMs >= d2PauseEndMs)
        {
            d2Pausing = false;
            d2Rising  = ! d2Rising;   // turn around after the extreme pause
            d2U       = 0.0;
        }
        return;
    }

    d2U += dtMs / 4000.0;             // 0% <-> 100% in 4 s per traverse
    if (d2U >= 1.0)
    {
        d2SetDepth (d2Rising ? 1.0f : 0.0f);
        d2Pausing    = true;          // sit at the extreme for 300 ms
        d2PauseEndMs = nowMs + 300.0;
        return;
    }

    const double c = std::cos (juce::MathConstants<double>::pi * d2U);
    d2SetDepth ((float) (d2Rising ? 0.5 - 0.5 * c : 0.5 + 0.5 * c));
}

void GlitchwaveAudioProcessorEditor::setGateOpen (bool shouldBeOpen)
{
    gateOpen = shouldBeOpen;
    gateCover.setVisible (! gateOpen);
    gateCoverBtn.setVisible (gateOpen);
    for (auto* c : std::initializer_list<juce::Component*> {
             &threshKnob, &threshLabel, &holdKnob, &holdLabel, &fadeKnob, &fadeLabel, &gateLed })
        c->setVisible (gateOpen);
}

void GlitchwaveAudioProcessorEditor::timerCallback()
{
    ++frame;
    constexpr float fallPerFrame = 40.0f / 60.0f;   // 60 fps
    meterIn.push  (processor.readMeterPeak (0));
    meterOut.push (processor.readMeterPeak (1));
    meterIn.fall  (fallPerFrame);
    meterOut.fall (fallPerFrame);

    // v0.13: LFOs never grey out — CVs just breathe their depth (VCA).
    // Filter Mode Off still disables the envelope-filter block.
    const bool filterOn = lpfMode.getSelectedIndex() > 0;

    for (auto* c : std::initializer_list<juce::Component*> {
             &envTarget, &envDrive, &lpfRange, &envGainKnob, &envGainLabel,
             &lpfKnob, &lpfLabel, &resKnob, &resLabel })
        c->setEnabled (filterOn);

    for (auto* s : { &lfo1Shape, &lfo1Target, &lfo2Shape, &lfo2Target,
                     &envTarget, &envDrive, &lpfMode, &lpfRange })
        s->refresh();

    // ---- LEDs ----------------------------------------------------------------
    lfo1Led.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (0)));         // unipolar

    // LFO2 LED + the dual-hold depth sweep (v0.17)
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        d2Frame (nowMs);

        if (d2Sweeping)
        {
            lfo2Led.setColour (juce::Colour (0xff6fa8dc));            // 2nd colour: blue
            lfo2Led.setLevel (d2GetDepth());                          // brightness = depth %
        }
        else
        {
            lfo2Led.setColour (kAmber);                               // 1st colour: rate mode
            lfo2Led.setLevel ((processor.readVis (1) + 1.0f) * 0.5f); // bipolar wave
        }
    }

    // LFO2 live readout (its rate/depth have no knobs)
    if (auto* pr = processor.apvts.getParameter ("lfo2rate"))
        if (auto* pd = processor.apvts.getParameter ("lfo2depth"))
            lfo2ValueLabel.setText (
                juce::String (pr->convertFrom0to1 (pr->getValue()), 2) + juce::String::fromUTF8 (" Hz  \xc2\xb7  ")
                    + juce::String (juce::roundToInt (pd->getValue() * 100.0f)) + " %",
                juce::dontSendNotification);
    envLed.setLevel  (filterOn ? juce::jmin (1.0f, processor.readVis (2) * 4.0f) : 0.0f);
    cv1Led.setLevel  (juce::jmin (1.0f, processor.readVis (3) * 1.5f));
    cv2Led.setLevel  (juce::jmin (1.0f, processor.readVis (4) * 1.5f));

    // gate LED: green = open, amber blinking = fading, red = fully closed
    const float atten = processor.readVis (5);
    const bool  blink = ((frame / 14) % 2) == 0;   // ~2 Hz at 60 fps
    juce::Colour gc; float gl;
    if (atten > -0.5f)       { gc = kGreen; gl = 1.0f; }
    else if (atten > -95.0f) { gc = kAmber; gl = blink ? 1.0f : 0.15f; }
    else                     { gc = kRed;   gl = 1.0f; }
    gateLed.setColour (gc);
    gateLed.setLevel (gl);
    gateCover.setLedState (gc, gl);

    // gate cover summary reads the live parameter values
    gateCover.setSummary (juce::String (threshKnob.getValue(), 0) + juce::String::fromUTF8 (" dB   ·   ")
                        + juce::String (holdKnob.getValue(), 1) + juce::String::fromUTF8 (" s   ·   ")
                        + juce::String (fadeKnob.getValue(), 0) + " s");
}

void GlitchwaveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kAccent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GLITCHWAVE 567", 20, 10, 400, 30, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String::fromUTF8 ("LM567 glitch pedal — hardware layout — v0.18"),
                20, 38, 500, 16, juce::Justification::centredLeft);

    drawSection (g, { 12,  60, 1036, 206 }, "PEDAL");
    drawSection (g, { 12, 274, 332, 252 }, juce::String::fromUTF8 ("LFO 1  \xc2\xb7  UNI \xe2\x86\x91"));
    drawSection (g, { 352, 274, 332, 252 }, juce::String::fromUTF8 ("LFO 2  \xc2\xb7  BIPOLAR"));
    drawSection (g, { 692, 274, 356, 252 }, "ENVELOPE FOLLOWER");
    drawSection (g, { 12, 534, 332, 154 }, "CV 1 JACK  (SIDECHAIN L)");
    drawSection (g, { 352, 534, 332, 154 }, "CV 2 JACK  (SIDECHAIN R)");
    if (gateOpen)
        drawSection (g, { 692, 534, 356, 154 }, "OUTPUT GATE");

    // button captions, printed like a control plate
    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("TAP = SHAPE",        216, 462, 124, 11, juce::Justification::centred);
    g.drawText ("HOLD = TARGET",      216, 474, 124, 11, juce::Justification::centred);
    g.drawText ("TAP = TEMPO",            352, 350, 116, 11, juce::Justification::centred);
    g.drawText ("HOLD BOTH BTNS = DEPTH", 340, 362, 140, 11, juce::Justification::centred);
    g.setFont (juce::FontOptions (8.5f));
    g.drawText ("(sim: CTRL + hold)",     340, 374, 140, 10, juce::Justification::centred);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("TAP = SHAPE",        558, 506, 124, 11, juce::Justification::centred);
    g.drawText ("HOLD = TARGET",      558, 518, 124, 11, juce::Justification::centred);
    g.drawText ("TAP = MODE",         880, 408, 120, 11, juce::Justification::centred);
    g.drawText ("HOLD = DRIVE+RANGE", 880, 420, 120, 11, juce::Justification::centred);

    // CV jack panels: hardwired routing, printed like a control plate
    g.setColour (kText);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92  LFO 1 DEPTH"), 60, 584, 240, 18, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92  LFO 2 DEPTH"), 400, 584, 240, 18, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("sidechain level opens the LFO's depth (VCA)", 24, 610, 300, 14,
                juce::Justification::centredLeft);
    g.drawText ("no signal for 3 s = jack out, full depth", 24, 626, 300, 14,
                juce::Justification::centredLeft);
    g.drawText ("sidechain level opens the LFO's depth (VCA)", 364, 610, 300, 14,
                juce::Justification::centredLeft);
    g.drawText ("no signal for 3 s = jack out, full depth", 364, 626, 300, 14,
                juce::Justification::centredLeft);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("DIRT", 764, 96, 60, 12, juce::Justification::centredLeft);
    g.setColour (kText);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("BAZZ FUSS", 786, 112, 110, 16, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("IN",  976, 84, 24, 12, juce::Justification::centred);
    g.drawText ("OUT", 1006, 84, 30, 12, juce::Justification::centred);
}

void GlitchwaveAudioProcessorEditor::resized()
{
    // ---- pedal row -----------------------------------------------------------
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
        dirtLed.setBounds (764, 112, 16, 16);
        meterIn.setBounds  (980, 98, 16, 150);
        meterOut.setBounds (1012, 98, 16, 150);
    }

    // ---- LFO panels ------------------------------------------------------------
    {
        const int y = 296;
        // LFO 1
        lfo1RateLabel.setBounds (24, y, 80, 12);
        lfo1RateKnob.setBounds  (24, y + 12, 80, 90);
        lfo1Led.setBounds       (106, y + 40, 14, 14);
        lfo1DepthLabel.setBounds (24, y + 112, 80, 12);
        lfo1DepthKnob.setBounds  (24, y + 124, 80, 90);
        lfo1Shape.setBounds  (128, y - 4, 90, lfo1Shape.idealHeight());
        lfo1Target.setBounds (228, y, 106, lfo1Target.idealHeight());
        lfo1Btn.setBounds    (256, y + 116, 44, 44);
        // LFO 2 — one button for rate (tap tempo) + depth (hold), no knobs
        lfo2RateBtn.setBounds   (382, y + 8, 48, 48);
        lfo2Led.setBounds       (440, y + 24, 14, 14);
        lfo2ValueLabel.setBounds (356, y + 88, 108, 16);
        lfo2Shape.setBounds  (468, y - 4, 90, lfo2Shape.idealHeight());
        lfo2Target.setBounds (568, y, 110, lfo2Target.idealHeight());
        lfo2Btn.setBounds    (598, y + 160, 44, 44);
    }

    // ---- envelope follower panel -------------------------------------------------
    {
        const int y = 296;
        envGainLabel.setBounds (704, y, 76, 12);
        envGainKnob.setBounds  (704, y + 12, 76, 90);
        envLed.setBounds       (782, y + 40, 14, 14);
        envTarget.setBounds (704, y + 120, 100, envTarget.idealHeight());
        lpfMode.setBounds   (816, y, 76, lpfMode.idealHeight());
        envDrive.setBounds  (816, y + lpfMode.idealHeight() + 8, 76, envDrive.idealHeight());
        lpfRange.setBounds  (902, y, 76, lpfRange.idealHeight());
        envBtn.setBounds    (916, y + 62, 48, 48);
    }

    // ---- CV jack panels + gate -------------------------------------------------------
    {
        cv1Led.setBounds (28, 582, 22, 22);
        cv2Led.setBounds (368, 582, 22, 22);

        // gate (under the cover)
        gateCover.setBounds (692, 534, 356, 154);
        gateCoverBtn.setBounds (930, 540, 110, 20);
        gateLed.setBounds (706, 542, 14, 14);
        const int gx[3] = { 704, 816, 928 };
        juce::Slider* gk[] = { &threshKnob, &holdKnob, &fadeKnob };
        juce::Label*  gl[] = { &threshLabel, &holdLabel, &fadeLabel };
        for (int i = 0; i < 3; ++i)
        {
            gl[i]->setBounds (gx[i], 568, 96, 12);
            gk[i]->setBounds (gx[i], 580, 96, 96);
        }
    }
}
