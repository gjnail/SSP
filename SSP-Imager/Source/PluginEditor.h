#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ImagerGraphComponent.h"
#include "ImagerControlsComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessor& processorRef;

    ImagerGraphComponent graph;
    ImagerControlsComponent controls;

    juce::Label titleLabel;
    juce::Label hintLabel;

    ssp::ui::SSPKnob outputGainKnob;
    juce::Label outputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    ssp::ui::SSPButton bypassButton{"BYP"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
