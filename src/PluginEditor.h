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
// Hardware-style option selector: a labelled column of LEDs, one per option.
// Click a row to select it; click the title to cycle (like the pedal's button).
// Bound directly to an AudioParameterChoice. ZERO dropdowns.
// ---------------------------------------------------------------------------
class LedSelector : public juce::Component
{
public:
    LedSelector (const juce::String& titleToUse, juce::Colour ledColour)
        : title (titleToUse), colour (ledColour) {}

    void bind (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));
        if (param != nullptr)
            items = param->choices;
    }

    // indicator mode: LEDs only, all changes come from the section's button
    void setInteractive (bool b) noexcept { interactive = b; }

    // v0.17: 18-item shape lists need tighter rows to fit the panel
    void setRowHeight (int h) noexcept { rowH = h; }

    int getSelectedIndex() const noexcept
    {
        return param != nullptr ? param->getIndex() : 0;
    }

    void refresh()   // called from the editor timer
    {
        const int idx = getSelectedIndex();
        if (idx != shownIndex)
        {
            shownIndex = idx;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr || ! isEnabled() || ! interactive)
            return;

        int newIndex;
        if (e.y < kTitleH)                                   // title = cycle button
            newIndex = (param->getIndex() + 1) % items.size();
        else
        {
            const int row = (e.y - kTitleH) / rowH;
            if (row < 0 || row >= items.size())
                return;
            newIndex = row;
        }
        param->beginChangeGesture();
        *param = newIndex;
        param->endChangeGesture();
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        const float alpha = isEnabled() ? 1.0f : 0.35f;
        const int   idx   = shownIndex;

        // title acts as the pedal's cycle button
        g.setColour (juce::Colour (0xff2e3340).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) getWidth(), (float) kTitleH - 2.0f, 4.0f);
        g.setColour (juce::Colour (0xffe8e8e8).withMultipliedAlpha (alpha));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (title, 0, 0, getWidth(), kTitleH - 2, juce::Justification::centred);

        const float ledD  = rowH < 14 ? 7.0f : 9.0f;
        g.setFont (juce::FontOptions (rowH < 14 ? 9.0f : 10.0f));
        for (int i = 0; i < items.size(); ++i)
        {
            const int y = kTitleH + i * rowH;
            const bool on = (i == idx);
            auto led = juce::Rectangle<float> (3.0f, (float) y + (rowH - ledD) * 0.5f,
                                               ledD, ledD);
            if (on)
            {
                g.setColour (colour.withAlpha (0.30f * alpha));
                g.fillEllipse (led.expanded (3.0f));
            }
            g.setColour (juce::Colour (0xff15171b)
                             .interpolatedWith (colour, on ? 1.0f : 0.15f)
                             .withMultipliedAlpha (alpha));
            g.fillEllipse (led);
            g.setColour ((on ? juce::Colour (0xffe8e8e8) : juce::Colour (0xff9aa0a6))
                             .withMultipliedAlpha (alpha));
            g.drawText (items[i], 14, y, getWidth() - 14, rowH,
                        juce::Justification::centredLeft);
        }
    }

    int idealHeight() const noexcept { return kTitleH + items.size() * rowH + 2; }

    static constexpr int kTitleH = 16;

private:
    juce::String title;
    juce::Colour colour;
    juce::StringArray items;
    juce::AudioParameterChoice* param = nullptr;
    int shownIndex = -1;
    int rowH = 15;
    bool interactive = true;
};

// ---------------------------------------------------------------------------
// Hardware-style momentary button (v0.19 timing): TAP fires onTap on release;
// holding for 750 ms starts firing onHoldTick every 750 ms, like the real
// pedal button will. cancelPressActions() consumes the current press (no tap,
// no further hold ticks) — used when the press turns out to be a knob-shift
// gesture instead.
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
        startTimer (750);   // v0.19: cycle every 750 ms while held
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
    void cycleChoice (const juce::String& paramID);
    void advanceDriveRange();

    // ---- v0.19 control scheme ---------------------------------------------
    // The LFO2 rate/depth button is the TAP TEMPO STOMP and doubles as the
    // pedal's SHIFT key (sim: holding CTRL = holding the stomp):
    //   * tap (release < 750 ms, nothing else used) = tempo tap (0.02-10 Hz),
    //     committed at the PRESS time; also re-seeds the chaos generators.
    //   * hold alone 750 ms = depth sweep: LED 2nd colour (blue) @ depth %,
    //     depth rides the 4 s sine-like traverse from its current % in its
    //     remembered direction, turning around instantly at 0% / 100% (no
    //     dwell). Release = freeze + back to 1st colour.
    //   * hold + move a knob = the knob's 2ND FUNCTION (shift):
    //       FREQ->GAIN, LPF->RES, MIX->VOL, LFO1 RATE->DEPTH,
    //       ENV GAIN->drive x range combo. Using shift cancels the sweep.
    //   * hold + press a section button = step that section's TARGET.
    // The LFO1 and ENV buttons also shift their own knob while held:
    //   LFO1 btn + RATE knob = LFO1 depth; ENV btn + GAIN knob = drive/range.
    //   Held alone 750 ms they cycle their TARGET every 750 ms.
    void d2StompPress();
    void d2StompRelease();
    void d2Frame (double nowMs);               // called every timer frame
    void d2Engage();                           // 750 ms reached -> start sweep
    void d2SetDepth (float newDepth01);
    float d2GetDepth() const;
    void d2SetRateFromInterval (double intervalMs);
    bool stompDown() const;                    // button or CTRL (sim stand-in)
    void updateKnobModes();                    // swap slider attachments
    void applyEnvComboFromKnob();              // knob position -> drive x range

    GlitchwaveAudioProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    // pedal row — v0.19: THREE dual-function knobs (FREQ/GAIN, LPF/RES,
    // MIX/VOL); attachments swap to the 2nd param while the stomp is held
    juce::Slider freqKnob, lpfKnob, mixKnob;
    juce::Label  freqLabel, lpfLabel, mixLabel;
    std::unique_ptr<SliderAttachment> freqAtt, lpfAtt, mixAtt;
    PPMMeter meterIn, meterOut;

    // LFO panels (LFO1 always unipolar-up, LFO2 always bipolar — no selectors)
    // v0.19: LFO1 has ONE knob (RATE; depth via button-hold + knob); LFO2 has
    // no knobs at all — tap tempo + hold sweep on the stomp
    juce::Slider lfo1RateKnob;
    juce::Label  lfo1RateLabel;
    juce::Label  lfo2ValueLabel;
    std::unique_ptr<SliderAttachment> lfo1RateAtt;
    LedSelector lfo1Shape  { "SHAPE",  juce::Colour (0xffd9a441) };
    LedSelector lfo1Target { "TARGET", juce::Colour (0xff7bd88f) };
    LedSelector lfo2Shape  { "SHAPE",  juce::Colour (0xffd9a441) };
    LedSelector lfo2Target { "TARGET", juce::Colour (0xff7bd88f) };
    LedIndicator lfo1Led, lfo2Led;
    TapHoldButton lfo1Btn, lfo2Btn, envBtn, lfo2RateBtn;

    double d2LastTapMs   = 0.0;    // previous committed tempo tap (press time)
    double d2PressMs     = 0.0;    // current stomp press time (0 = not pressed)
    double d2StompSince  = 0.0;    // when the stomp (button or CTRL) went down
    double d2PrevFrameMs = 0.0;    // for per-frame dt
    bool   d2Sweeping    = false;  // depth sweep engaged
    bool   d2SweptThisHold = false;// a sweep happened during this stomp hold
    bool   d2Rising      = true;   // remembered wave direction
    double d2U           = 0.0;    // progress 0..1 along the current half-wave

    // shift / dual-function state
    bool shiftAttached   = false;  // top-row + LFO1 + ENV knobs on 2nd params
    bool shiftConsumed   = false;  // a knob or button used the shift this hold
    bool lfo1DepthMode   = false;  // RATE knob currently writes lfo1depth
    bool envComboMode    = false;  // GAIN knob currently selects drive x range
    bool suppressSliderCb = false; // guard while swapping attachments
    bool lfo1KnobUsed    = false;  // knob moved during lfo1Btn hold
    bool envKnobUsed     = false;  // knob moved during envBtn hold

    // envelope follower panel
    juce::Slider envGainKnob;
    juce::Label  envGainLabel;
    std::unique_ptr<SliderAttachment> envGainAtt;
    LedSelector envTarget { "TARGET", juce::Colour (0xff7bd88f) };
    LedSelector envDrive  { "DRIVE",  juce::Colour (0xff6fa8dc) };
    LedSelector lpfMode   { "MODE",   juce::Colour (0xffd95050) };
    LedSelector lpfRange  { "RANGE",  juce::Colour (0xff6fa8dc) };
    LedIndicator envLed;

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
