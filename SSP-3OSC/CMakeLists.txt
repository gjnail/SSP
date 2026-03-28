cmake_minimum_required(VERSION 3.22)

project(SSP_Reactor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SSPJuce.cmake")

juce_add_plugin(SSP3OSC
    COMPANY_NAME "Stupid Simple Plugins"
    BUNDLE_ID com.ssp.reactor
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ssp1
    PLUGIN_CODE Ss3o
    FORMATS VST3 Standalone
    PRODUCT_NAME "SSP Reactor"
)

juce_generate_juce_header(SSP3OSC)

target_sources(SSP3OSC
    PRIVATE
        Source/FactoryPresetBank.cpp
        Source/FactoryPresetLoader.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/BitcrusherModuleComponent.cpp
        Source/HeaderSectionComponent.cpp
        Source/PresetBrowserComponent.cpp
        Source/PresetLibraryComponent.cpp
        Source/ChorusModuleComponent.cpp
        Source/CompressorModuleComponent.cpp
        Source/DelayModuleComponent.cpp
        Source/DistortionModuleComponent.cpp
        Source/EQModuleComponent.cpp
        Source/FilterModuleComponent.cpp
        Source/FlangerModuleComponent.cpp
        Source/GateModuleComponent.cpp
        Source/ImagerModuleComponent.cpp
        Source/MultibandCompressorModuleComponent.cpp
        Source/PhaserModuleComponent.cpp
        Source/ReverbModuleComponent.cpp
        Source/ShiftModuleComponent.cpp
        Source/TremoloModuleComponent.cpp
        Source/EffectsRackComponent.cpp
        Source/SynthVoice.cpp
        Source/WavetableLibrary.cpp
        Source/OscillatorSectionComponent.cpp
        Source/SamplerOscillatorComponent.cpp
        Source/WavetableOscillatorComponent.cpp
        Source/FilterSectionComponent.cpp
        Source/EnvelopeSectionComponent.cpp
        Source/LfoPanelComponent.cpp
        Source/ModSectionComponent.cpp
        Source/ModulationKnob.cpp
        Source/ModMatrixComponent.cpp
        Source/NoiseSectionComponent.cpp
        Source/SubOscSectionComponent.cpp
        Source/VoiceSectionComponent.cpp
        Source/PerformanceSectionComponent.cpp
        Source/WarpSectionComponent.cpp
        Source/MasterSectionComponent.cpp
        Source/OscillatorSettingsComponent.cpp
)

target_compile_definitions(SSP3OSC
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSP3OSC
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (MSVC AND TARGET SSP3OSC_Standalone)
    target_link_options(SSP3OSC_Standalone PRIVATE
        "$<$<CONFIG:Release>:/MAP:${CMAKE_BINARY_DIR}/SSP3OSC-Standalone-release.map>"
    )
endif()
