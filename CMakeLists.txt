cmake_minimum_required(VERSION 3.22)

# macOS: build universal binaries (Apple Silicon + Intel) unless the user overrides it.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()
endif()

project(LHSampleSynth VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LHSS_BUILD_TESTS "Build the unit test executable" ON)
option(LHSS_BUILD_STANDALONE "Also build a standalone application" ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt"
   OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/clap-juce-extensions/clap-libs/clap/include/clap/clap.h")
    message(FATAL_ERROR "Submodules are missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------------
# Formats. AU only exists on macOS; it is never requested on Windows or Linux.
# CLAP is added below through clap-juce-extensions, wrapping the very same processor.
# ---------------------------------------------------------------------------------------------
set(LHSS_FORMATS VST3)
if(APPLE)
    list(APPEND LHSS_FORMATS AU)
endif()
if(LHSS_BUILD_STANDALONE)
    list(APPEND LHSS_FORMATS Standalone)
endif()

juce_add_plugin(LHSampleSynth
    COMPANY_NAME "LH Audio"
    COMPANY_WEBSITE "https://github.com/shenphenchoedron-hue/LH-Sample-Synth"
    BUNDLE_ID com.lhaudio.lhsamplesynth
    PLUGIN_MANUFACTURER_CODE Lhau
    PLUGIN_CODE Lhss
    PRODUCT_NAME "LH Sample Synth"
    DESCRIPTION "Recorded-sound instrument with granular pitch shifting"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo-256.png"
    FORMATS ${LHSS_FORMATS})

# Logo for the editor header (rendered from resources/logo.svg).
juce_add_binary_data(LHSampleSynthAssets
    HEADER_NAME LHSSBinaryData.h
    NAMESPACE LHSSBinaryData
    SOURCES resources/logo-256.png)

set(LHSS_CORE_SOURCES
    src/engine/InstrumentEngine.cpp
    src/engine/Voice.cpp
    src/engine/VoiceManager.cpp
    src/engine/SampleData.cpp
    src/engine/PitchDetector.cpp
    src/engine/SampleLoader.cpp
    src/engine/SampleStore.cpp
    src/engine/SampleRegion.cpp
    src/engine/Envelope.cpp
    src/dsp/GranularPitchProcessor.cpp
    src/dsp/Grain.cpp
    src/dsp/NaturalPlaybackProcessor.cpp
    src/dsp/MultimodeFilter.cpp
    src/dsp/FormantProcessor.cpp
    src/dsp/ParameterSmoother.cpp
    src/dsp/Lfo.cpp
    src/dsp/Effects.cpp
    src/parameters/ParameterLayout.cpp)

set(LHSS_PLUGIN_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/gui/WaveformView.cpp
    src/gui/LHLookAndFeel.cpp
    src/gui/Knob.cpp)

set(LHSS_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_MP3AUDIOFORMAT=0
    LHSS_VERSION_STRING="${PROJECT_VERSION}")

target_sources(LHSampleSynth PRIVATE ${LHSS_CORE_SOURCES} ${LHSS_PLUGIN_SOURCES})
target_include_directories(LHSampleSynth PRIVATE src)
target_compile_definitions(LHSampleSynth PUBLIC ${LHSS_DEFINITIONS})
target_link_libraries(LHSampleSynth
    PRIVATE
        LHSampleSynthAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET LHSampleSynth
    CLAP_ID "com.lhaudio.lhsamplesynth"
    CLAP_FEATURES instrument sampler stereo)

# ---------------------------------------------------------------------------------------------
# Tests: the same engine/processor sources compiled into a console runner.
# ---------------------------------------------------------------------------------------------
if(LHSS_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(LHSampleSynthTests PRODUCT_NAME "LHSampleSynthTests")
    target_sources(LHSampleSynthTests PRIVATE
        tests/TestMain.cpp
        tests/EngineTests.cpp
        tests/StateTests.cpp
        tests/SynthTests.cpp
        ${LHSS_CORE_SOURCES}
        ${LHSS_PLUGIN_SOURCES})
    target_include_directories(LHSampleSynthTests PRIVATE src)
    # Modal loops: lets tests pump the message loop (runDispatchLoopUntil) to deliver async updates.
    target_compile_definitions(LHSampleSynthTests PRIVATE ${LHSS_DEFINITIONS} LHSS_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(LHSampleSynthTests
        PRIVATE
            LHSampleSynthAssets
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    add_test(NAME LHSampleSynthTests COMMAND LHSampleSynthTests)
endif()

# ---------------------------------------------------------------------------------------------
# Linux desktop integration for the Standalone app (icon + menu entry):
#   cmake --install build --component desktop
# ---------------------------------------------------------------------------------------------
if(UNIX AND NOT APPLE AND LHSS_BUILD_STANDALONE)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/LHSampleSynth_artefacts/$<CONFIG>/Standalone/LH Sample Synth" DESTINATION bin COMPONENT desktop)
    install(FILES resources/logo-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME lh-sample-synth.png COMPONENT desktop)
    install(FILES resources/logo-1024.png DESTINATION share/icons/hicolor/1024x1024/apps RENAME lh-sample-synth.png COMPONENT desktop)
    configure_file(resources/lh-sample-synth.desktop.in "${CMAKE_BINARY_DIR}/lh-sample-synth.desktop" @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/lh-sample-synth.desktop" DESTINATION share/applications COMPONENT desktop)
endif()
