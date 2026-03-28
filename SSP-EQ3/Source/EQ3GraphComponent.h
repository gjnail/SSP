#pragma once

#include <JuceHeader.h>
#include "EQ3VectorUI.h"
#include "PluginProcessor.h"

class EQ3GraphComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit EQ3GraphComponent(PluginProcessor&);
    ~EQ3GraphComponent() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> getPlotBounds() const;
    float frequencyToX(double frequency, juce::Rectangle<float> plot) const;
    float responseToY(double responseDb, juce::Rectangle<float> plot) const;

    PluginProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ3GraphComponent)
};
