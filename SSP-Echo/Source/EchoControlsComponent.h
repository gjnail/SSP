#pragma once

#include <JuceHeader.h>
#include "EchoVectorUI.h"
#include "PluginProcessor.h"

class EchoControlsComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    EchoControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~EchoControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class MeterColumn;
    class CharacterDisplay;

    void timerCallback() override;
    void refreshMeters();
    void updateLinkState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Rectangle<int> heroSectionBounds;
    juce::Rectangle<int> contourSectionBounds;
    juce::Rectangle<int> meterSectionBounds;

    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label linkLabel;
    ssp::ui::SSPToggle unlinkToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> unlinkAttachment;
    std::unique_ptr<ParameterKnob> leftTimeKnob;
    std::unique_ptr<ParameterKnob> rightTimeKnob;
    std::unique_ptr<ParameterKnob> leftFeedbackKnob;
    std::unique_ptr<ParameterKnob> rightFeedbackKnob;
    std::unique_ptr<ParameterKnob> mixKnob;
    std::unique_ptr<ParameterKnob> colorKnob;
    std::unique_ptr<ParameterKnob> driveKnob;
    std::unique_ptr<ParameterKnob> flutterKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> echoMeter;
    std::unique_ptr<MeterColumn> outputMeter;
    std::unique_ptr<CharacterDisplay> characterDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoControlsComponent)
};
