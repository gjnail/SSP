cmake_minimum_required(VERSION 3.22)

project(SSP_TRANSIENT_SPLITTER VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSPTransientSplitter
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.transientsplitter
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Ssts
    FORMATS VST3 Standalone
    PRODUCT_NAME "SSP Transient Splitter"
)

juce_generate_juce_header(SSPTransientSplitter)

target_sources(SSPTransientSplitter
    PRIVATE
        Source/BitcrusherModuleComponent.cpp
        Source/ChorusModuleComponent.cpp
        Source/CompressorModuleComponent.cpp
        Source/DelayModuleComponent.cpp
        Source/DistortionModuleComponent.cpp
        Source/EQModuleComponent.cpp
        Source/FilterModuleComponent.cpp
        Source/FlangerModuleComponent.cpp
        Source/GateModuleComponent.cpp
        Source/ImagerModuleComponent.cpp
        Source/ModulationKnob.cpp
        Source/MultibandCompressorModuleComponent.cpp
        Source/PhaserModuleComponent.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/ReverbModuleComponent.cpp
        Source/ShiftModuleComponent.cpp
        Source/TremoloModuleComponent.cpp
)

target_compile_definitions(SSPTransientSplitter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSPTransientSplitter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
