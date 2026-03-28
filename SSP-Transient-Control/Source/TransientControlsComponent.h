#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TransientControlsComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    TransientControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~TransientControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class EnvelopeDisplay;
    class TransientKnob;

    void timerCallback() override;
    void refreshFromProcessor();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label attackActivityLabel;
    juce::Label sustainActivityLabel;
    std::unique_ptr<EnvelopeDisplay> envelopeDisplay;
    std::unique_ptr<TransientKnob> attackKnob;
    std::unique_ptr<TransientKnob> sustainKnob;
    std::unique_ptr<TransientKnob> clipKnob;
    std::unique_ptr<TransientKnob> outputKnob;
    std::unique_ptr<TransientKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransientControlsComponent)
};
