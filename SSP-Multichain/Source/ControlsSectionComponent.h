#pragma once

#include <JuceHeader.h>
#include "TriggerControlsComponent.h"

class ControlsSectionComponent final : public juce::Component
{
public:
    ControlsSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ControlsSectionComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ParameterSlider;

    juce::AudioProcessorValueTreeState& apvts;

    TriggerControlsComponent triggerControls;
    std::unique_ptr<ParameterSlider> lowMixSlider;
    std::unique_ptr<ParameterSlider> midMixSlider;
    std::unique_ptr<ParameterSlider> highMixSlider;
    std::unique_ptr<ParameterSlider> masterMixSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsSectionComponent)
};
