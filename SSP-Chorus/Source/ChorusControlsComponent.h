#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChorusVectorUI.h"

class ChorusControlsComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    ChorusControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ChorusControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class InputFilterGraph;
    class VoiceGraph;

    void timerCallback() override;
    void applyTypePreset(int presetIndex);

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Rectangle<int> typeSelectorBounds;
    juce::Rectangle<int> inputSectionBounds;
    juce::Rectangle<int> motionSectionBounds;
    juce::Rectangle<int> modifiersSectionBounds;
    juce::Rectangle<int> globalSectionBounds;
    juce::Rectangle<int> voiceSectionBounds;
    juce::Rectangle<int> bodyVoiceBounds;
    juce::Rectangle<int> airVoiceBounds;

    InputFilterGraph* inputFilterGraphPtr = nullptr;
    VoiceGraph* voiceGraphPtr = nullptr;
    std::unique_ptr<InputFilterGraph> inputFilterGraph;
    std::unique_ptr<VoiceGraph> voiceGraph;

    ParameterKnob* loCutKnobPtr = nullptr;
    ParameterKnob* hiCutKnobPtr = nullptr;
    ParameterKnob* motionAmountKnobPtr = nullptr;
    ParameterKnob* motionRateKnobPtr = nullptr;
    ParameterKnob* motionShapeKnobPtr = nullptr;
    ParameterKnob* voiceCrossoverKnobPtr = nullptr;
    ParameterKnob* lowVoiceAmountKnobPtr = nullptr;
    ParameterKnob* lowVoiceScaleKnobPtr = nullptr;
    ParameterKnob* lowVoiceRateKnobPtr = nullptr;
    ParameterKnob* highVoiceAmountKnobPtr = nullptr;
    ParameterKnob* highVoiceScaleKnobPtr = nullptr;
    ParameterKnob* highVoiceRateKnobPtr = nullptr;
    ParameterKnob* delayKnobPtr = nullptr;
    ParameterKnob* feedbackKnobPtr = nullptr;
    ParameterKnob* toneKnobPtr = nullptr;
    ParameterKnob* widthKnobPtr = nullptr;
    ParameterKnob* driveKnobPtr = nullptr;
    ParameterKnob* dryWetKnobPtr = nullptr;
    ParameterKnob* shineAmountKnobPtr = nullptr;
    ParameterKnob* shineRateKnobPtr = nullptr;
    ParameterKnob* spreadKnobPtr = nullptr;

    std::unique_ptr<ParameterKnob> loCutKnob;
    std::unique_ptr<ParameterKnob> hiCutKnob;
    std::unique_ptr<ParameterKnob> motionAmountKnob;
    std::unique_ptr<ParameterKnob> motionRateKnob;
    std::unique_ptr<ParameterKnob> motionShapeKnob;
    std::unique_ptr<ParameterKnob> voiceCrossoverKnob;
    std::unique_ptr<ParameterKnob> lowVoiceAmountKnob;
    std::unique_ptr<ParameterKnob> lowVoiceScaleKnob;
    std::unique_ptr<ParameterKnob> lowVoiceRateKnob;
    std::unique_ptr<ParameterKnob> highVoiceAmountKnob;
    std::unique_ptr<ParameterKnob> highVoiceScaleKnob;
    std::unique_ptr<ParameterKnob> highVoiceRateKnob;
    std::unique_ptr<ParameterKnob> delayKnob;
    std::unique_ptr<ParameterKnob> feedbackKnob;
    std::unique_ptr<ParameterKnob> toneKnob;
    std::unique_ptr<ParameterKnob> widthKnob;
    std::unique_ptr<ParameterKnob> driveKnob;
    std::unique_ptr<ParameterKnob> dryWetKnob;
    std::unique_ptr<ParameterKnob> shineAmountKnob;
    std::unique_ptr<ParameterKnob> shineRateKnob;
    std::unique_ptr<ParameterKnob> spreadKnob;

    reverbui::SSPToggle motionSpinToggle { "Spin" };
    reverbui::SSPToggle vibratoToggle { "Vibrato" };
    reverbui::SSPToggle focusCutToggle { "Focus Cut" };
    juce::Label typeLabel;
    reverbui::SSPDropdown typeDropdown;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> motionSpinAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> vibratoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> focusCutAttachment;
    bool ignoreTypeSelection = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusControlsComponent)
};
