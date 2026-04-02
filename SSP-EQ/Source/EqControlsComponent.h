#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class EqControlsComponent final : public juce::Component,
                                  private juce::Timer,
                                  private juce::ListBoxModel
{
public:
    explicit EqControlsComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void setSelectedPoint(int index);
    void focusFrequencyInput();

    std::function<void(int)> onSelectionChanged;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override;
    void timerCallback() override;
    void refreshFromPoint();
    void updateEnabledState();
    void updateSectionVisibility();
    void commitPointChange();
    void selectRelativePoint(int delta);
    void styleComboBox(juce::ComboBox& box);
    void styleLabel(juce::Label& label, const juce::String& text);
    void commitFrequencyText();
    void refreshFrequencySuggestions();
    void applySuggestion(int row);
    void commitDynamicDirection(int direction);
    void updateDynamicSectionVisibility();
    bool hasSelectedBand() const;
    bool selectedBandIsDynamic() const;

    juce::Rectangle<int> selectedPanelBounds;
    juce::Rectangle<int> typePanelBounds;
    juce::Rectangle<int> bandPanelBounds;
    juce::Rectangle<int> dynamicPanelBounds;
    juce::Rectangle<int> statusPanelBounds;
    juce::Rectangle<int> gainReductionMeterBounds;

    PluginProcessor& processor;
    int selectedPoint = -1;
    bool syncing = false;
    float dynamicSectionAmount = 0.0f;
    float dynamicSectionTarget = 0.0f;

    juce::Label selectedLabel;
    juce::Label helperLabel;
    juce::Label typeLabel;
    juce::Label slopeLabel;
    juce::Label stereoLabel;
    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label gainReadout;
    juce::Label qReadout;
    juce::Label pointStateLabel;
    juce::Label processingLabel;
    juce::Label latencyLabel;
    juce::Label analyzerLabel;
    juce::Label resolutionLabel;
    juce::Label decayLabel;
    juce::Label outputLabel;
    juce::Label outputReadout;
    juce::Label dynamicSectionLabel;
    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label kneeLabel;
    juce::Label rangeLabel;
    juce::Label sidechainSourceLabel;
    juce::Label sidechainFilterLabel;
    juce::Label gainReductionLabel;
    juce::Label latencyReadout;
    juce::Label sidechainHighPassReadout;
    juce::Label sidechainLowPassReadout;

    ssp::ui::SSPDropdown typeBox;
    ssp::ui::SSPDropdown slopeBox;
    ssp::ui::SSPDropdown stereoModeBox;
    ssp::ui::SSPDropdown processingModeBox;
    ssp::ui::SSPDropdown analyzerModeBox;
    ssp::ui::SSPDropdown analyzerResolutionBox;
    ssp::ui::SSPDropdown sidechainSourceBox;
    ssp::ui::SSPDropdown sidechainBandBox;

    ssp::ui::SSPKnob frequencySlider;
    ssp::ui::SSPKnob gainSlider;
    ssp::ui::SSPKnob qSlider;
    ssp::ui::SSPKnob thresholdSlider;
    ssp::ui::SSPKnob ratioSlider;
    ssp::ui::SSPKnob attackSlider;
    ssp::ui::SSPKnob releaseSlider;
    ssp::ui::SSPKnob kneeSlider;
    ssp::ui::SSPKnob rangeSlider;
    ssp::ui::SSPKnob sidechainHighPassSlider;
    ssp::ui::SSPKnob sidechainLowPassSlider;
    juce::Slider analyzerDecaySlider;
    juce::TextEditor frequencyInput;
    juce::ListBox noteSuggestionList;
    juce::StringArray noteSuggestions;

    ssp::ui::SSPToggle pointEnableToggle{"Band Enabled"};
    ssp::ui::SSPButton soloButton{"S"};
    ssp::ui::SSPButton dynamicToggle{"DYN"};
    ssp::ui::SSPButton dynamicAboveButton{"Above"};
    ssp::ui::SSPButton dynamicBelowButton{"Below"};
    ssp::ui::SSPButton sidechainListenButton{"SC Listen"};
    ssp::ui::SSPToggle globalBypassToggle{"Global Bypass"};
    ssp::ui::SSPButton previousBandButton{"<"};
    ssp::ui::SSPButton nextBandButton{">"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqControlsComponent)
};
