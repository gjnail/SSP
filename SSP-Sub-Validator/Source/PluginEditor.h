#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SubValidatorComponent.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    SubValidatorComponent validatorComponent;
    juce::Label titleLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
