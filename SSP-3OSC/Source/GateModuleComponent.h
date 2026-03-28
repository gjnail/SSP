#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class GateModuleComponent final : public FXModuleComponent,
                                  private juce::Timer
{
public:
    GateModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~GateModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int getPackedVariant() const;
    void setPackedVariant(int variant);
    int getSelectedType() const;
    void setSelectedType(int typeIndex);
    bool isStepEnabled(int stepIndex) const;
    void setStepEnabled(int stepIndex, bool enabled);
    int findStepAt(juce::Point<float> position) const;
    void syncTypeSelector();
    void updateDescription();
    void updateControlPresentation();
    void updateRateFormatting();

    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::Label typeLabel;
    juce::ComboBox typeSelector;
    std::array<juce::Label, 6> controlLabels;
    std::array<ModulationKnob, 6> controls;
    juce::Rectangle<int> typeDeckBounds;
    juce::Rectangle<int> stepDeckBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 16> stepCellBounds;
    std::array<juce::Rectangle<int>, 6> knobCellBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> controlAttachments;
    int presentedType = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateModuleComponent)
};
