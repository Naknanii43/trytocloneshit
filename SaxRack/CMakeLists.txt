cmake_minimum_required(VERSION 3.22)
project(Reedstorm VERSION 1.0.0)

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

set(REEDSTORM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND REEDSTORM_FORMATS AU)
endif()

juce_add_plugin(Reedstorm
    COMPANY_NAME "Reedstorm Audio"
    PRODUCT_NAME "Reedstorm"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    BUNDLE_ID com.reedstormaudio.reedstorm
    PLUGIN_MANUFACTURER_CODE Rdst
    PLUGIN_CODE Rs01
    VST3_CATEGORIES Fx
    FORMATS ${REEDSTORM_FORMATS})

set(REEDSTORM_SOURCES
    Source/Rack.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Reedstorm PRIVATE ${REEDSTORM_SOURCES})

set(REEDSTORM_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(Reedstorm PUBLIC ${REEDSTORM_DEFS})

# Let the compiler inline/vectorise sqrt, floor, etc. (no global fast-math: NaN checks must keep working)
if(NOT MSVC)
    target_compile_options(Reedstorm PUBLIC -fno-math-errno)
endif()

target_link_libraries(Reedstorm
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test + demo renderer: cmake -DREEDSTORM_BUILD_TESTS=ON
option(REEDSTORM_BUILD_TESTS "Build the offline render test" OFF)
if(REEDSTORM_BUILD_TESTS)
    juce_add_console_app(ReedstormRenderTest PRODUCT_NAME "ReedstormRenderTest")
    target_sources(ReedstormRenderTest PRIVATE tools/RenderTest.cpp ${REEDSTORM_SOURCES})
    target_compile_definitions(ReedstormRenderTest PRIVATE ${REEDSTORM_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ReedstormRenderTest PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        juce::juce_recommended_config_flags)

    juce_add_console_app(ReedstormBench PRODUCT_NAME "ReedstormBench")
    target_sources(ReedstormBench PRIVATE tools/Bench.cpp ${REEDSTORM_SOURCES})
    target_compile_definitions(ReedstormBench PRIVATE ${REEDSTORM_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(ReedstormBench PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
endif()
