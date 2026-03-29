#pragma once

#include <memory>
#include <JuceHeader.h>
#include "EQ3ControlsComponent.h"
#include "EQ3GraphComponent.h"
#include "EQ3VectorUI.h"
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EQ3GraphComponent graph;
    EQ3ControlsComponent controls;
    juce::Label titleLabel;
    juce::Label tagLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
