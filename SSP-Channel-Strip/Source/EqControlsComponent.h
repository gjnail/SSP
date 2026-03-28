#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EqControlsComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit EqControlsComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void setSelectedPoint(int index);

    std::function<void(int)> onSelectionChanged;

private:
    void timerCallback() override;
    void refreshFromPoint();
    void updateEnabledState();
    void commitPointChange();
    void selectRelativePoint(int delta);
    void styleComboBox(juce::ComboBox& box);
    void styleLabel(juce::Label& label, const juce::String& text);

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
    juce::Label freqReadout;
    juce::Label gainReadout;
    juce::Label qReadout;
    juce::Label pointStateLabel;
    juce::Label processingLabel;
    juce::Label analyzerLabel;
    juce::Label resolutionLabel;
    juce::Label decayLabel;
    juce::Label outputLabel;
    juce::Label outputReadout;

    juce::ComboBox typeBox;
    juce::ComboBox slopeBox;
    juce::ComboBox stereoModeBox;
    juce::ComboBox processingModeBox;
    juce::ComboBox analyzerModeBox;
    juce::ComboBox analyzerResolutionBox;

    juce::Slider frequencySlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Slider analyzerDecaySlider;

    juce::ToggleButton pointEnableToggle{"Band Enabled"};
    juce::ToggleButton globalBypassToggle{"Global Bypass"};
    juce::TextButton previousBandButton{"<"};
    juce::TextButton nextBandButton{">"};
    juce::TextButton removeButton{"Remove Point"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqControlsComponent)
};
