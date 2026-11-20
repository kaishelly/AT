cmake_minimum_required(VERSION 3.22)
project(DepnoteTune VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

option(DT_COPY "Copy plugin to system folder after build" TRUE)

if(APPLE)
    set(DT_FORMATS VST3 AU Standalone)
else()
    set(DT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DepnoteTune
    COMPANY_NAME "Depnote"
    PLUGIN_MANUFACTURER_CODE Depn
    PLUGIN_CODE Dtun
    PRODUCT_NAME "Depnote Tune"
    FORMATS ${DT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${DT_COPY})

juce_generate_juce_header(DepnoteTune)

target_sources(DepnoteTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DepnoteTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DepnoteTune
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
