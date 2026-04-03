#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ShifterVectorUI.h"

class ShifterControlsComponent final : public juce::Component,
                                       private juce::AudioProcessorValueTreeState::Listener,
                                       private juce::AsyncUpdater
{
public:
    ShifterControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ShifterControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class EngineDisplay;

    void parameterChanged(const juce::String&, float) override;
    void handleAsyncUpdate() override;
    void updateModeState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Rectangle<int> modeSectionBounds;
    juce::Rectangle<int> pitchSectionBounds;
    juce::Rectangle<int> frequencySectionBounds;
    juce::Rectangle<int> sharedSectionBounds;

    juce::Label modeLabel;
    juce::Label modeHintLabel;
    shifterui::SSPDropdown modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    std::unique_ptr<ParameterKnob> pitchKnob;
    std::unique_ptr<ParameterKnob> fineKnob;
    std::unique_ptr<ParameterKnob> grainKnob;
    std::unique_ptr<ParameterKnob> frequencyKnob;
    std::unique_ptr<ParameterKnob> toneKnob;
    std::unique_ptr<ParameterKnob> mixKnob;
    std::unique_ptr<ParameterKnob> outputKnob;
    std::unique_ptr<EngineDisplay> engineDisplay;
    bool showingPitchMode = true;
    std::atomic<bool> pendingModeRefresh{ false };
    std::atomic<bool> pendingEngineRefresh{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShifterControlsComponent)
};
