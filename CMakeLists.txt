cmake_minimum_required(VERSION 3.22)
project(BasslineYF21 VERSION 0.1.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: link the C++ runtime statically so the .exe / .vst3 run without
# the Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# To build against a JUCE checkout you already have:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(BasslineYF21
    COMPANY_NAME "Decu"
    PLUGIN_MANUFACTURER_CODE Decu
    PLUGIN_CODE Yf21
    PRODUCT_NAME "Bassline YF-21"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BasslineYF21 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/PatchManager.cpp
    Source/PatchManager.h
    Source/PatternManager.cpp
    Source/PatternManager.h
    Source/FactoryPatches.h
    Source/FactoryPatterns.h
    Source/Pattern.h
    Source/Sequencer.h
    Source/Wavetables.h
    Source/Oscillator.h
    Source/Filter.h
    Source/Envelope.h
    Source/Lfo.h
    Source/Effects.h
    Source/AcidVoice.h
    Source/UI/Theme.cpp
    Source/UI/Theme.h
    Source/UI/Widgets.cpp
    Source/UI/Widgets.h
    Source/UI/SynthPanels.cpp
    Source/UI/SynthPanels.h
    Source/UI/SequencerPanel.cpp
    Source/UI/SequencerPanel.h)

# Embedded fonts (SIL OFL 1.1 - see Resources/Fonts)
juce_add_binary_data(YF21Fonts SOURCES
    Resources/Fonts/DecuDisplay-Bold.ttf
    Resources/Fonts/DecuDisplay-Black.ttf
    Resources/Fonts/ShareTechMono-Regular.ttf)

target_compile_definitions(BasslineYF21 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Linux: ALSA (default) plus JACK for the Standalone's audio device list.
# JACK is loaded at runtime if installed, so it's not a hard dependency.
option(YF21_HEADLESS_TEST_BUILD "Build without ALSA/JACK and optional X11 extensions (CI/test machines only)" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(YF21_HEADLESS_TEST_BUILD)
        target_compile_definitions(BasslineYF21 PUBLIC JUCE_ALSA=0 JUCE_JACK=0
            JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
    else()
        target_compile_definitions(BasslineYF21 PUBLIC JUCE_ALSA=1 JUCE_JACK=1)
    endif()
endif()

target_link_libraries(BasslineYF21
    PRIVATE juce::juce_audio_utils
            juce::juce_dsp
            YF21Fonts
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

# Optional tests: plugin-level checks (sequencer, MIDI modes, recording, state) + editor screenshot.
#   cmake -B build -DYF21_BUILD_TESTS=ON && cmake --build build --target YF21Tests
option(YF21_BUILD_TESTS "Build the plugin test program" OFF)
if(YF21_BUILD_TESTS)
    juce_add_console_app(YF21Tests PRODUCT_NAME "YF21Tests")
    get_target_property(yf21_sources BasslineYF21 SOURCES)
    target_sources(YF21Tests PRIVATE Tests/plugin_tests.cpp ${yf21_sources})
    target_include_directories(YF21Tests PRIVATE Source)
    target_compile_definitions(YF21Tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(YF21_HEADLESS_TEST_BUILD)
        target_compile_definitions(YF21Tests PRIVATE JUCE_ALSA=0 JUCE_JACK=0
            JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
    endif()
    target_link_libraries(YF21Tests PRIVATE juce::juce_audio_utils juce::juce_dsp YF21Fonts
                                            juce::juce_recommended_config_flags)
endif()
if(YF21_BUILD_TESTS)
    juce_add_console_app(YF21HostTest PRODUCT_NAME "YF21HostTest")
    target_sources(YF21HostTest PRIVATE Tests/vst3_host_test.cpp)
    target_compile_definitions(YF21HostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(YF21_HEADLESS_TEST_BUILD)
        target_compile_definitions(YF21HostTest PRIVATE JUCE_ALSA=0 JUCE_JACK=0
            JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
    endif()
    target_link_libraries(YF21HostTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
