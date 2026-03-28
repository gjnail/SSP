#include "PresetLibraryComponent.h"
#include "ReactorUI.h"

namespace
{
constexpr auto allFactoryPacksLabel = "All Factory Packs";
constexpr auto userPresetsLabel = "User Presets";
constexpr auto allCategoriesLabel = "All Categories";
constexpr auto allSubCategoriesLabel = "All Subcategories";

void styleLibraryLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(size >= 14.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(just);
}

void styleLibraryButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141a20));
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
}

void styleSearchEditor(juce::TextEditor& editor, juce::Colour accent)
{
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff10161d));
    editor.setColour(juce::TextEditor::outlineColourId, reactorui::outline());
    editor.setColour(juce::TextEditor::focusedOutlineColourId, accent);
    editor.setColour(juce::TextEditor::textColourId, reactorui::textStrong());
    editor.setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.22f));
    editor.setColour(juce::TextEditor::highlightedTextColourId, reactorui::textStrong());
    editor.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    editor.setTextToShowWhenEmpty("Search presets, categories, or styles", reactorui::textMuted().withAlpha(0.7f));
    editor.setIndents(10, 8);
}

void styleInfoEditor(juce::TextEditor& editor)
{
    editor.setMultiLine(true, true);
    editor.setReadOnly(true);
    editor.setCaretVisible(false);
    editor.setScrollbarsShown(true);
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff10161d));
    editor.setColour(juce::TextEditor::outlineColourId, reactorui::outline());
    editor.setColour(juce::TextEditor::focusedOutlineColourId, reactorui::outline());
    editor.setColour(juce::TextEditor::textColourId, reactorui::textMuted());
    editor.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    editor.setIndents(10, 10);
    editor.setFont(reactorui::bodyFont(12.0f));
}

void styleListBox(juce::ListBox& listBox)
{
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
}

juce::Colour accentForPack(const juce::String& pack)
{
    if (pack == "Skyline Mainroom")
        return juce::Colour(0xffffc15c);
    if (pack == "Midnight Circuits")
        return juce::Colour(0xff4ec9be);
    if (pack == "Rhythm Borough")
        return juce::Colour(0xffff9a5e);
    if (pack == "Pressure Engine")
        return juce::Colour(0xfff0654c);
    if (pack == userPresetsLabel)
        return juce::Colour(0xffff7f63);

    return juce::Colour(0xff97d16f);
}

template <typename ArrayType>
void addIfMissing(ArrayType& array, const juce::String& value)
{
    if (value.isNotEmpty() && ! array.contains(value))
        array.add(value);
}
}

class PresetLibraryComponent::FilterListModel final : public juce::ListBoxModel
{
public:
    enum class Kind
    {
        packs,
        categories,
        subCategories
    };

    FilterListModel(PresetLibraryComponent& componentToControl, Kind kindToUse)
        : owner(componentToControl), kind(kindToUse)
    {
    }

    int getNumRows() override
    {
        return getItems().size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        const auto& items = getItems();
        if (! juce::isPositiveAndBelow(rowNumber, items.size()))
            return;

        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(2.0f, 3.0f);
        const auto text = items[rowNumber];
        const auto accent = kind == Kind::packs ? accentForPack(text) : owner.getSelectedPackAccent();

        if (rowIsSelected)
        {
            juce::ColourGradient fill(juce::Colour(0xff1d232a), bounds.getTopLeft(),
                                      juce::Colour(0xff11161c), bounds.getBottomLeft(), false);
            fill.addColour(0.10, accent.withAlpha(0.20f));
            g.setGradientFill(fill);
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(accent.withAlpha(0.95f));
            auto accentStrip = bounds;
            g.fillRoundedRectangle(accentStrip.removeFromLeft(4.0f), 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(0x12000000));
            g.fillRoundedRectangle(bounds, 6.0f);
        }

        g.setColour(juce::Colour(0xff060708));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
        g.setColour(rowIsSelected ? reactorui::textStrong() : reactorui::textMuted());
        g.setFont(kind == Kind::packs ? reactorui::sectionFont(12.5f) : reactorui::bodyFont(12.0f));
        g.drawFittedText(text, bounds.getSmallestIntegerContainer().reduced(12, 0), juce::Justification::centredLeft, 1);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        switch (kind)
        {
            case Kind::packs: owner.handlePackSelection(lastRowSelected); break;
            case Kind::categories: owner.handleCategorySelection(lastRowSelected); break;
            case Kind::subCategories: owner.handleSubCategorySelection(lastRowSelected); break;
        }
    }

private:
    const juce::StringArray& getItems() const
    {
        switch (kind)
        {
            case Kind::packs: return owner.packItems;
            case Kind::categories: return owner.categoryItems;
            case Kind::subCategories: return owner.subCategoryItems;
        }

        return owner.packItems;
    }

    PresetLibraryComponent& owner;
    Kind kind;
};

class PresetLibraryComponent::PresetListModel final : public juce::ListBoxModel
{
public:
    explicit PresetListModel(PresetLibraryComponent& componentToControl)
        : owner(componentToControl)
    {
    }

    int getNumRows() override
    {
        return owner.visiblePresetItems.size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow(rowNumber, owner.visiblePresetItems.size()))
            return;

        const auto& item = owner.visiblePresetItems.getReference(rowNumber);
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(4.0f, 3.0f);
        const auto accent = accentForPack(item.isFactory ? item.pack : userPresetsLabel);

        juce::ColourGradient fill(juce::Colour(0xff171b20), bounds.getTopLeft(),
                                  juce::Colour(0xff111419), bounds.getBottomLeft(), false);
        if (rowIsSelected)
            fill.addColour(0.12, accent.withAlpha(0.20f));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 7.0f);
        g.setColour(juce::Colour(0xff050607));
        g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
        if (rowIsSelected)
        {
            g.setColour(accent.withAlpha(0.95f));
            auto accentStrip = bounds;
            g.fillRoundedRectangle(accentStrip.removeFromLeft(4.0f), 2.0f);
        }

        auto content = bounds.getSmallestIntegerContainer().reduced(14, 7);
        auto top = content.removeFromTop(18);
        auto meta = content.removeFromTop(16);

        g.setColour(reactorui::textStrong());
        g.setFont(reactorui::sectionFont(13.0f));
        g.drawFittedText(item.name, top, juce::Justification::centredLeft, 1);

        g.setColour(rowIsSelected ? accent.brighter(0.16f) : reactorui::textMuted());
        g.setFont(reactorui::bodyFont(11.0f));
        const auto metaText = item.isFactory
            ? item.category + " / " + item.subCategory + " / " + item.pack
            : "USER / SAVED / CUSTOM";
        g.drawFittedText(metaText, meta, juce::Justification::centredLeft, 1);

        g.setColour(reactorui::textMuted().withAlpha(0.74f));
        g.setFont(reactorui::bodyFont(10.5f));
        g.drawFittedText(item.description, content, juce::Justification::centredLeft, 1);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        owner.handlePresetSelection(lastRowSelected, false);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        owner.handlePresetSelection(row, true);
    }

private:
    PresetLibraryComponent& owner;
};

PresetLibraryComponent::~PresetLibraryComponent() = default;

PresetLibraryComponent::PresetLibraryComponent(PluginProcessor& p)
    : processor(p)
{
    packModel = std::make_unique<FilterListModel>(*this, FilterListModel::Kind::packs);
    categoryModel = std::make_unique<FilterListModel>(*this, FilterListModel::Kind::categories);
    subCategoryModel = std::make_unique<FilterListModel>(*this, FilterListModel::Kind::subCategories);
    presetModel = std::make_unique<PresetListModel>(*this);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(searchLabel);
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(packLabel);
    addAndMakeVisible(categoryLabel);
    addAndMakeVisible(subCategoryLabel);
    addAndMakeVisible(resultsLabel);
    addAndMakeVisible(currentLabel);
    addAndMakeVisible(detailNameLabel);
    addAndMakeVisible(detailMetaLabel);
    addAndMakeVisible(detailDescriptionBox);
    addAndMakeVisible(detailFooterLabel);
    addAndMakeVisible(packListBox);
    addAndMakeVisible(categoryListBox);
    addAndMakeVisible(subCategoryListBox);
    addAndMakeVisible(presetListBox);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(refreshButton);

    titleLabel.setText("PRESET LIBRARY", juce::dontSendNotification);
    subtitleLabel.setText("Browser for SSP 3OSC's 4 genre-focused EDM factory packs, 200 original presets, and your own saved patches.", juce::dontSendNotification);
    searchLabel.setText("Search", juce::dontSendNotification);
    packLabel.setText("Packs", juce::dontSendNotification);
    categoryLabel.setText("Categories", juce::dontSendNotification);
    subCategoryLabel.setText("Subcategories", juce::dontSendNotification);
    resultsLabel.setText("Library", juce::dontSendNotification);
    currentLabel.setText("Loaded Preset", juce::dontSendNotification);
    detailNameLabel.setText("Select a preset", juce::dontSendNotification);
    detailMetaLabel.setText("Pack / Category / Subcategory", juce::dontSendNotification);
    detailFooterLabel.setText("Double-click a preset row or use Load Selected.", juce::dontSendNotification);

    styleLibraryLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLibraryLabel(subtitleLabel, 11.5f, reactorui::textMuted());
    styleLibraryLabel(searchLabel, 11.5f, reactorui::textMuted());
    styleLibraryLabel(packLabel, 12.5f, reactorui::textStrong());
    styleLibraryLabel(categoryLabel, 12.5f, reactorui::textStrong());
    styleLibraryLabel(subCategoryLabel, 12.5f, reactorui::textStrong());
    styleLibraryLabel(resultsLabel, 12.5f, reactorui::textStrong());
    styleLibraryLabel(currentLabel, 11.0f, reactorui::textMuted());
    styleLibraryLabel(detailNameLabel, 16.0f, reactorui::textStrong());
    styleLibraryLabel(detailMetaLabel, 11.5f, reactorui::textMuted());
    styleLibraryLabel(detailFooterLabel, 11.0f, reactorui::textMuted());

    searchLabel.setJustificationType(juce::Justification::centredRight);
    detailMetaLabel.setJustificationType(juce::Justification::topLeft);
    detailFooterLabel.setJustificationType(juce::Justification::topLeft);

    styleSearchEditor(searchEditor, juce::Colour(0xff9cff67));
    styleInfoEditor(detailDescriptionBox);

    packListBox.setModel(packModel.get());
    categoryListBox.setModel(categoryModel.get());
    subCategoryListBox.setModel(subCategoryModel.get());
    presetListBox.setModel(presetModel.get());
    packListBox.setRowHeight(30);
    categoryListBox.setRowHeight(28);
    subCategoryListBox.setRowHeight(28);
    presetListBox.setRowHeight(58);
    styleListBox(packListBox);
    styleListBox(categoryListBox);
    styleListBox(subCategoryListBox);
    styleListBox(presetListBox);

    loadButton.setButtonText("Load Selected");
    saveButton.setButtonText("Save User");
    refreshButton.setButtonText("Rescan");
    styleLibraryButton(loadButton, juce::Colour(0xff9cff67));
    styleLibraryButton(saveButton, juce::Colour(0xffffc95b));
    styleLibraryButton(refreshButton, juce::Colour(0xff78c9ff));

    searchEditor.onTextChange = [this] { rebuildLibrary(true); };
    loadButton.onClick = [this] { loadSelectedPreset(); };
    saveButton.onClick = [this] { promptSavePreset(); };
    refreshButton.onClick = [this]
    {
        processor.refreshUserPresets();
        rebuildLibrary(true);
    };

    rebuildLibrary(false);
    startTimerHz(3);
}

void PresetLibraryComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff8cff6a), 14.0f);

    if (! filterPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, filterPanelBounds.toFloat(), juce::Colour(0xffffc95b), 12.0f);

    if (! presetPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, presetPanelBounds.toFloat(), juce::Colour(0xff7fd1ff), 12.0f);

    if (! detailPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, detailPanelBounds.toFloat(), getSelectedPackAccent(), 12.0f);
}

void PresetLibraryComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);
    auto topRow = area.removeFromTop(46);
    auto titleArea = topRow.removeFromLeft(420);
    titleLabel.setBounds(titleArea.removeFromTop(20));
    subtitleLabel.setBounds(titleArea.removeFromTop(18));

    auto searchArea = topRow.removeFromRight(420);
    searchLabel.setBounds(searchArea.removeFromLeft(54));
    searchArea.removeFromLeft(10);
    searchEditor.setBounds(searchArea.removeFromTop(32));

    area.removeFromTop(12);
    detailPanelBounds = area.removeFromRight(312);
    area.removeFromRight(14);
    filterPanelBounds = area.removeFromLeft(286);
    area.removeFromLeft(14);
    presetPanelBounds = area;

    auto filterArea = filterPanelBounds.reduced(14, 12);
    packLabel.setBounds(filterArea.removeFromTop(18));
    packListBox.setBounds(filterArea.removeFromTop(170));
    filterArea.removeFromTop(10);
    categoryLabel.setBounds(filterArea.removeFromTop(18));
    categoryListBox.setBounds(filterArea.removeFromTop(150));
    filterArea.removeFromTop(10);
    subCategoryLabel.setBounds(filterArea.removeFromTop(18));
    subCategoryListBox.setBounds(filterArea);

    auto presetArea = presetPanelBounds.reduced(14, 12);
    auto presetTop = presetArea.removeFromTop(22);
    resultsLabel.setBounds(presetTop.removeFromLeft(240));
    presetArea.removeFromTop(8);
    presetListBox.setBounds(presetArea);

    auto detailArea = detailPanelBounds.reduced(14, 12);
    currentLabel.setBounds(detailArea.removeFromTop(18));
    detailNameLabel.setBounds(detailArea.removeFromTop(24));
    detailMetaLabel.setBounds(detailArea.removeFromTop(44));
    detailArea.removeFromTop(8);
    auto buttonRow = detailArea.removeFromTop(34);
    loadButton.setBounds(buttonRow.removeFromLeft(118));
    buttonRow.removeFromLeft(8);
    saveButton.setBounds(buttonRow.removeFromLeft(92));
    buttonRow.removeFromLeft(8);
    refreshButton.setBounds(buttonRow);
    detailArea.removeFromTop(10);
    detailDescriptionBox.setBounds(detailArea.removeFromTop(168));
    detailArea.removeFromTop(8);
    detailFooterLabel.setBounds(detailArea);
}

void PresetLibraryComponent::timerCallback()
{
    const int userCount = processor.getUserPresetNames().size();
    const auto currentName = processor.getCurrentPresetName();
    const bool isFactory = processor.isCurrentPresetFactory();
    if (userCount != lastSeenUserPresetCount
        || currentName != lastSeenPresetName
        || isFactory != lastSeenPresetWasFactory)
    {
        rebuildLibrary(true);
    }
}

void PresetLibraryComponent::rebuildLibrary(bool preservePresetSelection)
{
    suppressCallbacks = true;

    BrowserPresetItem previousSelection;
    bool hadPreviousSelection = false;
    if (preservePresetSelection && juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size()))
    {
        previousSelection = visiblePresetItems.getReference(selectedPresetRow);
        hadPreviousSelection = true;
    }

    processor.refreshUserPresets();
    rebuildPackItems();
    rebuildCategoryItems();
    rebuildSubCategoryItems();
    rebuildPresetItems();

    if (hadPreviousSelection)
    {
        int matchedRow = -1;
        for (int i = 0; i < visiblePresetItems.size(); ++i)
        {
            const auto& item = visiblePresetItems.getReference(i);
            if (item.isFactory == previousSelection.isFactory && item.name == previousSelection.name)
            {
                matchedRow = i;
                break;
            }
        }
        selectedPresetRow = matchedRow;
    }

    if (! juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size()))
    {
        if (searchEditor.getText().trim().isEmpty())
            syncSelectionFromCurrentPreset();
        else if (! visiblePresetItems.isEmpty())
            selectedPresetRow = 0;
    }

    packListBox.updateContent();
    categoryListBox.updateContent();
    subCategoryListBox.updateContent();
    presetListBox.updateContent();
    packListBox.selectRow(selectedPackRow, false, true);
    categoryListBox.selectRow(selectedCategoryRow, false, true);
    subCategoryListBox.selectRow(selectedSubCategoryRow, false, true);
    presetListBox.selectRow(selectedPresetRow, false, true);

    if (juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size()))
    {
        const auto& item = visiblePresetItems.getReference(selectedPresetRow);
        currentLabel.setText("Loaded: " + processor.getCurrentPresetName(), juce::dontSendNotification);
        detailNameLabel.setText(item.name, juce::dontSendNotification);
        detailMetaLabel.setText(item.isFactory
                                    ? item.pack + "\n" + item.category + " / " + item.subCategory
                                    : "USER BANK\nSaved / Custom",
                                juce::dontSendNotification);
        detailDescriptionBox.setText(item.description, false);
        detailFooterLabel.setText(item.isFactory
                                      ? "Factory pack: " + item.pack + ". Double-click in the preset list to load instantly."
                                      : "User preset from your Reactor library. Save over it from the quick bar or create a new version here.",
                                  juce::dontSendNotification);
    }
    else
    {
        currentLabel.setText("Loaded: " + processor.getCurrentPresetName(), juce::dontSendNotification);
        detailNameLabel.setText("No Preset Found", juce::dontSendNotification);
        detailMetaLabel.setText("Adjust the search or filter columns.", juce::dontSendNotification);
        detailDescriptionBox.setText("There are no presets matching the current pack, category, subcategory, and search settings.", false);
        detailFooterLabel.setText("Try clearing the search or switching back to All Factory Packs.", juce::dontSendNotification);
    }

    loadButton.setEnabled(juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size()));
    resultsLabel.setText("Results: " + juce::String(visiblePresetItems.size()) + " presets", juce::dontSendNotification);

    lastSeenUserPresetCount = processor.getUserPresetNames().size();
    lastSeenPresetName = processor.getCurrentPresetName();
    lastSeenPresetWasFactory = processor.isCurrentPresetFactory();
    suppressCallbacks = false;
    repaint();
}

void PresetLibraryComponent::rebuildPackItems()
{
    packItems.clear();
    packItems.add(allFactoryPacksLabel);
    for (const auto& item : PluginProcessor::getFactoryPresetLibrary())
        addIfMissing(packItems, item.pack);
    packItems.add(userPresetsLabel);

    selectedPackRow = juce::jlimit(0, juce::jmax(0, packItems.size() - 1), selectedPackRow);
}

void PresetLibraryComponent::rebuildCategoryItems()
{
    categoryItems.clear();

    if (isUserPackSelected())
    {
        categoryItems.add("All User Presets");
        selectedCategoryRow = 0;
        return;
    }

    categoryItems.add(allCategoriesLabel);
    const auto selectedPack = packItems[selectedPackRow];
    for (const auto& item : PluginProcessor::getFactoryPresetLibrary())
    {
        if (selectedPack != allFactoryPacksLabel && item.pack != selectedPack)
            continue;
        addIfMissing(categoryItems, item.category);
    }

    selectedCategoryRow = juce::jlimit(0, juce::jmax(0, categoryItems.size() - 1), selectedCategoryRow);
}

void PresetLibraryComponent::rebuildSubCategoryItems()
{
    subCategoryItems.clear();

    if (isUserPackSelected())
    {
        subCategoryItems.add("All Saved Types");
        selectedSubCategoryRow = 0;
        return;
    }

    subCategoryItems.add(allSubCategoriesLabel);
    const auto selectedPack = packItems[selectedPackRow];
    const auto selectedCategory = categoryItems[selectedCategoryRow];

    for (const auto& item : PluginProcessor::getFactoryPresetLibrary())
    {
        if (selectedPack != allFactoryPacksLabel && item.pack != selectedPack)
            continue;
        if (selectedCategory != allCategoriesLabel && item.category != selectedCategory)
            continue;
        addIfMissing(subCategoryItems, item.subCategory);
    }

    selectedSubCategoryRow = juce::jlimit(0, juce::jmax(0, subCategoryItems.size() - 1), selectedSubCategoryRow);
}

void PresetLibraryComponent::rebuildPresetItems()
{
    const auto previousName = juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size())
        ? visiblePresetItems.getReference(selectedPresetRow).name
        : juce::String();
    const auto previousIsFactory = juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size())
        ? visiblePresetItems.getReference(selectedPresetRow).isFactory
        : true;

    visiblePresetItems.clearQuick();
    const auto selectedPack = packItems[selectedPackRow];
    const auto selectedCategory = categoryItems[selectedCategoryRow];
    const auto selectedSubCategory = subCategoryItems[selectedSubCategoryRow];
    const auto needle = searchEditor.getText().trim().toLowerCase();

    auto matchesSearch = [&needle](const BrowserPresetItem& item)
    {
        if (needle.isEmpty())
            return true;

        const auto haystack = (item.name + " " + item.pack + " " + item.category + " "
                               + item.subCategory + " " + item.description).toLowerCase();
        return haystack.contains(needle);
    };

    if (isUserPackSelected())
    {
        const auto& userPresets = processor.getUserPresetNames();
        for (int i = 0; i < userPresets.size(); ++i)
        {
            BrowserPresetItem item;
            item.isFactory = false;
            item.index = i;
            item.name = userPresets[i];
            item.pack = userPresetsLabel;
            item.category = "Saved";
            item.subCategory = "Custom";
            item.description = "User-saved Reactor patch stored in your personal library.";
            if (matchesSearch(item))
                visiblePresetItems.add(item);
        }
    }
    else
    {
        for (const auto& meta : PluginProcessor::getFactoryPresetLibrary())
        {
            if (selectedPack != allFactoryPacksLabel && meta.pack != selectedPack)
                continue;
            if (selectedCategory != allCategoriesLabel && meta.category != selectedCategory)
                continue;
            if (selectedSubCategory != allSubCategoriesLabel && meta.subCategory != selectedSubCategory)
                continue;

            BrowserPresetItem item;
            item.isFactory = true;
            item.index = meta.index;
            item.name = meta.name;
            item.pack = meta.pack;
            item.category = meta.category;
            item.subCategory = meta.subCategory;
            item.description = meta.description;
            if (matchesSearch(item))
                visiblePresetItems.add(item);
        }
    }

    selectedPresetRow = -1;
    if (previousName.isNotEmpty())
    {
        for (int i = 0; i < visiblePresetItems.size(); ++i)
        {
            const auto& item = visiblePresetItems.getReference(i);
            if (item.name == previousName && item.isFactory == previousIsFactory)
            {
                selectedPresetRow = i;
                break;
            }
        }
    }

    if (selectedPresetRow < 0 && ! visiblePresetItems.isEmpty())
        selectedPresetRow = 0;
}

void PresetLibraryComponent::syncSelectionFromCurrentPreset()
{
    const auto currentName = processor.getCurrentPresetName();

    if (processor.isCurrentPresetFactory())
    {
        const auto& library = PluginProcessor::getFactoryPresetLibrary();
        for (const auto& meta : library)
        {
            if (meta.name != currentName)
                continue;

            selectedPackRow = juce::jmax(0, packItems.indexOf(meta.pack));
            rebuildCategoryItems();
            selectedCategoryRow = juce::jmax(0, categoryItems.indexOf(meta.category));
            rebuildSubCategoryItems();
            selectedSubCategoryRow = juce::jmax(0, subCategoryItems.indexOf(meta.subCategory));
            rebuildPresetItems();

            for (int i = 0; i < visiblePresetItems.size(); ++i)
                if (visiblePresetItems.getReference(i).name == meta.name && visiblePresetItems.getReference(i).isFactory)
                    selectedPresetRow = i;
            return;
        }
    }
    else
    {
        selectedPackRow = juce::jmax(0, packItems.indexOf(userPresetsLabel));
        rebuildCategoryItems();
        rebuildSubCategoryItems();
        rebuildPresetItems();

        for (int i = 0; i < visiblePresetItems.size(); ++i)
            if (visiblePresetItems.getReference(i).name == currentName && ! visiblePresetItems.getReference(i).isFactory)
                selectedPresetRow = i;
    }
}

void PresetLibraryComponent::loadSelectedPreset()
{
    if (! juce::isPositiveAndBelow(selectedPresetRow, visiblePresetItems.size()))
        return;

    const auto& item = visiblePresetItems.getReference(selectedPresetRow);
    if (item.isFactory)
        processor.loadFactoryPreset(item.index);
    else
        processor.loadUserPreset(item.index);

    rebuildLibrary(true);
}

void PresetLibraryComponent::promptSavePreset()
{
    auto suggestedFile = processor.getUserPresetDirectory().getChildFile(processor.getCurrentPresetName().isNotEmpty()
                                                                             ? processor.getCurrentPresetName() + ".sspreset"
                                                                             : "New Reactor Patch.sspreset");
    saveChooser = std::make_unique<juce::FileChooser>("Save SSP Reactor Preset", suggestedFile, "*.sspreset");
    auto* chooserPtr = saveChooser.get();
    juce::Component::SafePointer<PresetLibraryComponent> safeThis(this);
    saveChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [safeThis, chooserPtr](const juce::FileChooser&)
                             {
                                 if (safeThis == nullptr || chooserPtr == nullptr)
                                     return;

                                 const auto result = chooserPtr->getResult();
                                 safeThis->saveChooser.reset();
                                 if (result == juce::File{})
                                     return;

                                 if (! safeThis->processor.saveUserPreset(result.getFileNameWithoutExtension()))
                                 {
                                     juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                                 "Preset Save Failed",
                                                                                 "Reactor couldn't save that preset. Try a different name.");
                                     return;
                                 }

                                 safeThis->rebuildLibrary(true);
                             });
}

bool PresetLibraryComponent::isUserPackSelected() const
{
    return juce::isPositiveAndBelow(selectedPackRow, packItems.size())
        && packItems[selectedPackRow] == userPresetsLabel;
}

juce::Colour PresetLibraryComponent::getSelectedPackAccent() const
{
    if (! juce::isPositiveAndBelow(selectedPackRow, packItems.size()))
        return juce::Colour(0xff8cff6a);

    return accentForPack(packItems[selectedPackRow]);
}

void PresetLibraryComponent::handlePackSelection(int row)
{
    if (suppressCallbacks || ! juce::isPositiveAndBelow(row, packItems.size()))
        return;

    selectedPackRow = row;
    selectedCategoryRow = 0;
    selectedSubCategoryRow = 0;
    rebuildCategoryItems();
    rebuildSubCategoryItems();
    rebuildPresetItems();
    categoryListBox.updateContent();
    subCategoryListBox.updateContent();
    presetListBox.updateContent();
    categoryListBox.selectRow(selectedCategoryRow, false, true);
    subCategoryListBox.selectRow(selectedSubCategoryRow, false, true);
    presetListBox.selectRow(selectedPresetRow, false, true);
    rebuildLibrary(true);
}

void PresetLibraryComponent::handleCategorySelection(int row)
{
    if (suppressCallbacks || ! juce::isPositiveAndBelow(row, categoryItems.size()))
        return;

    selectedCategoryRow = row;
    selectedSubCategoryRow = 0;
    rebuildSubCategoryItems();
    rebuildPresetItems();
    subCategoryListBox.updateContent();
    presetListBox.updateContent();
    subCategoryListBox.selectRow(selectedSubCategoryRow, false, true);
    presetListBox.selectRow(selectedPresetRow, false, true);
    rebuildLibrary(true);
}

void PresetLibraryComponent::handleSubCategorySelection(int row)
{
    if (suppressCallbacks || ! juce::isPositiveAndBelow(row, subCategoryItems.size()))
        return;

    selectedSubCategoryRow = row;
    rebuildPresetItems();
    presetListBox.updateContent();
    presetListBox.selectRow(selectedPresetRow, false, true);
    rebuildLibrary(true);
}

void PresetLibraryComponent::handlePresetSelection(int row, bool shouldLoad)
{
    if (suppressCallbacks || ! juce::isPositiveAndBelow(row, visiblePresetItems.size()))
        return;

    selectedPresetRow = row;
    if (shouldLoad)
        loadSelectedPreset();
    else
        rebuildLibrary(true);
}
