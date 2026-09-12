cmake_minimum_required(VERSION 3.22)

project(Glitchwave567 VERSION 0.42.0 LANGUAGES C CXX)

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
    FORMATS VST3 Standalone
    LV2URI "https://github.com/jdg511/567Terror"
    PRODUCT_NAME "Where The Fuzz Meets The Funk"
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

# v0.40 demo clips, embedded so the player works with no files on disk.
# Mono Ogg Vorbis q2 (about 1.5 MB for all 27). ORDER IS THE DROPDOWN ORDER
# and is also the saved parameter index, so only ever append to this list.
# HEADER_NAME matters: without it this target would also generate a
# "BinaryData.h" and collide with GwAssets' one.
juce_add_binary_data(GwDemos HEADER_NAME DemoData.h NAMESPACE DemoData SOURCES
    assets/demos/arpeggio-quick-clean.ogg
    assets/demos/arpeggio-deluxe-clean.ogg
    assets/demos/arpeggio-iconic-cleanish.ogg
    assets/demos/arpeggio-warm-crunch.ogg
    assets/demos/arpeggio-dirty-punk.ogg
    assets/demos/rhythm-chords-fender-clean.ogg
    assets/demos/rhythm-chords-suhr-clean.ogg
    assets/demos/rhythm-chords-ac30-crunch.ogg
    assets/demos/power-chords-iconic-cleanish.ogg
    assets/demos/power-chords-plexi.ogg
    assets/demos/power-chords-punk-rock-rhythm.ogg
    assets/demos/power-chords-classic-hi-gain.ogg
    assets/demos/solo-mesa.ogg
    assets/demos/solo-diezel.ogg
    assets/demos/solo-fortin.ogg
    assets/demos/metal-5150.ogg
    assets/demos/metal-blackstar.ogg
    assets/demos/metal-dualrec.ogg
    assets/demos/pick-bass-clean-bright.ogg
    assets/demos/pick-bass-bite.ogg
    assets/demos/pick-bass-growl.ogg
    assets/demos/pick-bass-rock-classic.ogg
    assets/demos/pick-bass-metalcore.ogg
    assets/demos/finger-bass-clean-bright.ogg
    assets/demos/finger-bass-nice-warm.ogg
    assets/demos/finger-bass-bite.ogg
    assets/demos/finger-bass-growl.ogg
)
set_target_properties(GwDemos PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(Glitchwave567 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

# v0.35: custom standalone app — adds "Scale and Feedback..." to the
# Options menu (standalone target only; other formats untouched)
target_compile_definitions(Glitchwave567_Standalone PRIVATE
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_sources(Glitchwave567_Standalone PRIVATE src/StandaloneApp.cpp)

target_compile_definitions(Glitchwave567 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Glitchwave567
    PRIVATE
        GwAssets
        GwDemos
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
