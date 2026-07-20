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
    const juce::Colour kBlue       { 0xff6fa8dc };   // the "2nd colour" everywhere

    // v0.20: NeoPixel hue per shape slot (same slot in Bank A and Bank B)
    const juce::Colour kShapeHues[8] = {
        juce::Colour (0xffff4444),   // 0  red      Ramp Up   / Lorenz
        juce::Colour (0xffff8c00),   // 1  orange   Ramp Dn   / Rossler
        juce::Colour (0xffffd400),   // 2  yellow   Square    / Drunk Walk
        juce::Colour (0xff44dd66),   // 3  green    Triangle  / Perlin
        juce::Colour (0xff33cccc),   // 4  cyan     Sine      / Wobble
        juce::Colour (0xff4488ff),   // 5  blue     Sweep     / Glitch
        juce::Colour (0xff9955ff),   // 6  violet   Rnd Slope / White Noise
        juce::Colour (0xffff55bb),   // 7  pink     S&H       / Pink Noise
    };
    const char* kShapeChartA[8] = { "Ramp Up", "Ramp Dn", "Square", "Triangle",
                                    "Sine", "Sweep", "Rnd Slope", "S&H" };
    const char* kShapeChartB[8] = { "Lorenz", "Rossler", "Drunk Wk", "Perlin",
                                    "Wobble", "Glitch", "White Ns", "Pink Ns" };

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

    // ---- pedal row (v0.19: 3 dual-function knobs) ----------------------------
    setupKnob (freqKnob, freqLabel, "FREQ", true);   // shift: GAIN
    setupKnob (lpfKnob,  lpfLabel,  "LPF",  true);   // shift: RES
    setupKnob (mixKnob,  mixLabel,  "MIX",  true);   // shift: VOL
    freqAtt = std::make_unique<SliderAttachment> (apvts, "freq", freqKnob);
    lpfAtt  = std::make_unique<SliderAttachment> (apvts, "fizz", lpfKnob);
    mixAtt  = std::make_unique<SliderAttachment> (apvts, "dry",  mixKnob);

    // moving a shifted knob consumes the shift (and cancels the depth sweep)
    auto shiftKnobMoved = [this]
    {
        if (suppressSliderCb) return;
        if (shiftAttached) { shiftConsumed = true; d2Sweeping = false; }
    };
    freqKnob.onValueChange = shiftKnobMoved;
    lpfKnob.onValueChange  = shiftKnobMoved;
    mixKnob.onValueChange  = shiftKnobMoved;
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    // ---- LFO panels ----------------------------------------------------------
    // v0.19: LFO1 has ONE knob. Plain = RATE; while the LFO1 button (or the
    // stomp/CTRL shift) is held it writes DEPTH instead.
    setupKnob (lfo1RateKnob, lfo1RateLabel, "RATE", false);
    lfo1RateAtt = std::make_unique<SliderAttachment> (apvts, "lfo1rate", lfo1RateKnob);
    lfo1RateKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        if (shiftAttached) { shiftConsumed = true; d2Sweeping = false; }
        if (lfo1Btn.isDown()) { lfo1KnobUsed = true; lfo1Btn.cancelPressActions(); }
    };

    // The TAP TEMPO STOMP (also the pedal's shift key; sim: CTRL = stomp).
    lfo2RateBtn.onPress   = [this] { d2StompPress(); };
    lfo2RateBtn.onRelease = [this] { d2StompRelease(); };
    addAndMakeVisible (lfo2RateBtn);

    lfo2ValueLabel.setJustificationType (juce::Justification::centred);
    lfo2ValueLabel.setFont (juce::FontOptions (12.0f));
    lfo2ValueLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (lfo2ValueLabel);
    // v0.21: one LED per section — cache the choice params it displays
    auto choice = [&apvts] (const char* id)
    { return dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)); };
    lfo1ShapeParam  = choice ("lfo1shape5");
    lfo2ShapeParam  = choice ("lfo2shape4");
    lfo1TargetParam = choice ("lfo1target5");
    lfo2TargetParam = choice ("lfo2target4");
    envTargetParam  = choice ("envtarget5");
    lpfModeParam    = choice ("lpfmode3");
    envDriveParam   = choice ("envdrive");
    lpfRangeParam   = choice ("lpfrange");
    addAndMakeVisible (lfo1Led);
    addAndMakeVisible (lfo2Led);

    // one button per LFO: tap = shape, hold 750 ms = cycle target every 750 ms.
    // With the stomp/CTRL shift held, a PRESS steps the target instead.
    // Holding the LFO1 button also shifts its RATE knob to DEPTH.
    auto ctxStamp = [] (int& ctx, double& until, int newCtx)
    {
        ctx   = newCtx;
        until = juce::Time::getMillisecondCounterHiRes() + 1500.0;
    };

    lfo1Btn.onPress = [this, ctxStamp]
    {
        if (stompDown())
        {
            cycleChoice ("lfo1target5");
            ctxStamp (lfo1Ctx, lfo1CtxUntil, kCtxTarget);
            shiftConsumed = true; d2Sweeping = false;
            lfo1Btn.cancelPressActions();
            return;
        }
        lfo1KnobUsed = false;
        updateKnobModes();               // RATE knob -> DEPTH while held
    };
    lfo1Btn.onTap      = [this, ctxStamp] { cycleChoice ("lfo1shape5");
                                            ctxStamp (lfo1Ctx, lfo1CtxUntil, kCtxShape); };
    lfo1Btn.onHoldTick = [this, ctxStamp] { if (! lfo1KnobUsed) { cycleChoice ("lfo1target5");
                                            ctxStamp (lfo1Ctx, lfo1CtxUntil, kCtxTarget); } };
    lfo1Btn.onRelease  = [this] { updateKnobModes(); };

    lfo2Btn.onPress = [this, ctxStamp]
    {
        if (stompDown())
        {
            cycleChoice ("lfo2target4");
            ctxStamp (lfo2Ctx, lfo2CtxUntil, kCtxTarget);
            shiftConsumed = true; d2Sweeping = false;
            lfo2Btn.cancelPressActions();
        }
    };
    lfo2Btn.onTap      = [this, ctxStamp] { cycleChoice ("lfo2shape4");
                                            ctxStamp (lfo2Ctx, lfo2CtxUntil, kCtxShape); };
    lfo2Btn.onHoldTick = [this, ctxStamp] { cycleChoice ("lfo2target4");
                                            ctxStamp (lfo2Ctx, lfo2CtxUntil, kCtxTarget); };
    addAndMakeVisible (lfo1Btn);
    addAndMakeVisible (lfo2Btn);

    // ---- envelope follower panel --------------------------------------------
    setupKnob (envGainKnob, envGainLabel, "GAIN", false);
    envGainAtt = std::make_unique<SliderAttachment> (apvts, "envgain", envGainKnob);
    envGainKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        if (shiftAttached) { shiftConsumed = true; d2Sweeping = false; }
        if (envBtn.isDown()) { envKnobUsed = true; envBtn.cancelPressActions(); }
        if (envComboMode) applyEnvComboFromKnob();   // knob picks drive x range
    };
    addAndMakeVisible (envLed);

    // v0.21 env button: tap = filter mode (LED: mode hue @ 3 Hz); hold + GAIN
    // knob = drive x range combo (LED: combo hue @ 6 Hz); hold alone 750 ms =
    // cycle TARGET every 750 ms (LED: solid target hue). Shift-press = step
    // target.
    envBtn.onPress = [this, ctxStamp]
    {
        if (stompDown())
        {
            cycleChoice ("envtarget5");
            ctxStamp (envCtx, envCtxUntil, kCtxTarget);
            shiftConsumed = true; d2Sweeping = false;
            envBtn.cancelPressActions();
            return;
        }
        envKnobUsed = false;
        updateKnobModes();               // GAIN knob -> combo select while held
    };
    envBtn.onTap      = [this, ctxStamp] { cycleChoice ("lpfmode3");
                                           ctxStamp (envCtx, envCtxUntil, kCtxMode); };
    envBtn.onHoldTick = [this, ctxStamp] { if (! envKnobUsed) { cycleChoice ("envtarget5");
                                           ctxStamp (envCtx, envCtxUntil, kCtxTarget); } };
    envBtn.onRelease  = [this] { updateKnobModes(); };
    addAndMakeVisible (envBtn);

    // ---- v0.21 bypass stomp + supply "jack" ------------------------------------
    bypassBtn.onTap = [this]
    {
        if (auto* p = processor.apvts.getParameter ("bypass"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() >= 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    };
    addAndMakeVisible (bypassBtn);
    bypassLed.setColour (kGreen);
    addAndMakeVisible (bypassLed);

    supplyBtn.onClick = [this]
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                          processor.apvts.getParameter ("supply")))
        {
            p->beginChangeGesture();
            *p = 1 - p->getIndex();      // swap the wall-wart: 9 V <-> 18 V
            p->endChangeGesture();
            supplyBtn.setButtonText (p->getIndex() == 1 ? "SUPPLY: 18V" : "SUPPLY: 9V");
        }
    };
    addAndMakeVisible (supplyBtn);

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
    setSize (1060, 764);
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

bool GlitchwaveAudioProcessorEditor::stompDown() const
{
    return lfo2RateBtn.isDown()
        || juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown();
}

void GlitchwaveAudioProcessorEditor::d2StompPress()
{
    d2PressMs = juce::Time::getMillisecondCounterHiRes();
    updateKnobModes();                        // shift engages immediately
}

void GlitchwaveAudioProcessorEditor::d2StompRelease()
{
    const double now  = juce::Time::getMillisecondCounterHiRes();
    const bool   ctrl = juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown();

    if (! ctrl && ! d2SweptThisHold && ! shiftConsumed
        && d2PressMs > 0.0 && now - d2PressMs < 750.0)
    {
        // a clean tap: commit the tempo using the PRESS instant
        if (d2LastTapMs > 0.0 && d2PressMs - d2LastTapMs < 5000.0
                              && d2PressMs - d2LastTapMs > 40.0)
            d2SetRateFromInterval (d2PressMs - d2LastTapMs);
        processor.requestLfo2Retrigger();     // taps re-seed the chaos waves
        d2LastTapMs = d2PressMs;
    }
    else
    {
        d2LastTapMs = 0.0;                    // hold/shift use: reset tap chain
    }
    d2PressMs = 0.0;
    updateKnobModes();                        // CTRL may still hold the shift
}

void GlitchwaveAudioProcessorEditor::updateKnobModes()
{
    auto& ap = processor.apvts;
    const bool shift   = stompDown();
    const bool l1depth = shift || lfo1Btn.isDown();
    const bool envCmb  = shift || envBtn.isDown();

    // v0.21 secret starve: BOTH stomps held (bypass + tap; sim: CTRL + ALT)
    // turns the MIX knob into the rail-starve control.
    const bool starve = shift
        && (bypassBtn.isDown()
            || juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown());

    if (shift != shiftAttached || starve != starveMode)
    {
        suppressSliderCb = true;
        freqAtt.reset(); lpfAtt.reset(); mixAtt.reset();
        freqAtt = std::make_unique<SliderAttachment> (ap, shift ? "dirtgain" : "freq", freqKnob);
        lpfAtt  = std::make_unique<SliderAttachment> (ap, shift ? "lpfq"     : "fizz", lpfKnob);
        mixAtt  = std::make_unique<SliderAttachment> (ap,
                      starve ? "starve" : (shift ? "vol" : "dry"), mixKnob);
        suppressSliderCb = false;
        shiftAttached = shift;
        starveMode    = starve;
        if (! shift)
            shiftConsumed = false;            // fresh shift next time
    }

    if (l1depth != lfo1DepthMode)             // RATE knob <-> DEPTH
    {
        suppressSliderCb = true;
        lfo1RateAtt.reset();
        lfo1RateAtt = std::make_unique<SliderAttachment> (
                          ap, l1depth ? "lfo1depth" : "lfo1rate", lfo1RateKnob);
        suppressSliderCb = false;
        lfo1DepthMode = l1depth;
    }

    if (envCmb != envComboMode)               // GAIN knob <-> combo selector
    {
        suppressSliderCb = true;
        if (envCmb)
            envGainAtt.reset();               // detached: position picks combo
        else
            envGainAtt = std::make_unique<SliderAttachment> (ap, "envgain", envGainKnob);
        suppressSliderCb = false;
        envComboMode = envCmb;
    }
}

void GlitchwaveAudioProcessorEditor::applyEnvComboFromKnob()
{
    envCtx      = kCtxCombo;
    envCtxUntil = juce::Time::getMillisecondCounterHiRes() + 1500.0;

    // knob quarters, in Jason's ring order: up&hi, up&low, down&hi, down&low
    const double prop  = envGainKnob.valueToProportionOfLength (envGainKnob.getValue());
    const int    combo = juce::jlimit (0, 3, (int) (prop * 4.0));

    auto* d = dynamic_cast<juce::AudioParameterChoice*> (
                  processor.apvts.getParameter ("envdrive"));
    auto* r = dynamic_cast<juce::AudioParameterChoice*> (
                  processor.apvts.getParameter ("lpfrange"));
    if (d == nullptr || r == nullptr)
        return;

    d->beginChangeGesture(); *d = combo / 2;                 d->endChangeGesture();
    r->beginChangeGesture(); *r = (combo % 2 == 0) ? 1 : 0;  r->endChangeGesture();
}

void GlitchwaveAudioProcessorEditor::d2Engage()
{
    d2Sweeping = true;

    // Resume the wave from the current depth %, in the remembered direction.
    float d = d2GetDepth();
    if (d2Rising && d >= 0.999f)        { d2Rising = false; d = 1.0f; }
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

    if (! stompDown())
    {
        d2StompSince    = 0.0;
        d2Sweeping      = false;   // releasing the stomp freezes the depth
        d2SweptThisHold = false;
        return;
    }

    if (d2StompSince == 0.0)
        d2StompSince = nowMs;

    const bool starveHold = bypassBtn.isDown()
        || juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
    if (shiftConsumed || starveHold)   // shift/starve in use: no sweep this hold
    {
        d2Sweeping = false;
        return;
    }

    if (! d2Sweeping)
    {
        if (nowMs - d2StompSince >= 750.0)
        {
            d2Engage();
            d2SweptThisHold = true;
        }
        return;
    }

    // ---- advance the sine-like depth wave: no dwell at 0% / 100% ----------
    d2U += dtMs / 4000.0;          // 0% <-> 100% in 4 s per traverse
    while (d2U >= 1.0)
    {
        d2U -= 1.0;
        d2Rising = ! d2Rising;     // turn around instantly at the extreme
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

    // v0.19: keep the shift state honest every frame (CTRL can change any time)
    updateKnobModes();

    // v0.13: LFOs never grey out — CVs just breathe their depth (VCA).
    // Filter Mode Off still disables the envelope-filter block.
    const bool filterOn = lpfModeParam != nullptr && lpfModeParam->getIndex() > 0;

    for (auto* c : std::initializer_list<juce::Component*> {
             &envGainKnob, &envGainLabel, &lpfKnob, &lpfLabel })
        c->setEnabled (filterOn);

    // ---- dual-function labels (2nd colour while shifted) ---------------------
    freqLabel.setText (shiftAttached ? "GAIN" : "FREQ", juce::dontSendNotification);
    lpfLabel.setText  (shiftAttached ? "RES"  : "LPF",  juce::dontSendNotification);
    mixLabel.setText  (starveMode ? "STARVE" : (shiftAttached ? "VOL" : "MIX"),
                       juce::dontSendNotification);
    for (auto* l : { &freqLabel, &lpfLabel })
        l->setColour (juce::Label::textColourId, shiftAttached ? kBlue : kText);
    mixLabel.setColour (juce::Label::textColourId,
                        starveMode ? kRed : (shiftAttached ? kBlue : kText));
    lfo1RateLabel.setText (lfo1DepthMode ? "DEPTH" : "RATE", juce::dontSendNotification);
    lfo1RateLabel.setColour (juce::Label::textColourId, lfo1DepthMode ? kBlue : kDim);
    envGainLabel.setText (envComboMode ? "DRV+RNG" : "GAIN", juce::dontSendNotification);
    envGainLabel.setColour (juce::Label::textColourId, envComboMode ? kBlue : kDim);

    // ---- the three section LEDs (v0.21: one LED shows everything) ------------
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    d2Frame (nowMs);

    const bool f3 = ((int) (nowMs / 166.67)) % 2 == 0;   // 3 Hz square
    const bool f6 = ((int) (nowMs /  83.33)) % 2 == 0;   // 6 Hz square
    const juce::Colour kWhite { 0xffffffff };

    auto expire = [nowMs] (int& ctx, double until)
    { if (ctx != kCtxIdle && nowMs > until) ctx = kCtxIdle; };
    expire (lfo1Ctx, lfo1CtxUntil);
    expire (lfo2Ctx, lfo2CtxUntil);
    expire (envCtx,  envCtxUntil);

    auto shapeShow = [&] (LedIndicator& led, juce::AudioParameterChoice* p)
    {
        const int idx = p != nullptr ? p->getIndex() : 0;
        led.setColour (kShapeHues[idx % 8]);
        led.setLevel ((idx >= 8 ? f6 : f3) ? 1.0f : 0.0f);   // A = 3 Hz, B = 6 Hz
    };
    auto targetShow = [&] (LedIndicator& led, juce::AudioParameterChoice* p)
    {
        const int idx = p != nullptr ? p->getIndex() : 0;
        if (idx == 0) { led.setColour (kWhite); led.setLevel (0.12f); }   // Off = dim
        else          { led.setColour (kShapeHues[idx]); led.setLevel (1.0f); }
    };

    // LFO 1
    if (lfo1DepthMode)
    {
        lfo1Led.setColour (kBlue);
        if (auto* pd = processor.apvts.getParameter ("lfo1depth"))
            lfo1Led.setLevel (pd->getValue());
    }
    else if (lfo1Ctx == kCtxShape)  shapeShow  (lfo1Led, lfo1ShapeParam);
    else if (lfo1Ctx == kCtxTarget) targetShow (lfo1Led, lfo1TargetParam);
    else   // idle: WHITE breathing at the LFO rate
    {
        lfo1Led.setColour (kWhite);
        lfo1Led.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (0)));
    }

    // LFO 2
    if (d2Sweeping)
    {
        lfo2Led.setColour (kBlue);
        lfo2Led.setLevel (d2GetDepth());
    }
    else if (lfo2Ctx == kCtxShape)  shapeShow  (lfo2Led, lfo2ShapeParam);
    else if (lfo2Ctx == kCtxTarget) targetShow (lfo2Led, lfo2TargetParam);
    else
    {
        lfo2Led.setColour (kWhite);
        lfo2Led.setLevel ((processor.readVis (1) + 1.0f) * 0.5f);
    }

    // ENV
    if (envCtx == kCtxMode)         // filter mode: hue @ 3 Hz
    {
        const int m = lpfModeParam != nullptr ? lpfModeParam->getIndex() : 0;
        envLed.setColour (kShapeHues[juce::jlimit (0, 4, m)]);
        envLed.setLevel (f3 ? 1.0f : 0.0f);
    }
    else if (envCtx == kCtxCombo)   // drive x range combo: hue @ 6 Hz
    {
        const int d = envDriveParam  != nullptr ? envDriveParam->getIndex()  : 0;
        const int r = lpfRangeParam  != nullptr ? lpfRangeParam->getIndex()  : 0;
        const int combo = d * 2 + (r == 1 ? 0 : 1);   // ring order up&hi..dn&lo
        envLed.setColour (kShapeHues[juce::jlimit (0, 3, combo)]);
        envLed.setLevel (f6 ? 1.0f : 0.0f);
    }
    else if (envCtx == kCtxTarget)  targetShow (envLed, envTargetParam);
    else                            // idle: WHITE @ envelope level
    {
        envLed.setColour (kWhite);
        envLed.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (2)));
    }

    // bypass status LED (green = effect active, like every pedal)
    if (auto* pb = processor.apvts.getParameter ("bypass"))
        bypassLed.setLevel (pb->getValue() >= 0.5f ? 0.0f : 1.0f);

    // LFO2 live readout (its rate/depth have no knobs)
    if (auto* pr = processor.apvts.getParameter ("lfo2rate"))
        if (auto* pd = processor.apvts.getParameter ("lfo2depth"))
            lfo2ValueLabel.setText (
                juce::String (pr->convertFrom0to1 (pr->getValue()), 2) + juce::String::fromUTF8 (" Hz  \xc2\xb7  ")
                    + juce::String (juce::roundToInt (pd->getValue() * 100.0f)) + " %",
                juce::dontSendNotification);
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
    g.drawText (juce::String::fromUTF8 ("LM567 glitch pedal — hardware layout — v0.21"),
                20, 38, 500, 16, juce::Justification::centredLeft);

    drawSection (g, { 12,  60, 1036, 206 }, "PEDAL");
    drawSection (g, { 12, 274, 332, 252 }, juce::String::fromUTF8 ("LFO 1  \xc2\xb7  UNI \xe2\x86\x91"));
    drawSection (g, { 352, 274, 332, 252 }, juce::String::fromUTF8 ("LFO 2  \xc2\xb7  BIPOLAR"));
    drawSection (g, { 692, 274, 356, 252 }, "ENVELOPE FOLLOWER");
    drawSection (g, { 12, 534, 332, 154 }, "CV 1 JACK  (SIDECHAIN L)");
    drawSection (g, { 352, 534, 332, 154 }, "CV 2 JACK  (SIDECHAIN R)");
    if (gateOpen)
        drawSection (g, { 692, 534, 356, 154 }, "OUTPUT GATE");
    drawSection (g, { 12, 696, 1036, 56 }, juce::String::fromUTF8 ("FOOTSWITCHES  ·  POWER"));

    // button captions, printed like a control plate (v0.21 scheme)
    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("TAP = SHAPE",          20, 462, 160, 11, juce::Justification::centredLeft);
    g.drawText ("HOLD = TARGET",        20, 474, 160, 11, juce::Justification::centredLeft);
    g.drawText ("HOLD + RATE = DEPTH",  20, 486, 160, 11, juce::Justification::centredLeft);
    g.drawText ("TAP = TEMPO",          340, 404, 122, 11, juce::Justification::centred);
    g.drawText ("HOLD = DEPTH SWEEP",   334, 416, 128, 11, juce::Justification::centred);
    g.drawText ("HOLD + KNOB = 2ND FN", 334, 428, 128, 11, juce::Justification::centred);
    g.setFont (juce::FontOptions (8.5f));
    g.drawText ("(sim: CTRL = stomp)",  334, 440, 128, 10, juce::Justification::centred);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("TAP = SHAPE",          556, 506, 140, 11, juce::Justification::centred);
    g.drawText ("HOLD = TARGET",        556, 518, 140, 11, juce::Justification::centred);
    g.drawText ("TAP = MODE",           704, 496, 170, 11, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.drawText ("HOLD = TARGET   HOLD + GAIN = DRV/RNG", 704, 508, 190, 11, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));

    // ---- v0.21 legend charts: what the one LED is telling you ---------------
    auto chart = [&g] (int x, int yHdr, const char* title, const char* const* names,
                       int n, int hueOffset, bool dimFirst)
    {
        g.setColour (kDim);
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText (title, x, yHdr, 110, 10, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (8.0f));
        for (int i = 0; i < n; ++i)
        {
            const int ry = yHdr + 12 + i * 13;
            if (dimFirst && i == 0)
                g.setColour (juce::Colour (0xff3a3f47));
            else
                g.setColour (kShapeHues[(i + hueOffset) & 7]);
            g.fillEllipse ((float) x, (float) ry + 3.0f, 7.0f, 7.0f);
            g.setColour (kDim);
            g.drawText (names[i], x + 11, ry, 96, 13, juce::Justification::centredLeft);
        }
    };

    static const char* kShapeNames[8] = { "Ramp Up / Lorenz", "Ramp Dn / Rossler",
        "Square / Drunk", "Triangle / Perlin", "Sine / Wobble", "Sweep / Glitch",
        "R.Slope / White", "S&H / Pink" };
    static const char* kT1[8] = { "Off", "Freq", "LPF", "Res", "Mix", "Gain",
                                  "Env Gain", "Env Level" };
    static const char* kT2[8] = { "Off", "Freq", "LPF", "Res", "Mix",
                                  "LFO1 Rate", "LFO1 Depth", "Env Gain" };
    static const char* kTE[8] = { "Off", "Freq", "LPF", "Res", "Mix", "Gain",
                                  "LFO1 Rate", "LFO1 Depth" };
    static const char* kModeN[5]  = { "Off", "LP", "BP", "HP", "Notch" };
    static const char* kComboN[4] = { "Up-Hi", "Up-Lo", "Dn-Hi", "Dn-Lo" };

    // LFO panels: shape chart (flashing) + target chart (solid)
    chart (122, 344, "SHAPE  A=3Hz B=6Hz", kShapeNames, 8, 0, false);
    chart (232, 344, "TARGET  solid",      kT1,         8, 0, true);
    chart (466, 344, "SHAPE  A=3Hz B=6Hz", kShapeNames, 8, 0, false);
    chart (576, 344, "TARGET  solid",      kT2,         8, 0, true);
    // env panel: target + mode + combo charts
    chart (800, 330, "TARGET  solid",      kTE,     8, 0, true);
    chart (912, 330, "MODE  3Hz",          kModeN,  5, 0, false);
    chart (912, 414, "DRV/RNG  6Hz",       kComboN, 4, 0, false);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("LED white = LFO wave",  122, 458, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED white = LFO wave",  466, 458, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED white = env level", 912, 486, 130, 10, juce::Justification::centredLeft);

    // footswitch strip lettering
    g.setColour (kText);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("BYPASS", 332, 722, 70, 14, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.5f));
    g.drawText (juce::String::fromUTF8 ("HOLD BYPASS + TAP TEMPO and turn MIX = secret rail STARVE (floors at 5 V)  ·  sim: CTRL + ALT + drag MIX"),
                430, 708, 640, 12, juce::Justification::centredLeft);
    g.drawText ("9-18 V centre-negative, polarity-protected & filtered; digital rails regulated, never starved",
                430, 724, 640, 12, juce::Justification::centredLeft);

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

    g.setColour (kText);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("BAZZ FUSS", 764, 112, 110, 16, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.5f));
    g.drawText ("always on", 764, 130, 110, 12, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("IN",  976, 84, 24, 12, juce::Justification::centred);
    g.drawText ("OUT", 1006, 84, 30, 12, juce::Justification::centred);
}

void GlitchwaveAudioProcessorEditor::resized()
{
    // ---- pedal row (v0.19: 3 dual-function knobs) ----------------------------
    {
        const int y = 82, knobW = 130, knobH = 140, labelH = 18;
        const int xs[3] = { 90, 320, 550 };
        juce::Slider* ks[] = { &freqKnob, &lpfKnob, &mixKnob };
        juce::Label*  ls[] = { &freqLabel, &lpfLabel, &mixLabel };
        for (int i = 0; i < 3; ++i)
        {
            ls[i]->setBounds (xs[i], y, knobW, labelH);
            ks[i]->setBounds (xs[i], y + labelH, knobW, knobH);
        }
        meterIn.setBounds  (980, 98, 16, 150);
        meterOut.setBounds (1012, 98, 16, 150);
    }

    // ---- LFO panels ------------------------------------------------------------
    {
        const int y = 296;
        // LFO 1 (single dual-function knob: RATE / hold = DEPTH)
        lfo1RateLabel.setBounds (24, y + 10, 80, 12);
        lfo1RateKnob.setBounds  (24, y + 22, 80, 90);
        lfo1Led.setBounds       (148, y + 4, 34, 34);   // THE section LED
        lfo1Btn.setBounds    (280, y + 160, 44, 44);
        // LFO 2 — one button for rate (tap tempo) + depth (hold), no knobs
        lfo2RateBtn.setBounds   (382, y + 8, 48, 48);
        lfo2Led.setBounds       (492, y + 4, 34, 34);  // THE section LED
        lfo2ValueLabel.setBounds (356, y + 88, 108, 16);
        lfo2Btn.setBounds    (614, y + 160, 44, 44);
    }

    // ---- envelope follower panel -------------------------------------------------
    {
        const int y = 296;
        envGainLabel.setBounds (704, y + 10, 76, 12);
        envGainKnob.setBounds  (704, y + 22, 76, 90);
        envLed.setBounds       (836, y + 4, 34, 34);    // THE section LED
        envBtn.setBounds       (718, y + 148, 48, 48);
    }

    // ---- v0.21 footswitch strip -------------------------------------------------
    {
        bypassBtn.setBounds (250, 706, 40, 40);
        bypassLed.setBounds (304, 716, 20, 20);
        supplyBtn.setBounds (920, 714, 116, 24);
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
