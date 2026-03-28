#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ClipperControlsComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    ClipperControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ClipperControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ClipperKnob;
    class MeterColumn;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label oversamplingLabel;
    std::unique_ptr<ClipperKnob> driveKnob;
    std::unique_ptr<ClipperKnob> ceilingKnob;
    std::unique_ptr<ClipperKnob> softnessKnob;
    std::unique_ptr<ClipperKnob> trimKnob;
    std::unique_ptr<ClipperKnob> mixKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> clipMeter;
    std::unique_ptr<MeterColumn> outputMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipperControlsComponent)
};
