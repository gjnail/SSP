#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"

class WarpSectionComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    WarpSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~WarpSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSaturatorPlacement(bool afterFilter);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Label titleLabel;
    juce::Label saturatorLabel;
    juce::Label mutateLabel;
    ModulationKnob saturatorSlider;
    ModulationKnob mutateSlider;
    juce::TextButton beforeFilterButton;
    juce::TextButton afterFilterButton;

    std::array<juce::Label, 3> oscTitleLabels;
    std::array<juce::Label, 3> fmSourceLabels;
    std::array<juce::ComboBox, 3> fmSourceBoxes;
    std::array<std::array<juce::Label, 2>, 3> warpSlotLabels;
    std::array<std::array<juce::ComboBox, 2>, 3> warpModeBoxes;
    std::array<std::array<std::unique_ptr<ModulationKnob>, 2>, 3> warpAmountSliders;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturatorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mutateAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 3> fmSourceAttachments;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 2>, 3> warpModeAttachments;
    std::array<std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2>, 3> warpAmountAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WarpSectionComponent)
};
