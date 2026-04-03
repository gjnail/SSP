#pragma once

#include <JuceHeader.h>
#include "ChorusControlsComponent.h"
#include "ChorusPresetBrowserComponent.h"
#include "PluginProcessor.h"

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

    PluginProcessor& processor;
    ChorusControlsComponent controls;
    ChorusPresetBrowserComponent presetBrowser;
    juce::Label titleLabel;
    juce::Label hintLabel;
    reverbui::SSPButton previousPresetButton{"<"};
    reverbui::SSPButton presetButton{"Default Setting"};
    reverbui::SSPButton nextPresetButton{">"};
    reverbui::SSPButton browserButton{"BROWSE"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
