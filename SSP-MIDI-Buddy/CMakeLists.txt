cmake_minimum_required(VERSION 3.22)

project(SSP_MIDI_BUDDY VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSPMIDIBuddy
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.midibuddy
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Smdb
    FORMATS VST3
    PRODUCT_NAME "SSP MIDI Buddy"
)

juce_generate_juce_header(SSPMIDIBuddy)

target_sources(SSPMIDIBuddy
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/MidiBuddyVectorUI.cpp
)

target_compile_definitions(SSPMIDIBuddy
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSPMIDIBuddy
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
