#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ResonatorGraphComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    explicit ResonatorGraphComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Rectangle<int> getPlotBounds() const;
    float frequencyToX(float frequency, const juce::Rectangle<int>& plot) const;
    float buildResonanceProfile(float frequency,
                                const PluginProcessor::VoiceArray& voices,
                                float resonance,
                                float brightness) const;

    PluginProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorGraphComponent)
};
