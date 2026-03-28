#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HihatGodControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    HihatGodControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~HihatGodControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class GodKnob;

    void timerCallback() override;
    void refreshFromProcessor();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label currentGainLabel;
    juce::Label currentPanLabel;
    std::unique_ptr<GodKnob> variationKnob;
    std::unique_ptr<GodKnob> rateKnob;
    std::unique_ptr<GodKnob> volumeRangeKnob;
    std::unique_ptr<GodKnob> panRangeKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HihatGodControlsComponent)
};
