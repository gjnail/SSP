#pragma once

#include <memory>
#include <JuceHeader.h>
#include "PrismVectorUI.h"

class PrismControlsComponent final : public juce::Component
{
public:
    explicit PrismControlsComponent(juce::AudioProcessorValueTreeState&);
    ~PrismControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class ChoiceField;
    class NoteDisplay;
    class FrequencyRangeSlider;

    void drawSectionChrome(juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent);

    juce::AudioProcessorValueTreeState& apvts;

    std::unique_ptr<ChoiceField> chordChoice;
    std::unique_ptr<ChoiceField> rootChoice;
    std::unique_ptr<ChoiceField> octaveChoice;
    std::unique_ptr<ChoiceField> spreadChoice;
    std::unique_ptr<NoteDisplay> noteDisplay;
    std::unique_ptr<ParameterKnob> attackKnob;
    std::unique_ptr<ParameterKnob> decayKnob;

    std::unique_ptr<ChoiceField> arpPatternChoice;
    std::unique_ptr<ParameterKnob> arpRateKnob;
    std::unique_ptr<reverbui::SSPToggle> arpEnabledToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpEnabledAttachment;
    std::unique_ptr<reverbui::SSPToggle> arpSyncToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpSyncAttachment;

    std::unique_ptr<ParameterKnob> warpKnob;
    std::unique_ptr<ParameterKnob> driftKnob;
    std::unique_ptr<ParameterKnob> shiftKnob;

    std::unique_ptr<ParameterKnob> motionStrengthKnob;
    std::unique_ptr<ParameterKnob> motionRateKnob;
    std::unique_ptr<ParameterKnob> motionShapeKnob;
    std::unique_ptr<ParameterKnob> motionUnisonKnob;
    std::unique_ptr<reverbui::SSPToggle> motionSyncToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> motionSyncAttachment;

    std::unique_ptr<ParameterKnob> filterTiltKnob;
    std::unique_ptr<ParameterKnob> filterQKnob;
    std::unique_ptr<ParameterKnob> depthKnob;
    std::unique_ptr<FrequencyRangeSlider> rangeSlider;

    juce::Rectangle<int> chordSection;
    juce::Rectangle<int> arpSection;
    juce::Rectangle<int> spacingSection;
    juce::Rectangle<int> motionSection;
    juce::Rectangle<int> filterSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismControlsComponent)
};
