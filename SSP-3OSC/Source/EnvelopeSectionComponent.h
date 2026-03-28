#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class EnvelopeSectionComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    EnvelopeSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, juce::String title, juce::String parameterPrefix, juce::Colour accent);
    ~EnvelopeSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    const juce::String titleText;
    const juce::String parameterPrefix;
    const juce::Colour accentColour;
    juce::Label titleLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    ModulationKnob attackSlider;
    ModulationKnob decaySlider;
    ModulationKnob sustainSlider;
    ModulationKnob releaseSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeSectionComponent)
};
