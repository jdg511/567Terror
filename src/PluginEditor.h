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
// A single LED with variable brightness (0..1) and colour — like the real thing.
// ---------------------------------------------------------------------------
class LedIndicator : public juce::Component
{
public:
    void setColour (juce::Colour c) noexcept
    {
        if (c != colour) { colour = c; repaint(); }
    }

    void setLevel (float newLevel) noexcept
    {
        newLevel = juce::jlimit (0.0f, 1.0f, newLevel);
        if (std::fabs (newLevel - level) > 0.02f)
        {
            level = newLevel;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto led = juce::Rectangle<float> (d, d).withCentre (r.getCentre());

        if (level > 0.05f)   // glow halo
        {
            g.setColour (colour.withAlpha (0.25f * level));
            g.fillEllipse (led.expanded (d * 0.35f * level));
        }
        g.setColour (juce::Colour (0xff15171b).interpolatedWith (colour, 0.15f + 0.85f * level));
        g.fillEllipse (led);
        g.setColour (juce::Colour (0xff000000).withAlpha (0.5f));
        g.drawEllipse (led, 1.0f);
    }

private:
    juce::Colour colour { 0xffffd166 };
    float level = 0.0f;
};

// ---------------------------------------------------------------------------
// Hardware-style momentary stomp (v0.19 timing): TAP fires onTap on release
// (< 750 ms, not consumed). cancelPressActions() consumes the current press
// (no tap) — used when a knob turns the hold into a layer-shift gesture.
// ---------------------------------------------------------------------------
class TapHoldButton : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onTap, onHoldTick;
    std::function<void()> onPress;     // fires on the press itself (mouse down)
    std::function<void()> onRelease;   // always fires on release (after onTap)

    bool isDown() const noexcept { return pressed; }

    void cancelPressActions() noexcept
    {
        consumed = true;      // suppress the tap and any further hold ticks
        stopTimer();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (! isEnabled()) return;
        pressed = true;
        holdStarted = false;
        consumed = false;
        if (onPress)
            onPress();
        if (! consumed)       // onPress may have consumed the press
            startTimer (750);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        stopTimer();
        if (pressed && ! holdStarted && ! consumed && onTap)
            onTap();
        pressed = false;
        holdStarted = false;
        consumed = false;
        if (onRelease)
            onRelease();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
        auto btn = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        const float a = isEnabled() ? 1.0f : 0.35f;
        g.setColour (juce::Colour (0xff15171b).withMultipliedAlpha (a));
        g.fillEllipse (btn.expanded (3.0f));
        g.setColour ((pressed ? juce::Colour (0xff53565e) : juce::Colour (0xff2e3340))
                         .withMultipliedAlpha (a));
        g.fillEllipse (btn);
        g.setColour (juce::Colour (0xff9aa0a6).withMultipliedAlpha (a));
        g.drawEllipse (btn, 1.5f);
    }

private:
    void timerCallback() override
    {
        holdStarted = true;
        if (! consumed && onHoldTick)
            onHoldTick();
        startTimer (750);
    }

    bool pressed = false, holdStarted = false, consumed = false;
};

// ---------------------------------------------------------------------------
// The gate's hinged cover: closed, it shows the settings summary + status LED;
// click to open and reveal the knobs.
// ---------------------------------------------------------------------------
class GateCover : public juce::Component
{
public:
    std::function<void()> onToggle;

    void setSummary (const juce::String& s)
    {
        if (s != summary) { summary = s; repaint(); }
    }

    void setLedState (juce::Colour c, float level)
    {
        ledColour = c; ledLevel = level; repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff2a2d34));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        // fake screws
        g.setColour (juce::Colour (0xff53565e));
        for (auto p : { juce::Point<float> (14, 14), { r.getWidth() - 14, 14 },
                        { 14, r.getHeight() - 14 }, { r.getWidth() - 14, r.getHeight() - 14 } })
            g.fillEllipse (p.x - 3, p.y - 3, 6, 6);

        // status LED
        auto led = juce::Rectangle<float> (r.getWidth() * 0.5f - 6, 26, 12, 12);
        if (ledLevel > 0.05f)
        {
            g.setColour (ledColour.withAlpha (0.3f * ledLevel));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (juce::Colour (0xff15171b).interpolatedWith (ledColour, 0.15f + 0.85f * ledLevel));
        g.fillEllipse (led);

        g.setColour (juce::Colour (0xffe8e8e8));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText ("OUTPUT GATE", 0, 44, getWidth(), 18, juce::Justification::centred);
        g.setColour (juce::Colour (0xff9aa0a6));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (summary, 0, 66, getWidth(), 16, juce::Justification::centred);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("click to open cover", 0, getHeight() - 22, getWidth(), 12,
                    juce::Justification::centred);
    }

private:
    juce::String summary;
    juce::Colour ledColour { 0xff4caf6d };
    float ledLevel = 1.0f;
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
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name, bool big);
    void setGateOpen (bool shouldBeOpen);

    // ---- v0.24 control scheme: three knob layers, ZERO section buttons -----
    // Six knobs (Freq, LPF, Mix | LFO1 Rate, LFO2 Rate, Env Gain):
    //   C1 (nothing held):        Freq   LPF     Mix     Rate   Rate   Gain
    //   C2 (TAP held / CTRL):     Gain   Res     Vol     Target Target Target
    //   C3 (BYPASS held / SHIFT): L1 Dep L2 Dep  DrvRng  Shape  Shape  Mode
    //   BOTH held (secret):       Mix knob -> STARVE; every other knob dead.
    // TAP stomp: 4-tap average sets LFO1 rate (with BYPASS held: LFO2 rate),
    // rolling window of the last 4 taps, clamped 0.2..20 Hz. 1-3 taps arm
    // only. A committed tap re-seeds that LFO's chaos generators.
    // The section LEDs show the live value colour of whatever the active
    // layer edits (Jason: "I couldn't see it as it was changing" — fixed).
    bool tapStompDown() const;      // TAP stomp or CTRL (sim stand-in)
    bool bypassStompDown() const;   // BYPASS stomp or SHIFT (sim stand-in)
    int  computeLayer() const;      // 0 = C1, 1 = C2, 2 = C3, 3 = secret starve
    void updateKnobModes();         // swap slider attachments per layer
    void knobTouched();             // any knob move consumes the held stomps
    void applyZone (juce::Slider& s, const char* paramID, int zones,
                    int& ctx, double& ctxUntil, int ctxKind);
    void applyComboFromMixKnob();   // C3 Mix knob -> env drive x range
    void recordTap (bool lfo2, double pressMs);

    GlitchwaveAudioProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    // the six knobs
    juce::Slider freqKnob, lpfKnob, mixKnob;
    juce::Label  freqLabel, lpfLabel, mixLabel;
    std::unique_ptr<SliderAttachment> freqAtt, lpfAtt, mixAtt;
    juce::Slider lfo1RateKnob, lfo2RateKnob, envGainKnob;
    juce::Label  lfo1RateLabel, lfo2RateLabel, envGainLabel;
    std::unique_ptr<SliderAttachment> lfo1RateAtt, lfo2RateAtt, envGainAtt;
    juce::Label  lfo1ValueLabel, lfo2ValueLabel;   // live rate · depth readouts
    PPMMeter meterIn, meterOut;

    // cached choice params the LEDs display
    juce::AudioParameterChoice* lfo1ShapeParam  = nullptr;
    juce::AudioParameterChoice* lfo2ShapeParam  = nullptr;
    juce::AudioParameterChoice* lfo1TargetParam = nullptr;
    juce::AudioParameterChoice* lfo2TargetParam = nullptr;
    juce::AudioParameterChoice* envTargetParam  = nullptr;
    juce::AudioParameterChoice* lpfModeParam    = nullptr;
    juce::AudioParameterChoice* envDriveParam   = nullptr;
    juce::AudioParameterChoice* lpfRangeParam   = nullptr;
    LedIndicator lfo1Led, lfo2Led, envLed;

    // transient LED display contexts (linger 1.5 s after a change)
    enum { kCtxIdle = 0, kCtxShape, kCtxTarget, kCtxMode, kCtxCombo, kCtxDepth };
    int    lfo1Ctx = 0, lfo2Ctx = 0, envCtx = 0;
    double lfo1CtxUntil = 0.0, lfo2CtxUntil = 0.0, envCtxUntil = 0.0;

    // the two stomps
    TapHoldButton tapStompBtn, bypassBtn;
    LedIndicator  bypassLed;
    juce::TextButton supplyBtn { "9V" };
    juce::TextButton boostBtn  { "+6 dB: ON" };   // v0.23 boost audition
    juce::TextButton clipBtn   { "CLIP" };        // v0.22/24 clip-stage audition

    // layer state
    int  knobLayer        = 0;
    bool suppressSliderCb = false;   // guard while swapping attachments

    // tap tempo state (press times; commit = average of the last 4)
    double tapHist1[4] {}, tapHist2[4] {};
    int    tapN1 = 0, tapN2 = 0;
    double tapPressMs   = 0.0;
    bool   tapPressLfo2 = false;     // BYPASS was held at the press

    // CV jacks (hardwired: CV1 -> LFO1 depth VCA, CV2 -> LFO2 depth VCA)
    LedIndicator cv1Led, cv2Led;

    // output gate (under the cover)
    juce::Slider threshKnob, holdKnob, fadeKnob;
    juce::Label  threshLabel, holdLabel, fadeLabel;
    std::unique_ptr<SliderAttachment> threshAtt, holdAtt, fadeAtt;
    GateCover gateCover;
    juce::TextButton gateCoverBtn { "CLOSE COVER" };
    LedIndicator gateLed;
    bool gateOpen = false;

    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchwaveAudioProcessorEditor)
};
