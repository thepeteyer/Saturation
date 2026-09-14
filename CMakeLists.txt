cmake_minimum_required(VERSION 3.22)

project(NeoSat VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE acquisition
#
# By default we pull JUCE 7.x via FetchContent so the project builds with a
# single `cmake` invocation.  If you already have JUCE on disk, pass
#   -DNEOSAT_JUCE_PATH=/path/to/JUCE
# and that copy is used instead (faster configure, no network needed).
# ---------------------------------------------------------------------------
set(NEOSAT_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(NEOSAT_JUCE_PATH)
    message(STATUS "NeoSat: using local JUCE at ${NEOSAT_JUCE_PATH}")
    add_subdirectory(${NEOSAT_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    message(STATUS "NeoSat: fetching JUCE 8.0.4 via FetchContent")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(NeoSat
    COMPANY_NAME              "Taberia"
    BUNDLE_ID                 "ca.taberia.neosat"
    PLUGIN_MANUFACTURER_CODE  Tbra
    PLUGIN_CODE               Nsat
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "NeoSat"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT           FALSE
    # Set to TRUE to auto-install into the system VST3 folder after each build
    # (may require write access / admin on Windows).
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Distortion"
)

juce_generate_juce_header(NeoSat)

target_sources(NeoSat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/SaturationEngine.cpp
    Source/gui/NeonLookAndFeel.cpp
    Source/gui/GlowKnob.cpp
    Source/gui/TransferCurveDisplay.cpp
    Source/gui/WaveScope.cpp
)

target_include_directories(NeoSat PRIVATE
    Source
)

target_compile_definitions(NeoSat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(NeoSat PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
