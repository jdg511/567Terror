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

    // v0.30: the stomp doubles as an INDICATOR: it lights while the layer is
    // held (press-and-hold or right-click latch), so the widget shows the
    // held state AND can be activated by mouse.
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
// v0.41 / hardware rev 7: the 9/12/15/18 V selector is gone. There is one
// adapter voltage now, and the only rail worth showing under the cover is the
// LM567's own, which rev 7 drops out of VA with two 1N4148W because TI's
// recommended maximum for the part is 8.5 V. Read-only, by design: it is set
// by two diodes, not by a trimmer.
// ---------------------------------------------------------------------------
class RailReadout : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff050508));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (gw::kBtnEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        g.setColour (gw::kGreen);
        g.setFont (gw::mono (13.0f, 500));
        g.drawText ("7.5 V", getLocalBounds().reduced (14, 0),
                    juce::Justification::centredLeft);
        g.setColour (gw::kDim);
        g.setFont (gw::mono (9.5f, 400));
        g.drawText ("VA - D105 - D107", getLocalBounds().reduced (14, 0),
                    juce::Justification::centredRight);
    }
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
        juce::String gate;
        bool jfet = false, hints = true;
        bool c41 = false, c42 = false;          // v0.39 LM567 filter pads

        bool operator!= (const Summary& o) const
        {
            return gate != o.gate || jfet != o.jfet || hints != o.hints
                || c41 != o.c41 || c42 != o.c42;
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
            put (summary.jfet ? "IN" : "OUT", summary.jfet ? gw::kGreen : gw::kDim2);
            put ("   C41 ", gw::kDim);
            put (summary.c41 ? "IN" : "OUT", summary.c41 ? gw::kGreen : gw::kDim2);
            put ("   C42 ", gw::kDim);
            put (summary.c42 ? "IN" : "OUT", summary.c42 ? gw::kGreen : gw::kDim2);
            put ("   V567 ", gw::kDim);
            put ("7.5 V", gw::kText);
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
        g.drawText (juce::String::fromUTF8 ("UNDER THE COVER \xc2\xb7 TRIM POTS / SWITCHES / LM567 RAIL"),
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
        g.drawText (juce::String::fromUTF8 ("PCB SWITCHES \xc2\xb7 SW1 SITS BEFORE THE FUSS"),
                    500, 72, 300, 12, juce::Justification::centredLeft);
        g.drawText ("LM567 RAIL",   758, 110, 200, 12, juce::Justification::centredLeft);
        g.setColour (gw::kDim);
        g.setFont (gw::barlow (9.5f, true, 0.16f));
        g.drawText (juce::String::fromUTF8 ("LM567 FILTER PADS \xc2\xb7 DNP ON THE BOARD"),
                    758, 172, 260, 12, juce::Justification::centredLeft);
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
                    500, 246, 480, 12, juce::Justification::centredLeft);
        g.setColour (gw::kGrey);
        g.setFont (gw::mono (8.5f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("C41 1u pin 2 \xc2\xb7 C42 220n pin 1 \xc2\xb7 fit them and the decoder stops chattering"),
                    500, 262, 520, 11, juce::Justification::centredLeft);
        g.setColour (gw::kGrey);
        g.setFont (gw::mono (8.5f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("SW1 at natural gain (x4..x10) slams the fuss \xc2\xb7 GAIN minimum is already fuzz"),
                    500, 276, 520, 11, juce::Justification::centredLeft);
    }
};

// ---------------------------------------------------------------------------
// v0.40 demo player. A dropdown of embedded clips, a start/stop transport and
// a level knob, in a strip below the pedal face. The processor sums the clip
// into the pedal's input, so it runs through the whole circuit. The strip sits
// outside the cover's dim veil on purpose: the point of it is A/B-ing the
// under-the-cover switches against real playing while the cover is open.
// ---------------------------------------------------------------------------
class DemoSelector : public juce::Component
{
public:
    std::function<void()> onChange;

    void attach (juce::AudioParameterChoice* p) { param = p; idx = -1; refresh(); }

    void refresh()
    {
        const int now = param != nullptr ? param->getIndex() : 0;
        if (now != idx) { idx = now; repaint(); }
    }

    juce::String currentName() const
    {
        if (param == nullptr || param->choices.isEmpty()) return "-";
        return param->choices[juce::jlimit (0, param->choices.size() - 1, idx)];
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (param == nullptr) return;

        // 27 clips is far too many for one flat list, and the names already
        // group by the text before the dash, so build a submenu per family.
        const auto& names = param->choices;
        juce::PopupMenu menu, sub;
        juce::String lastCat;

        for (int i = 0; i < names.size(); ++i)
        {
            auto cat  = names[i].upToFirstOccurrenceOf (" - ", false, false).trim();
            auto leaf = names[i].fromFirstOccurrenceOf (" - ", false, false).trim();
            if (leaf.isEmpty()) { cat = "Clips"; leaf = names[i]; }

            if (cat != lastCat)
            {
                if (lastCat.isNotEmpty()) menu.addSubMenu (lastCat, sub);
                sub.clear();
                lastCat = cat;
            }
            sub.addItem (i + 1, leaf, true, i == param->getIndex());
        }
        if (lastCat.isNotEmpty()) menu.addSubMenu (lastCat, sub);

        menu.setLookAndFeel (&getLookAndFeel());
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (this)
                                .withMinimumWidth (getWidth()),
                            [this] (int result)
                            {
                                if (result <= 0 || param == nullptr) return;
                                param->beginChangeGesture();
                                param->setValueNotifyingHost (
                                    param->convertTo0to1 ((float) (result - 1)));
                                param->endChangeGesture();
                                refresh();
                                if (onChange) onChange();
                            });
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff050508));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (gw::kBtnEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        g.setColour (gw::kText);
        g.setFont (gw::barlow (12.0f, true, 0.10f));
        g.drawText (currentName(), getLocalBounds().reduced (14, 0).withTrimmedRight (22),
                    juce::Justification::centredLeft, true);

        const float cx = r.getRight() - 16.0f, cy = r.getCentreY();
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.5f, cy - 2.0f);
        chevron.lineTo (cx, cy + 3.0f);
        chevron.lineTo (cx + 4.5f, cy - 2.0f);
        g.setColour (gw::kGrey);
        g.strokePath (chevron, juce::PathStrokeType (1.6f));
    }

private:
    juce::AudioParameterChoice* param = nullptr;
    int idx = -1;
};

// ---------------------------------------------------------------------------
class DemoTransportButton : public juce::Component
{
public:
    std::function<void()> onToggle;

    void setPlaying (bool p) { if (p != playing) { playing = p; repaint(); } }

    void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const auto accent = playing ? gw::kRed : gw::kGreen;

        g.setColour (juce::Colour (0xff0a0a0e));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent.withAlpha (0.65f));
        g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 1.0f);
        g.setColour (gw::kBtnEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        const float cy = r.getCentreY(), gx = r.getX() + 22.0f;
        g.setColour (accent);
        if (playing)
            g.fillRect (gx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        else
        {
            juce::Path tri;
            tri.addTriangle (gx - 4.5f, cy - 6.5f, gx - 4.5f, cy + 6.5f, gx + 6.5f, cy);
            g.fillPath (tri);
        }

        g.setColour (gw::kText);
        g.setFont (gw::barlow (11.5f, true, 0.16f));
        g.drawText (playing ? "STOP" : "START",
                    getLocalBounds().withTrimmedLeft (40), juce::Justification::centredLeft);
    }

private:
    bool playing = false;
};

// ---------------------------------------------------------------------------
class DemoPanel : public juce::Component
{
public:
    juce::Slider* volKnob = nullptr;
    bool   playing     = false;
    double clipSeconds = 0.0;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (gw::kPanelBg);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (gw::kHairline);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (gw::kText);
        g.setFont (gw::barlow (11.5f, true, 0.16f));
        g.drawText (juce::String::fromUTF8 ("DEMO PLAYER \xc2\xb7 STRAIGHT INTO THE PEDAL"),
                    14, 10, 520, 16, juce::Justification::centredLeft);

        g.setColour (gw::kDim);
        g.setFont (gw::mono (9.0f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("27 clips baked into the plugin \xc2\xb7 loops until you stop it \xc2\xb7 stays live while the cover is open"),
                    14, 28, 760, 12, juce::Justification::centredLeft);

        g.setColour (gw::kHairline);
        g.fillRect (14, 46, 1008, 1);

        {   // running LED
            auto led = juce::Rectangle<float> (628.0f, 68.0f, 11.0f, 11.0f);
            const auto c = playing ? gw::kGreen : gw::kChipOff;
            if (playing)
            {
                g.setColour (c.withAlpha (0.35f));
                g.fillEllipse (led.expanded (5.0f));
            }
            g.setColour (juce::Colour (0xff0a0c10).interpolatedWith (c, playing ? 1.0f : 0.55f));
            g.fillEllipse (led);
        }
        g.setColour (playing ? gw::kGreen : gw::kDim2);
        g.setFont (gw::mono (10.0f, 500));
        g.drawText (playing ? "LOOPING" : "STOPPED", 648, 67, 140, 14,
                    juce::Justification::centredLeft);

        if (clipSeconds > 0.0)
        {
            g.setColour (gw::kGrey);
            g.setFont (gw::mono (9.0f, 400));
            g.drawText (juce::String (clipSeconds, 1) + " s loop", 648, 84, 140, 12,
                        juce::Justification::centredLeft);
        }

        g.setColour (gw::kGrey);
        g.setFont (gw::mono (9.0f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("Clip and level save with the preset \xc2\xb7 start/stop does not"),
                    14, 100, 470, 12, juce::Justification::centredLeft);

        g.setColour (gw::kDim);
        g.setFont (gw::barlow (9.5f, true, 0.16f));
        g.drawText ("DEMO LEVEL", 888, 30, 140, 12, juce::Justification::centred);

        if (volKnob != nullptr)
        {
            const double dv = volKnob->getValue();
            juce::String v = (dv > 0.0 ? "+" : "") + juce::String (dv, 1);
            v = v.replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")) + " dB";
            g.setColour (gw::kText);
            g.setFont (gw::mono (12.0f, 400));
            g.drawText (v, 888, 122, 140, 14, juce::Justification::centred);
        }
    }
};

// ---------------------------------------------------------------------------
// "Where the Fuzz Meets the Funk" — chromatic-aberration name with the
// occasional horizontal tear. v0.35: the brand/version line below it is gone
// (version now lives in the standalone's Scale and Feedback window) and the
// name grew to fill the freed space.
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
        g.setFont (gw::barlow (40.0f, true, 0.01f));
        const bool moving = std::fabs (tear) > 0.1f;

        auto draw = [&] (juce::Rectangle<int> clip, float off, int dy, int chrom)
        {
            g.saveState();
            g.reduceClipRegion (clip);
            auto a = getLocalBounds().translated ((int) off, dy);
            g.setColour (juce::Colour (0xffff00be).withAlpha (0.85f));
            g.drawText (t, a.translated (chrom, 0), juce::Justification::centredRight);
            g.setColour (gw::kCyan.withAlpha (0.85f));
            g.drawText (t, a.translated (-chrom, 0), juce::Justification::centredRight);
            g.setColour (gw::kText);
            g.drawText (t, a, juce::Justification::centredRight);
            g.restoreState();
        };
        const auto r = getLocalBounds();

        if (! moving)
        {
            draw (r, 0.0f, 0, 3);
            return;
        }

        // v0.35: a properly glitchy jiggle — smear trails, four slices torn
        // in different directions with jumping RGB split, plus dropouts
        auto& rr = juce::Random::getSystemRandom();

        for (int tr = 4; tr >= 1; --tr)   // the smear
        {
            g.setColour (gw::kText.withAlpha (0.05f + 0.20f / (float) tr));
            g.drawText (t, getLocalBounds().translated (
                            (int) (tear * (1.0f + 0.9f * (float) tr)), 0),
                        juce::Justification::centredRight);
        }

        const float mult[4] = { 1.0f,
                                -0.8f - rr.nextFloat() * 0.6f,   // slice 2 rips the other way
                                1.7f + rr.nextFloat() * 0.8f,
                                1.2f };
        const int sh = r.getHeight() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto slice = r.withTrimmedTop (i * sh).withHeight (i == 3 ? r.getHeight() - 3 * sh : sh);
            draw (slice,
                  tear * mult[i] + (float) rr.nextInt (7) - 3.0f,
                  rr.nextInt (5) - 2,
                  3 + rr.nextInt (7));
        }

        for (int d = 0; d < 3; ++d)       // dropout slivers through the text
        {
            g.setColour (juce::Colours::black.withAlpha (0.9f));
            g.fillRect (r.getX() + rr.nextInt (juce::jmax (1, r.getWidth() - 200)),
                        r.getY() + rr.nextInt (juce::jmax (1, r.getHeight() - 4)),
                        60 + rr.nextInt (180), 2 + rr.nextInt (4));
        }
        for (int c = 0; c < 2; ++c)       // stray confetti
        {
            g.setColour (juce::Colour::fromHSV (rr.nextFloat(), 0.95f, 1.0f, 0.85f));
            g.fillRect (r.getX() + rr.nextInt (juce::jmax (1, r.getWidth() - 30)),
                        r.getY() + rr.nextInt (juce::jmax (1, r.getHeight() - 8)),
                        8 + rr.nextInt (24), 3 + rr.nextInt (6));
        }
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

    // set by the editor: grabs a snapshot of the whole face for the smears
    std::function<juce::Image()> grabFace;

    // v0.35 test-panel triggers: fire any glitch immediately (same visuals
    // the schedule produces — visual only, audio untouched)
    void triggerBars()     { pendLight = true; }
    void triggerMajor()    { pendMajor = true; }
    void triggerSmear()    { pendSmall = true; }
    void triggerMed()      { pendMed = true; }

    // called from the editor timer; nowMs is a steadily increasing clock
    void tick (double nowMs)
    {
        if (t0 <= 0.0) t0 = nowMs;
        const double t = nowMs - t0;

        if (pendLight) { pendLight = false; manLight = t; }
        if (pendMajor) { pendMajor = false; manMajor = t; }
        if (pendSmall) { pendSmall = false; manSmall = t; }
        if (pendMed)   { pendMed   = false; manMed   = t; }

        // 7 s scan sweep
        const float newY = (float) std::fmod (t / 7000.0, 1.0) * 780.0f - 140.0f;
        if (std::fabs (newY - scanY) > 0.8f)
        {
            const auto oldR = juce::Rectangle<int> (0, (int) scanY - 1, getWidth(), 143);
            scanY = newY;
            repaint (oldR.getUnion ({ 0, (int) scanY - 1, getWidth(), 143 }));
        }

        // ---- digital smears (visual ONLY, audio untouched) -----------------
        float target = 0.0f;
        bool  wantSmear = false;
        {
            // small/medium tear RIGHT — every 6:06.006 (Jason), 550 ms
            double p = std::fmod (t, 366006.0) - 365456.0;
            if (manSmall >= 0.0)
            {
                const double q = t - manSmall;
                if (q < 550.0) p = q; else manSmall = -1.0;
            }
            if (p >= 0.0 && p < 550.0)
            {
                wantSmear = true;
                target = 95.0f * std::pow ((float) (p / 550.0), 1.7f)
                       + (float) (((int) (t / 40.0) * 7919) % 13) - 6.0f;
            }
        }
        smearOff = target;

        // light burst: every 3:33 (or triggered), ~0.36 s of flickering bars
        double lw = -1.0;
        {
            const double lp = std::fmod (t, 213000.0) - 212640.0;
            if (lp >= 0.0) lw = lp;
        }
        if (manLight >= 0.0)
        {
            const double q = t - manLight;
            if (q < 360.0) lw = q; else manLight = -1.0;
        }
        const bool light = lw >= 0.0;
        if (light != lightOn || (light && (int) (lw / 60.0) != lightFrame))
        {
            lightOn = light;
            lightFrame = (int) (lw / 60.0);
            repaint (0, 205, getWidth(), 20);
            repaint (0, 410, getWidth(), 16);
        }

        // major glitch v2 — every 33:33 (or triggered), 1.3 s: datamosh
        // ripple -> rainbow pixel-sort melt -> comb/colour-band tear ->
        // liquid hue-wash, then snap back (Jason's reference art)
        double mw = -1.0;
        {
            const double mp = std::fmod (t, 2013000.0) - 2011700.0;
            if (mp >= 0.0 && mp < 1300.0) mw = mp;
        }
        if (manMajor >= 0.0)
        {
            const double q = t - manMajor;
            if (q < 1300.0) mw = q; else manMajor = -1.0;
        }
        const bool major = mw >= 0.0;
        if (major != majorOn || (major && (int) (mw / 70.0) != majorFrame))
        {
            majorOn = major;
            majorFrame = (int) (mw / 70.0);
            repaint();
        }

        // Med Glitch — every 11:11 (or triggered), 325 ms: the same two-act
        // cut as before, played at double speed (Jason: literally half the
        // time), from the major's reference art
        double dw = -1.0;
        {
            const double dp = std::fmod (t, 671000.0) - 670675.0;
            if (dp >= 0.0 && dp < 325.0) dw = dp;
        }
        if (manMed >= 0.0)
        {
            const double q = t - manMed;
            if (q < 325.0) dw = q; else manMed = -1.0;
        }
        const bool med = dw >= 0.0;
        if (med != medOn || (med && (int) (dw / 35.0) != medFrame))
        {
            medOn = med;
            medFrame = (int) (dw / 35.0);
            repaint();
        }

        // shared face snapshot for smears + major + med
        const bool wantFace = wantSmear || major || med;
        if (wantFace && ! snap.isValid() && grabFace)
        {
            snapping = true;
            snap = grabFace();
            snapping = false;
        }
        if (! wantFace && snap.isValid())
        {
            snap = juce::Image();
            repaint();
        }
        smearing = wantSmear && snap.isValid();
        if (smearing)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (snapping)
            return;                      // never paint into our own snapshot

        if (majorOn && snap.isValid())   // the major glitch owns the frame
        {
            paintMajor (g);
            return;
        }

        if (medOn && snap.isValid())     // ...as does the med glitch
        {
            paintMed (g);
            return;
        }

        if (smearing && snap.isValid())  // the smear replaces the whole face
        {
            paintSmear (g);
            return;
        }

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
    }

private:
    // deterministic per-frame noise so each strobe frame mutates
    static const juce::Colour* glitchPal()
    {
        static const juce::Colour pal[7] = {
            juce::Colour (0xffff00ff), juce::Colour (0xff00ff44), juce::Colour (0xff00eaff),
            juce::Colour (0xffffe600), juce::Colour (0xffff2222), juce::Colour (0xff2b6bff),
            juce::Colour (0xffff8c00) };
        return pal;
    }

    juce::uint32 rnd (int salt) const
    {
        auto h = (juce::uint32) (fxFrame + 1) * 2246822519u
               ^ (juce::uint32) (salt + 1) * 2654435761u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return h;
    }
    float rf (int salt) const { return (float) (rnd (salt) & 0xffff) / 65535.0f; }

    void paintMajor (juce::Graphics& g)
    {
        fxFrame = majorFrame;
        const int W = getWidth(), H = getHeight();
        const juce::Colour* pal = glitchPal();

        g.fillAll (juce::Colours::black);
        const int f = majorFrame;   // ~70 ms per frame, 0..18

        if (f < 5)
        {
            // PHASE 1 — datamosh: wavy row displacement + corrupted blocks
            const float amp = 18.0f + 60.0f * rf (3);
            const float k   = 0.02f + 0.05f * rf (4);
            const float ph  = rf (5) * 6.283f;
            for (int y = 0; y < H; y += 4)
            {
                const int dx = (int) (amp * std::sin (k * (float) y + ph)
                                    + 0.4f * amp * std::sin (2.6f * k * (float) y - ph));
                g.drawImage (snap, dx, y, W, 4, 0, y, W, 4);
            }
        }
        else if (f < 11)
        {
            // PHASE 2 — rainbow pixel-sort melt: columns drip downward,
            // trails stretching longer every frame (video A)
            const float growth = (float) (f - 4) / 6.0f;   // 0..1
            for (int x = 0, i = 0; x < W; x += 6, ++i)
            {
                const float v  = rf (100 + (i % 97));
                const int drop = (int) (v * v * 260.0f * growth);
                const int head = 40 + (int) (rf (140 + (i % 89)) * 120.0f);
                // the un-melted top of the column
                g.drawImage (snap, x, 0, 6, head, x, 0, 6, head);
                // the drip: a thin slice of the column stretched downward
                g.drawImage (snap, x, head, 6, drop + (H - head), x, head, 6,
                             juce::jmax (8, (H - head) / 3));
                // rainbow tint per streak
                const float hue = std::fmod ((float) i * 0.021f + (float) f * 0.06f, 1.0f);
                g.setColour (juce::Colour::fromHSV (hue, 0.9f, 1.0f, 0.18f + 0.14f * v));
                g.fillRect (x, head - 8, 6, H - head + 8);
            }
        }
        else if (f < 16)
        {
            // PHASE 3 — vertical comb + saturated colour bands + black
            // diagonal tears (still A)
            for (int x = 0, i = 0; x < W; x += 6, ++i)
            {
                const float v = rf (200 + (i % 61));
                const int dy = (int) ((v - 0.5f) * (60.0f + 180.0f * rf (2)));
                g.drawImage (snap, x, dy, 6, H, x, 0, 6, H);
            }
            int y = 0;
            for (int b = 0; y < H; ++b)
            {
                const int bh = 26 + (int) (rf (300 + b) * 90.0f);
                g.setColour (pal[rnd (320 + b) % 7].withAlpha (0.16f + 0.22f * rf (340 + b)));
                g.fillRect (0, y, W, bh);
                y += bh;
            }
            for (int d = 0; d < 4; ++d)
            {
                juce::Path p;
                const float x0 = rf (400 + d) * (float) W;
                const float w0 = 18.0f + rf (420 + d) * 46.0f;
                p.addQuadrilateral (x0, 0.0f, x0 + w0, 0.0f,
                                    x0 + w0 - 240.0f, (float) H, x0 - 240.0f, (float) H);
                g.setColour (juce::Colours::black.withAlpha (0.85f));
                g.fillPath (p);
            }
        }
        else
        {
            // PHASE 4 — liquid psychedelic hue-wash (video B), then snap back
            const float amp = 50.0f + 40.0f * rf (7);
            for (int y = 0; y < H; y += 4)
            {
                const int dx = (int) (amp * std::sin (0.012f * (float) y + (float) f)
                                    + 24.0f * std::sin (0.05f * (float) y - (float) f * 1.7f));
                g.drawImage (snap, dx, y, W, 4, 0, y, W, 4);
            }
            for (int b = 0; b < 7; ++b)
            {
                const float cx = rf (500 + b) * (float) W, cy = rf (520 + b) * (float) H;
                const float rr = 140.0f + rf (540 + b) * 260.0f;
                juce::ColourGradient grad (
                    juce::Colour::fromHSV (rf (560 + b), 0.95f, 1.0f, 0.30f), cx, cy,
                    juce::Colours::transparentBlack, cx + rr, cy + rr, true);
                g.setGradientFill (grad);
                g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
            }
        }

        // corrupted macroblocks + solid confetti, all phases
        const int nBlocks = 8 + (int) (rf (6) * 8.0f);
        for (int b = 0; b < nBlocks; ++b)
        {
            const int bw = 28 + (int) (rf (600 + b) * 80.0f);
            const int bh = 18 + (int) (rf (620 + b) * 56.0f);
            const int dx = (int) (rf (640 + b) * (float) (W - bw));
            const int dy = (int) (rf (660 + b) * (float) (H - bh));
            if ((rnd (680 + b) & 3) == 0)
            {
                g.setColour (pal[rnd (700 + b) % 7].withAlpha (0.85f));
                g.fillRect (dx, dy, bw, bh);
            }
            else
            {
                const int sx = (int) (rf (720 + b) * (float) (W - bw));
                const int sy = (int) (rf (740 + b) * (float) (H - bh));
                g.drawImage (snap, dx, dy, bw, bh, sx, sy, bw, bh);
            }
        }

        if (f == 5 || f == 11 || f == 16)   // flash on phase changes
        {
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillAll();
        }
    }

    // Med Glitch: 650 ms two-act cut from the same reference art — comb
    // strips with sparse rainbow drips, then liquid ripple + hue washes
    void paintMed (juce::Graphics& g)
    {
        fxFrame = medFrame + 991;   // its own noise stream, distinct from major
        const int W = getWidth(), H = getHeight();
        const juce::Colour* pal = glitchPal();

        g.fillAll (juce::Colours::black);
        const int f = medFrame;   // ~35 ms per frame, 0..9

        if (f < 5)
        {
            // act 1 — vertical comb with sparse rainbow melt streaks
            for (int x = 0, i = 0; x < W; x += 8, ++i)
            {
                const float v = rf (900 + (i % 71));
                const int dy = (int) ((v - 0.5f) * (40.0f + 120.0f * rf (12)));
                g.drawImage (snap, x, dy, 8, H, x, 0, 8, H);
                if ((rnd (930 + i) & 7) == 0)
                {
                    const int head = 60 + (int) (rf (950 + i) * 180.0f);
                    g.drawImage (snap, x, head, 8, H - head, x, head, 8,
                                 juce::jmax (8, (H - head) / 4));
                    const float hue = std::fmod ((float) i * 0.037f + (float) f * 0.09f, 1.0f);
                    g.setColour (juce::Colour::fromHSV (hue, 0.9f, 1.0f, 0.30f));
                    g.fillRect (x, head, 8, H - head);
                }
            }
        }
        else
        {
            // act 2 — liquid ripple + saturated hue washes, then snap back
            const float amp = 30.0f + 34.0f * rf (13);
            for (int y = 0; y < H; y += 4)
            {
                const int dx = (int) (amp * std::sin (0.018f * (float) y + (float) f * 1.3f)
                                    + 16.0f * std::sin (0.06f * (float) y - (float) f));
                g.drawImage (snap, dx, y, W, 4, 0, y, W, 4);
            }
            for (int b = 0; b < 4; ++b)
            {
                const float cx = rf (960 + b) * (float) W, cy = rf (970 + b) * (float) H;
                const float rr = 110.0f + rf (980 + b) * 190.0f;
                juce::ColourGradient grad (
                    juce::Colour::fromHSV (rf (990 + b), 0.95f, 1.0f, 0.28f), cx, cy,
                    juce::Colours::transparentBlack, cx + rr, cy + rr, true);
                g.setGradientFill (grad);
                g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
            }
        }

        // corrupted macroblocks + confetti, both acts
        for (int b = 0; b < 7; ++b)
        {
            const int bw = 24 + (int) (rf (860 + b) * 70.0f);
            const int bh = 16 + (int) (rf (870 + b) * 48.0f);
            const int dx = (int) (rf (880 + b) * (float) (W - bw));
            const int dy = (int) (rf (890 + b) * (float) (H - bh));
            if ((rnd (895 + b) & 3) == 0)
            {
                g.setColour (pal[rnd (897 + b) % 7].withAlpha (0.85f));
                g.fillRect (dx, dy, bw, bh);
            }
            else
                g.drawImage (snap, dx, dy, bw, bh,
                             (int) (rf (898 + b) * (float) (W - bw)),
                             (int) (rf (899 + b) * (float) (H - bh)), bw, bh);
        }

        if (f == 5)   // flash on the act change
        {
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillAll();
        }
    }

    void paintSmear (juce::Graphics& g)
    {
        g.fillAll (juce::Colours::black);
        const int W = getWidth(), H = getHeight();
        const int bands = 16;
        const float bh = (float) H / (float) bands;
        const int trails = juce::jlimit (2, 5, (int) (std::fabs (smearOff) / 60.0f) + 2);
        for (int b = 0; b < bands; ++b)
        {
            // deterministic per-band factor 0.55 .. 1.45 -> ragged tear edge
            const auto h = (juce::uint32) (b + 1) * 2654435761u;
            const float k = 0.55f + 0.9f * (float) ((h >> 16) & 1023) / 1023.0f;
            const float off = smearOff * k;
            const int sy = (int) (b * bh), sh = (int) bh + 1;
            for (int tr = trails; tr >= 1; --tr)   // smear ghosts
            {
                g.setOpacity (0.14f);
                g.drawImage (snap, (int) (off * (float) tr / (float) (trails + 1)), sy, W, sh,
                             0, sy, W, sh);
            }
            g.setOpacity (1.0f);
            g.drawImage (snap, (int) off, sy, W, sh, 0, sy, W, sh);
        }
    }

    double t0 = -1.0;
    float scanY = -140.0f;
    bool lightOn = false, majorOn = false, medOn = false;
    int lightFrame = 0, majorFrame = 0, medFrame = 0, fxFrame = 0;
    juce::Image snap;
    float smearOff = 0.0f;
    bool smearing = false, snapping = false;
    // manual trigger state (v0.35 test panel)
    bool pendLight = false, pendMajor = false, pendSmall = false, pendMed = false;
    double manLight = -1.0, manMajor = -1.0, manSmall = -1.0, manMed = -1.0;
};

// ---------------------------------------------------------------------------
// v0.37: one-time "right-click to hold" callout, replacing the removed
// INS/DEL keyboard emulation (see docs/MODS.md v0.37 for why). Big centred
// text + two arrows pointing at the TAP/BYPASS stomps; holds still, then
// travels down and shrinks onto the stomps, then fades. A click anywhere in
// the plugin cuts the fade to one second from wherever it currently is,
// continuing to travel first if it was still mid-move when clicked.
// ---------------------------------------------------------------------------
class HoldHintOverlay : public juce::Component
{
public:
    HoldHintOverlay() { setInterceptsMouseClicks (false, false); }

    // called once from resized(), with the two stomps' current centres
    void setTargets (juce::Point<float> tapCentre, juce::Point<float> bypassCentre)
    {
        targetA = tapCentre;
        targetB = bypassCentre;
        textEnd = { (targetA.x + targetB.x) * 0.5f, juce::jmin (targetA.y, targetB.y) - 46.0f };
    }

    // called every frame from the editor's timerCallback, same clock as GlitchFx::tick
    void tick (double nowMs)
    {
        if (done) return;
        if (t0 <= 0.0) t0 = nowMs;
        const double t = (nowMs - t0) / 1000.0;   // seconds since first shown

        double posT = t;
        double opacity;

        if (clickT < 0.0)
        {
            opacity = opacityAt (t);
        }
        else
        {
            const double dt = t - clickT;
            if (dt >= 1.0) { done = true; repaint(); return; }
            opacity = juce::jmap (dt, 0.0, 1.0, (double) opacityAtClick, 0.0);
            // still moving at the moment of the click -> keep moving during the fade
            posT = (clickT >= 2.0 && clickT < 7.0) ? juce::jmin (t, 7.0) : clickT;
        }

        const auto pose = poseAt (posT);
        curPos = pose.first;
        curSize = pose.second;
        curOpacity = (float) opacity;
        repaint();
    }

    // called from the editor's mouseDown (via addMouseListener) for a click anywhere
    void registerClick (double nowMs)
    {
        if (done || clickT >= 0.0) return;
        if (t0 <= 0.0) t0 = nowMs;
        clickT = (nowMs - t0) / 1000.0;
        opacityAtClick = (float) opacityAt (clickT);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        registerClick (juce::Time::getMillisecondCounterHiRes());
    }

    void paint (juce::Graphics& g) override
    {
        if (done || curOpacity <= 0.001f) return;

        // text box tracks curPos as its CENTRE -- this is what actually
        // makes the callout travel; sized from the real glyph width so it
        // never clips as curSize shrinks from 60 down to 15
        const auto font = gw::barlow (curSize, true, 0.02f);
        const float tw = gw::textW (font, kMessage);
        const juce::Rectangle<float> textBox (curPos.x - tw * 0.5f - 6.0f,
                                              curPos.y - curSize * 0.65f,
                                              tw + 12.0f, curSize * 1.3f);

        g.setColour (gw::kText.withAlpha (curOpacity));

        // arrow tails hang off the bottom of the text box; the heads stay
        // pinned on the stomps
        const float thick = juce::jmap (curSize, 15.0f, 60.0f, 2.0f, 5.0f);
        const float headW = juce::jmap (curSize, 15.0f, 60.0f, 10.0f, 26.0f);
        const float headL = juce::jmap (curSize, 15.0f, 60.0f, 12.0f, 30.0f);
        const juce::Point<float> tail { curPos.x, textBox.getBottom() };

        auto arrow = [&] (juce::Point<float> head)
        {
            juce::Path p;
            p.addArrow (juce::Line<float> (tail, head), thick, headW, headL);
            g.fillPath (p);
        };
        arrow (targetA);
        arrow (targetB);

        g.setFont (font);
        g.drawText (kMessage, textBox, juce::Justification::centred, false);
    }

private:
    static constexpr const char* kMessage = "RIGHT-CLICK TO HOLD";
    static double smoothstep (double u) { u = juce::jlimit (0.0, 1.0, u); return u * u * (3.0 - 2.0 * u); }

    double opacityAt (double t) const
    {
        if (t < 7.0) return 1.0;
        if (t < 12.0) return 1.0 - (t - 7.0) / 5.0;
        return 0.0;
    }

    std::pair<juce::Point<float>, float> poseAt (double t) const
    {
        const juce::Point<float> centre { 530.0f, 320.0f };
        if (t < 2.0) return { centre, 60.0f };
        const double u = smoothstep ((t - 2.0) / 5.0);
        const auto pos = centre + (textEnd - centre) * (float) u;
        const float size = 60.0f + (15.0f - 60.0f) * (float) u;
        return { pos, size };
    }

    juce::Point<float> targetA, targetB, textEnd;
    juce::Point<float> curPos { 530.0f, 320.0f };
    float curSize = 60.0f;
    float curOpacity = 1.0f;
    double t0 = 0.0, clickT = -1.0;
    float opacityAtClick = 1.0f;
    bool done = false;
};

// ---------------------------------------------------------------------------
class ScaleFeedbackWindow;   // v0.37: opened from settingsBtn, defined in ScaleFeedback.h

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
    //   Y (TAP held):             Gain   Res     Vol      Shape  Shape  Mode
    //   Z (BYPASS held):          L1 Dep L2 Dep  DrvRng   Target Target Target
    //   A (BOTH held, secret):   Mix -> STARVE   Freq -> env THRESHOLD
    //                            LPF -> env RATIO   Gain -> env SHAPE
    //                            (Rate 1/2 still dead.)
    //
    // v0.37: keyboard emulation (INS/DEL) is removed. A layer can only be
    // held by pressing and holding a stomp with the mouse, or right-click
    // to latch it held. See docs/MODS.md v0.37.
    bool tapStompDown() const;      // TAP stomp held, or latched
    bool bypassStompDown() const;   // BYPASS stomp held, or latched
    int  computeLayer() const;      // 0 = X, 1 = Y, 2 = Z, 3 = A (secret shaping)
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

    // v0.37: in-plugin Scale/Feedback access (the standalone-only title-bar
    // Options menu never existed for VST3/plugin builds) + the one-time
    // right-click hold callout that replaces INS/DEL
    juce::TextButton settingsBtn { "SETTINGS" };
    std::unique_ptr<ScaleFeedbackWindow> scaleFeedbackWindow;
    HoldHintOverlay holdHint;

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
    PcbSwitchRow  jfetRow;                        // v0.41 / rev 7: the only one left
    PcbSwitchRow  c41Row, c42Row;                 // v0.39 the two DNP LM567 pads
    RailReadout   railRead;                       // v0.41 V567 = 7.5 V, read-only
    InternalStrip strip;
    CoverDim   coverDim;
    CoverPanel cover;
    bool gateOpen = false;

    // v0.40 demo player strip, below the pedal face
    DemoPanel           demoPanel;
    DemoSelector        demoSel;
    DemoTransportButton demoBtn;
    juce::Slider        demoVolKnob;
    std::unique_ptr<SliderAttachment> demoVolAtt;

    // hints (v0.39: ship them ON)
    bool showHints = true;
    juce::Label hintChips, hintLayers, hintLfo1, hintLfo2, hintStomp1, hintStomp2;

    // decoration
    GlitchFx fx;

    // v0.35: window scale, driven by the standalone's Scale and Feedback window
    float appliedScale = 1.0f;

    // v0.35: title jiggle — fires ~every 1:11 +/- 12 s of random slack, and
    // each jiggle's speed shifts +/- 13 %
    double nextTearAt = 0.0, tearStart = -1.0;
    float  tearSpeed  = 1.0f;
    juce::Random rng;

    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchwaveAudioProcessorEditor)
};
