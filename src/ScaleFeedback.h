#pragma once

#include "PluginProcessor.h"
#include "PluginEditor.h"   // gw:: palette + fonts

// ===========================================================================
// v0.35 "Scale and Feedback" — opened from the standalone's Options menu.
// Top: UI scale picker (x0.5 / x1 / x1.5 / x2, applied live; every launch
// starts at x1) + the version number. Below: an optional feedback form.
// Only Name, Email and Zip are mandatory (red asterisks). Saved locally to
// Documents/Illicit Apothecary/ — nothing is ever sent anywhere.
// ===========================================================================
class ScaleFeedbackComponent : public juce::Component
{
public:
    explicit ScaleFeedbackComponent (WtfAudioProcessor& p) : proc (p)
    {
        auto style = [this] (juce::TextEditor& e, bool multiline)
        {
            e.setMultiLine (multiline, true);
            e.setReturnKeyStartsNewLine (multiline);
            e.setFont (gw::mono (13.0f, 400));
            e.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0a0a0e));
            e.setColour (juce::TextEditor::textColourId, gw::kText);
            e.setColour (juce::TextEditor::highlightColourId, gw::kYellow.withAlpha (0.35f));
            e.setColour (juce::TextEditor::outlineColourId, gw::kBtnEdge);
            e.setColour (juce::TextEditor::focusedOutlineColourId, gw::kYellow.withAlpha (0.6f));
            e.onTextChange = [this, &e]
            { e.setColour (juce::TextEditor::outlineColourId, gw::kBtnEdge); e.repaint(); };
            addAndMakeVisible (e);
        };
        for (auto* e : { &name, &email, &address, &state, &zip, &country, &phone })
            style (*e, false);
        style (feedback, true);

        auto radio = [this] (juce::ToggleButton& b, const juce::String& text)
        {
            b.setButtonText (text);
            b.setRadioGroupId (567);
            b.setColour (juce::ToggleButton::textColourId, gw::kText);
            b.setColour (juce::ToggleButton::tickColourId, gw::kGreen);
            b.setColour (juce::ToggleButton::tickDisabledColourId, gw::kBtnEdge);
            addAndMakeVisible (b);
        };
        radio (nSale, "Notify me when the real-world pedal goes up for sale");
        radio (nFree, "Only notify me when a new FREE plugin or real-world product is released");
        radio (nNone, "Don't notify me about anything at all");
        nNone.setToggleState (true, juce::dontSendNotification);

        saveBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0a0a0e));
        saveBtn.setColour (juce::TextButton::textColourOffId, gw::kText);
        saveBtn.onClick = [this] { doSave(); };
        addAndMakeVisible (saveBtn);

        setSize (720, 836);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 4; ++i)
            if (scaleBox (i).contains (e.getPosition()))
            {
                proc.uiScale.store (kScales[i], std::memory_order_relaxed);
                repaint();
                return;
            }
    }

    void resized() override
    {
        const int L = 24, W = getWidth() - 2 * L;
        int y = kFormTop;

        auto row = [&] (std::initializer_list<std::pair<juce::TextEditor*, int>> cells)
        {
            int x = L;
            for (auto& c : cells)
            {
                c.first->setBounds (x, y + 16, c.second, 26);
                x += c.second + 14;
            }
            y += 52;
        };
        const int half = (W - 14) / 2;
        row ({ { &name, half },  { &email, half } });
        row ({ { &address, W } });
        const int third = (W - 28) / 3;
        row ({ { &state, third }, { &zip, third }, { &country, W - 2 * third - 28 } });
        row ({ { &phone, half } });

        feedback.setBounds (L, y + 16, W, 172);   // ~10 text lines, very wide
        y += 16 + 172 + 12;

        nSale.setBounds (L, y, W, 22);
        nFree.setBounds (L, y + 24, W, 22);
        nNone.setBounds (L, y + 48, W, 22);
        y += 76;

        saveBtn.setBounds (L, y + 34, 140, 32);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        const int L = 24, W = getWidth() - 2 * L;

        // header + version
        g.setColour (gw::kText);
        auto ft = gw::barlow (22.0f, true, 0.06f);
        g.setFont (ft);
        const auto title = juce::String (JucePlugin_Name).toUpperCase();
        g.drawText (title, L, 18, 520, 26, juce::Justification::centredLeft);
        g.setColour (gw::kGrey);
        g.setFont (gw::mono (13.0f, 500));
        g.drawText ("v" JucePlugin_VersionString,
                    L + (int) gw::textW (ft, title) + 12, 24, 200, 18,
                    juce::Justification::centredLeft);
        g.setColour (gw::kDim2);
        g.setFont (gw::mono (10.0f, 400, 0.04f));
        g.drawText ("ILLICIT APOTHECARY", L, 46, 300, 12, juce::Justification::centredLeft);

        auto header = [&] (const juce::String& t, int yy)
        {
            g.setColour (gw::kDim);
            g.setFont (gw::barlow (9.5f, true, 0.16f));
            g.drawText (t, L, yy, 300, 12, juce::Justification::centredLeft);
            g.setColour (gw::kHairline);
            g.fillRect (L, yy + 16, W, 1);
        };

        // ---- UI SCALE ------------------------------------------------------
        header ("UI SCALE", 72);
        static const char* names[4] = { "\xc3\x97""0.5", "\xc3\x97""1", "\xc3\x97""1.5", "\xc3\x97""2" };
        const float cur = proc.uiScale.load (std::memory_order_relaxed);
        for (int i = 0; i < 4; ++i)
        {
            auto r = scaleBox (i).toFloat();
            const bool on = std::fabs (cur - kScales[i]) < 0.01f;
            if (on)
            {
                g.setColour (gw::kYellow.withAlpha (0.35f));
                g.fillRoundedRectangle (r.expanded (3.0f), 8.0f);
                g.setColour (gw::kYellow);
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (juce::Colours::black);
            }
            else
            {
                g.setColour (juce::Colour (0xff0a0a0e));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (gw::kBtnEdge);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (gw::kDim);
            }
            g.setFont (gw::mono (14.0f, 500));
            g.drawText (juce::String::fromUTF8 (names[i]), r, juce::Justification::centred);
        }
        g.setColour (gw::kGrey);
        g.setFont (gw::mono (9.0f, 400, 0.03f));
        g.drawText (juce::String::fromUTF8 ("applies instantly \xc2\xb7 every launch starts at \xc3\x97""1"),
                    L, 148, W, 12, juce::Justification::centredLeft);

        // ---- FEEDBACK ------------------------------------------------------
        header (juce::String::fromUTF8 ("FEEDBACK \xe2\x80\x94 OPTIONAL"), kFormTop - 26);

        auto caption = [&] (const juce::String& t, bool required, juce::Component& over)
        {
            auto f = gw::barlow (10.0f, true, 0.10f);
            g.setColour (gw::kDim);
            g.setFont (f);
            g.drawText (t, over.getX(), over.getY() - 15, 200, 13, juce::Justification::centredLeft);
            if (required)
            {
                g.setColour (gw::kRed);
                g.drawText ("*", over.getX() + (int) gw::textW (f, t) + 4, over.getY() - 15, 12, 13,
                            juce::Justification::centredLeft);
            }
        };
        caption ("NAME", true,  name);
        caption ("EMAIL", true, email);
        caption ("ADDRESS", false, address);
        caption ("STATE", false,  state);
        caption ("ZIP CODE", true, zip);
        caption ("COUNTRY", false, country);
        caption ("PHONE #", false, phone);
        caption ("PLUGIN FEEDBACK", false, feedback);

        // privacy note under the radios
        g.setColour (gw::kDim2);
        g.setFont (gw::mono (9.5f, 400, 0.02f));
        const int py = nNone.getBottom() + 8;
        g.drawText ("We will never, ever, even CONSIDER selling your info to anyone.",
                    L, py, W, 12, juce::Justification::centredLeft);
        g.drawText (juce::String::fromUTF8 ("It's a sad state of affairs that we even have to say that to our customers \xe2\x80\x94 but there it is."),
                    L, py + 14, W, 12, juce::Justification::centredLeft);

        // save status
        if (status.isNotEmpty())
        {
            g.setColour (statusIsError ? gw::kRed : gw::kGreen);
            g.setFont (gw::mono (10.0f, 400));
            g.drawText (status, saveBtn.getRight() + 14, saveBtn.getY(),
                        getWidth() - saveBtn.getRight() - 14 - L, saveBtn.getHeight(),
                        juce::Justification::centredLeft);
        }
    }

private:
    juce::Rectangle<int> scaleBox (int i) const
    {
        return { 24 + i * 110, 96, 96, 44 };
    }

    void doSave()
    {
        bool ok = true;
        auto need = [&] (juce::TextEditor& e, bool cond)
        {
            if (! cond)
            {
                e.setColour (juce::TextEditor::outlineColourId, gw::kRed);
                e.repaint();
                ok = false;
            }
        };
        need (name,  name.getText().trim().isNotEmpty());
        need (email, email.getText().contains ("@") && email.getText().trim().length() >= 5);
        need (zip,   zip.getText().trim().isNotEmpty());
        if (! ok)
        {
            status = "Please fill in the fields marked *";
            statusIsError = true;
            repaint();
            return;
        }

        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("Illicit Apothecary");
        dir.createDirectory();
        auto f = dir.getChildFile ("FuzzMeetsFunk_feedback_"
                     + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".txt");

        const juce::String notify =
            nSale.getToggleState() ? "Notify when the real-world pedal goes up for sale"
          : nFree.getToggleState() ? "Notify ONLY for new FREE plugins / real-world products"
                                   : "No notifications at all";
        juce::String out;
        out << JucePlugin_Name << " v" << JucePlugin_VersionString << " feedback\n"
            << "Date:     " << juce::Time::getCurrentTime().toString (true, true) << "\n"
            << "Name:     " << name.getText() << "\n"
            << "Email:    " << email.getText() << "\n"
            << "Address:  " << address.getText() << "\n"
            << "State:    " << state.getText() << "\n"
            << "Zip:      " << zip.getText() << "\n"
            << "Country:  " << country.getText() << "\n"
            << "Phone:    " << phone.getText() << "\n"
            << "Notify:   " << notify << "\n\n"
            << "Feedback:\n" << feedback.getText() << "\n";

        if (f.replaceWithText (out))
        {
            status = "Saved: " + f.getFullPathName();
            statusIsError = false;
        }
        else
        {
            status = "Could not write " + f.getFullPathName();
            statusIsError = true;
        }
        repaint();
    }

    static constexpr float kScales[4] = { 0.5f, 1.0f, 1.5f, 2.0f };
    static constexpr int   kFormTop   = 196;

    WtfAudioProcessor& proc;
    juce::TextEditor name, email, address, state, zip, country, phone, feedback;
    juce::ToggleButton nSale, nFree, nNone;
    juce::TextButton saveBtn { "SAVE" };
    juce::String status;
    bool statusIsError = false;
};

// ---------------------------------------------------------------------------
class ScaleFeedbackWindow : public juce::DocumentWindow
{
public:
    explicit ScaleFeedbackWindow (WtfAudioProcessor& p)
        : DocumentWindow ("Scale and Feedback", juce::Colours::black,
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ScaleFeedbackComponent (p), true);
        setResizable (false, false);
        centreWithSize (getContentComponent()->getWidth(),
                        getContentComponent()->getHeight());
        setVisible (true);
        toFront (true);
    }

    void closeButtonPressed() override { setVisible (false); }
};
