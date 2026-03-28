#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SubValidatorComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit SubValidatorComponent(PluginProcessor&);
    ~SubValidatorComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFromProcessor();
    juce::String getStatusText() const;

    PluginProcessor& processor;
    juce::ToggleButton freezeButton{"Freeze"};
    std::vector<float> spectrumValues;
    float displayedSubPeakDb = -100.0f;
    bool displayedSubInRange = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubValidatorComponent)
};
