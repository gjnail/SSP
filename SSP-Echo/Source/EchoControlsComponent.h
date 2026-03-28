#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EchoControlsComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    EchoControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~EchoControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class EchoKnob;
    class MeterColumn;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    std::unique_ptr<EchoKnob> timeKnob;
    std::unique_ptr<EchoKnob> feedbackKnob;
    std::unique_ptr<EchoKnob> mixKnob;
    std::unique_ptr<EchoKnob> colorKnob;
    std::unique_ptr<EchoKnob> driveKnob;
    std::unique_ptr<EchoKnob> flutterKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> echoMeter;
    std::unique_ptr<MeterColumn> outputMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoControlsComponent)
};
