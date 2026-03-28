#pragma once

#include <JuceHeader.h>

class ModSectionComponent final : public juce::Component
{
public:
    ModSectionComponent(juce::AudioProcessorValueTreeState&);
    ~ModSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label subLabel;
    juce::Label rateLabel;
    juce::Label depthLabel;
    juce::Slider rateSlider;
    juce::Slider depthSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSectionComponent)
};
