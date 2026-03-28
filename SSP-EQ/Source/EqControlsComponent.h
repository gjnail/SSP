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
    void commitTypeChange();
    void commitQChange();
    void updateEnabledState();
    juce::Rectangle<int> typePanelBounds;
    juce::Rectangle<int> qPanelBounds;
    juce::Rectangle<int> actionPanelBounds;

    PluginProcessor& processor;
    int selectedPoint = -1;
    bool syncing = false;

    juce::Label selectedLabel;
    juce::Label typeLabel;
    juce::Label qLabel;
    juce::Label helperLabel;
    juce::ComboBox typeBox;
    juce::Slider qSlider;
    juce::TextButton removeButton{"Remove Point"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqControlsComponent)
};
