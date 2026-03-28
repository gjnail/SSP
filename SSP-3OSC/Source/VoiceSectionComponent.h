#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class VoiceSectionComponent final : public juce::Component
{
public:
    VoiceSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~VoiceSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    juce::Label glideLabel;
    ModulationKnob glideSlider;
    juce::ToggleButton legatoButton;
    juce::ToggleButton portamentoButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> legatoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> portamentoAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSectionComponent)
};
