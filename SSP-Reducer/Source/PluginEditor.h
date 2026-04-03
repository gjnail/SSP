#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ReducerGraphComponent.h"
#include "ReducerControlsComponent.h"
#include "ReducerVectorUI.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ReducerGraphComponent graph;
    ReducerControlsComponent controls;
    juce::Label titleLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
