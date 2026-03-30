cmake_minimum_required(VERSION 3.22)

project(SSP_Imager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSPImager
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.imager
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Ssim
    FORMATS VST3 Standalone
    PRODUCT_NAME "SSP Imager"
)

juce_generate_juce_header(SSPImager)

target_sources(SSPImager
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ImagerGraphComponent.cpp
        Source/ImagerControlsComponent.cpp
        Source/PresetBrowserComponent.cpp
        Source/ImagerPresetLibrary.cpp
        Source/SSPVectorUI.cpp
)

target_compile_definitions(SSPImager
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSPImager
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
