#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class PluginProcessor;

class SubOscSectionComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    SubOscSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~SubOscSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Label titleLabel;
    juce::Label waveLabel;
    juce::Label levelLabel;
    juce::Label octaveLabel;
    juce::ComboBox waveBox;
    ModulationKnob levelSlider;
    ModulationKnob octaveSlider;
    juce::ToggleButton directOutButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> directOutAttachment;

    std::atomic<float>* waveParam = nullptr;
    std::atomic<float>* levelParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubOscSectionComponent)
};
