#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class OscillatorSettingsComponent final : public juce::Component
{
public:
    OscillatorSettingsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~OscillatorSettingsComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    std::array<juce::Label, 3> oscLabels;
    std::array<juce::Label, 3> widthLabels;
    std::array<ModulationKnob, 3> widthSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> widthAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorSettingsComponent)
};
