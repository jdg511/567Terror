#pragma once

#include "PluginProcessor.h"
#include <BinaryData.h>

// ===========================================================================
// v0.34 "Terror" — Jason's glitch-art restyle from the Claude Design project
// (docs/ui/Glitchwave 567 - v0.34 Terror.dc.html). GRAPHICS ONLY: the whole
// X/Y/Z/A layer machine, tap tempo, latches and gate behaviour are v0.32.
// ===========================================================================

namespace gw
{
    // ---- palette (straight from the design) --------------------------------
    const juce::Colour kText     { 0xfff2f4f8 };
    const juce::Colour kDim      { 0xff98a2b3 };
    const juce::Colour kDim2     { 0xff949aa2 };
    const juce::Colour kGrey     { 0xff7c838d };
    const juce::Colour kDead     { 0xff5a6070 };
    const juce::Colour kYellow   { 0xffffe600 };
    const juce::Colour kCyan     { 0xff00eaff };
    const juce::Colour kMagenta  { 0xffff2bd6 };
    const juce::Colour kGreen    { 0xff4dff3a };
    const juce::Colour kRed      { 0xffff2b5e };
    const juce::Colour kTrack    { 0xff14161c };
    const juce::Colour kHairline { 0xff1b1f28 };
    const juce::Colour kRowLine  { 0xff14171e };
    const juce::Colour kChipOff  { 0xff1f2430 };
    const juce::Colour kBtnEdge  { 0xff2b3140 };
    const juce::Colour kKnobEdge { 0x80394152 };
    const juce::Colour kPanelBg  { 0xbf030305 };   // rgba(3,3,5,.75)

    // NeoPixel hue per slot (Bank A / Bank B share a slot)
    const juce::Colour kHues[8] = {
        juce::Colour (0xffff4444), juce::Colour (0xffff8c00),
        juce::Colour (0xffffd400), juce::Colour (0xff44dd66),
        juce::Colour (0xff33cccc), juce::Colour (0xff4488ff),
        juce::Colour (0xff9955ff), juce::Colour (0xffff55bb),
    };

    // ---- embedded typefaces ------------------------------------------------
    struct Fonts
    {
        juce::Typeface::Ptr barlow600, barlow700, mono400, mono500, mono600;

        Fonts()
        {
            auto tf = [] (const void* d, int n)
            { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); };
            barlow600 = tf (BinaryData::barlow_semibold_ttf, BinaryData::barlow_semibold_ttfSize);
            barlow700 = tf (BinaryData::barlow_bold_ttf,     BinaryData::barlow_bold_ttfSize);
            mono400   = tf (BinaryData::plexmono_regular_ttf,  BinaryData::plexmono_regular_ttfSize);
            mono500   = tf (BinaryData::plexmono_medium_ttf,   BinaryData::plexmono_medium_ttfSize);
            mono600   = tf (BinaryData::plexmono_semibold_ttf, BinaryData::plexmono_semibold_ttfSize);
        }

        static const Fonts& get() { static Fonts f; return f; }
    };

    inline juce::Font barlow (float px, bool bold = true, float trackEm = 0.0f)
    {
        juce::Font f (juce::FontOptions (bold ? Fonts::get().barlow700
                                              : Fonts::get().barlow600).withPointHeight (px));
        if (trackEm != 0.0f) f.setExtraKerningFactor (trackEm);
        return f;
    }

    inline juce::Font mono (float px, int weight = 400, float trackEm = 0.0f)
    {
        auto& F = Fonts::get();
        juce::Font f (juce::FontOptions (weight >= 600 ? F.mono600
                                       : weight >= 500 ? F.mono500 : F.mono400)
                          .withPointHeight (px));
        if (trackEm != 0.0f) f.setExtraKerningFactor (trackEm);
        return f;
    }

    inline float textW (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getRight();
    }

    // rotary sweep straight from the design's conic-gradient: 216 deg .. 504 deg
    constexpr float kAngle0 = juce::MathConstants<float>::pi * 1.2f;
    constexpr float kAngle1 = juce::MathConstants<float>::pi * 2.8f;
}

// ---------------------------------------------------------------------------
// The design's knob: outer conic value ring, soft-gradient body, glowing
// pointer. Ring colour comes from the slider's rotarySliderFillColourId.
// ---------------------------------------------------------------------------
class GwLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto  r     = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto  c     = r.getCentre();
        const bool  big   = w >= 100;
        const float scale = (float) w / (big ? 120.0f : 76.0f);
        const float ringR = (big ? 57.0f : 35.5f) * scale;
        const float thick = (big ? 6.0f  : 5.0f)  * scale;
        const float bodyR = (big ? 44.0f : 27.0f) * scale;
        const float pIn   = (big ? 33.0f : 20.0f) * scale;
        const float pOut  = (big ? 50.0f : 31.0f) * scale;
        const float pW    = (big ? 3.0f  : 2.5f)  * scale;
        const float alpha = s.isEnabled() ? 1.0f : 0.55f;
        const auto  col   = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, a1, true);
        g.setColour (gw::kTrack.withMultipliedAlpha (alpha));
        g.strokePath (track, juce::PathStrokeType (thick));

        const float av = a0 + pos * (a1 - a0);
        if (av > a0 + 0.004f)
        {
            juce::Path v;
            v.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, av, true);
            g.setColour (col.withAlpha (0.30f * alpha));
            g.strokePath (v, juce::PathStrokeType (thick + 3.5f * scale));
            g.setColour (col.withMultipliedAlpha (alpha));
            g.strokePath (v, juce::PathStrokeType (thick));
        }

        {   // body
            juce::ColourGradient grad (juce::Colour (0xff1e1e28),
                                       c.x - bodyR * 0.32f, c.y - bodyR * 0.48f,
                                       juce::Colour (0xff050508),
                                       c.x + bodyR * 0.55f, c.y + bodyR * 0.95f, true);
            g.setGradientFill (grad);
            g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (gw::kKnobEdge.withMultipliedAlpha (alpha));
            g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f * alpha));
            g.drawEllipse (c.x - bodyR + 1.5f, c.y - bodyR + 1.5f,
                           bodyR * 2.0f - 3.0f, bodyR * 2.0f - 3.0f, 1.0f);
        }

        {   // pointer
            const float sn = std::sin (av), cs = std::cos (av);
            const juce::Line<float> ln (c.x + sn * pIn, c.y - cs * pIn,
                                        c.x + sn * pOut, c.y - cs * pOut);
            g.setColour (col.withAlpha (0.35f * alpha));
            g.drawLine (ln, pW + 4.0f * scale);
            g.setColour (col.withMultipliedAlpha (alpha));
            g.drawLine (ln, pW);
        }
    }
};

// ---------------------------------------------------------------------------
// Simple digital PPM: instant attack, timed fall — design meter colours.
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
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (gw::kHairline);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        const float frac = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
        if (frac <= 0.001f)
            return;

        auto bar = r.reduced (3.0f);
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
        seg (-60.0f, -12.0f, gw::kGreen);
        seg (-12.0f,  -4.0f, gw::kYellow);
        seg ( -4.0f,   0.0f, gw::kRed);
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
            g.setColour (colour.withAlpha (0.30f * level));
            g.fillEllipse (led.expanded (d * 0.38f * level));
        }
        g.setColour (juce::Colour (0xff0a0c10).interpolatedWith (colour, 0.14f + 0.86f * level));
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (led, 1.0f);
    }

private:
    juce::Colour colour { 0xffffe600 };
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

    // v0.28 sim aid: RIGHT-CLICK latches the stomp "held" until the next
    // right-click — a pure-mouse stand-in for a held footswitch, immune to
    // whatever eats lone modifier keys on the host machine.
    bool isLatched() const noexcept { return latched; }

    // v0.30: the stomp doubles as an INDICATOR — it lights while its layer
    // key (INS/DEL) is held, so the widget shows the held state AND can be
    // activated by mouse.
    void setIndicated (bool b) noexcept
    {
        if (indicated != b) { indicated = b; repaint(); }
    }

    void cancelPressActions() noexcept
    {
        consumed = true;      // suppress the tap and any further hold ticks
        stopTimer();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! isEnabled()) return;
        if (e.mods.isRightButtonDown())   // right-click = toggle the latch
        {
            rightPress = true;
            latched = ! latched;
            repaint();
            return;
        }
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
        if (rightPress) { rightPress = false; return; }
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
        const float d = juce::jmin (r.getWidth(), r.getHeight()) - 3.0f;
        auto btn = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        const float a = isEnabled() ? 1.0f : 0.35f;
        const bool held = pressed || latched || indicated;

        if (latched)   // latched = "foot stays on it": strong glow
        {
            g.setColour (gw::kYellow.withAlpha (0.35f * a));
            g.fillEllipse (btn.expanded (5.0f));
        }
        juce::ColourGradient grad (juce::Colour (0xff1a1a24),
                                   btn.getX() + btn.getWidth() * 0.38f,
                                   btn.getY() + btn.getHeight() * 0.30f,
                                   juce::Colours::black,
                                   btn.getRight(), btn.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (btn);
        g.setColour ((held ? gw::kYellow : juce::Colour (0xff4b5364)).withMultipliedAlpha (a));
        g.drawEllipse (btn, held ? 2.5f : 1.5f);
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
    bool latched = false, rightPress = false, indicated = false;
};

// ---------------------------------------------------------------------------
// The X / Y / Z / A layer chips in the header.
// ---------------------------------------------------------------------------
class LayerChips : public juce::Component
{
public:
    void setLayer (int l)
    {
        if (l != layer) { layer = l; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        static const char* names[4] = { "X", "Y", "Z", "A" };
        const juce::Colour cols[4]  = { gw::kText, gw::kCyan, gw::kYellow, gw::kRed };
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<float> ((float) i * 42.0f, 0.0f, 36.0f, 26.0f);
            if (i == layer)
            {
                g.setColour (cols[i].withAlpha (0.30f));
                g.fillRoundedRectangle (r.expanded (3.0f), 7.0f);
                g.setColour (cols[i]);
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (juce::Colours::black);
            }
            else
            {
                g.setColour (gw::kChipOff);
                g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
                g.setColour (gw::kGrey);
            }
            g.setFont (gw::mono (13.2f, 600));
            g.drawText (names[i], r, juce::Justification::centred);
        }
    }

private:
    int layer = 0;
};

// ---------------------------------------------------------------------------
// A row of NeoPixel swatches showing which choice is selected (display only —
// the selector knobs do the choosing, exactly as in v0.32).
// ---------------------------------------------------------------------------
class SwatchRuler : public juce::Component
{
public:
    void configure (int numCells, bool firstIsOff)
    {
        cells = numCells; offFirst = firstIsOff;
    }

    void setSelected (int displayIndex)
    {
        if (displayIndex != sel) { sel = displayIndex; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const float gap = 3.0f;
        const float w   = ((float) getWidth() - gap * (float) (cells - 1)) / (float) cells;
        const float h   = (float) getHeight();
        for (int i = 0; i < cells; ++i)
        {
            const float x = (float) i * (w + gap);
            const bool  isOffCell = offFirst && i == 0;
            const auto  base = isOffCell ? gw::kTrack : gw::kHues[i % 8];
            const bool  live = i == sel;
            auto r = juce::Rectangle<float> (x, 0.0f, w, h);
            if (live && ! isOffCell)
            {
                g.setColour (base.withAlpha (0.45f));
                g.fillRoundedRectangle (r.expanded (3.0f), 3.0f);
            }
            g.setColour (isOffCell ? base : base.withAlpha (live ? 1.0f : 0.34f));
            g.fillRoundedRectangle (r, 2.0f);
            if (live)
            {
                g.setColour (juce::Colours::black);
                g.drawRoundedRectangle (r.expanded (1.0f), 2.5f, 1.5f);
                g.setColour (gw::kText);
                g.drawRoundedRectangle (r.expanded (2.0f), 3.0f, 1.0f);
            }
        }
    }

private:
    int cells = 8, sel = 0;
    bool offFirst = false;
};

// ---------------------------------------------------------------------------
// "Sine / Wobble" — a mono label whose two halves carry different colours.
// ---------------------------------------------------------------------------
class TwoToneLabel : public juce::Component
{
public:
    void set (const juce::String& newA, juce::Colour newColA,
              const juce::String& newB = {}, juce::Colour newColB = gw::kGrey)
    {
        if (a == newA && b == newB && colA == newColA)
            return;
        a = newA; b = newB; colA = newColA; colB = newColB;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto f = gw::mono (11.0f, 400);
        g.setFont (f);
        g.setColour (colA);
        g.drawText (a, getLocalBounds(), juce::Justification::centredLeft);
        if (b.isNotEmpty())
        {
            g.setColour (colB);
            g.drawText (b, getLocalBounds().withTrimmedLeft (
                            juce::roundToInt (gw::textW (f, a + " "))),
                        juce::Justification::centredLeft);
        }
    }

private:
    juce::String a, b;
    juce::Colour colA { gw::kText }, colB { gw::kGrey };
};

// ---------------------------------------------------------------------------
// One PCB switch row under the cover: "JFET STAGE          ON".
// ---------------------------------------------------------------------------
class PcbSwitchRow : public juce::Component
{
public:
    void attach (juce::RangedAudioParameter* p, const juce::String& text)
    {
        param = p; label = text; refresh();
    }

    void refresh()
    {
        const bool now = param != nullptr && param->getValue() >= 0.5f;
        if (now != on) { on = now; repaint(); }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() >= 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff050508));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (gw::kBtnEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (gw::kText);
        g.setFont (gw::barlow (11.0f, true, 0.13f));
        g.drawText (label, getLocalBounds().reduced (14, 0), juce::Justification::centredLeft);
        g.setColour (on ? gw::kGreen : gw::kGrey);
        g.setFont (gw::mono (11.0f, 500));
        g.drawText (on ? "ON" : "OFF", getLocalBounds().reduced (14, 0),
                    juce::Justification::centredRight);
    }

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    bool on = false;
};

// ---------------------------------------------------------------------------
// The 9V / 12V / 15V / 18V simulated-supply selector under the cover.
// ---------------------------------------------------------------------------
class SupplySelector : public juce::Component
{
public:
    void attach (juce::AudioParameterChoice* p) { param = p; refresh(); }

    void refresh()
    {
        const int now = param != nullptr ? param->getIndex() : 0;
        if (now != sel) { sel = now; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        const int i = juce::jlimit (0, 3, e.x / 59);
        param->beginChangeGesture();
        *param = i;
        param->endChangeGesture();
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        static const char* names[4] = { "9V", "12V", "15V", "18V" };
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<float> ((float) i * 59.0f, 0.0f, 52.0f, (float) getHeight());
            if (i == sel)
            {
                g.setColour (gw::kGreen.withAlpha (0.40f));
                g.fillRoundedRectangle (r.expanded (3.0f), 8.0f);
                g.setColour (gw::kGreen);
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (juce::Colours::black);
            }
            else
            {
                g.setColour (juce::Colour (0xff050508));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (gw::kBtnEdge);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (gw::kDim);
            }
            g.setFont (gw::mono (12.0f, 400));
            g.drawText (names[i], r, juce::Justification::centred);
        }
    }

private:
    juce::AudioParameterChoice* param = nullptr;
    int sel = 0;
};

// ---------------------------------------------------------------------------
// Closed cover: the full-width INTERNAL strip with LED + live summary line.
// Click anywhere (or the OPEN COVER button) to open.
// ---------------------------------------------------------------------------
class InternalStrip : public juce::Component
{
public:
    std::function<void()> onOpen;

    struct Summary
    {
        juce::String gate, supply;
        bool jfet = true, ladder = false, boost = true, hints = false;

        bool operator!= (const Summary& o) const
        {
            return gate != o.gate || supply != o.supply || jfet != o.jfet
                || ladder != o.ladder || boost != o.boost || hints != o.hints;
        }
    };

    void setSummary (const Summary& s)
    {
        if (s != summary) { summary = s; repaint(); }
    }

    void setLedState (juce::Colour c, float level)
    {
        if (c != ledColour || std::fabs (level - ledLevel) > 0.02f)
        {
            ledColour = c; ledLevel = level; repaint();
        }
    }

    void mouseDown (const juce::MouseEvent&) override { if (onOpen) onOpen(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (gw::kPanelBg);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (gw::kHairline);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (gw::kChipOff);   // fake screws
        for (auto p : { juce::Point<float> (14.5f, 14.5f), { r.getWidth() - 14.5f, 14.5f },
                        { 14.5f, r.getHeight() - 14.5f }, { r.getWidth() - 14.5f, r.getHeight() - 14.5f } })
            g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);

        // gate status LED
        {
            auto led = juce::Rectangle<float> (32.0f, 31.0f, 11.0f, 11.0f);
            if (ledLevel > 0.05f)
            {
                g.setColour (ledColour.withAlpha (0.35f * ledLevel));
                g.fillEllipse (led.expanded (5.0f));
            }
            g.setColour (juce::Colour (0xff0a0c10).interpolatedWith (ledColour,
                             0.14f + 0.86f * ledLevel));
            g.fillEllipse (led);
        }

        g.setColour (gw::kText);
        g.setFont (gw::barlow (11.5f, true, 0.16f));
        g.drawText (juce::String::fromUTF8 ("INTERNAL \xc2\xb7 UNDER THE COVER"),
                    58, 14, 400, 16, juce::Justification::centredLeft);

        // the live summary line, segment colours like the design
        {
            float x = 58.0f;
            const float y = 36.0f, h = 14.0f;
            auto f  = gw::mono (10.0f, 400);
            auto put = [&] (const juce::String& t, juce::Colour c)
            {
                g.setColour (c);
                g.setFont (f);
                g.drawText (t, juce::Rectangle<float> (x, y, 600.0f, h),
                            juce::Justification::centredLeft);
                x += gw::textW (f, t);
            };
            put ("GATE ", gw::kDim);
            put (summary.gate, gw::kText);
            put ("   |   ", gw::kGrey);
            put ("JFET ", gw::kDim);
            put (summary.jfet ? "ON" : "OFF", summary.jfet ? gw::kGreen : gw::kDim2);
            put ("   LADDER ", gw::kDim);
            put (summary.ladder ? "ON" : "OFF", summary.ladder ? gw::kGreen : gw::kDim2);
            put ("   +6dB ", gw::kDim);
            put (summary.boost ? "ON" : "OFF", summary.boost ? gw::kGreen : gw::kDim2);
            put ("   SUPPLY ", gw::kDim);
            put (summary.supply, gw::kText);
            put ("   HINTS ", gw::kDim);
            put (summary.hints ? "ON" : "OFF", summary.hints ? gw::kYellow : gw::kDim2);
        }

        // OPEN COVER button
        {
            auto b = juce::Rectangle<float> (906.0f, 24.0f, 106.0f, 26.0f);
            g.setColour (juce::Colour (0xff0a0a0e));
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (gw::kYellow.withAlpha (0.25f));
            g.drawRoundedRectangle (b.reduced (1.0f), 5.0f, 1.0f);
            g.setColour (gw::kKnobEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (gw::kText);
            g.setFont (gw::barlow (10.5f, true, 0.14f));
            g.drawText ("OPEN COVER", b, juce::Justification::centred);
        }
    }

private:
    Summary summary;
    juce::Colour ledColour { gw::kGreen };
    float ledLevel = 1.0f;
};

// ---------------------------------------------------------------------------
// Dim veil behind the open cover — clicking it closes the cover.
// ---------------------------------------------------------------------------
class CoverDim : public juce::Component
{
public:
    std::function<void()> onDismiss;
    void mouseDown (const juce::MouseEvent&) override { if (onDismiss) onDismiss(); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.78f));
    }
};

// ---------------------------------------------------------------------------
// The open cover: gate trimmers, PCB switches, sim supply, HINTS + CLOSE.
// The gate sliders / switch rows / supply selector are its children.
// ---------------------------------------------------------------------------
class CoverPanel : public juce::Component
{
public:
    std::function<void()> onClose, onHintsToggle;

    juce::Slider* gateKnobs[3] { nullptr, nullptr, nullptr };
    bool hintsOn = false;
    juce::Colour ledColour { gw::kGreen };
    float ledLevel = 1.0f;

    void setLedState (juce::Colour c, float level)
    {
        if (c != ledColour || std::fabs (level - ledLevel) > 0.02f)
        {
            ledColour = c; ledLevel = level; repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (juce::Rectangle<int> (902, 16, 114, 26).contains (e.getPosition()))
        { if (onClose) onClose(); return; }
        if (juce::Rectangle<int> (780, 16, 114, 26).contains (e.getPosition()))
        { if (onHintsToggle) onHintsToggle(); return; }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (gw::kGreen.withAlpha (0.22f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 3.0f);
        g.setColour (gw::kGreen);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        g.fillRect (2.0f, 0.0f, r.getWidth() - 4.0f, 2.0f);   // accent top

        {   // gate LED, echoed inside the cover
            auto led = juce::Rectangle<float> (24.0f, 20.0f, 11.0f, 11.0f);
            if (ledLevel > 0.05f)
            {
                g.setColour (ledColour.withAlpha (0.35f * ledLevel));
                g.fillEllipse (led.expanded (5.0f));
            }
            g.setColour (juce::Colour (0xff0a0c10).interpolatedWith (ledColour,
                             0.14f + 0.86f * ledLevel));
            g.fillEllipse (led);
        }

        g.setColour (gw::kText);
        g.setFont (gw::barlow (12.0f, true, 0.18f));
        g.drawText (juce::String::fromUTF8 ("UNDER THE COVER \xc2\xb7 TRIM POTS / SWITCHES / SIM VOLTAGE"),
                    48, 12, 640, 16, juce::Justification::centredLeft);
        g.setColour (gw::kDim);
        g.setFont (gw::mono (9.0f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("Not on the pedal face \xe2\x80\x94 set once with a trimmer and forget."),
                    48, 32, 640, 12, juce::Justification::centredLeft);

        auto button = [&] (juce::Rectangle<float> b, const juce::String& t, juce::Colour edge)
        {
            g.setColour (juce::Colour (0xff0a0a0e));
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (edge);
            g.drawRoundedRectangle (b.reduced (1.0f), 5.0f, 1.0f);
            g.setColour (gw::kBtnEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (gw::kText);
            g.setFont (gw::barlow (10.5f, true, 0.14f));
            g.drawText (t, b, juce::Justification::centred);
        };
        button ({ 780.0f, 16.0f, 114.0f, 26.0f },
                hintsOn ? "HINTS: ON" : "HINTS: OFF",
                gw::kYellow.withAlpha (hintsOn ? 0.75f : 0.30f));
        button ({ 902.0f, 16.0f, 114.0f, 26.0f }, "CLOSE COVER", gw::kGreen.withAlpha (0.4f));

        // section headers + rules
        g.setColour (gw::kDim);
        g.setFont (gw::barlow (9.5f, true, 0.16f));
        g.drawText ("OUTPUT GATE",  12, 72, 200, 12, juce::Justification::centredLeft);
        g.drawText ("PCB SWITCHES", 500, 72, 200, 12, juce::Justification::centredLeft);
        g.drawText ("SIM SUPPLY",   758, 110, 200, 12, juce::Justification::centredLeft);
        g.setColour (gw::kHairline);
        g.fillRect (12, 90, 414, 1);
        g.fillRect (500, 90, 512, 1);
        g.fillRect (450, 100, 1, 150);   // divider

        // gate captions / live values / ranges
        static const char* caps[3]   = { "THRESH", "HOLD", "FADE" };
        static const char* ranges[3] = { "\xe2\x88\x92""96 \xe2\x80\xa6 0", "0.1 \xe2\x80\xa6 10", "0.1 \xe2\x80\xa6 60" };
        for (int i = 0; i < 3; ++i)
        {
            const int x = 28 + i * 132;
            g.setColour (gw::kText);
            g.setFont (gw::barlow (10.0f, true, 0.14f));
            g.drawText (caps[i], x, 104, 76, 12, juce::Justification::centred);
            if (gateKnobs[i] != nullptr)
            {
                juce::String v;
                const double val = gateKnobs[i]->getValue();
                if (i == 0)      v = juce::String (val, 1) + " dB";
                else             v = juce::String (val, val < 0.9995 ? 2 : 1) + " s";
                v = v.replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92"));
                g.setColour (gw::kText);
                g.setFont (gw::mono (12.0f, 400));
                g.drawText (v, x, 206, 76, 14, juce::Justification::centred);
            }
            g.setColour (gw::kGrey);
            g.setFont (gw::mono (9.0f, 400));
            g.drawText (juce::String::fromUTF8 (ranges[i]), x, 223, 76, 11, juce::Justification::centred);
        }

        g.setColour (gw::kGrey);
        g.setFont (gw::mono (9.0f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("Gate LED: green = open \xc2\xb7 amber = fading \xc2\xb7 red = fully closed"),
                    500, 246, 500, 12, juce::Justification::centredLeft);
    }
};

// ---------------------------------------------------------------------------
// "Where the Fuzz Meets the Funk" — chromatic-aberration tagline with the
// occasional horizontal tear, plus the brand line under it.
// ---------------------------------------------------------------------------
class TaglineComp : public juce::Component
{
public:
    void setTear (float px)
    {
        if (std::fabs (px - tear) > 0.1f) { tear = px; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const juce::String t = "Where the Fuzz Meets the Funk";
        auto f = gw::barlow (27.6f, true, 0.02f);
        auto area = getLocalBounds().withHeight (34).translated ((int) tear, 0);
        g.setFont (f);
        g.setColour (juce::Colour (0xffff00be).withAlpha (0.85f));
        g.drawText (t, area.translated (2, 0), juce::Justification::centredRight);
        g.setColour (gw::kCyan.withAlpha (0.85f));
        g.drawText (t, area.translated (-2, 0), juce::Justification::centredRight);
        g.setColour (gw::kText);
        g.drawText (t, area, juce::Justification::centredRight);

        // ILLICIT APOTHECARY · FILTER · v0.34
        auto fm = gw::mono (11.4f, 400, 0.06f);
        float x = (float) getWidth();
        auto put = [&] (const juce::String& s, juce::Colour c)
        {
            x -= gw::textW (fm, s);
            g.setColour (c);
            g.setFont (fm);
            g.drawText (s, juce::Rectangle<float> (x, 38.0f, 600.0f, 14.0f),
                        juce::Justification::topLeft);
        };
        put ("v0.34", gw::kGrey);
        put (juce::String::fromUTF8 (" \xc2\xb7 "), gw::kGrey);
        put ("FILTER", gw::kGreen);
        put (juce::String::fromUTF8 (" \xc2\xb7 "), gw::kGrey);
        put ("ILLICIT APOTHECARY", gw::kDim);
    }

private:
    float tear = 0.0f;
};

// ---------------------------------------------------------------------------
// CRT scan sweep + the design's timed glitch bursts. Pure decoration,
// mouse-transparent, painted over the whole face.
// ---------------------------------------------------------------------------
class GlitchFx : public juce::Component
{
public:
    GlitchFx() { setInterceptsMouseClicks (false, false); }

    // called from the editor timer; nowMs is a steadily increasing clock
    void tick (double nowMs)
    {
        if (t0 <= 0.0) t0 = nowMs;
        const double t = nowMs - t0;

        // 7 s scan sweep
        const float newY = (float) std::fmod (t / 7000.0, 1.0) * 780.0f - 140.0f;
        if (std::fabs (newY - scanY) > 0.8f)
        {
            const auto oldR = juce::Rectangle<int> (0, (int) scanY - 1, getWidth(), 143);
            scanY = newY;
            repaint (oldR.getUnion ({ 0, (int) scanY - 1, getWidth(), 143 }));
        }

        // light burst: every 33 s, ~0.36 s of flickering colour bars
        const double lp = std::fmod (t, 33000.0);
        const bool light = lp < 360.0;
        if (light != lightOn || (light && (int) (lp / 60.0) != lightFrame))
        {
            lightOn = light;
            lightFrame = (int) (lp / 60.0);
            repaint (0, 205, getWidth(), 20);
            repaint (0, 410, getWidth(), 16);
        }

        // major glitch: every 666 s, ~1.3 s of mayhem
        const double mp = std::fmod (t, 666000.0);
        const bool major = mp > 1000.0 && mp < 2300.0;
        if (major != majorOn || (major && (int) (mp / 70.0) != majorFrame))
        {
            majorOn = major;
            majorFrame = (int) (mp / 70.0);
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        {   // scan sweep, 6 % white band
            juce::ColourGradient grad (juce::Colours::transparentWhite, 0.0f, scanY,
                                       juce::Colours::transparentWhite, 0.0f, scanY + 140.0f, false);
            grad.addColour (0.45, juce::Colours::white.withAlpha (0.06f));
            g.setGradientFill (grad);
            g.fillRect (0.0f, scanY, (float) getWidth(), 140.0f);
        }

        if (lightOn)
        {
            auto bar = [&] (int y, int h, juce::Colour c1, int w1, int gap1,
                            juce::Colour c2, int w2, int gap2, int seed)
            {
                const int jitter = ((lightFrame * 37 + seed) % 23) - 11;
                int x = -40 + jitter;
                const float a = 0.55f + 0.35f * (float) ((lightFrame + seed) % 2);
                while (x < getWidth())
                {
                    g.setColour (c1.withAlpha (a));
                    g.fillRect (x, y, w1, h);
                    x += w1 + gap1;
                    g.setColour (c2.withAlpha (a));
                    g.fillRect (x, y, w2, h);
                    x += w2 + gap2;
                }
            };
            bar (210, 9, gw::kCyan, 4, 7, gw::kMagenta, 3, 12, 0);
            bar (415, 6, gw::kGreen, 3, 6, gw::kYellow, 2, 11, 5);
        }

        if (majorOn)
        {
            // veil of magenta / cyan scan pairs
            const float va = 0.10f + 0.12f * (float) (majorFrame % 3);
            for (int y = 0; y < getHeight(); y += 13)
            {
                g.setColour (gw::kMagenta.withAlpha (va));
                g.fillRect (0, y, getWidth(), 2);
                g.setColour (gw::kCyan.withAlpha (va * 0.8f));
                g.fillRect (0, y + 5, getWidth(), 2);
            }
            // three tear bands sliding pseudo-randomly
            struct Band { int y, h; } bands[3] = { { 102, 54 }, { 311, 88 }, { 500, 120 } };
            for (int b = 0; b < 3; ++b)
            {
                const int off = (((majorFrame * 131 + b * 977) % 361) - 180);
                int x = -200 + off;
                const juce::Colour cols[4] = { gw::kYellow, gw::kCyan, gw::kMagenta, gw::kGreen };
                int i = 0;
                while (x < getWidth())
                {
                    g.setColour (cols[(i + b) % 4].withAlpha (0.5f));
                    const int w = 3 + ((i * 7 + b) % 5);
                    g.fillRect (x, bands[b].y, w, bands[b].h);
                    x += w + 8 + ((i * 3) % 9);
                    ++i;
                }
            }
            if (majorFrame % 6 == 0)   // stroboscopic full-face flash
            {
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillAll();
            }
        }
    }

private:
    double t0 = -1.0;
    float scanY = -140.0f;
    bool lightOn = false, majorOn = false;
    int lightFrame = 0, majorFrame = 0;
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
    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name,
                    bool big, juce::Colour ring);
    void setGateOpen (bool shouldBeOpen);
    void applyHints();
    void refreshReadouts (int layer);

    // ---- v0.30 control scheme: Jason's X/Y/Z/A knob layers -----------------
    // Six knobs (Freq, LPF, Mix | LFO1 Rate, LFO2 Rate, Env Gain):
    //   X (nothing held):        Freq   LPF     Mix      Rate   Rate   Gain
    //   Y (TAP held / INS):      Gain   Res     Vol      Shape  Shape  Mode
    //   Z (BYPASS held / DEL):   L1 Dep L2 Dep  DrvRng   Target Target Target
    //   A (BOTH held, secret):   Mix knob -> STARVE; every other knob dead.
    bool tapStompDown() const;      // TAP stomp, latch, or INS held
    bool bypassStompDown() const;   // BYPASS stomp, latch, or DEL held
    int  computeLayer() const;      // 0 = X, 1 = Y, 2 = Z, 3 = A (starve)
    void updateKnobModes();         // swap slider attachments per layer
    void knobTouched();             // any knob move consumes the held stomps
    void applyZone (juce::Slider& s, const char* paramID, int zones,
                    int& ctx, double& ctxUntil, int ctxKind);
    void applyComboFromMixKnob();   // Z Mix knob -> env drive x range
    void recordTap (bool lfo2, double pressMs);

    GlitchwaveAudioProcessor& processor;
    GwLookAndFeel lnf;

    // baked art from the design bundle
    juce::Image bgImage, logoFx, logoPlain;

    // the six knobs
    juce::Slider freqKnob, lpfKnob, mixKnob;
    juce::Label  freqLabel, lpfLabel, mixLabel;
    std::unique_ptr<SliderAttachment> freqAtt, lpfAtt, mixAtt;
    juce::Slider lfo1RateKnob, lfo2RateKnob, envGainKnob;
    juce::Label  lfo1RateLabel, lfo2RateLabel, envGainLabel;
    std::unique_ptr<SliderAttachment> lfo1RateAtt, lfo2RateAtt, envGainAtt;

    juce::Label  vals[6];              // live value line under each knob
    juce::Label  depth1Cap, depth2Cap, envSubCap;
    PPMMeter meterIn, meterOut;

    // header
    LayerChips chips;
    TaglineComp tagline;

    // selection rulers + names (display only — knobs do the choosing)
    SwatchRuler l1ShapeRuler, l1TargetRuler, l2ShapeRuler, l2TargetRuler;
    SwatchRuler envModeRuler, envTargetRuler, envComboRuler;
    TwoToneLabel l1ShapeName, l1TargetName, l2ShapeName, l2TargetName;
    TwoToneLabel envModeName, envTargetName, envComboName;

    // cached choice params the LEDs + rulers display
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
    LedIndicator  tapLed;              // v0.32: blinks the tap tempo + flashes presses
    double lastTapFlashMs = 0.0;

    // layer state
    int  knobLayer        = 0;
    bool suppressSliderCb = false;   // guard while swapping attachments

    // tap tempo state (press times; commit = rolling 3-press average)
    double tapHist1[4] {}, tapHist2[4] {};
    int    tapN1 = 0, tapN2 = 0;
    double tapPressMs   = 0.0;
    bool   tapPressLfo2 = false;     // BYPASS was held at the press

    // output gate + internal switches (all under the cover)
    juce::Slider threshKnob, holdKnob, fadeKnob;
    juce::Label  threshLabel, holdLabel, fadeLabel;   // hidden (cover paints captions)
    std::unique_ptr<SliderAttachment> threshAtt, holdAtt, fadeAtt;
    PcbSwitchRow  jfetRow, ladderRow, boostRow;   // +6 dB stays: it IS on Jason's PCB
    SupplySelector supplySel;
    InternalStrip strip;
    CoverDim   coverDim;
    CoverPanel cover;
    bool gateOpen = false;

    // hints (design ships them off)
    bool showHints = false;
    juce::Label hintChips, hintLayers, hintLfo1, hintLfo2, hintStomp1, hintStomp2;

    // decoration
    GlitchFx fx;

    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchwaveAudioProcessorEditor)
};
