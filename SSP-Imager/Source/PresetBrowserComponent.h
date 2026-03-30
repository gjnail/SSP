#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class PresetBrowserComponent final : public juce::Component,
                                     private juce::ListBoxModel
{
public:
    explicit PresetBrowserComponent(PluginProcessor&);
    ~PresetBrowserComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    void open();
    void close();
    void toggle();
    bool isOpen() const noexcept { return openState; }
    bool handleKeyPress(const juce::KeyPress&);

private:
    enum class FilterMode
    {
        all,
        factoryOnly,
        userOnly
    };

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    juce::Rectangle<int> getCardBounds() const;
    void rebuildVisiblePresets(bool preserveSelection);
    void syncSelectionToCurrentPreset();
    void loadVisiblePreset(int row);
    void updateFilterButtons();
    void updateSaveModeVisibility();
    void beginSaveMode();
    void cancelSaveMode();
    void confirmSavePreset();
    void deleteSelectedPreset();
    void launchImportChooser();
    void launchExportChooser();
    juce::String describePreset(const PluginProcessor::PresetRecord&) const;
    bool matchesSearch(const PluginProcessor::PresetRecord&) const;
    bool matchesFilter(const PluginProcessor::PresetRecord&) const;

    PluginProcessor& processor;
    bool openState = false;
    bool saveMode = false;
    bool ignoreSelectionChange = false;
    FilterMode filterMode = FilterMode::all;
    juce::Array<PluginProcessor::PresetRecord> visiblePresets;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label saveHintLabel;
    juce::TextEditor searchEditor;
    juce::ListBox presetListBox;
    juce::TextEditor presetNameEditor;
    ssp::ui::SSPDropdown categoryBox;

    ssp::ui::SSPButton allFilterButton{"ALL"};
    ssp::ui::SSPButton factoryFilterButton{"FACTORY"};
    ssp::ui::SSPButton userFilterButton{"USER"};
    ssp::ui::SSPButton saveAsButton{"SAVE AS"};
    ssp::ui::SSPButton deleteButton{"DELETE"};
    ssp::ui::SSPButton importButton{"IMPORT"};
    ssp::ui::SSPButton exportButton{"EXPORT"};
    ssp::ui::SSPButton resetButton{"RESET"};
    ssp::ui::SSPButton confirmSaveButton{"SAVE"};
    ssp::ui::SSPButton cancelSaveButton{"CANCEL"};
    ssp::ui::SSPButton closeButton{"CLOSE"};

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserComponent)
};
