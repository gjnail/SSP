#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"
#include "PluginProcessor.h"
#include "SamplerOscillatorComponent.h"
#include "WavetableOscillatorComponent.h"

class OscillatorSectionComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    OscillatorSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int oscillatorIndex);
    ~OscillatorSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    const int oscIndex;
    SamplerOscillatorComponent samplerComponent;
    WavetableOscillatorComponent wavetableComponent;
    juce::Label titleLabel;
    juce::Label typeLabel;
    juce::ToggleButton filterRouteButton;
    juce::ToggleButton unisonButton;
    juce::Label waveLabel;
    juce::Label octaveLabel;
    juce::Label levelLabel;
    juce::Label coarseLabel;
    juce::Label detuneLabel;
    juce::Label panLabel;
    juce::Label unisonVoicesLabel;
    juce::Label unisonDetuneLabel;
    juce::ComboBox typeBox;
    juce::ComboBox waveBox;
    ModulationKnob octaveSlider;
    ModulationKnob levelSlider;
    ModulationKnob coarseSlider;
    ModulationKnob detuneSlider;
    ModulationKnob panSlider;
    ModulationKnob unisonVoicesSlider;
    ModulationKnob unisonDetuneSlider;
    std::atomic<float>* oscTypeParam = nullptr;
    std::atomic<float>* warpFMSourceParam = nullptr;
    std::array<std::atomic<float>*, 2> warpModeParams{};
    std::array<std::atomic<float>*, 2> warpAmountParams{};
    std::atomic<float>* mutateParam = nullptr;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterRouteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> unisonOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonVoicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonDetuneAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorSectionComponent)
};
