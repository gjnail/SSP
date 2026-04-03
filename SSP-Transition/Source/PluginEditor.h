#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TransitionControlsComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    TransitionControlsComponent controls;
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label subHintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
