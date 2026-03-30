#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ImagerGraphComponent.h"
#include "ImagerControlsComponent.h"
#include "PresetBrowserComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void refreshPresetHeader();

    PluginProcessor& processorRef;

    ImagerGraphComponent graph;
    ImagerControlsComponent controls;
    PresetBrowserComponent presetBrowser;

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label presetNameLabel;
    juce::Label presetMetaLabel;
    juce::Label visualiserModeLabel;

    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton nextPresetButton{">"};
    ssp::ui::SSPButton browsePresetsButton{"BROWSE"};
    ssp::ui::SSPButton learnButton{"LEARN"};
    ssp::ui::SSPDropdown visualiserModeBox;

    ssp::ui::SSPKnob outputGainKnob;
    juce::Label outputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    ssp::ui::SSPButton bypassButton{"BYP"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
