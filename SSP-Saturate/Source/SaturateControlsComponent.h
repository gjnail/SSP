#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SaturateControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    SaturateControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~SaturateControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class WaveshaperDisplay;
    class SaturateKnob;

    void timerCallback() override;
    void refreshMeters();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label inputMeterLabel;
    juce::Label heatMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<WaveshaperDisplay> waveshaperDisplay;
    std::unique_ptr<SaturateKnob> driveKnob;
    std::unique_ptr<SaturateKnob> colorKnob;
    std::unique_ptr<SaturateKnob> biasKnob;
    std::unique_ptr<SaturateKnob> outputKnob;
    std::unique_ptr<SaturateKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturateControlsComponent)
};
