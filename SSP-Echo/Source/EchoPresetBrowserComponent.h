#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EchoVectorUI.h"

class EchoPresetBrowserComponent final : public juce::Component,
                                     private juce::Timer,
                                     private juce::ListBoxModel
{
public:
    explicit EchoPresetBrowserComponent(PluginProcessor&);
    ~EchoPresetBrowserComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setAnchorBounds(juce::Rectangle<int> buttonBounds, juce::Rectangle<int> graphBounds);
    void open();
    void close();
    bool isOpen() const noexcept { return visibleOrAnimating; }
    bool handleKeyPress(const juce::KeyPress&);

private:
    enum class FilterMode
    {
        all,
        userOnly,
        factoryOnly
    };

    enum class DialogMode
    {
        none,
        saveAs,
        deleteUserPreset,
        confirmReset
    };

    struct MenuItem
    {
        enum class Kind
        {
            preset,
            action,
            divider,
            placeholder
        };

        Kind kind = Kind::action;
        juce::String label;
        juce::String actionID;
        bool checked = false;
        bool enabled = true;
        bool isUserPreset = false;
        PluginProcessor::PresetRecord preset;
        std::vector<MenuItem> children;
    };

    struct MenuPanelState
    {
        std::vector<MenuItem> items;
        juce::Rectangle<float> bounds;
        float scrollOffset = 0.0f;
        int highlightedIndex = -1;
    };

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    void rebuildMenu();
    std::vector<MenuItem> buildRootMenuItems() const;
    std::vector<MenuItem> buildCategoryItems(const juce::Array<PluginProcessor::PresetRecord>& presets, int depth) const;
    void updateMenuLayout();
    void updateDialogLayout();
    void setHighlightedItem(int panelIndex, int itemIndex, bool openChildren);
    void moveHighlight(int delta);
    void openHighlightedSubmenu();
    void closeLastSubmenu();
    void activateHighlightedItem();
    int findNextSelectableIndex(const MenuPanelState&, int startIndex, int direction) const;
    float getItemHeight(const MenuItem&) const;
    float getPanelContentHeight(const MenuPanelState&) const;
    juce::Rectangle<float> getPanelContentBounds(const MenuPanelState&) const;
    juce::Rectangle<float> getItemBounds(const MenuPanelState&, int itemIndex) const;
    bool isPointInsideAnyPanel(juce::Point<float>) const;
    std::pair<int, int> hitTestItem(juce::Point<float>) const;
    void updateHoverState(juce::Point<float>);
    void clearHoverPreviewPending();
    void maybeBeginHoverPreview();
    void endPreviewIfNeeded();
    void executeMenuItem(const MenuItem&);
    void executeAction(const juce::String& actionID);
    void loadPreset(const PluginProcessor::PresetRecord&);
    void showSaveDialog();
    void showDeleteDialog();
    void showResetDialog();
    void hideDialog();
    void confirmSaveDialog();
    void confirmResetDialog();
    void deletePresetAtRow(int row);
    void launchImportChooser();
    void launchExportChooser();
    juce::Colour accentColour() const;
    juce::String categorySegment(const juce::String& category, int depth) const;
    bool presetMatchesFilter(const PluginProcessor::PresetRecord&) const;
    bool presetMatchesSearch(const PluginProcessor::PresetRecord&) const;

    PluginProcessor& processor;
    bool visibleOrAnimating = false;
    bool isClosing = false;
    float openProgress = 0.0f;
    juce::Rectangle<int> anchorBounds;
    juce::Rectangle<int> graphBounds;
    FilterMode filterMode = FilterMode::all;
    bool hoverPreviewEnabled = true;
    DialogMode dialogMode = DialogMode::none;
    std::vector<MenuPanelState> panels;
    juce::String pendingPreviewPresetKey;
    juce::uint32 pendingPreviewDeadline = 0;
    juce::ListBox deleteListBox;
    juce::Array<PluginProcessor::PresetRecord> deletableUserPresets;
    juce::Label dialogTitleLabel;
    juce::Label dialogMessageLabel;
    juce::TextEditor searchEditor;
    juce::TextEditor presetNameEditor;
    ssp::ui::SSPDropdown categoryBox;
    ssp::ui::SSPButton clearSearchButton{"Clear"};
    ssp::ui::SSPButton primaryDialogButton{"Save"};
    ssp::ui::SSPButton secondaryDialogButton{"Cancel"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String searchText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoPresetBrowserComponent)
};
