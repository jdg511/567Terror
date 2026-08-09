// ===========================================================================
// v0.35 custom standalone app: identical to JUCE's stock StandaloneFilterApp
// except the Options menu gains "Scale and Feedback..." right under
// "Audio/MIDI Settings..." (Jason's spec). Built only into the Standalone
// target via JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1.
// ===========================================================================

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "PluginProcessor.h"
#include "ScaleFeedback.h"

namespace
{

class GwFilterWindow final : public juce::StandaloneFilterWindow
{
public:
    GwFilterWindow (const juce::String& title, juce::Colour bg,
                    std::unique_ptr<juce::StandalonePluginHolder> holder)
        : juce::StandaloneFilterWindow (title, bg, std::move (holder))
    {
        // hide the stock Options button; ours shows the extended menu
        for (int i = getNumChildComponents(); --i >= 0;)
            if (auto* b = dynamic_cast<juce::TextButton*> (getChildComponent (i)))
                if (b->getButtonText() == "Options")
                    b->setVisible (false);

        gwOptions.setTriggeredOnMouseDown (true);
        gwOptions.onClick = [this] { showMenu(); };
        juce::Component::addAndMakeVisible (gwOptions);
        resized();
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        gwOptions.setBounds (8, 6, 60, getTitleBarHeight() - 8);
    }

private:
    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, TRANS ("Audio/MIDI Settings..."));
        m.addItem (5, "Scale and Feedback...");
        m.addSeparator();
        m.addItem (2, TRANS ("Save current state..."));
        m.addItem (3, TRANS ("Load a saved state..."));
        m.addSeparator();
        m.addItem (4, TRANS ("Reset to default state"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (gwOptions),
                         [safe = juce::Component::SafePointer<GwFilterWindow> (this)] (int r)
                         {
                             if (safe == nullptr || r == 0)
                                 return;
                             if (r == 5)
                                 safe->openScaleFeedback();
                             else
                                 safe->handleMenuResult (r);
                         });
    }

    void openScaleFeedback()
    {
        if (auto* gp = dynamic_cast<GlitchwaveAudioProcessor*> (pluginHolder->processor.get()))
        {
            if (dialog == nullptr)
                dialog = std::make_unique<ScaleFeedbackWindow> (*gp);
            dialog->setVisible (true);
            dialog->toFront (true);
        }
    }

    juce::TextButton gwOptions { "Options" };
    std::unique_ptr<ScaleFeedbackWindow> dialog;
};

//==============================================================================
class GwStandaloneApp final : public juce::JUCEApplication
{
public:
    GwStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    juce::StandaloneFilterWindow* createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new GwFilterWindow (getApplicationName(),
                                   juce::LookAndFeel::getDefaultLookAndFeel()
                                       .findColour (juce::ResizableWindow::backgroundColourId),
                                   createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(),
                                                               false, juce::String{}, nullptr,
                                                               channelConfig, false);
    }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
            mainWindow->setVisible (true);
        else
            pluginHolder = createPluginHolder();
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace

juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new GwStandaloneApp();
}
