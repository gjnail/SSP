#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class BitcrusherModuleComponent final : public FXModuleComponent,
                                        private juce::Timer
{
public:
    BitcrusherModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~BitcrusherModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSelectedType(int typeIndex);
    void syncTypeSelector();
    void updateDescription();
    int getSelectedType() const;

    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::Label typeLabel;
    juce::ComboBox typeSelector;
    std::array<juce::Label, 5> controlLabels;
    std::array<ModulationKnob, 5> controls;
    juce::Rectangle<int> typeDeckBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 5> knobCellBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> controlAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BitcrusherModuleComponent)
};
