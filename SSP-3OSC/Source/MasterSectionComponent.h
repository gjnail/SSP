#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class MasterSectionComponent final : public juce::Component
{
public:
    MasterSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~MasterSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label spreadLabel;
    juce::Label gainLabel;
    ModulationKnob spreadSlider;
    ModulationKnob gainSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterSectionComponent)
};
