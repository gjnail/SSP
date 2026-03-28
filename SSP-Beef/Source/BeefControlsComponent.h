#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class BeefControlsComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    BeefControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~BeefControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class BeefKnob;

    void timerCallback() override;
    void refreshDescription();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label descriptionLabel;
    std::unique_ptr<BeefKnob> amountKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeefControlsComponent)
};
