#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../../SSP-Reverb/Source/ReverbVectorUI.h"

class ClipperPresetBrowserComponent final : public juce::Component,
                                            private juce::ListBoxModel
{
public:
    explicit ClipperPresetBrowserComponent(PluginProcessor&);
    ~ClipperPresetBrowserComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    void setAnchorBounds(juce::Rectangle<int>);
    void open();
    void close();
    bool isOpen() const noexcept { return isVisible(); }

private:
    struct RowItem
    {
        bool isFactory = true;
        int index = -1;
        PluginProcessor::PresetRecord preset;
    };

    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void listBoxItemClicked(int, const juce::MouseEvent&) override;

    void refreshRows();
    void loadSelectedPreset();
    void saveCurrentPreset();
    void deleteSelectedPreset();
    void importPreset();
    void exportPreset();

    PluginProcessor& processor;
    juce::Rectangle<int> anchorBounds;
    juce::Array<RowItem> rows;
    juce::ListBox listBox;
    juce::TextEditor searchEditor;
    juce::TextEditor presetNameEditor;
    reverbui::SSPDropdown categoryBox;
    reverbui::SSPButton saveButton{"SAVE"};
    reverbui::SSPButton deleteButton{"DELETE"};
    reverbui::SSPButton importButton{"IMPORT"};
    reverbui::SSPButton exportButton{"EXPORT"};
    reverbui::SSPButton closeButton{"CLOSE"};
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipperPresetBrowserComponent)
};
