#include "PluginEditor.h"

namespace
{
    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    const char* kDash = "\xe2\x80\x94";   // em dash for dead knobs

    juce::String shortMode (const juce::String& s)   // "Mode LP" -> "LP"
    {
        return s.startsWith ("Mode ") ? s.substring (5) : s;
    }

    // panel chrome straight from the design: translucent black card, hairline
    // border, glowing 2 px accent bar across the top
    void panel (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent)
    {
        g.setColour (gw::kPanelBg);
        g.fillRoundedRectangle (r.toFloat(), 12.0f);
        g.setColour (gw::kHairline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 12.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (r.toFloat().reduced (1.5f), 11.0f, 1.0f);
        if (! accent.isTransparent())
        {
            g.setColour (accent.withAlpha (0.22f));
            g.fillRect (r.getX(), r.getY() - 2, r.getWidth(), 7);
            g.setColour (accent);
            g.fillRect (r.getX() + 2, r.getY(), r.getWidth() - 4, 2);
        }
    }
}

GlitchwaveAudioProcessorEditor::GlitchwaveAudioProcessorEditor (GlitchwaveAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    bgImage   = juce::ImageCache::getFromMemory (BinaryData::bg_face_jpg,    BinaryData::bg_face_jpgSize);
    logoFx    = juce::ImageCache::getFromMemory (BinaryData::logo_fx_png,    BinaryData::logo_fx_pngSize);
    logoPlain = juce::ImageCache::getFromMemory (BinaryData::logo_plain_png, BinaryData::logo_plain_pngSize);

    auto& apvts = processor.apvts;

    // ---- the six knobs (v0.24: all layer-switched, no section buttons) ------
    setupKnob (freqKnob, freqLabel, "FREQ", true,  gw::kYellow);
    setupKnob (lpfKnob,  lpfLabel,  "LPF",  true,  gw::kYellow);
    setupKnob (mixKnob,  mixLabel,  "MIX",  true,  gw::kYellow);
    setupKnob (lfo1RateKnob, lfo1RateLabel, "RATE", false, gw::kCyan);
    setupKnob (lfo2RateKnob, lfo2RateLabel, "RATE", false, gw::kMagenta);
    setupKnob (envGainKnob,  envGainLabel,  "GAIN", false, gw::kGreen);
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
        if (knobLayer == 2)   // writing lfo1depth: LED shows depth
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
            applyComboFromMixKnob();          // Z: knob quarters pick DRV x RNG
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

    // live value line under each knob
    for (int i = 0; i < 6; ++i)
    {
        vals[i].setJustificationType (juce::Justification::centred);
        vals[i].setFont (i < 3 ? gw::mono (13.0f, 500) : gw::mono (12.0f, 400));
        vals[i].setColour (juce::Label::textColourId, gw::kText);
        addAndMakeVisible (vals[i]);
    }
    for (auto* l : { &depth1Cap, &depth2Cap, &envSubCap })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (gw::mono (9.0f, 400, 0.06f));
        l->setColour (juce::Label::textColourId, gw::kDim2);
        addAndMakeVisible (*l);
    }

    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);
    addAndMakeVisible (chips);
    addAndMakeVisible (tagline);

    // selection rulers + names
    l1ShapeRuler.configure (8, false);
    l2ShapeRuler.configure (8, false);
    l1TargetRuler.configure (8, true);
    l2TargetRuler.configure (8, true);
    envModeRuler.configure (5, true);
    envTargetRuler.configure (8, true);
    envComboRuler.configure (4, false);
    for (auto* c : std::initializer_list<juce::Component*> {
             &l1ShapeRuler, &l1TargetRuler, &l2ShapeRuler, &l2TargetRuler,
             &envModeRuler, &envTargetRuler, &envComboRuler,
             &l1ShapeName, &l1TargetName, &l2ShapeName, &l2TargetName,
             &envModeName, &envTargetName, &envComboName })
        addAndMakeVisible (*c);

    // cache the choice params the LEDs + rulers display
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

    // ---- the TAP TEMPO stomp -------------------------------------------------
    tapStompBtn.onPress = [this]
    {
        tapPressMs     = nowMs();
        lastTapFlashMs = tapPressMs;          // v0.32: tempo LED flashes each press
        tapPressLfo2 = bypassStompDown();
        if (tapPressLfo2 && bypassBtn.isDown())
            bypassBtn.cancelPressActions();   // that hold is a tap-shift now, not a bypass toggle
        updateKnobModes();
    };
    tapStompBtn.onTap     = [this] { recordTap (tapPressLfo2, tapPressMs); };
    tapStompBtn.onRelease = [this] { tapPressMs = 0.0; updateKnobModes(); };
    addAndMakeVisible (tapStompBtn);

    // ---- the BYPASS stomp ----------------------------------------------------
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
    bypassLed.setColour (gw::kGreen);
    addAndMakeVisible (bypassLed);

    tapLed.setColour (gw::kYellow);
    addAndMakeVisible (tapLed);

    // ---- hints (the design ships them hidden) --------------------------------
    auto hint = [this] (juce::Label& l, const juce::String& text, float px, juce::Colour c)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (gw::mono (px, 400, 0.03f));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };
    hint (hintChips,  juce::String::fromUTF8 ("HOLD TAP \xe2\x86\x92 Y \xc2\xb7 HOLD BYPASS \xe2\x86\x92 Z \xc2\xb7 BOTH \xe2\x86\x92 A"),
          10.2f, gw::kDim2);
    hint (hintLayers, juce::String::fromUTF8 ("Hold the stomp or its key \xe2\x80\x94 or right-click a stomp to latch it."),
          8.5f, gw::kDim2);
    hint (hintLfo1,   juce::String::fromUTF8 ("Z \xc2\xb7 FREQ knob = depth \xc2\xb7 LED: wave / depth %"),
          9.0f, gw::kDim2);
    hint (hintLfo2,   juce::String::fromUTF8 ("Z \xc2\xb7 LPF knob = depth \xc2\xb7 TAP \xc3\x97""3 + BYPASS = rate"),
          9.0f, gw::kDim2);
    hint (hintStomp1, juce::String::fromUTF8 ("TAP \xc3\x97""3 avg = LFO 1 rate \xc2\xb7 hold BYPASS while tapping = LFO 2 rate \xc2\xb7 0.2\xe2\x80\x93""20 Hz"),
          9.0f, gw::kDim);
    hint (hintStomp2, juce::String::fromUTF8 ("Amber LED blinks the tempo \xc2\xb7 green LED = effect active"),
          9.0f, gw::kDim2);

    // ---- output gate + internal switches (all under the cover) ---------------
    auto gateKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setVelocityModeParameters (1.0, 1, 0.0, false);
        s.setRotaryParameters (gw::kAngle0, gw::kAngle1, true);
        s.setColour (juce::Slider::rotarySliderFillColourId, gw::kGreen);
        s.onValueChange = [this] { cover.repaint(); };
        cover.addAndMakeVisible (s);
    };
    gateKnob (threshKnob);
    gateKnob (holdKnob);
    gateKnob (fadeKnob);
    threshAtt = std::make_unique<SliderAttachment> (apvts, "gatethresh", threshKnob);
    holdAtt   = std::make_unique<SliderAttachment> (apvts, "gatehold",   holdKnob);
    fadeAtt   = std::make_unique<SliderAttachment> (apvts, "gatefade",   fadeKnob);
    cover.gateKnobs[0] = &threshKnob;
    cover.gateKnobs[1] = &holdKnob;
    cover.gateKnobs[2] = &fadeKnob;

    // the PCB switches — +6 dB kept: Jason's actual PCB carries that stage
    jfetRow.attach   (apvts.getParameter ("jfeton"),   "JFET STAGE");
    ladderRow.attach (apvts.getParameter ("ladder36"), juce::String::fromUTF8 ("\xe2\x88\x92""3/\xe2\x88\x92""6 LADDER"));
    boostRow.attach  (apvts.getParameter ("boost6"),   "+6 dB BOOST");
    supplySel.attach (choice ("supply4"));
    cover.addAndMakeVisible (jfetRow);
    cover.addAndMakeVisible (ladderRow);
    cover.addAndMakeVisible (boostRow);
    cover.addAndMakeVisible (supplySel);

    strip.onOpen = [this] { setGateOpen (true); };
    addAndMakeVisible (strip);

    coverDim.onDismiss = [this] { setGateOpen (false); };
    addChildComponent (coverDim);
    cover.onClose = [this] { setGateOpen (false); };
    cover.onHintsToggle = [this] { showHints = ! showHints; applyHints(); };
    addChildComponent (cover);

    addAndMakeVisible (fx);   // decoration on top, mouse-transparent
    setGateOpen (false);
    applyHints();

    startTimerHz (60);
    setSize (1060, 640);
    setScaleFactor (2.5f);   // v0.34: matches the design's 25 % bigger type
}

GlitchwaveAudioProcessorEditor::~GlitchwaveAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GlitchwaveAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, bool big,
                                                juce::Colour ring)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    // v0.25: stop JUCE from hijacking a modifier-held drag into its
    // "velocity" fine-adjust mode (which makes the knob appear frozen
    // while a layer key is held)
    s.setVelocityModeParameters (1.0, 1, 0.0, false);
    s.setRotaryParameters (gw::kAngle0, gw::kAngle1, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, ring);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (big ? gw::barlow (14.0f, true, 0.16f) : gw::barlow (10.0f, true, 0.14f));
    l.setColour (juce::Label::textColourId, big ? gw::kText : gw::kDim);
    addAndMakeVisible (l);
}

// ---------------------------------------------------------------------------
// v0.24 layer machinery — UNCHANGED from v0.32
// ---------------------------------------------------------------------------
bool GlitchwaveAudioProcessorEditor::tapStompDown() const
{
    // v0.30 (Jason's X/Y/Z/A spec): Y = TAP held = INS, Z = BYPASS held =
    // DEL. Polled globally; right-click LATCH remains the mouse-only hold.
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
    // every knob is released.
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
        case 0:   // X — the printed panel
            freqAtt     = std::make_unique<SliderAttachment> (ap, "freq",     freqKnob);
            lpfAtt      = std::make_unique<SliderAttachment> (ap, "fizz",     lpfKnob);
            mixAtt      = std::make_unique<SliderAttachment> (ap, "dry",      mixKnob);
            lfo1RateAtt = std::make_unique<SliderAttachment> (ap, "lfo1rate", lfo1RateKnob);
            lfo2RateAtt = std::make_unique<SliderAttachment> (ap, "lfo2rate", lfo2RateKnob);
            envGainAtt  = std::make_unique<SliderAttachment> (ap, "envgain",  envGainKnob);
            break;
        case 1:   // Y (TAP held) — Gain / Res / Vol + zone-select Shape/Shape/Mode
            freqAtt = std::make_unique<SliderAttachment> (ap, "dirtgain", freqKnob);
            lpfAtt  = std::make_unique<SliderAttachment> (ap, "lpfq",     lpfKnob);
            mixAtt  = std::make_unique<SliderAttachment> (ap, "vol",      mixKnob);
            break;
        case 2:   // Z (BYPASS held) — L1/L2 Depth, DrvRng, Target x3
            freqAtt = std::make_unique<SliderAttachment> (ap, "lfo1depth", freqKnob);
            lpfAtt  = std::make_unique<SliderAttachment> (ap, "lfo2depth", lpfKnob);
            break;
        case 3:   // secret starve: MIX only; every other knob is dead
            mixAtt  = std::make_unique<SliderAttachment> (ap, "starve", mixKnob);
            break;
    }

    // v0.31 (Jason's find): every layer entry parks each selector knob at the
    // CENTRE of its current selection's zone, so the position always tells
    // the truth and a small turn steps to the neighbouring choice.
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
// v0.32 tap tempo: rolling average of the last THREE presses (2 intervals);
// 1-2 presses arm only, the 3rd (and every press after) commits.
// ---------------------------------------------------------------------------
void GlitchwaveAudioProcessorEditor::recordTap (bool lfo2, double pressMs)
{
    if (pressMs <= 0.0)
        return;
    double* h = lfo2 ? tapHist2 : tapHist1;
    int&    n = lfo2 ? tapN2    : tapN1;

    if (n > 0 && pressMs - h[n - 1] > 5000.0)
        n = 0;                                   // stale chain: start over
    if (n == 3)
    { h[0] = h[1]; h[1] = h[2]; n = 2; }
    h[n++] = pressMs;

    if (n == 3)
    {
        const double avgMs = (h[2] - h[0]) / 2.0;
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
    coverDim.setVisible (gateOpen);
    cover.setVisible (gateOpen);
}

void GlitchwaveAudioProcessorEditor::applyHints()
{
    for (auto* l : { &hintChips, &hintLayers, &hintLfo1, &hintLfo2,
                     &hintStomp1, &hintStomp2 })
        l->setVisible (showHints);
    cover.hintsOn = showHints;
    cover.repaint();
}

// ---------------------------------------------------------------------------
// the live readouts: captions, value lines, rulers + names, per active layer
// ---------------------------------------------------------------------------
void GlitchwaveAudioProcessorEditor::refreshReadouts (int layer)
{
    auto& apvts = processor.apvts;
    auto pTxt = [&apvts] (const char* id) -> juce::String
    {
        if (auto* p = apvts.getParameter (id))
            return p->getCurrentValueAsText();
        return {};
    };
    auto idxOf = [] (juce::AudioParameterChoice* pc) { return pc != nullptr ? pc->getIndex() : 0; };

    const int shape1 = idxOf (lfo1ShapeParam), shape2 = idxOf (lfo2ShapeParam);
    const int targ1  = idxOf (lfo1TargetParam), targ2 = idxOf (lfo2TargetParam);
    const int mode   = idxOf (lpfModeParam),   targE  = idxOf (envTargetParam);
    const int combo  = idxOf (envDriveParam) * 2 + (idxOf (lpfRangeParam) == 1 ? 0 : 1);

    // ---- captions ----------------------------------------------------------
    struct Cap { const char* t[6]; };
    static const Cap caps[4] = {
        {{ "FREQ",     "LPF",      "MIX",     "RATE",  "RATE",  "GAIN" }},    // X
        {{ "GAIN",     "RES",      "VOL",     "SHAPE", "SHAPE", "MODE" }},    // Y
        {{ "L1 DEPTH", "L2 DEPTH", "DRV/RNG", "TARGET","TARGET","TARGET" }},  // Z
        {{ kDash,      kDash,      "?",       kDash,   kDash,   kDash }},     // A
    };
    static const juce::Colour bigCols[4]   = { gw::kText, gw::kCyan, gw::kYellow, gw::kDead };
    static const juce::Colour smallCols[4] = { gw::kDim,  gw::kCyan, gw::kYellow, gw::kDead };
    juce::Label* ls[6] = { &freqLabel, &lpfLabel, &mixLabel,
                           &lfo1RateLabel, &lfo2RateLabel, &envGainLabel };
    for (int i = 0; i < 6; ++i)
    {
        ls[i]->setText (juce::String::fromUTF8 (caps[layer].t[i]), juce::dontSendNotification);
        auto c = i < 3 ? bigCols[layer] : smallCols[layer];
        if (layer == 3 && i == 2)
            c = gw::kRed;                       // the secret "?" burns red
        ls[i]->setColour (juce::Label::textColourId, c);
    }

    // ---- value lines -------------------------------------------------------
    juce::String text[6];
    juce::Colour col[6] { gw::kText, gw::kText, gw::kText, gw::kText, gw::kText, gw::kText };
    switch (layer)
    {
        case 0:
            text[0] = pTxt ("freq");     text[1] = pTxt ("fizz");     text[2] = pTxt ("dry");
            text[3] = pTxt ("lfo1rate"); text[4] = pTxt ("lfo2rate"); text[5] = pTxt ("envgain");
            break;
        case 1:
            text[0] = pTxt ("dirtgain"); text[1] = pTxt ("lpfq");     text[2] = pTxt ("vol");
            text[3] = lfo1ShapeParam != nullptr ? lfo1ShapeParam->getCurrentChoiceName() : juce::String();
            col[3]  = gw::kHues[shape1 % 8];
            text[4] = lfo2ShapeParam != nullptr ? lfo2ShapeParam->getCurrentChoiceName() : juce::String();
            col[4]  = gw::kHues[shape2 % 8];
            text[5] = shortMode (lpfModeParam != nullptr ? lpfModeParam->getCurrentChoiceName() : juce::String());
            col[5]  = mode == 0 ? gw::kGrey : gw::kHues[juce::jlimit (0, 7, mode)];
            break;
        case 2:
        {
            static const char* comboNames[4] = { "Up-Hi", "Up-Lo", "Dn-Hi", "Dn-Lo" };
            text[0] = pTxt ("lfo1depth"); text[1] = pTxt ("lfo2depth");
            text[2] = comboNames[juce::jlimit (0, 3, combo)];
            col[2]  = gw::kHues[juce::jlimit (0, 3, combo)];
            auto tName = [] (juce::AudioParameterChoice* pc, int idx) -> juce::String
            { return pc != nullptr ? pc->getCurrentChoiceName() : juce::String (idx); };
            text[3] = tName (lfo1TargetParam, targ1);
            col[3]  = targ1 == 0 ? gw::kGrey : gw::kHues[juce::jlimit (0, 7, targ1)];
            text[4] = tName (lfo2TargetParam, targ2);
            col[4]  = targ2 == 0 ? gw::kGrey : gw::kHues[juce::jlimit (0, 7, targ2)];
            text[5] = tName (envTargetParam, targE);
            col[5]  = targE == 0 ? gw::kGrey : gw::kHues[juce::jlimit (0, 7, targE)];
            break;
        }
        case 3:
        {
            for (int i = 0; i < 6; ++i) { text[i] = juce::String::fromUTF8 (kDash); col[i] = gw::kGrey; }
            // the "?" reads out as the sagging rail: supply .. 5 V floor
            static constexpr float kVolts[4] = { 9.0f, 12.0f, 15.0f, 18.0f };
            float volts = kVolts[0];
            if (auto* ps = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("supply4")))
                volts = kVolts[juce::jlimit (0, 3, ps->getIndex())];
            float sv = 0.0f;
            if (auto* p = apvts.getParameter ("starve"))
                sv = p->getValue();
            text[2] = juce::String (volts - sv * (volts - 5.0f), 1) + " V";
            col[2]  = gw::kText;
            break;
        }
    }
    for (int i = 0; i < 6; ++i)
    {
        vals[i].setText (text[i], juce::dontSendNotification);
        vals[i].setColour (juce::Label::textColourId, col[i]);
    }

    // ---- rulers + names ----------------------------------------------------
    l1ShapeRuler.setSelected (shape1 % 8);
    l2ShapeRuler.setSelected (shape2 % 8);
    l1TargetRuler.setSelected (targ1);
    l2TargetRuler.setSelected (targ2);
    envModeRuler.setSelected (mode);
    envTargetRuler.setSelected (targE);
    envComboRuler.setSelected (combo);

    auto shapePair = [] (juce::AudioParameterChoice* pc, int idx) -> juce::String
    {
        if (pc == nullptr) return {};
        return "/ " + pc->choices[(idx + 8) % 16];
    };
    l1ShapeName.set (lfo1ShapeParam != nullptr ? lfo1ShapeParam->getCurrentChoiceName() : juce::String(),
                     gw::kHues[shape1 % 8], shapePair (lfo1ShapeParam, shape1));
    l2ShapeName.set (lfo2ShapeParam != nullptr ? lfo2ShapeParam->getCurrentChoiceName() : juce::String(),
                     gw::kHues[shape2 % 8], shapePair (lfo2ShapeParam, shape2));
    l1TargetName.set (lfo1TargetParam != nullptr ? lfo1TargetParam->getCurrentChoiceName() : juce::String(),
                      targ1 == 0 ? gw::kDim : gw::kHues[juce::jlimit (0, 7, targ1)]);
    l2TargetName.set (lfo2TargetParam != nullptr ? lfo2TargetParam->getCurrentChoiceName() : juce::String(),
                      targ2 == 0 ? gw::kDim : gw::kHues[juce::jlimit (0, 7, targ2)]);
    envModeName.set (shortMode (lpfModeParam != nullptr ? lpfModeParam->getCurrentChoiceName() : juce::String()),
                     mode == 0 ? gw::kDim : gw::kHues[juce::jlimit (0, 7, mode)],
                     juce::String::fromUTF8 ("\xc2\xb7 ") + pTxt ("lpfq")
                         + juce::String::fromUTF8 (" \xc2\xb7 ")
                         + (lpfRangeParam != nullptr ? lpfRangeParam->getCurrentChoiceName() : juce::String()));
    envTargetName.set (envTargetParam != nullptr ? envTargetParam->getCurrentChoiceName() : juce::String(),
                       targE == 0 ? gw::kDim : gw::kHues[juce::jlimit (0, 7, targE)]);
    {
        static const char* comboNames[4] = { "Up-Hi", "Up-Lo", "Dn-Hi", "Dn-Lo" };
        const int cb = juce::jlimit (0, 3, combo);
        envComboName.set (comboNames[cb], gw::kHues[cb],
                          juce::String::fromUTF8 ("\xc2\xb7 drive ") + (cb < 2 ? "up" : "down")
                              + ", range " + (cb % 2 == 0 ? "hi" : "lo"));
    }

    // depth captions + env routing caption
    auto pctOf = [&apvts] (const char* id)
    {
        if (auto* p = apvts.getParameter (id))
            return juce::String (juce::roundToInt (p->getValue() * 100.0f));
        return juce::String();
    };
    depth1Cap.setText ("DEPTH " + pctOf ("lfo1depth") + " %", juce::dontSendNotification);
    depth2Cap.setText ("DEPTH " + pctOf ("lfo2depth") + " %", juce::dontSendNotification);
    envSubCap.setText (juce::String::fromUTF8 ("ENV \xe2\x86\x92 ")
                           + (envTargetParam != nullptr ? envTargetParam->getCurrentChoiceName()
                                                        : juce::String()),
                       juce::dontSendNotification);
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

    // stomp held-indicators
    tapStompBtn.setIndicated (tapStompDown());
    bypassBtn.setIndicated (bypassStompDown());
    chips.setLayer (layer);

    // v0.32 tempo LED: blinks at the tapped LFO1 rate, full flash on a press
    const double t = nowMs();
    {
        float rate = 0.5f;
        if (auto* pr = processor.apvts.getParameter ("lfo1rate"))
            rate = pr->convertFrom0to1 (pr->getValue());
        const double phase = std::fmod (t * rate / 1000.0, 1.0);
        const bool flash = (t - lastTapFlashMs) < 120.0;
        tapLed.setLevel (flash ? 1.0f : (phase < 0.5 ? 0.7f : 0.05f));
    }

    const bool filterOn = lpfModeParam != nullptr && lpfModeParam->getIndex() > 0;

    // knob enables per layer. The env-gain knob must stay alive in Y even
    // with the filter Off — it's how the Mode gets turned back on.
    freqKnob.setEnabled     (layer != 3);
    lpfKnob.setEnabled      (layer == 2 ? true : (layer == 3 ? false : filterOn));
    mixKnob.setEnabled      (true);
    lfo1RateKnob.setEnabled (layer != 3);
    lfo2RateKnob.setEnabled (layer != 3);
    envGainKnob.setEnabled  (layer == 0 ? filterOn : layer != 3);

    refreshReadouts (layer);

    // ---- the three section LEDs: live value colour of the active layer -------
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
        led.setColour (gw::kHues[idx % 8]);
        led.setLevel ((idx >= 8 ? f5 : f2) ? 1.0f : 0.0f);   // A = 2 Hz, B = 5 Hz
    };
    auto targetShow = [&] (LedIndicator& led, juce::AudioParameterChoice* pc)
    {
        const int idx = pc != nullptr ? pc->getIndex() : 0;
        if (idx == 0) { led.setColour (kWhite); led.setLevel (0.12f); }   // Off = dim
        else          { led.setColour (gw::kHues[idx]); led.setLevel (1.0f); }
    };
    auto depthShow = [&] (LedIndicator& led, const char* id)
    {
        led.setColour (juce::Colour (0xff4488ff));
        if (auto* pd = processor.apvts.getParameter (id))
            led.setLevel (pd->getValue());
    };
    auto modeShow = [&] (LedIndicator& led)
    {
        const int m = lpfModeParam != nullptr ? lpfModeParam->getIndex() : 0;
        led.setColour (gw::kHues[juce::jlimit (0, 4, m)]);
        led.setLevel (f3 ? 1.0f : 0.0f);
    };
    auto comboShow = [&] (LedIndicator& led)   // v0.24: SOLID, no blink
    {
        const int d = envDriveParam != nullptr ? envDriveParam->getIndex() : 0;
        const int r = lpfRangeParam != nullptr ? lpfRangeParam->getIndex() : 0;
        const int combo = d * 2 + (r == 1 ? 0 : 1);   // ring order up&hi..dn&lo
        led.setColour (gw::kHues[juce::jlimit (0, 3, combo)]);
        led.setLevel (1.0f);
    };

    // LFO 1 — Y shows SHAPE, Z shows TARGET (or depth while the Z depth
    // knob is being turned)
    if (layer == 1)      shapeShow (lfo1Led, lfo1ShapeParam);
    else if (layer == 2) (lfo1Ctx == kCtxDepth ? depthShow (lfo1Led, "lfo1depth")
                                               : targetShow (lfo1Led, lfo1TargetParam));
    else if (layer == 0 && lfo1Ctx == kCtxShape)  shapeShow  (lfo1Led, lfo1ShapeParam);
    else if (layer == 0 && lfo1Ctx == kCtxTarget) targetShow (lfo1Led, lfo1TargetParam);
    else if (layer == 0 && lfo1Ctx == kCtxDepth)  depthShow  (lfo1Led, "lfo1depth");
    else   // idle (and the dead starve layer): breathing at the LFO wave
    {
        lfo1Led.setColour (gw::kCyan);
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
        lfo2Led.setColour (gw::kMagenta);
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
        envLed.setColour (gw::kGreen);
        envLed.setLevel (juce::jlimit (0.0f, 1.0f, processor.readVis (2)));
    }

    // bypass status LED (green = effect active, like every pedal)
    if (auto* pb = processor.apvts.getParameter ("bypass"))
        bypassLed.setLevel (pb->getValue() >= 0.5f ? 0.0f : 1.0f);

    // gate LED: green = open, amber blinking = fading, red = fully closed
    const float atten = processor.readVis (5);
    const bool  blink = ((frame / 14) % 2) == 0;
    juce::Colour gc; float gl;
    if (atten > -0.5f)       { gc = gw::kGreen;  gl = 1.0f; }
    else if (atten > -95.0f) { gc = gw::kYellow; gl = blink ? 1.0f : 0.15f; }
    else                     { gc = gw::kRed;    gl = 1.0f; }
    strip.setLedState (gc, gl);
    cover.setLedState (gc, gl);

    // the strip's live summary line
    {
        InternalStrip::Summary s;
        s.gate = (juce::String (threshKnob.getValue(), 0) + " dB"
                  + juce::String::fromUTF8 (" \xc2\xb7 ")
                  + juce::String (holdKnob.getValue(), 1) + " s"
                  + juce::String::fromUTF8 (" \xc2\xb7 ")
                  + juce::String (fadeKnob.getValue(), 0) + " s")
                     .replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92"));
        auto onOf = [this] (const char* id)
        {
            auto* pp = processor.apvts.getParameter (id);
            return pp != nullptr && pp->getValue() >= 0.5f;
        };
        s.jfet   = onOf ("jfeton");
        s.ladder = onOf ("ladder36");
        s.boost  = onOf ("boost6");
        s.hints  = showHints;
        if (auto* ps = dynamic_cast<juce::AudioParameterChoice*> (
                           processor.apvts.getParameter ("supply4")))
            s.supply = ps->getCurrentChoiceName().replace ("V", " V");
        strip.setSummary (s);
    }

    // switches can also move under host automation — keep the rows honest
    jfetRow.refresh();
    ladderRow.refresh();
    boostRow.refresh();
    supplySel.refresh();

    // ---- decoration ----------------------------------------------------------
    fx.tick (t);
    {
        const double ph = std::fmod (t, 5500.0);
        float tear = 0.0f;
        if (ph < 240.0)
        {
            static const float offs[4] = { -7.0f, 5.0f, -2.0f, 0.0f };
            tear = offs[juce::jlimit (0, 3, (int) (ph / 60.0))];
        }
        tagline.setTear (tear);
    }
}

void GlitchwaveAudioProcessorEditor::paint (juce::Graphics& g)
{
    // the baked mosaic face (dark veil + scanlines already in the art)
    g.drawImage (bgImage, { 0.0f, 0.0f, 1060.0f, 640.0f });

    {   // rainbow hairline under the header
        juce::ColourGradient grad (gw::kMagenta, 0.0f, 70.0f, gw::kYellow, 1060.0f, 70.0f, false);
        grad.addColour (0.35, gw::kCyan);
        grad.addColour (0.62, gw::kGreen);
        g.setGradientFill (grad);
        g.setOpacity (0.6f);
        g.fillRect (0.0f, 70.0f, 1060.0f, 1.0f);
        g.setOpacity (1.0f);
    }

    // logo (pre-baked invert + chromatic aberration + glow; 16 px pad in the art)
    g.drawImage (logoFx, { 4.66f - 16.0f, -4.93f - 16.0f, 173.0f + 32.0f, 86.5f + 32.0f });

    // ---- PEDAL panel ---------------------------------------------------------
    panel (g, { 12, 74, 1036, 200 }, gw::kYellow);
    g.setColour (gw::kDim);
    g.setFont (gw::barlow (11.0f, true, 0.18f));
    g.drawText ("PEDAL", 28, 84, 200, 13, juce::Justification::centredLeft);

    {   // vertical divider
        juce::ColourGradient grad (juce::Colours::transparentBlack, 480.0f, 106.0f,
                                   juce::Colours::transparentBlack, 480.0f, 256.0f, false);
        grad.addColour (0.2, juce::Colour (0xff33373f));
        grad.addColour (0.8, juce::Colour (0xff33373f));
        g.setGradientFill (grad);
        g.fillRect (480.0f, 106.0f, 1.0f, 150.0f);
    }

    // ---- KNOB LAYERS chart ---------------------------------------------------
    g.setColour (gw::kDim);
    g.setFont (gw::barlow (10.0f, true, 0.18f));
    g.drawText ("KNOB LAYERS", 510, 106, 200, 12, juce::Justification::centredLeft);
    {
        const char* head[6] = { "FREQ", "LPF", "MIX", "RATE 1", "RATE 2", "GAIN" };
        struct Row { const char* label; const char* c[6]; };
        static const Row rows[4] = {
            { "X \xc2\xb7 NOTHING",   { "Freq", "LPF", "Mix", "Rate", "Rate", "Gain" } },
            { "Y \xc2\xb7 TAP / INS", { "Gain", "Res", "Vol", "Shape", "Shape", "Mode" } },
            { "Z \xc2\xb7 BYP / DEL", { "L1 Depth", "L2 Depth", "Drv/Rng", "Target", "Target", "Target" } },
            { "A \xc2\xb7 BOTH",      { "\xe2\x80\x94", "\xe2\x80\x94", "?", "\xe2\x80\x94", "\xe2\x80\x94", "\xe2\x80\x94" } },
        };
        const juce::Colour rowCols[4] = { gw::kText, gw::kCyan, gw::kYellow, gw::kRed };
        const juce::Colour labCols[4] = { gw::kDim,
                                          gw::kCyan.withAlpha (0.72f),
                                          gw::kYellow.withAlpha (0.72f),
                                          gw::kRed.withAlpha (0.72f) };
        auto cellX = [] (int i) { return 510 + 104 + i * 58; };
        g.setFont (gw::mono (9.5f, 400, 0.04f));
        g.setColour (gw::kDim2);
        for (int i = 0; i < 6; ++i)
            g.drawText (head[i], cellX (i), 126, 56, 16, juce::Justification::centredLeft);
        for (int rI = 0; rI < 4; ++rI)
        {
            const int y = 142 + rI * 19;
            g.setColour (gw::kRowLine);
            g.fillRect (510, y, 452, 1);
            g.setFont (gw::mono (9.5f, 400, 0.02f));
            g.setColour (labCols[rI]);
            g.drawText (juce::String::fromUTF8 (rows[rI].label), 510, y, 104, 19,
                        juce::Justification::centredLeft);
            for (int i = 0; i < 6; ++i)
            {
                auto c = rowCols[rI];
                if (rI == 3)
                    c = i == 2 ? gw::kRed : gw::kGrey;
                g.setColour (c);
                g.drawText (juce::String::fromUTF8 (rows[rI].c[i]), cellX (i), y, 56, 19,
                            juce::Justification::centredLeft);
            }
        }
    }

    // IN / OUT meter labels
    g.setColour (gw::kDim2);
    g.setFont (gw::mono (9.0f, 600, 0.10f));
    g.drawText ("IN",  984, 106, 30, 11, juce::Justification::centred);
    g.drawText ("OUT", 1012, 106, 30, 11, juce::Justification::centred);

    // ---- LFO 1 / LFO 2 / ENVELOPE panels ------------------------------------
    auto section = [&g] (juce::Rectangle<int> r, juce::Colour accent,
                         const juce::String& title, const juce::String& sub)
    {
        panel (g, r, accent);
        g.setColour (gw::kText);
        auto f = gw::barlow (12.0f, true, 0.18f);
        g.setFont (f);
        g.drawText (title, r.getX() + 16, r.getY() + 13, 200, 14, juce::Justification::centredLeft);
        g.setColour (gw::kDim2);
        g.setFont (gw::mono (8.5f, 400, 0.09f));
        g.drawText (sub, r.getX() + 16 + (int) gw::textW (f, title) + 9,
                    r.getY() + 15, 260, 12, juce::Justification::centredLeft);
    };
    section ({ 12, 279, 330, 210 }, gw::kCyan, "LFO 1",
             juce::String::fromUTF8 ("UNIPOLAR \xe2\x86\x91 \xc2\xb7 SC\xe2\x86\x92""DEPTH"));
    section ({ 352, 279, 330, 210 }, gw::kMagenta, "LFO 2",
             juce::String::fromUTF8 ("BIPOLAR \xc2\xb7 SC\xe2\x86\x92""DEPTH"));
    section ({ 692, 279, 356, 210 }, gw::kGreen, "ENVELOPE", "FOLLOWER + FILTER");

    // ruler headers
    auto rulerHead = [&g] (int x, int y, int w, const juce::String& a, const juce::String& b)
    {
        g.setColour (gw::kDim);
        g.setFont (gw::barlow (9.5f, true, 0.16f));
        g.drawText (a, x, y, w, 11, juce::Justification::centredLeft);
        g.setColour (gw::kGrey);
        g.setFont (gw::mono (9.0f, 400));
        g.drawText (b, x, y, w, 11, juce::Justification::centredRight);
    };
    rulerHead (142, 333, 184, "SHAPE",  juce::String::fromUTF8 ("Y \xc2\xb7 RATE KNOB"));
    rulerHead (142, 393, 184, "TARGET", juce::String::fromUTF8 ("Z \xc2\xb7 RATE KNOB"));
    rulerHead (482, 333, 184, "SHAPE",  juce::String::fromUTF8 ("Y \xc2\xb7 RATE KNOB"));
    rulerHead (482, 393, 184, "TARGET", juce::String::fromUTF8 ("Z \xc2\xb7 RATE KNOB"));
    rulerHead (822, 321, 210, "FILTER MODE",   juce::String::fromUTF8 ("Y \xc2\xb7 GAIN KNOB"));
    rulerHead (822, 372, 210, "TARGET",        juce::String::fromUTF8 ("Z \xc2\xb7 GAIN KNOB"));
    rulerHead (822, 424, 210, "DRIVE / RANGE", juce::String::fromUTF8 ("Z \xc2\xb7 MIX KNOB"));

    // ---- stomp strip ---------------------------------------------------------
    panel (g, { 12, 573, 1036, 56 }, juce::Colours::transparentBlack);
    g.setColour (gw::kText);
    g.setFont (gw::barlow (12.0f, true, 0.16f));
    g.drawText ("TAP",    110, 594, 60, 14, juce::Justification::centredLeft);
    g.drawText ("BYPASS", 256, 594, 80, 14, juce::Justification::centredLeft);

    g.setColour (gw::kGrey);
    g.setFont (gw::mono (6.4f, 400, 0.08f));
    g.drawText (juce::String::fromUTF8 ("MADE ON\xe2\x80\xa6 EARTH?"), 848, 598, 100, 10,
                juce::Justification::centredRight);
    g.setOpacity (0.55f);
    g.drawImage (logoPlain, { 958.0f, 587.0f, 66.0f, 33.0f });
    g.setOpacity (1.0f);
}

void GlitchwaveAudioProcessorEditor::resized()
{
    // header
    chips.setBounds (184, 22, 162, 26);
    hintChips.setBounds (184, 50, 430, 12);
    tagline.setBounds (480, 8, 566, 56);

    // ---- pedal row (3 big knobs + the 3 small section knobs) -----------------
    {
        const int xs[3] = { 50, 190, 330 };
        juce::Slider* ks[] = { &freqKnob, &lpfKnob, &mixKnob };
        juce::Label*  ls[] = { &freqLabel, &lpfLabel, &mixLabel };
        for (int i = 0; i < 3; ++i)
        {
            ls[i]->setBounds  (xs[i], 104, 120, 18);
            ks[i]->setBounds  (xs[i], 128, 120, 120);
            vals[i].setBounds (xs[i], 252, 120, 16);
        }
        meterIn.setBounds  (990, 122, 18, 126);
        meterOut.setBounds (1018, 122, 18, 126);
        hintLayers.setBounds (510, 232, 452, 12);
    }

    // ---- LFO panels ----------------------------------------------------------
    {
        lfo1RateLabel.setBounds (30, 331, 76, 14);
        lfo1RateKnob.setBounds  (30, 351, 76, 76);
        vals[3].setBounds       (30, 433, 76, 15);
        depth1Cap.setBounds     (30, 450, 76, 12);
        lfo1Led.setBounds       (298, 291, 28, 28);
        l1ShapeRuler.setBounds  (142, 348, 184, 9);
        l1ShapeName.setBounds   (142, 361, 184, 14);
        l1TargetRuler.setBounds (142, 408, 184, 9);
        l1TargetName.setBounds  (142, 421, 184, 14);
        hintLfo1.setBounds      (28, 465, 300, 12);

        lfo2RateLabel.setBounds (370, 331, 76, 14);
        lfo2RateKnob.setBounds  (370, 351, 76, 76);
        vals[4].setBounds       (370, 433, 76, 15);
        depth2Cap.setBounds     (370, 450, 76, 12);
        lfo2Led.setBounds       (638, 291, 28, 28);
        l2ShapeRuler.setBounds  (482, 348, 184, 9);
        l2ShapeName.setBounds   (482, 361, 184, 14);
        l2TargetRuler.setBounds (482, 408, 184, 9);
        l2TargetName.setBounds  (482, 421, 184, 14);
        hintLfo2.setBounds      (368, 465, 300, 12);
    }

    // ---- envelope panel ------------------------------------------------------
    {
        envGainLabel.setBounds  (710, 331, 76, 14);
        envGainKnob.setBounds   (710, 351, 76, 76);
        vals[5].setBounds       (710, 433, 76, 15);
        envSubCap.setBounds     (704, 450, 88, 12);
        envLed.setBounds        (1004, 291, 28, 28);
        envModeRuler.setBounds  (822, 336, 210, 9);
        envModeName.setBounds   (822, 349, 210, 14);
        envTargetRuler.setBounds(822, 387, 210, 9);
        envTargetName.setBounds (822, 400, 210, 14);
        envComboRuler.setBounds (822, 439, 210, 9);
        envComboName.setBounds  (822, 451, 210, 14);
    }

    // ---- internal strip + cover ----------------------------------------------
    strip.setBounds (12, 494, 1036, 74);
    coverDim.setBounds (0, 0, 1060, 640);
    cover.setBounds (12, 279, 1036, 292);
    {
        // cover-local children
        threshKnob.setBounds (28, 124, 76, 76);
        holdKnob.setBounds   (160, 124, 76, 76);
        fadeKnob.setBounds   (292, 124, 76, 76);
        jfetRow.setBounds    (500, 104, 224, 36);
        ladderRow.setBounds  (500, 146, 224, 36);
        boostRow.setBounds   (500, 188, 224, 36);
        supplySel.setBounds  (758, 134, 229, 36);
    }

    // ---- footswitch strip -----------------------------------------------------
    tapLed.setBounds      (34, 593, 16, 16);
    tapStompBtn.setBounds (62, 582, 38, 38);
    bypassBtn.setBounds   (182, 582, 38, 38);
    bypassLed.setBounds   (230, 593, 16, 16);
    hintStomp1.setBounds  (360, 586, 620, 11);
    hintStomp2.setBounds  (360, 601, 620, 11);

    fx.setBounds (0, 0, 1060, 640);
}
