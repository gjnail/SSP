#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class VintageMeterComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit VintageMeterComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    PluginProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageMeterComponent)
};
