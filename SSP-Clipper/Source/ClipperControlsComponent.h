#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../../SSP-Reverb/Source/ReverbVectorUI.h"

class ClipperControlsComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    ClipperControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ClipperControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class ClipVisualizer;
    class MeterBridge;

    void timerCallback() override;
    void refreshDynamicState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label badgeLabel;
    juce::Label summaryLabel;
    juce::Label engineStatusLabel;
    reverbui::SSPDropdown clipTypeBox;
    reverbui::SSPDropdown oversamplingBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clipTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<ClipVisualizer> visualizer;
    std::unique_ptr<ParameterKnob> driveKnob;
    std::unique_ptr<ParameterKnob> ceilingKnob;
    std::unique_ptr<ParameterKnob> trimKnob;
    std::unique_ptr<ParameterKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipperControlsComponent)
};
