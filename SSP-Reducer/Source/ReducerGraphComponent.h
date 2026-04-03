#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ReducerDSP.h"
#include "ReducerVectorUI.h"

class ReducerGraphComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit ReducerGraphComponent(PluginProcessor&);
    ~ReducerGraphComponent() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> getPlotBounds() const;
    float frequencyToX(double frequency, juce::Rectangle<float> plot) const;
    float magnitudeToY(float magnitudeDb, juce::Rectangle<float> plot) const;

    PluginProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReducerGraphComponent)
};
