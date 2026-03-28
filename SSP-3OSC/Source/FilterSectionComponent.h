#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"
#include "PluginProcessor.h"

class FilterSectionComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    FilterSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~FilterSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Label titleLabel;
    juce::ToggleButton filterOnButton;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    juce::Label cutoffLabel;
    juce::Label resonanceLabel;
    juce::Label driveLabel;
    juce::Label envLabel;
    ModulationKnob cutoffSlider;
    ModulationKnob resonanceSlider;
    ModulationKnob driveSlider;
    ModulationKnob envSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterSectionComponent)
};
