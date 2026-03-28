#pragma once

#include <JuceHeader.h>
#include "MinimizeControlsComponent.h"
#include "MinimizeResponseComponent.h"
#include "MinimizeUI.h"
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    minimizeui::LookAndFeel lookAndFeel;
    MinimizeResponseComponent response;
    MinimizeControlsComponent controls;
    juce::Label titleLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
