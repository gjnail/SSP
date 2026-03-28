#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CombControlsComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    CombControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~CombControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class CombKnob;
    class MeterColumn;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label polarityLabel;
    juce::ComboBox polarityBox;
    std::unique_ptr<CombKnob> frequencyKnob;
    std::unique_ptr<CombKnob> feedbackKnob;
    std::unique_ptr<CombKnob> dampKnob;
    std::unique_ptr<CombKnob> spreadKnob;
    std::unique_ptr<CombKnob> mixKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> combMeter;
    std::unique_ptr<MeterColumn> outputMeter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CombControlsComponent)
};
