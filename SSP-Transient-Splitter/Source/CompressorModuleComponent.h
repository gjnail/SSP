#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class PluginProcessor;

class CompressorModuleComponent final : public FXModuleComponent,
                                        private juce::Timer
{
public:
    CompressorModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~CompressorModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int getSelectedRatio() const;
    void setSelectedRatio(int ratioIndex);
    void syncRatioButtons();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::Label ratioLabel;
    juce::Rectangle<int> meterBounds;
    juce::Rectangle<int> ratioDeckBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::TextButton, 5> ratioButtons;
    std::array<juce::Label, 5> controlLabels;
    std::array<ModulationKnob, 5> controls;
    std::array<juce::Rectangle<int>, 5> knobCellBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> controlAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorModuleComponent)
};
