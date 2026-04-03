#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TransitionVisualizerComponent final : public juce::Component,
                                            private juce::Timer
{
public:
    explicit TransitionVisualizerComponent(PluginProcessor&);
    ~TransitionVisualizerComponent() override = default;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    PluginProcessor& processor;
    PluginProcessor::TransitionVisualState displayState;
    float motionPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransitionVisualizerComponent)
};
