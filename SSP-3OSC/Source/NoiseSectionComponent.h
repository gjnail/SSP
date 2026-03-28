#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class NoiseSectionComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    NoiseSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~NoiseSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Label titleLabel;
    juce::Label typeLabel;
    juce::Label amountLabel;
    juce::ComboBox typeBox;
    ModulationKnob amountSlider;
    juce::ToggleButton toFilterButton;
    juce::ToggleButton toAmpButton;
    juce::Path previewPath;
    juce::Random previewRandom;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toFilterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toAmpAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseSectionComponent)
};
