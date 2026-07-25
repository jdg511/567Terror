cmake_minimum_required(VERSION 3.22)

project(Glitchwave567 VERSION 0.34.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE (downloaded automatically on first configure) ---------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.14.tar.gz
)
FetchContent_MakeAvailable(JUCE)

# ---- Plugin ------------------------------------------------------------------
juce_add_plugin(Glitchwave567
    COMPANY_NAME "Illicit Apothecary"
    BUNDLE_ID "com.illicitapothecary.glitchwave567"
    PLUGIN_MANUFACTURER_CODE Jdgw
    PLUGIN_CODE Gw67
    FORMATS VST3 LV2 Standalone AU
    LV2URI "https://github.com/jdg511/567Terror"
    PRODUCT_NAME "Glitchwave 567"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Filter
)

# v0.34 art + typefaces (baked from the Design bundle in docs/ui/)
juce_add_binary_data(GwAssets SOURCES
    assets/bg_face.jpg
    assets/logo_fx.png
    assets/logo_plain.png
    assets/barlow_semibold.ttf
    assets/barlow_bold.ttf
    assets/plexmono_regular.ttf
    assets/plexmono_medium.ttf
    assets/plexmono_semibold.ttf
)
set_target_properties(GwAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(Glitchwave567 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_compile_definitions(Glitchwave567 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Glitchwave567
    PRIVATE
        GwAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
