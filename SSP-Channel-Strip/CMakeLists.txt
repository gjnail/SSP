cmake_minimum_required(VERSION 3.22)

project(SSP_CHANNEL_STRIP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSPChannelStrip
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.channelstrip
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Sscs
    FORMATS VST3 Standalone
    PRODUCT_NAME "SSP Channel Strip"
)

juce_generate_juce_header(SSPChannelStrip)

target_sources(SSPChannelStrip
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EqGraphComponent.cpp
        Source/EqControlsComponent.cpp
        Source/MusicNoteUtils.cpp
        Source/SSPVectorUI.cpp
)

target_compile_definitions(SSPChannelStrip
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSPChannelStrip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
