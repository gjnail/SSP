#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HyperResonanceControlsComponent final : public juce::Component,
                                              private juce::Timer
{
public:
    HyperResonanceControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~HyperResonanceControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class HyperKnob;
    class MeterColumn;
    class DispersionDisplay;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    std::unique_ptr<DispersionDisplay> display;
    std::unique_ptr<HyperKnob> frequencyKnob;
    std::unique_ptr<HyperKnob> amountKnob;
    std::unique_ptr<HyperKnob> pinchKnob;
    std::unique_ptr<HyperKnob> spreadKnob;
    std::unique_ptr<HyperKnob> mixKnob;
    std::unique_ptr<HyperKnob> outputKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> resonanceMeter;
    std::unique_ptr<MeterColumn> outputMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HyperResonanceControlsComponent)
};
