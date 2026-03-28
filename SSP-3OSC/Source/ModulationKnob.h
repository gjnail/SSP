#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ReactorUI.h"

class ModulationKnob final : public juce::Slider,
                             public juce::DragAndDropTarget,
                             private juce::Timer
{
public:
    ModulationKnob() = default;
    ModulationKnob(PluginProcessor& processor,
                   juce::String parameterID,
                   reactormod::Destination destination,
                   juce::Colour accent,
                   int textBoxWidth = 68,
                   int textBoxHeight = 22);
    ~ModulationKnob() override = default;

    void setupModulation(PluginProcessor& processor,
                         juce::String parameterID,
                         reactormod::Destination destination,
                         juce::Colour accent,
                         int textBoxWidth = 68,
                         int textBoxHeight = 22);

    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void paintOverChildren(juce::Graphics& g) override;

    void refreshModulationState();

private:
    void timerCallback() override;
    bool isLfoDragDescription(const juce::var& description) const;
    int sourceIndexFromDescription(const juce::var& description) const;
    juce::Rectangle<float> getKnobBounds() const;
    juce::Rectangle<float> getTargetChipBounds() const;
    bool shouldShowTargetChip() const;
    void updateTargetHover(const juce::Point<float>& position);
    void applyTargetAmount(float amount);
    void initialiseValueLabel();
    void setValueLabelVisible(bool shouldBeVisible);
    void showValueLabelTemporarily(int durationMs = 900);

    PluginProcessor* processor = nullptr;
    juce::String parameterID;
    reactormod::Destination destination = reactormod::Destination::none;
    PluginProcessor::DestinationModulationInfo modulationInfo;
    bool dragActive = false;
    bool dragAccepting = false;
    int dragSourceIndex = 0;
    bool targetHovered = false;
    bool editingTargetDepth = false;
    float targetDragStartAmount = 0.0f;
    juce::Point<float> targetDragStartPosition;
    juce::Label* valueLabel = nullptr;
    uint32_t valueLabelHideTimeMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationKnob)
};
