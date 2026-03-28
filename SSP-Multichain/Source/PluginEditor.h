#pragma once

#include <JuceHeader.h>
#include "BandCrossoverEQComponent.h"
#include "BandCurveEditorComponent.h"
#include "ControlsSectionComponent.h"
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;

    BandCrossoverEQComponent crossoverView;
    BandCurveEditorComponent bandCurveEditor;
    ControlsSectionComponent controlsSection;

    juce::Label titleLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
