#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReverbControlsComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    ReverbControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ReverbControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ReverbKnob;

    void timerCallback() override;
    void refreshDescription();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    juce::Label descriptionLabel;
    std::unique_ptr<ReverbKnob> mixKnob;
    std::unique_ptr<ReverbKnob> decayKnob;
    std::unique_ptr<ReverbKnob> toneKnob;
    std::unique_ptr<ReverbKnob> widthKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbControlsComponent)
};
