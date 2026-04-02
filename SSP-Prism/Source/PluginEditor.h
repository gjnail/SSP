#pragma once

#include <memory>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PrismControlsComponent.h"
#include "PrismVectorUI.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class HeroDisplay;
    class MiniKnob;

    PluginProcessor& pluginProcessor;
    PrismControlsComponent controls;
    std::unique_ptr<HeroDisplay> heroDisplay;
    std::unique_ptr<MiniKnob> mixKnob;
    std::unique_ptr<MiniKnob> outputKnob;
    juce::Label titleLabel;
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
