#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../../SSP-Reverb/Source/ReverbVectorUI.h"

class ClipperControlsComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    ClipperControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ClipperControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class LimiterVisualizer;
    class GainReductionMeter;

    void timerCallback() override;
    void refreshDynamicState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label badgeLabel;
    juce::Label summaryLabel;
    juce::Label engineStatusLabel;
    reverbui::SSPDropdown limitTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> limitTypeAttachment;
    std::unique_ptr<LimiterVisualizer> visualizer;
    std::unique_ptr<GainReductionMeter> gainReductionMeter;
    std::unique_ptr<ParameterKnob> inputKnob;
    std::unique_ptr<ParameterKnob> ceilingKnob;
    std::unique_ptr<ParameterKnob> releaseKnob;
    std::unique_ptr<ParameterKnob> lookaheadKnob;
    std::unique_ptr<ParameterKnob> stereoLinkKnob;
    std::unique_ptr<ParameterKnob> outputKnob;
    std::unique_ptr<ParameterKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipperControlsComponent)
};
