#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PresetLibraryComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    explicit PresetLibraryComponent(PluginProcessor&);
    ~PresetLibraryComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct BrowserPresetItem
    {
        bool isFactory = true;
        int index = -1;
        juce::String name;
        juce::String pack;
        juce::String category;
        juce::String subCategory;
        juce::String description;
    };

    class FilterListModel;
    class PresetListModel;

    void timerCallback() override;
    void rebuildLibrary(bool preservePresetSelection);
    void rebuildPackItems();
    void rebuildCategoryItems();
    void rebuildSubCategoryItems();
    void rebuildPresetItems();
    void syncSelectionFromCurrentPreset();
    void loadSelectedPreset();
    void promptSavePreset();
    bool isUserPackSelected() const;
    juce::Colour getSelectedPackAccent() const;

    void handlePackSelection(int row);
    void handleCategorySelection(int row);
    void handleSubCategorySelection(int row);
    void handlePresetSelection(int row, bool shouldLoad);

    PluginProcessor& processor;
    bool suppressCallbacks = false;
    int selectedPackRow = 0;
    int selectedCategoryRow = 0;
    int selectedSubCategoryRow = 0;
    int selectedPresetRow = -1;
    int lastSeenUserPresetCount = -1;
    juce::String lastSeenPresetName;
    bool lastSeenPresetWasFactory = true;

    juce::StringArray packItems;
    juce::StringArray categoryItems;
    juce::StringArray subCategoryItems;
    juce::Array<BrowserPresetItem> visiblePresetItems;

    std::unique_ptr<FilterListModel> packModel;
    std::unique_ptr<FilterListModel> categoryModel;
    std::unique_ptr<FilterListModel> subCategoryModel;
    std::unique_ptr<PresetListModel> presetModel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::Label packLabel;
    juce::Label categoryLabel;
    juce::Label subCategoryLabel;
    juce::Label resultsLabel;
    juce::Label currentLabel;
    juce::Label detailNameLabel;
    juce::Label detailMetaLabel;
    juce::Label detailFooterLabel;
    juce::TextEditor detailDescriptionBox;
    juce::ListBox packListBox;
    juce::ListBox categoryListBox;
    juce::ListBox subCategoryListBox;
    juce::ListBox presetListBox;
    juce::TextButton loadButton;
    juce::TextButton saveButton;
    juce::TextButton refreshButton;
    std::unique_ptr<juce::FileChooser> saveChooser;

    juce::Rectangle<int> filterPanelBounds;
    juce::Rectangle<int> presetPanelBounds;
    juce::Rectangle<int> detailPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryComponent)
};
