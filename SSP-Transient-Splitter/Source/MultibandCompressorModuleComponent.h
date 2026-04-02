#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class PluginProcessor;

class MultibandCompressorModuleComponent final : public FXModuleComponent,
                                                 private juce::Timer
{
public:
    MultibandCompressorModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~MultibandCompressorModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateMeterDisplays();
    float getBandValue(int parameterIndex) const;
    void setBandValue(int parameterIndex, float value);
    std::pair<int, bool> findHandleAt(juce::Point<float> position) const;
    juce::Point<float> getHandlePosition(int bandIndex, bool upward) const;

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::Rectangle<int> graphBounds;
    juce::Rectangle<int> controlDeckBounds;
    std::array<juce::Label, 4> controlLabels;
    std::array<ModulationKnob, 4> controls;
    std::array<juce::Rectangle<int>, 4> knobCellBounds;
    std::array<float, 3> displayedUpwardMeters{};
    std::array<float, 3> displayedDownwardMeters{};
    std::array<float, 3> peakUpwardMeters{};
    std::array<float, 3> peakDownwardMeters{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> controlAttachments;
    int draggingBand = -1;
    bool draggingUpward = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandCompressorModuleComponent)
};
