#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class LfoPanelComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit LfoPanelComponent(PluginProcessor&);
    ~LfoPanelComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    class ShapeEditor;
    class DragBadge;

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subLabel;
    juce::Label selectorLabel;
    juce::Label typeLabel;
    juce::Label rateLabel;
    juce::Label triggerLabel;
    juce::Label countLabel;
    juce::ComboBox selectorBox;
    juce::ComboBox typeBox;
    juce::ComboBox triggerBox;
    juce::Slider rateSlider;
    juce::TextButton addButton;
    juce::TextButton targetsButton;
    juce::ToggleButton syncButton;
    juce::ToggleButton dottedButton;
    std::unique_ptr<DragBadge> dragBadge;
    std::unique_ptr<ShapeEditor> shapeEditor;
    int selectedLfoIndex = 0;
    bool syncing = false;

    void timerCallback() override;
    void refreshSelector();
    void syncFromProcessor();
    void refreshRouteSummary();
    void configureRateSliderForCurrentMode();
    void updateEditorDescription();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPanelComponent)
};
