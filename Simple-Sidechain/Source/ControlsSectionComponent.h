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
    class MixKnob;

    TriggerControlsComponent triggerControls;
    std::unique_ptr<MixKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsSectionComponent)
};
