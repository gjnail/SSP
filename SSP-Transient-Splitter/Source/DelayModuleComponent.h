#pragma once

#include <JuceHeader.h>
#include "FXModuleComponent.h"
#include "ModulationKnob.h"

class DelayModuleComponent final : public FXModuleComponent,
                                   private juce::Timer
{
public:
    DelayModuleComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int slotIndex);
    ~DelayModuleComponent() override = default;

    int getPreferredHeight() const override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    enum class FilterDragTarget
    {
        none,
        lowCut,
        highCut
    };

    void timerCallback() override;
    int getSelectedType() const;
    void setSelectedType(int typeIndex);
    void syncTypeSelector();
    void updateDescription();
    void updateTimeFormatting();
    void setSyncEnabled(bool shouldSync);
    void setLinkEnabled(bool shouldLink);
    bool isSyncEnabled() const;
    bool isLinkEnabled() const;
    void syncButtons();
    float getParamValue(int parameterIndex) const;
    void setParamValue(int parameterIndex, float value);
    void mirrorLinkedTime(int sourceIndex);
    void beginFilterDrag(juce::Point<float> position);
    void updateFilterDrag(juce::Point<float> position);

    juce::AudioProcessorValueTreeState& apvts;
    const int slotIndex;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton onButton;
    juce::TextButton syncButton;
    juce::TextButton linkButton;
    juce::Label typeLabel;
    juce::ComboBox typeSelector;
    std::array<juce::Label, 2> timeLabels;
    std::array<ModulationKnob, 2> timeControls;
    juce::Label feedbackLabel;
    ModulationKnob feedbackControl;
    juce::Label mixLabel;
    ModulationKnob mixControl;
    juce::Rectangle<int> rateDeckBounds;
    juce::Rectangle<int> typeDeckBounds;
    juce::Rectangle<int> filterDeckBounds;
    juce::Rectangle<int> filterPadBounds;
    juce::Rectangle<int> bottomDeckBounds;
    std::array<juce::Rectangle<int>, 2> timeCellBounds;
    std::array<juce::Rectangle<int>, 2> mixCellBounds;
    FilterDragTarget filterDragTarget = FilterDragTarget::none;
    bool suppressLinkedUpdate = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> timeAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayModuleComponent)
};
