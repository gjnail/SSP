cmake_minimum_required(VERSION 3.22)

project(SSP_MIXER VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSPMixer
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.mixer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Ssmx
    FORMATS VST3 Standalone
    PRODUCT_NAME "SSP Mixer"
)

juce_generate_juce_header(SSPMixer)

target_sources(SSPMixer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/MixerControlsComponent.cpp
        Source/MixerVectorUI.cpp
)

target_compile_definitions(SSPMixer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSPMixer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
