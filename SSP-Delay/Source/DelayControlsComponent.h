#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DelayControlsComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    DelayControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~DelayControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class DelayKnob;
    class MeterColumn;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label timeModeLabel;
    juce::ComboBox timeModeBox;
    std::unique_ptr<DelayKnob> timeKnob;
    std::unique_ptr<DelayKnob> feedbackKnob;
    std::unique_ptr<DelayKnob> mixKnob;
    std::unique_ptr<DelayKnob> toneKnob;
    std::unique_ptr<DelayKnob> widthKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> delayMeter;
    std::unique_ptr<MeterColumn> outputMeter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayControlsComponent)
};
