#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ClipperControlsComponent.h"
#include "ClipperPresetBrowserComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void refreshBackgroundCache();
    void timerCallback() override;

    ClipperControlsComponent controls;
    ClipperPresetBrowserComponent presetBrowser;
    juce::Label titleLabel;
    juce::Label hintLabel;
    reverbui::SSPButton previousPresetButton{"<"};
    reverbui::SSPButton presetButton{"Default Limiter"};
    reverbui::SSPButton nextPresetButton{">"};
    juce::Image backgroundCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
