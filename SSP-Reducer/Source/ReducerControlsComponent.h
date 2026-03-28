#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReducerControlsComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    ReducerControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ReducerControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ReducerKnob;

    void timerCallback() override;
    void refreshDescription();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    juce::Label descriptionLabel;
    std::unique_ptr<ReducerKnob> mixKnob;
    std::unique_ptr<ReducerKnob> bitsKnob;
    std::unique_ptr<ReducerKnob> rateKnob;
    std::unique_ptr<ReducerKnob> toneKnob;
    std::unique_ptr<ReducerKnob> jitterKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReducerControlsComponent)
};
