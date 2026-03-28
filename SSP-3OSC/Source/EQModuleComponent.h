#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class PluginProcessor;

class EQModuleComponent final : public FXModuleComponent,
                                private juce::Timer
{
public:
    EQModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~EQModuleComponent() override = default;

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
    bool isBandActive(int bandIndex) const;
    void setBandActive(int bandIndex, bool shouldBeActive);
    int getBandType(int bandIndex) const;
    void setBandType(int bandIndex, int typeIndex);
    int getBandTarget(int bandIndex) const;
    void setBandTarget(int bandIndex, int targetIndex);
    float getBandFrequencyNormal(int bandIndex) const;
    float getBandGainNormal(int bandIndex) const;
    void setBandPosition(int bandIndex, float frequencyNormal, float gainNormal);
    int findBandAt(juce::Point<float> position) const;
    int findFirstInactiveBand() const;
    juce::Point<float> getBandPosition(int bandIndex) const;
    void syncTypeSelectors();
    void syncControlState();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    std::array<juce::Label, 4> bandLabels;
    std::array<juce::Label, 4> qLabels;
    std::array<juce::ComboBox, 4> targetSelectors;
    std::array<juce::ComboBox, 4> typeSelectors;
    std::array<ModulationKnob, 4> qKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> qAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    juce::Rectangle<int> graphBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 4> bandCellBounds;
    int selectedBand = -1;
    int draggingBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQModuleComponent)
};
