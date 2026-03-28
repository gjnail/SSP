#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class DistortionModuleComponent final : public FXModuleComponent,
                                        private juce::Timer
{
public:
    DistortionModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~DistortionModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSelectedMode(int modeIndex);
    void syncModeSelector();
    void updateModeDescription();
    int getSelectedMode() const;

    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::Label modeLabel;
    juce::ComboBox modeSelector;
    std::array<juce::Label, 3> controlLabels;
    std::array<ModulationKnob, 3> controls;
    juce::Rectangle<float> previewBounds;
    juce::Rectangle<int> modeDeckBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 3> knobCellBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> controlAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionModuleComponent)
};
