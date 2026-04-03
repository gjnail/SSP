#pragma once

#include <JuceHeader.h>
#include "DelayVectorUI.h"
#include "PluginProcessor.h"

class DelayControlsComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    DelayControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~DelayControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class TimeControl;
    class MeterColumn;
    class StereoDisplay;

    void timerCallback() override;
    void refreshMeters();
    void updateLinkState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Rectangle<int> heroSectionBounds;
    juce::Rectangle<int> spreadSectionBounds;
    juce::Rectangle<int> meterSectionBounds;

    juce::Label summaryLabel;
    juce::Label badgeLabel;
    juce::Label linkLabel;
    ssp::ui::SSPToggle unlinkToggle;
    std::unique_ptr<TimeControl> leftTimeControl;
    std::unique_ptr<TimeControl> rightTimeControl;
    std::unique_ptr<ParameterKnob> leftFeedbackKnob;
    std::unique_ptr<ParameterKnob> rightFeedbackKnob;
    std::unique_ptr<ParameterKnob> mixKnob;
    std::unique_ptr<ParameterKnob> toneKnob;
    std::unique_ptr<ParameterKnob> widthKnob;
    std::unique_ptr<MeterColumn> inputMeter;
    std::unique_ptr<MeterColumn> delayMeter;
    std::unique_ptr<MeterColumn> outputMeter;
    std::unique_ptr<StereoDisplay> stereoDisplay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> unlinkAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayControlsComponent)
};
