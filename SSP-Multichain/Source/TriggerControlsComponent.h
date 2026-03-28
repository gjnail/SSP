#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TriggerControlsComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    TriggerControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~TriggerControlsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setTriggerMode(int modeIndex);
    void refreshState();
    void updateButtonStyles();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton audioButton{"Audio"};
    juce::TextButton bpmButton{"BPM Sync"};
    juce::TextButton midiButton{"Midi"};
    juce::Label titleLabel;
    juce::Label modeLabel;
    juce::Label rateLabel;
    juce::ComboBox rateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;
    int selectedMode = 0;
    float currentTriggerActivity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriggerControlsComponent)
};
