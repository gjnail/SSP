#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaveshaperControlsComponent final : public juce::Component,
                                          private juce::Timer
{
public:
    WaveshaperControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~WaveshaperControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TableStackDisplay;
    class CurveDisplay;
    class MeterBridge;
    class WaveshaperKnob;

    void timerCallback() override;
    void refreshSummary();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Label badgeLabel;
    juce::Label summaryLabel;
    juce::Label tableLabel;
    juce::Label overflowLabel;

    juce::ComboBox tableBox;
    juce::ComboBox overflowBox;
    juce::ToggleButton dcFilterButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> overflowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dcFilterAttachment;

    std::unique_ptr<TableStackDisplay> tableDisplay;
    std::unique_ptr<CurveDisplay> curveDisplay;
    std::unique_ptr<MeterBridge> meterBridge;
    std::unique_ptr<WaveshaperKnob> driveKnob;
    std::unique_ptr<WaveshaperKnob> frameKnob;
    std::unique_ptr<WaveshaperKnob> phaseKnob;
    std::unique_ptr<WaveshaperKnob> biasKnob;
    std::unique_ptr<WaveshaperKnob> smoothKnob;
    std::unique_ptr<WaveshaperKnob> outputKnob;
    std::unique_ptr<WaveshaperKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveshaperControlsComponent)
};
