#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ResonatorGraphComponent.h"
#include "ResonatorControlsComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor
                         , private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void showPresetMenu();

    PluginProcessor& processor;
    ResonatorGraphComponent graph;
    ResonatorControlsComponent controls;
    juce::Label titleLabel;
    juce::Label hintLabel;
    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton presetButton{"Laser Fifth Stack"};
    ssp::ui::SSPButton nextPresetButton{">"};
    ssp::ui::SSPButton differenceButton{"DIFF"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
