#pragma once

#include <JuceHeader.h>
#include <array>
#include "ModulationKnob.h"

class PluginProcessor;

class ModSectionComponent final : public juce::Component
{
public:
    ModSectionComponent(PluginProcessor&,
                        juce::AudioProcessorValueTreeState&,
                        juce::String titleText = "MOD",
                        juce::String subtitleText = "Drag the orange mod knobs onto any parameter.");
    ~ModSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    juce::String titleText;
    juce::String subtitleText;
    juce::Label titleLabel;
    juce::Label subLabel;
    std::array<juce::Label, reactormod::macroSourceCount> macroLabels;
    std::array<ModulationKnob, reactormod::macroSourceCount> macroKnobs;
    std::array<std::unique_ptr<juce::Component>, reactormod::macroSourceCount> dragBadges;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, reactormod::macroSourceCount> macroAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSectionComponent)
};
