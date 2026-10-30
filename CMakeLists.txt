cmake_minimum_required(VERSION 3.22)

project(VibeKnob VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VK_BUILD_TESTS "Build the test runner" OFF)
option(VK_BUILD_SCREENSHOTS "Build the tool that renders the editor to a PNG (needs a display)" OFF)

# Use a local JUCE checkout if one is given (-DJUCE_DIR=...), otherwise download it.
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

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

set(VK_SOUND_SOURCES
    Source/Mood.cpp
    Source/VibeEngine.cpp)

set(VK_SOURCES
    ${VK_SOUND_SOURCES}
    Source/MoodLibrary.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/KnobView.cpp
    Source/Panels.cpp)

# The free and paid editions are separate plugins built from the same code.
function(add_vibe_knob target product code pro)
    juce_add_plugin(${target}
        COMPANY_NAME "Joe"
        PRODUCT_NAME "${product}"
        PLUGIN_MANUFACTURER_CODE Joev
        PLUGIN_CODE ${code}
        FORMATS ${PLUGIN_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx)

    target_sources(${target} PRIVATE ${VK_SOURCES})
    target_include_directories(${target} PRIVATE Source)

    target_compile_definitions(${target} PUBLIC
        VK_PRO=${pro}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

add_vibe_knob(VibeKnob    "Vibe Knob"     Vbk1 0)
add_vibe_knob(VibeKnobPro "Vibe Knob Pro" Vbk2 1)

if(VK_BUILD_TESTS)
    juce_add_console_app(VibeTests PRODUCT_NAME "VibeTests")
    target_sources(VibeTests PRIVATE Tests/VibeTests.cpp ${VK_SOUND_SOURCES})
    target_include_directories(VibeTests PRIVATE Source)
    target_compile_definitions(VibeTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(VibeTests PRIVATE juce::juce_core juce::juce_graphics juce::juce_audio_basics juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

if(VK_BUILD_SCREENSHOTS)
    foreach(pro 0 1)
        juce_add_gui_app(VibeScreenshot${pro} PRODUCT_NAME "VibeScreenshot${pro}")
        target_sources(VibeScreenshot${pro} PRIVATE Tests/Screenshot.cpp ${VK_SOURCES})
        target_include_directories(VibeScreenshot${pro} PRIVATE Source)
        target_compile_definitions(VibeScreenshot${pro} PRIVATE VK_PRO=${pro} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JucePlugin_Name="Vibe Knob" JUCE_STANDALONE_APPLICATION=1)
        target_link_libraries(VibeScreenshot${pro} PRIVATE juce::juce_audio_utils juce::juce_dsp)
    endforeach()
endif()
