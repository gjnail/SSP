#pragma once

#include <JuceHeader.h>
#include "EchoControlsComponent.h"
#include "EchoPresetBrowserComponent.h"
#include "EchoVectorUI.h"
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
    EchoControlsComponent controls;
    EchoPresetBrowserComponent presetBrowser;
    juce::Label titleLabel;
    juce::Label hintLabel;
    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton presetButton{"Default Setting"};
    ssp::ui::SSPButton nextPresetButton{">"};
    ssp::ui::SSPButton settingsButton{"SET"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
