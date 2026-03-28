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
    void commitPointChange();
    void selectRelativePoint(int delta);
    void styleComboBox(juce::ComboBox& box);
    void styleLabel(juce::Label& label, const juce::String& text);
    void commitFrequencyText();
    void refreshFrequencySuggestions();
    void applySuggestion(int row);

    juce::Rectangle<int> selectedPanelBounds;
    juce::Rectangle<int> typePanelBounds;
    juce::Rectangle<int> bandPanelBounds;
    juce::Rectangle<int> statusPanelBounds;

    PluginProcessor& processor;
    int selectedPoint = -1;
    bool syncing = false;

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
    juce::Label analyzerLabel;
    juce::Label resolutionLabel;
    juce::Label decayLabel;
    juce::Label outputLabel;
    juce::Label outputReadout;

    ssp::ui::SSPDropdown typeBox;
    ssp::ui::SSPDropdown slopeBox;
    ssp::ui::SSPDropdown stereoModeBox;
    ssp::ui::SSPDropdown processingModeBox;
    ssp::ui::SSPDropdown analyzerModeBox;
    ssp::ui::SSPDropdown analyzerResolutionBox;

    ssp::ui::SSPKnob frequencySlider;
    ssp::ui::SSPKnob gainSlider;
    ssp::ui::SSPKnob qSlider;
    juce::Slider analyzerDecaySlider;
    juce::TextEditor frequencyInput;
    juce::ListBox noteSuggestionList;
    juce::StringArray noteSuggestions;

    ssp::ui::SSPToggle pointEnableToggle{"Band Enabled"};
    ssp::ui::SSPButton soloButton{"S"};
    ssp::ui::SSPToggle globalBypassToggle{"Global Bypass"};
    ssp::ui::SSPButton previousBandButton{"<"};
    ssp::ui::SSPButton nextBandButton{">"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqControlsComponent)
};
