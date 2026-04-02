#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class ChorusModuleComponent final : public FXModuleComponent,
                                    private juce::Timer
{
public:
    ChorusModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~ChorusModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSyncEnabled(bool shouldSync);
    bool isSyncEnabled() const;
    void syncSyncButton();
    void updateRateFormatting();

    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::TextButton syncButton;
    std::array<juce::Label, 5> controlLabels;
    std::array<ModulationKnob, 5> controls;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 5> knobCellBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> controlAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusModuleComponent)
};
