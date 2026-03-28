#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VintageCompControlsComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    VintageCompControlsComponent controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
