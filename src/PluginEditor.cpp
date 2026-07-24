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
    const juce::Colour kAmber      { 0xffd9a441 };   // the C3 colour
    const juce::Colour kRed        { 0xffd95050 };
    const juce::Colour kBlue       { 0xff6fa8dc };   // the C2 colour + depth LED

    // NeoPixel hue per slot (same slot in Bank A and Bank B / targets / modes)
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

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }
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

    // ---- the six knobs (v0.24: all layer-switched, no section buttons) ------
    setupKnob (freqKnob, freqLabel, "FREQ", true);   // C2 GAIN   C3 LFO1 DEPTH
    setupKnob (lpfKnob,  lpfLabel,  "LPF",  true);   // C2 RES    C3 LFO2 DEPTH
    setupKnob (mixKnob,  mixLabel,  "MIX",  true);   // C2 VOL    C3 DRV/RNG  (both: STARVE)
    setupKnob (lfo1RateKnob, lfo1RateLabel, "RATE", false); // C2 TARGET  C3 SHAPE
    setupKnob (lfo2RateKnob, lfo2RateLabel, "RATE", false); // C2 TARGET  C3 SHAPE
    setupKnob (envGainKnob,  envGainLabel,  "GAIN", false); // C2 TARGET  C3 MODE
    freqAtt     = std::make_unique<SliderAttachment> (apvts, "freq",     freqKnob);
    lpfAtt      = std::make_unique<SliderAttachment> (apvts, "fizz",     lpfKnob);
    mixAtt      = std::make_unique<SliderAttachment> (apvts, "dry",      mixKnob);
    lfo1RateAtt = std::make_unique<SliderAttachment> (apvts, "lfo1rate", lfo1RateKnob);
    lfo2RateAtt = std::make_unique<SliderAttachment> (apvts, "lfo2rate", lfo2RateKnob);
    envGainAtt  = std::make_unique<SliderAttachment> (apvts, "envgain",  envGainKnob);

    freqKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 2)   // writing lfo1depth: LED shows blue @ depth
        { lfo1Ctx = kCtxDepth; lfo1CtxUntil = nowMs() + 1500.0; }
    };
    lpfKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 2)   // writing lfo2depth
        { lfo2Ctx = kCtxDepth; lfo2CtxUntil = nowMs() + 1500.0; }
    };
    mixKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 2)
            applyComboFromMixKnob();          // C3: knob quarters pick DRV x RNG
    };
    // v0.30: Y (layer 1) = SHAPE / SHAPE / MODE, Z (layer 2) = TARGET x3
    lfo1RateKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 1)      applyZone (lfo1RateKnob, "lfo1shape5", 16,
                                            lfo1Ctx, lfo1CtxUntil, kCtxShape);
        else if (knobLayer == 2) applyZone (lfo1RateKnob, "lfo1target5", 8,
                                            lfo1Ctx, lfo1CtxUntil, kCtxTarget);
    };
    lfo2RateKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 1)      applyZone (lfo2RateKnob, "lfo2shape4", 16,
                                            lfo2Ctx, lfo2CtxUntil, kCtxShape);
        else if (knobLayer == 2) applyZone (lfo2RateKnob, "lfo2target4", 8,
                                            lfo2Ctx, lfo2CtxUntil, kCtxTarget);
    };
    envGainKnob.onValueChange = [this]
    {
        if (suppressSliderCb) return;
        knobTouched();
        if (knobLayer == 1)      applyZone (envGainKnob, "lpfmode3", 5,
                                            envCtx, envCtxUntil, kCtxMode);
        else if (knobLayer == 2) applyZone (envGainKnob, "envtarget5", 8,
                                            envCtx, envCtxUntil, kCtxTarget);
    };

    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    for (auto* l : { &lfo1ValueLabel, &lfo2ValueLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f));
        l->setColour (juce::Label::textColourId, kDim);
        addAndMakeVisible (*l);
    }

    // v0.30: permanent key-state readout (stays until the control scheme is
    // signed off) — raw INS/DEL as the app sees them + the active layer
    keyReadout.setJustificationType (juce::Justification::centredLeft);
    keyReadout.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    keyReadout.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (keyReadout);

    // cache the choice params the LEDs display
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
    addAndMakeVisible (envLed);

    // ---- the TAP TEMPO stomp (sim: CTRL = held) ------------------------------
    tapStompBtn.onPress = [this]
    {
        tapPressMs   = nowMs();
        tapPressLfo2 = bypassStompDown();
        if (tapPressLfo2 && bypassBtn.isDown())
            bypassBtn.cancelPressActions();   // that hold is a tap-shift now, not a bypass toggle
        updateKnobModes();
    };
    tapStompBtn.onTap     = [this] { recordTap (tapPressLfo2, tapPressMs); };
    tapStompBtn.onRelease = [this] { tapPressMs = 0.0; updateKnobModes(); };
    addAndMakeVisible (tapStompBtn);

    // ---- the BYPASS stomp (sim: ALT = held) ----------------------------------
    bypassBtn.onTap = [this]
    {
        if (auto* pb = processor.apvts.getParameter ("bypass"))
        {
            pb->beginChangeGesture();
            pb->setValueNotifyingHost (pb->getValue() >= 0.5f ? 0.0f : 1.0f);
            pb->endChangeGesture();
        }
    };
    bypassBtn.onPress   = [this] { updateKnobModes(); };
    bypassBtn.onRelease = [this] { updateKnobModes(); };
    addAndMakeVisible (bypassBtn);
    bypassLed.setColour (kGreen);
    addAndMakeVisible (bypassLed);

    supplyBtn.onClick = [this]
    {
        if (auto* ps = dynamic_cast<juce::AudioParameterChoice*> (
                          processor.apvts.getParameter ("supply")))
        {
            ps->beginChangeGesture();
            *ps = 1 - ps->getIndex();      // swap the wall-wart: 9 V <-> 18 V
            ps->endChangeGesture();
            supplyBtn.setButtonText (ps->getIndex() == 1 ? "18V" : "9V");
        }
    };
    addAndMakeVisible (supplyBtn);

    // v0.22/24: output clip stage audition — now 8 modes (the winner gets
    // hardwired like the dirt circuit was)
    clipBtn.onClick = [this]
    {
        if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (
                          processor.apvts.getParameter ("clipmode")))
        {
            pc->beginChangeGesture();
            *pc = (pc->getIndex() + 1) % pc->choices.size();
            pc->endChangeGesture();
            clipBtn.setButtonText ("CLIP " + pc->getCurrentChoiceName());
        }
    };
    clipBtn.setButtonText ("CLIP A: -6 Ladder");
    addAndMakeVisible (clipBtn);

    // v0.23: the +6 dB output boost has its own audition toggle
    boostBtn.onClick = [this]
    {
        if (auto* pb = processor.apvts.getParameter ("boost6"))
        {
            pb->beginChangeGesture();
            pb->setValueNotifyingHost (pb->getValue() >= 0.5f ? 0.0f : 1.0f);
            pb->endChangeGesture();
            boostBtn.setButtonText (pb->getValue() >= 0.5f ? "+6 dB: ON" : "+6 dB: OFF");
        }
    };
    boostBtn.setButtonText ("+6 dB: ON");
    addAndMakeVisible (boostBtn);

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

    startTimerHz (60);
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
    // v0.25: CTRL/ALT are the C2/C3 layer keys — stop JUCE from hijacking a
    // modifier-held drag into its "velocity" fine-adjust mode (which makes the
    // knob appear frozen while a layer key is held)
    s.setVelocityModeParameters (1.0, 1, 0.0, false);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (big ? 16.0f : 11.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, big ? kText : kDim);
    addAndMakeVisible (l);
}

// ---------------------------------------------------------------------------
// v0.24 layer machinery
// ---------------------------------------------------------------------------
bool GlitchwaveAudioProcessorEditor::tapStompDown() const
{
    // v0.30 (Jason's X/Y/Z/A spec): Y = TAP held = INS, Z = BYPASS held =
    // DEL. Polled globally via GetAsyncKeyState; right-click LATCH remains
    // the mouse-only hold.
    return tapStompBtn.isDown() || tapStompBtn.isLatched()
        || juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::insertKey);
}

bool GlitchwaveAudioProcessorEditor::bypassStompDown() const
{
    return bypassBtn.isDown() || bypassBtn.isLatched()
        || juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::deleteKey);
}

int GlitchwaveAudioProcessorEditor::computeLayer() const
{
    const bool t = tapStompDown(), b = bypassStompDown();
    return (t && b) ? 3 : b ? 2 : t ? 1 : 0;
}

void GlitchwaveAudioProcessorEditor::updateKnobModes()
{
    const int layer = computeLayer();
    if (layer == knobLayer)
        return;

    // v0.27: NEVER swap attachments while a knob is being dragged. A drag
    // latches the function it started on; the layer switch lands only after
    // every knob is released. (Before this, releasing the layer key a beat
    // before the mouse re-attached the knob to its C1 param MID-DRAG, and
    // the tail of the drag slammed that param — VOL to 0, rates to 20 Hz...)
    for (auto* s : { &freqKnob, &lpfKnob, &mixKnob,
                     &lfo1RateKnob, &lfo2RateKnob, &envGainKnob })
        if (s->isMouseButtonDown())
            return;

    auto& ap = processor.apvts;
    suppressSliderCb = true;
    freqAtt.reset(); lpfAtt.reset(); mixAtt.reset();
    lfo1RateAtt.reset(); lfo2RateAtt.reset(); envGainAtt.reset();

    switch (layer)
    {
        case 0:   // C1 — the printed panel
            freqAtt     = std::make_unique<SliderAttachment> (ap, "freq",     freqKnob);
            lpfAtt      = std::make_unique<SliderAttachment> (ap, "fizz",     lpfKnob);
            mixAtt      = std::make_unique<SliderAttachment> (ap, "dry",      mixKnob);
            lfo1RateAtt = std::make_unique<SliderAttachment> (ap, "lfo1rate", lfo1RateKnob);
            lfo2RateAtt = std::make_unique<SliderAttachment> (ap, "lfo2rate", lfo2RateKnob);
            envGainAtt  = std::make_unique<SliderAttachment> (ap, "envgain",  envGainKnob);
            break;
        case 1:   // C2 (TAP held) — Gain / Res / Vol / Target x3 (zone-select)
            freqAtt = std::make_unique<SliderAttachment> (ap, "dirtgain", freqKnob);
            lpfAtt  = std::make_unique<SliderAttachment> (ap, "lpfq",     lpfKnob);
            mixAtt  = std::make_unique<SliderAttachment> (ap, "vol",      mixKnob);
            break;
        case 2:   // C3 (BYPASS held) — L1 Depth / L2 Depth / DrvRng / Shape x2 / Mode
            freqAtt = std::make_unique<SliderAttachment> (ap, "lfo1depth", freqKnob);
            lpfAtt  = std::make_unique<SliderAttachment> (ap, "lfo2depth", lpfKnob);
            break;
        case 3:   // secret starve: MIX only; every other knob is dead
            mixAtt  = std::make_unique<SliderAttachment> (ap, "starve", mixKnob);
            break;
    }

    // v0.31 (Jason's find): the zone-select knobs have no attachment, so on
    // re-entering Y/Z they still SHOWED the X position even though the
    // selection was intact — and the first touch teleported it. Now every
    // layer entry parks each selector knob at the CENTRE of its current
    // selection's zone, so the position always tells the truth and a small
    // turn steps to the neighbouring choice.
    auto placeZone = [] (juce::Slider& s, juce::AudioParameterChoice* pc, int zones)
    {
        if (pc != nullptr)
            s.setValue (s.proportionOfLengthToValue ((pc->getIndex() + 0.5) / (double) zones),
                        juce::dontSendNotification);
    };
    if (layer == 1)          // Y: SHAPE / SHAPE / MODE
    {
        placeZone (lfo1RateKnob, lfo1ShapeParam, 16);
        placeZone (lfo2RateKnob, lfo2ShapeParam, 16);
        placeZone (envGainKnob,  lpfModeParam,    5);
    }
    else if (layer == 2)     // Z: TARGET x3 + Mix = DRV/RNG combo
    {
        placeZone (lfo1RateKnob, lfo1TargetParam, 8);
        placeZone (lfo2RateKnob, lfo2TargetParam, 8);
        placeZone (envGainKnob,  envTargetParam,  8);
        if (envDriveParam != nullptr && lpfRangeParam != nullptr)
        {
            const int combo = envDriveParam->getIndex() * 2
                            + (lpfRangeParam->getIndex() == 1 ? 0 : 1);   // ring order
            mixKnob.setValue (mixKnob.proportionOfLengthToValue ((combo + 0.5) / 4.0),
                              juce::dontSendNotification);
        }
    }

    suppressSliderCb = false;
    knobLayer = layer;
}

void GlitchwaveAudioProcessorEditor::knobTouched()
{
    // a knob move turns the held stomp(s) into pure layer-shifts: no tempo
    // tap on the TAP stomp, no bypass toggle on the BYPASS stomp
    if (tapStompBtn.isDown()) tapStompBtn.cancelPressActions();
    if (bypassBtn.isDown())   bypassBtn.cancelPressActions();
}

void GlitchwaveAudioProcessorEditor::applyZone (juce::Slider& s, const char* paramID,
                                                int zones, int& ctx, double& ctxUntil,
                                                int ctxKind)
{
    const double prop = s.valueToProportionOfLength (s.getValue());
    const int    idx  = juce::jlimit (0, zones - 1, (int) (prop * zones));
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (
                       processor.apvts.getParameter (paramID)))
        if (pc->getIndex() != idx)
        {
            pc->beginChangeGesture();
            *pc = idx;
            pc->endChangeGesture();
        }
    ctx      = ctxKind;
    ctxUntil = nowMs() + 1500.0;
}

void GlitchwaveAudioProcessorEditor::applyComboFromMixKnob()
{
    envCtx      = kCtxCombo;
    envCtxUntil = nowMs() + 1500.0;

    // knob quarters, in Jason's ring order: up&hi, up&low, down&hi, down&low
    const double prop  = mixKnob.valueToProportionOfLength (mixKnob.getValue());
    const int    combo = juce::jlimit (0, 3, (int) (prop * 4.0));

    auto* d = envDriveParam;
    auto* r = lpfRangeParam;
    if (d == nullptr || r == nullptr)
        return;

    d->beginChangeGesture(); *d = combo / 2;                 d->endChangeGesture();
    r->beginChangeGesture(); *r = (combo % 2 == 0) ? 1 : 0;  r->endChangeGesture();
}

// ---------------------------------------------------------------------------
// v0.24 tap tempo: rolling average of the last 4 taps. 1-3 taps arm only;
// the 4th (and every one after) commits. A gap > 5 s starts a new chain.
// ---------------------------------------------------------------------------
void GlitchwaveAudioProcessorEditor::recordTap (bool lfo2, double pressMs)
{
    if (pressMs <= 0.0)
        return;
    double* h = lfo2 ? tapHist2 : tapHist1;
    int&    n = lfo2 ? tapN2    : tapN1;

    if (n > 0 && pressMs - h[n - 1] > 5000.0)
        n = 0;                                   // stale chain: start over
    if (n == 4)
    { h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; n = 3; }
    h[n++] = pressMs;

    if (n == 4)
    {
        const double avgMs = (h[3] - h[0]) / 3.0;
        const float  hz    = juce::jlimit (0.2f, 20.0f, (float) (1000.0 / avgMs));
        if (auto* pr = processor.apvts.getParameter (lfo2 ? "lfo2rate" : "lfo1rate"))
        {
            pr->beginChangeGesture();
            pr->setValueNotifyingHost (pr->convertTo0to1 (hz));
            pr->endChangeGesture();
        }
        // a committed tap re-seeds that LFO's chaos generators (feels synced)
        if (lfo2) processor.requestLfo2Retrigger();
        else      processor.requestLfo1Retrigger();
    }
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

    // keep the layer honest every frame (keys can change any time)
    updateKnobModes();
    const int layer = knobLayer;

    // ---- v0.30 permanent key-state readout + stomp held-indicators -----------
    {
        const bool ins = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::insertKey);
        const bool del = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::deleteKey);
        static const char* layerNames[4] = { "X", "Y", "Z", "A" };
        juce::String s;
        s << "INS[" << (ins ? "#" : "-") << "]  DEL[" << (del ? "#" : "-") << "]  LAYER "
          << layerNames[juce::jlimit (0, 3, layer)];
        if (tapStompBtn.isLatched() || bypassBtn.isLatched())
            s << "  latched";
        keyReadout.setText (s, juce::dontSendNotification);
        keyReadout.setColour (juce::Label::textColourId,
                              layer == 3 ? kRed : layer == 2 ? kAmber
                                               : layer == 1 ? kBlue : kDim);
        tapStompBtn.setIndicated (tapStompDown());
        bypassBtn.setIndicated (bypassStompDown());
    }

    const bool filterOn = lpfModeParam != nullptr && lpfModeParam->getIndex() > 0;

    // knob enables per layer. The env-gain knob must stay alive in C3 even
    // with the filter Off — it's how the Mode gets turned back on.
    freqKnob.setEnabled     (layer != 3);
    lpfKnob.setEnabled      (layer == 2 ? true : (layer == 3 ? false : filterOn));
    mixKnob.setEnabled      (true);
    lfo1RateKnob.setEnabled (layer != 3);
    lfo2RateKnob.setEnabled (layer != 3);
    envGainKnob.setEnabled  (layer == 0 ? filterOn : layer != 3);

    // ---- knob labels follow the active layer ---------------------------------
    struct Cap { const char* t[6]; };
    static const Cap caps[4] = {
        {{ "FREQ",     "LPF",      "MIX",     "RATE",  "RATE",  "GAIN" }},    // X
        {{ "GAIN",     "RES",      "VOL",     "SHAPE", "SHAPE", "MODE" }},    // Y
        {{ "L1 DEPTH", "L2 DEPTH", "DRV/RNG", "TARGET","TARGET","TARGET" }},  // Z
        {{ "-",        "-",        "STARVE",  "-",     "-",     "-" }},       // A
    };
    const juce::Colour layerCol = layer == 1 ? kBlue : layer >= 2 ? kAmber : kText;
    const juce::Colour smallCol = layer == 0 ? kDim  : layerCol;
    juce::Label* ls[6] = { &freqLabel, &lpfLabel, &mixLabel,
                           &lfo1RateLabel, &lfo2RateLabel, &envGainLabel };
    for (int i = 0; i < 6; ++i)
    {
        ls[i]->setText (caps[layer].t[i], juce::dontSendNotification);
        ls[i]->setColour (juce::Label::textColourId, i < 3 ? layerCol : smallCol);
    }
    if (layer == 3)   // secret starve: MIX label burns red, dead knobs dim
    {
        mixLabel.setColour (juce::Label::textColourId, kRed);
        for (int i = 0; i < 6; ++i)
            if (i != 2)
                ls[i]->setColour (juce::Label::textColourId, juce::Colour (0xff53565e));
    }

    // ---- the three section LEDs: live value colour of the active layer -------
    const double t = nowMs();
    const bool f2 = ((int) (t / 250.0))   % 2 == 0;   // 2 Hz — Bank A
    const bool f5 = ((int) (t / 100.0))   % 2 == 0;   // 5 Hz — Bank B
    const bool f3 = ((int) (t / 166.67))  % 2 == 0;   // 3 Hz — filter mode
    const juce::Colour kWhite { 0xffffffff };

    auto expire = [t] (int& ctx, double until)
    { if (ctx != kCtxIdle && t > until) ctx = kCtxIdle; };
    expire (lfo1Ctx, lfo1CtxUntil);
    expire (lfo2Ctx, lfo2CtxUntil);
    expire (envCtx,  envCtxUntil);

    auto shapeShow = [&] (LedIndicator& led, juce::AudioParameterChoice* pc)
    {
        const int idx = pc != nullptr ? pc->getIndex() : 0;
        led.setColour (kShapeHues[idx % 8]);
        led.setLevel ((idx >= 8 ? f5 : f2) ? 1.0f : 0.0f);   // A = 2 Hz, B = 5 Hz
    };
    auto targetShow = [&] (LedIndicator& led, juce::AudioParameterChoice* pc)
    {
        const int idx = pc != nullptr ? pc->getIndex() : 0;
        if (idx == 0) { led.setColour (kWhite); led.setLevel (0.12f); }   // Off = dim
        else          { led.setColour (kShapeHues[idx]); led.setLevel (1.0f); }
    };
    auto depthShow = [&] (LedIndicator& led, const char* id)
    {
        led.setColour (kBlue);
        if (auto* pd = processor.apvts.getParameter (id))
            led.setLevel (pd->getValue());
    };
    auto modeShow = [&] (LedIndicator& led)
    {
        const int m = lpfModeParam != nullptr ? lpfModeParam->getIndex() : 0;
        led.setColour (kShapeHues[juce::jlimit (0, 4, m)]);
        led.setLevel (f3 ? 1.0f : 0.0f);
    };
    auto comboShow = [&] (LedIndicator& led)   // v0.24: SOLID, no blink
    {
        const int d = envDriveParam != nullptr ? envDriveParam->getIndex() : 0;
        const int r = lpfRangeParam != nullptr ? lpfRangeParam->getIndex() : 0;
        const int combo = d * 2 + (r == 1 ? 0 : 1);   // ring order up&hi..dn&lo
        led.setColour (kShapeHues[juce::jlimit (0, 3, combo)]);
        led.setLevel (1.0f);
    };

    // LFO 1 — Y shows SHAPE, Z shows TARGET (or blue depth while the Z
    // depth knob is being turned)
    if (layer == 1)      shapeShow (lfo1Led, lfo1ShapeParam);
    else if (layer == 2) (lfo1Ctx == kCtxDepth ? depthShow (lfo1Led, "lfo1depth")
                                               : targetShow (lfo1Led, lfo1TargetParam));
    else if (layer == 0 && lfo1Ctx == kCtxShape)  shapeShow  (lfo1Led, lfo1ShapeParam);
    else if (layer == 0 && lfo1Ctx == kCtxTarget) targetShow (lfo1Led, lfo1TargetParam);
    else if (layer == 0 && lfo1Ctx == kCtxDepth)  depthShow  (lfo1Led, "lfo1depth");
    else   // idle (and the dead starve layer): WHITE breathing at the LFO wave
    {
        lfo1Led.setColour (kWhite);
        lfo1Led.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (0)));
    }

    // LFO 2
    if (layer == 1)      shapeShow (lfo2Led, lfo2ShapeParam);
    else if (layer == 2) (lfo2Ctx == kCtxDepth ? depthShow (lfo2Led, "lfo2depth")
                                               : targetShow (lfo2Led, lfo2TargetParam));
    else if (layer == 0 && lfo2Ctx == kCtxShape)  shapeShow  (lfo2Led, lfo2ShapeParam);
    else if (layer == 0 && lfo2Ctx == kCtxTarget) targetShow (lfo2Led, lfo2TargetParam);
    else if (layer == 0 && lfo2Ctx == kCtxDepth)  depthShow  (lfo2Led, "lfo2depth");
    else
    {
        lfo2Led.setColour (kWhite);
        lfo2Led.setLevel ((processor.readVis (1) + 1.0f) * 0.5f);
    }

    // ENV — Y shows MODE, Z shows TARGET (or DRV/RNG combo while the Z Mix
    // knob is being turned)
    if (layer == 1)      modeShow (envLed);
    else if (layer == 2) (envCtx == kCtxCombo ? comboShow (envLed)
                                              : targetShow (envLed, envTargetParam));
    else if (layer == 0 && envCtx == kCtxTarget) targetShow (envLed, envTargetParam);
    else if (layer == 0 && envCtx == kCtxMode)   modeShow  (envLed);
    else if (layer == 0 && envCtx == kCtxCombo)  comboShow (envLed);
    else
    {
        envLed.setColour (kWhite);
        envLed.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (2)));
    }

    // bypass status LED (green = effect active, like every pedal)
    if (auto* pb = processor.apvts.getParameter ("bypass"))
        bypassLed.setLevel (pb->getValue() >= 0.5f ? 0.0f : 1.0f);

    // live rate · depth readouts
    auto readout = [this] (juce::Label& l, const char* rateId, const char* depthId)
    {
        auto* pr = processor.apvts.getParameter (rateId);
        auto* pd = processor.apvts.getParameter (depthId);
        if (pr != nullptr && pd != nullptr)
            l.setText (juce::String (pr->convertFrom0to1 (pr->getValue()), 2)
                           + juce::String::fromUTF8 (" Hz \xc2\xb7 ")
                           + juce::String (juce::roundToInt (pd->getValue() * 100.0f)) + " %",
                       juce::dontSendNotification);
    };
    readout (lfo1ValueLabel, "lfo1rate", "lfo1depth");
    readout (lfo2ValueLabel, "lfo2rate", "lfo2depth");

    cv1Led.setLevel (juce::jmin (1.0f, processor.readVis (3) * 1.5f));
    cv2Led.setLevel (juce::jmin (1.0f, processor.readVis (4) * 1.5f));

    // gate LED: green = open, amber blinking = fading, red = fully closed
    const float atten = processor.readVis (5);
    const bool  blink = ((frame / 14) % 2) == 0;
    juce::Colour gc; float gl;
    if (atten > -0.5f)       { gc = kGreen; gl = 1.0f; }
    else if (atten > -95.0f) { gc = kAmber; gl = blink ? 1.0f : 0.15f; }
    else                     { gc = kRed;   gl = 1.0f; }
    gateLed.setColour (gc);
    gateLed.setLevel (gl);
    gateCover.setLedState (gc, gl);

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
    g.drawText (juce::String::fromUTF8 ("LM567 glitch pedal — hardware layout — v0.31"),
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

    // ---- the C1/C2/C3 layer chart, printed on the pedal face ---------------
    g.setColour (kText);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("KNOB LAYERS", 700, 150, 260, 11, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (8.5f));
    g.setColour (kDim);
    g.drawText ("X              FREQ    LPF      MIX       RATE  RATE  GAIN",
                700, 164, 340, 10, juce::Justification::centredLeft);
    g.setColour (kBlue);
    g.drawText ("Y  TAP / INS   GAIN    RES      VOL       SHP   SHP   MODE",
                700, 176, 340, 10, juce::Justification::centredLeft);
    g.setColour (kAmber);
    g.drawText ("Z  BYP / DEL   L1 DEP  L2 DEP   DRV/RNG   TGT   TGT   TGT",
                700, 188, 340, 10, juce::Justification::centredLeft);
    g.setColour (kRed);
    g.drawText ("A  BOTH: MIX = STARVE (secret), other knobs dead",
                700, 200, 340, 10, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.drawText ("hold the key or the stomp - or RIGHT-CLICK a stomp to latch it",
                700, 212, 340, 10, juce::Justification::centredLeft);

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

    // ---- legend charts: what the one LED is telling you ---------------------
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

    chart (122, 344, "SHAPE (Y) A=2Hz B=5Hz", kShapeNames, 8, 0, false);
    chart (232, 344, "TARGET (Z) solid",      kT1,         8, 0, true);
    chart (466, 344, "SHAPE (Y) A=2Hz B=5Hz", kShapeNames, 8, 0, false);
    chart (576, 344, "TARGET (Z) solid",      kT2,         8, 0, true);
    chart (800, 330, "TARGET (Z) solid",      kTE,     8, 0, true);
    chart (912, 330, "MODE (Y) 3Hz",          kModeN,  5, 0, false);
    chart (912, 414, "DRV/RNG (Z) solid",     kComboN, 4, 0, false);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("LED white = LFO wave",           122, 458, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED blue = depth %",             122, 470, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED white = LFO wave",           466, 458, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED blue = depth %",             466, 470, 110, 10, juce::Justification::centredLeft);
    g.drawText ("LED white = env level",          912, 486, 130, 10, juce::Justification::centredLeft);

    // panel hints: where each section's C2/C3 things live
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("Y RATE-KNOB = SHAPE · Z = TARGET · Z FREQ-KNOB = DEPTH"),
                20, 502, 310, 11, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("Y RATE-KNOB = SHAPE · Z = TARGET · Z LPF-KNOB = DEPTH"),
                360, 502, 310, 11, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("Y GAIN-KNOB = MODE · Z = TARGET · Z MIX-KNOB = DRV/RNG"),
                704, 502, 336, 11, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("TAP \xc3\x97""4 = RATE"), 360, 490, 150, 11,
                juce::Justification::centredLeft);

    // footswitch strip lettering
    g.setColour (kText);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TAP",    236, 722, 40, 14, juce::Justification::centredLeft);
    g.drawText ("BYPASS", 378, 722, 60, 14, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (8.5f));
    g.drawText (juce::String::fromUTF8 ("TAP \xc3\x97""4 avg = LFO1 RATE · BYPASS held: LFO2 RATE · 0.2\xe2\x80\x93""20 Hz"),
                450, 718, 270, 12, juce::Justification::centredLeft);
    g.drawText ("Y = TAP/INS held, Z = BYPASS/DEL held, A = both = STARVE",
                450, 731, 270, 12, juce::Justification::centredLeft);

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
}

void GlitchwaveAudioProcessorEditor::resized()
{
    // ---- pedal row (3 big dual-layer knobs) ----------------------------------
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

    // ---- LFO panels (one RATE knob + one LED each, v0.24: no buttons) --------
    {
        const int y = 296;
        lfo1RateLabel.setBounds (24, y + 10, 80, 12);
        lfo1RateKnob.setBounds  (24, y + 22, 80, 90);
        lfo1Led.setBounds       (148, y + 4, 34, 34);   // THE section LED
        lfo1ValueLabel.setBounds (16, y + 118, 96, 14);

        lfo2RateLabel.setBounds (364, y + 10, 80, 12);
        lfo2RateKnob.setBounds  (364, y + 22, 80, 90);
        lfo2Led.setBounds       (492, y + 4, 34, 34);   // THE section LED
        lfo2ValueLabel.setBounds (356, y + 118, 96, 14);
    }

    // ---- envelope follower panel ---------------------------------------------
    {
        const int y = 296;
        envGainLabel.setBounds (704, y + 10, 76, 12);
        envGainKnob.setBounds  (704, y + 22, 76, 90);
        envLed.setBounds       (836, y + 4, 34, 34);    // THE section LED
    }

    // ---- footswitch strip -----------------------------------------------------
    {
        tapStompBtn.setBounds (190, 706, 40, 40);
        bypassBtn.setBounds   (300, 706, 40, 40);
        bypassLed.setBounds   (354, 716, 20, 20);
        keyReadout.setBounds  (450, 700, 270, 16);   // v0.30 permanent readout
        boostBtn.setBounds    (724, 714, 92, 24);
        clipBtn.setBounds     (824, 714, 138, 24);
        supplyBtn.setBounds   (970, 714, 70, 24);
    }

    // ---- CV jack panels + gate ------------------------------------------------
    {
        cv1Led.setBounds (28, 582, 22, 22);
        cv2Led.setBounds (368, 582, 22, 22);

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
