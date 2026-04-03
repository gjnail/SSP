#pragma once

#include <JuceHeader.h>
#include "MixerControlsComponent.h"
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    MixerControlsComponent controls;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
