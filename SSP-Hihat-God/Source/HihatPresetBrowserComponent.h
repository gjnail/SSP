#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class HihatPresetBrowserComponent final : public juce::Component,
                                          private juce::ListBoxModel,
                                          private juce::Timer
{
public:
    explicit HihatPresetBrowserComponent(PluginProcessor&);
    ~HihatPresetBrowserComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    void setAnchorBounds(juce::Rectangle<int> presetButtonBounds);
    void open();
    void close();
    bool isOpen() const noexcept { return visibleOrAnimating; }
    bool handleKeyPress(const juce::KeyPress&);

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void timerCallback() override;

    void rebuildFilteredPresets();
    juce::Rectangle<float> getPanelBounds() const;
    void updateChildLayout();
    void applyPresetForSelectedRow();

    PluginProcessor& processor;
    bool visibleOrAnimating = false;
    bool isClosing = false;
    float openProgress = 0.0f;
    juce::Rectangle<int> anchorBounds;
    juce::Array<int> filteredPresetIndices;
    juce::String searchText;

    juce::Label titleLabel;
    juce::Label summaryLabel;
    juce::Label resultsLabel;
    juce::TextEditor searchEditor;
    ssp::ui::SSPDropdown categoryBox;
    ssp::ui::SSPButton clearSearchButton{"Clear"};
    juce::ListBox presetList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HihatPresetBrowserComponent)
};
