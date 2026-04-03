#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TransitionVectorUI.h"
#include "TransitionVisualizerComponent.h"

class TransitionControlsComponent final : public juce::Component,
                                          private juce::Timer
{
public:
    TransitionControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~TransitionControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TransitionKnob;

    void timerCallback() override;
    void refreshPresetDetails();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label presetLabel;
    transitionui::SSPDropdown presetBox;
    juce::Label badgeLabel;
    juce::Label descriptionLabel;
    std::unique_ptr<TransitionKnob> amountKnob;
    std::unique_ptr<TransitionKnob> filterKnob;
    std::unique_ptr<TransitionKnob> spaceKnob;
    std::unique_ptr<TransitionKnob> widthKnob;
    std::unique_ptr<TransitionKnob> riseKnob;
    std::unique_ptr<TransitionKnob> mixKnob;
    std::unique_ptr<TransitionVisualizerComponent> visualizer;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransitionControlsComponent)
};
