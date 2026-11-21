cmake_minimum_required(VERSION 3.22)
project(Nebulon VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(NEBULON_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NEBULON_FORMATS AU)
endif()

juce_add_plugin(Nebulon
    COMPANY_NAME "Nebulon Audio"
    PRODUCT_NAME "Nebulon"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Nebu
    PLUGIN_CODE Nb01
    FORMATS ${NEBULON_FORMATS})

set(NEBULON_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Pages.cpp
    Source/Engine.cpp
    Source/Wavetables.cpp
    Source/Sources.cpp)

target_sources(Nebulon PRIVATE ${NEBULON_SOURCES})

target_compile_definitions(Nebulon PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Nebulon
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline smoke test: cmake -DNEBULON_BUILD_TESTS=ON
option(NEBULON_BUILD_TESTS "Build the offline render test" OFF)
if(NEBULON_BUILD_TESTS)
    juce_add_console_app(NebulonRenderTest PRODUCT_NAME "NebulonRenderTest")
    target_sources(NebulonRenderTest PRIVATE
        tools/RenderTest.cpp
        ${NEBULON_SOURCES})
    target_compile_definitions(NebulonRenderTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(NebulonRenderTest PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
