#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class LoudMeterComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit LoudMeterComponent(PluginProcessor&);
    ~LoudMeterComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFromProcessor();

    PluginProcessor& processor;
    juce::TextButton resetButton{"Reset Session"};
    std::vector<float> historyValues;
    float momentaryLufs = -100.0f;
    float shortTermLufs = -100.0f;
    float integratedLufs = -100.0f;
    float peakDb = -60.0f;
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudMeterComponent)
};
