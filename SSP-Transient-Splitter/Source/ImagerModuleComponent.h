#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class PluginProcessor;

class ImagerModuleComponent final : public FXModuleComponent,
                                    private juce::Timer
{
public:
    ImagerModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~ImagerModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float getParamValue(int parameterIndex) const;
    void setParamValue(int parameterIndex, float value);
    std::array<float, 3> getOrderedCrossovers() const;
    int findHandleAt(juce::Point<float> position) const;
    juce::Point<float> getHandlePosition(int handleIndex) const;
    void updateHandleFromPosition(int handleIndex, juce::Point<float> position);

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    std::array<juce::Label, 4> bandLabels;
    std::array<ModulationKnob, 4> widthKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> widthAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    juce::Rectangle<int> graphBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Rectangle<int>, 4> knobCellBounds;
    int draggingHandle = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImagerModuleComponent)
};
