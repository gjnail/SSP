#include "PresetBrowserComponent.h"

namespace
{
constexpr int rowHeight = 52;

juce::Colour browserCardAccent() { return juce::Colour(0xff4ee0d7); }
juce::Colour rowFill(bool selected) { return selected ? juce::Colour(0xff152c38) : juce::Colour(0xff101923); }
juce::Colour userTagFill() { return juce::Colour(0xff1a5148); }
juce::Colour factoryTagFill() { return juce::Colour(0xff493a15); }
juce::Colour mutedText() { return ssp::ui::textMuted(); }
juce::Colour strongText() { return ssp::ui::textStrong(); }

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification)
{
    label.setFont(juce::Font(size, juce::Font::bold));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}
} // namespace

PresetBrowserComponent::PresetBrowserComponent(PluginProcessor& p)
    : processor(p)
{
    setVisible(false);
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    styleLabel(titleLabel, 20.0f, strongText(), juce::Justification::centredLeft);
    titleLabel.setText("Preset Browser", juce::dontSendNotification);

    styleLabel(subtitleLabel, 12.0f, mutedText(), juce::Justification::centredLeft);
    subtitleLabel.setText("Audition factory presets, manage your own library, and save Imager setups.", juce::dontSendNotification);

    styleLabel(saveHintLabel, 11.0f, mutedText(), juce::Justification::centredLeft);
    saveHintLabel.setText("Type a preset name and optionally choose or enter a category path.", juce::dontSendNotification);

    searchEditor.setFont(juce::Font(13.0f, juce::Font::bold));
    searchEditor.setTextToShowWhenEmpty("Search presets", mutedText());
    searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0e161f));
    searchEditor.setColour(juce::TextEditor::outlineColourId, ssp::ui::outlineSoft());
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, browserCardAccent());
    searchEditor.setColour(juce::TextEditor::textColourId, strongText());
    searchEditor.onTextChange = [this] { rebuildVisiblePresets(true); };

    presetListBox.setModel(this);
    presetListBox.setRowHeight(rowHeight);
    presetListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0x00000000));
    presetListBox.setOutlineThickness(0);

    presetNameEditor.setFont(juce::Font(14.0f, juce::Font::bold));
    presetNameEditor.setTextToShowWhenEmpty("Preset name", mutedText());
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0e161f));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, ssp::ui::outlineSoft());
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, browserCardAccent());
    presetNameEditor.setColour(juce::TextEditor::textColourId, strongText());
    presetNameEditor.onReturnKey = [this] { confirmSavePreset(); };

    categoryBox.setEditableText(true);
    categoryBox.setTextWhenNothingSelected("Choose or type a category");

    for (auto* filterButton : { &allFilterButton, &factoryFilterButton, &userFilterButton })
        filterButton->setClickingTogglesState(true);

    allFilterButton.onClick = [this]
    {
        filterMode = FilterMode::all;
        updateFilterButtons();
        rebuildVisiblePresets(true);
    };
    factoryFilterButton.onClick = [this]
    {
        filterMode = FilterMode::factoryOnly;
        updateFilterButtons();
        rebuildVisiblePresets(true);
    };
    userFilterButton.onClick = [this]
    {
        filterMode = FilterMode::userOnly;
        updateFilterButtons();
        rebuildVisiblePresets(true);
    };

    saveAsButton.onClick = [this] { beginSaveMode(); };
    deleteButton.onClick = [this] { deleteSelectedPreset(); };
    importButton.onClick = [this] { launchImportChooser(); };
    exportButton.onClick = [this] { launchExportChooser(); };
    resetButton.onClick = [this]
    {
        processor.resetUserPresetsToFactoryDefaults();
        rebuildVisiblePresets(false);
    };
    confirmSaveButton.onClick = [this] { confirmSavePreset(); };
    cancelSaveButton.onClick = [this] { cancelSaveMode(); };
    closeButton.onClick = [this] { close(); };

    for (auto* component : { static_cast<juce::Component*>(&titleLabel),
                             static_cast<juce::Component*>(&subtitleLabel),
                             static_cast<juce::Component*>(&saveHintLabel),
                             static_cast<juce::Component*>(&searchEditor),
                             static_cast<juce::Component*>(&presetListBox),
                             static_cast<juce::Component*>(&presetNameEditor),
                             static_cast<juce::Component*>(&categoryBox),
                             static_cast<juce::Component*>(&allFilterButton),
                             static_cast<juce::Component*>(&factoryFilterButton),
                             static_cast<juce::Component*>(&userFilterButton),
                             static_cast<juce::Component*>(&saveAsButton),
                             static_cast<juce::Component*>(&deleteButton),
                             static_cast<juce::Component*>(&importButton),
                             static_cast<juce::Component*>(&exportButton),
                             static_cast<juce::Component*>(&resetButton),
                             static_cast<juce::Component*>(&confirmSaveButton),
                             static_cast<juce::Component*>(&cancelSaveButton),
                             static_cast<juce::Component*>(&closeButton) })
    {
        addAndMakeVisible(*component);
    }

    updateFilterButtons();
    updateSaveModeVisibility();
}

void PresetBrowserComponent::open()
{
    openState = true;
    setVisible(true);
    saveMode = false;
    updateSaveModeVisibility();
    rebuildVisiblePresets(false);
    toFront(true);
    grabKeyboardFocus();
}

void PresetBrowserComponent::close()
{
    openState = false;
    saveMode = false;
    fileChooser.reset();
    updateSaveModeVisibility();
    setVisible(false);
}

void PresetBrowserComponent::toggle()
{
    if (openState)
        close();
    else
        open();
}

bool PresetBrowserComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (! openState)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        if (saveMode)
            cancelSaveMode();
        else
            close();
        return true;
    }

    if (key == juce::KeyPress::returnKey && saveMode)
    {
        confirmSavePreset();
        return true;
    }

    if (getNumRows() <= 0)
        return false;

    if (key == juce::KeyPress::upKey)
    {
        presetListBox.selectRow(juce::jmax(0, presetListBox.getSelectedRow() - 1));
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        presetListBox.selectRow(juce::jmin(getNumRows() - 1, presetListBox.getSelectedRow() + 1));
        return true;
    }

    return false;
}

void PresetBrowserComponent::paint(juce::Graphics& g)
{
    if (! openState)
        return;

    g.fillAll(juce::Colours::black.withAlpha(0.48f));

    const auto cardBounds = getCardBounds().toFloat();
    ssp::ui::drawPanelBackground(g, cardBounds, browserCardAccent(), 18.0f);

    auto heroGlow = cardBounds.reduced(24.0f, 18.0f).removeFromTop(74.0f);
    juce::ColourGradient glow(browserCardAccent().withAlpha(0.10f), heroGlow.getTopLeft(),
                              juce::Colours::transparentBlack, heroGlow.getBottomLeft(), false);
    glow.addColour(0.72, juce::Colour(0xffffd66d).withAlpha(0.06f));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(heroGlow, 12.0f);
}

void PresetBrowserComponent::resized()
{
    const auto cardBounds = getCardBounds();
    auto area = cardBounds.reduced(26, 22);

    auto header = area.removeFromTop(44);
    titleLabel.setBounds(header.removeFromLeft(250));
    closeButton.setBounds(header.removeFromRight(92));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(14);

    auto controlsRow = area.removeFromTop(32);
    searchEditor.setBounds(controlsRow.removeFromLeft(260));
    controlsRow.removeFromLeft(10);
    allFilterButton.setBounds(controlsRow.removeFromLeft(72));
    controlsRow.removeFromLeft(8);
    factoryFilterButton.setBounds(controlsRow.removeFromLeft(92));
    controlsRow.removeFromLeft(8);
    userFilterButton.setBounds(controlsRow.removeFromLeft(72));

    area.removeFromTop(14);

    auto saveArea = area.removeFromBottom(saveMode ? 118 : 48);
    presetListBox.setBounds(area);

    if (saveMode)
    {
        saveHintLabel.setBounds(saveArea.removeFromTop(18));
        saveArea.removeFromTop(8);
        presetNameEditor.setBounds(saveArea.removeFromTop(30));
        saveArea.removeFromTop(10);
        categoryBox.setBounds(saveArea.removeFromTop(30));
        saveArea.removeFromTop(14);

        auto buttonRow = saveArea.removeFromTop(30);
        cancelSaveButton.setBounds(buttonRow.removeFromRight(104));
        buttonRow.removeFromRight(10);
        confirmSaveButton.setBounds(buttonRow.removeFromRight(116));
    }
    else
    {
        auto buttonRow = saveArea.removeFromTop(32);
        saveAsButton.setBounds(buttonRow.removeFromLeft(112));
        buttonRow.removeFromLeft(8);
        deleteButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        importButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        exportButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        resetButton.setBounds(buttonRow.removeFromLeft(92));
    }
}

void PresetBrowserComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! getCardBounds().contains(event.getPosition()))
        close();
}

int PresetBrowserComponent::getNumRows()
{
    return visiblePresets.size();
}

void PresetBrowserComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow(rowNumber, visiblePresets.size()))
        return;

    const auto& preset = visiblePresets.getReference(rowNumber);
    auto bounds = juce::Rectangle<float>(4.0f, 2.0f, (float) width - 8.0f, (float) height - 4.0f);

    g.setColour(rowFill(rowIsSelected));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(processor.matchesCurrentPreset(preset) ? browserCardAccent() : ssp::ui::outlineSoft());
    g.drawRoundedRectangle(bounds, 10.0f, processor.matchesCurrentPreset(preset) ? 1.5f : 1.0f);

    auto textBounds = bounds.reduced(14.0f, 8.0f).toNearestInt();
    auto titleBounds = textBounds.removeFromTop(18);
    auto metaBounds = textBounds.removeFromTop(16);

    g.setColour(strongText());
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(preset.name, titleBounds.removeFromLeft(titleBounds.getWidth() - 132), juce::Justification::centredLeft, true);

    const auto dirty = processor.matchesCurrentPreset(preset) && processor.isCurrentPresetDirty();
    auto stateBounds = titleBounds.removeFromRight(118);
    g.setColour(dirty ? juce::Colour(0xffffd66d) : browserCardAccent().withAlpha(0.88f));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(dirty ? "ACTIVE *" : (processor.matchesCurrentPreset(preset) ? "ACTIVE" : juce::String{}),
               stateBounds, juce::Justification::centredRight, false);

    g.setColour(mutedText());
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(describePreset(preset), metaBounds.removeFromLeft(metaBounds.getWidth() - 90), juce::Justification::centredLeft, true);

    auto tagBounds = metaBounds.removeFromRight(76).toFloat().reduced(0.0f, 1.0f);
    g.setColour(preset.isFactory ? factoryTagFill() : userTagFill());
    g.fillRoundedRectangle(tagBounds, 7.0f);
    g.setColour(preset.isFactory ? juce::Colour(0xffffd66d) : juce::Colour(0xff9bf1de));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(preset.isFactory ? "FACTORY" : "USER", tagBounds.toNearestInt(), juce::Justification::centred, false);
}

void PresetBrowserComponent::selectedRowsChanged(int lastRowSelected)
{
    if (! ignoreSelectionChange)
        loadVisiblePreset(lastRowSelected);
}

void PresetBrowserComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    loadVisiblePreset(row);
    close();
}

juce::Rectangle<int> PresetBrowserComponent::getCardBounds() const
{
    auto bounds = getLocalBounds().reduced(40, 28);
    const int width = juce::jmin(860, bounds.getWidth());
    const int height = juce::jmin(620, bounds.getHeight());
    return juce::Rectangle<int>(width, height).withCentre(bounds.getCentre());
}

void PresetBrowserComponent::rebuildVisiblePresets(bool preserveSelection)
{
    const auto previousKey = preserveSelection && juce::isPositiveAndBelow(presetListBox.getSelectedRow(), visiblePresets.size())
                                 ? visiblePresets.getReference(presetListBox.getSelectedRow()).id
                                 : juce::String{};

    visiblePresets.clearQuick();

    for (const auto& preset : processor.getSequentialPresetList())
    {
        if (matchesFilter(preset) && matchesSearch(preset))
            visiblePresets.add(preset);
    }

    presetListBox.updateContent();

    if (preserveSelection && previousKey.isNotEmpty())
    {
        for (int row = 0; row < visiblePresets.size(); ++row)
        {
            if (visiblePresets.getReference(row).id == previousKey)
            {
                ignoreSelectionChange = true;
                presetListBox.selectRow(row, false, true);
                ignoreSelectionChange = false;
                return;
            }
        }
    }

    syncSelectionToCurrentPreset();
}

void PresetBrowserComponent::syncSelectionToCurrentPreset()
{
    ignoreSelectionChange = true;
    presetListBox.deselectAllRows();

    for (int row = 0; row < visiblePresets.size(); ++row)
    {
        if (processor.matchesCurrentPreset(visiblePresets.getReference(row)))
        {
            presetListBox.selectRow(row, false, true);
            presetListBox.scrollToEnsureRowIsOnscreen(row);
            break;
        }
    }

    ignoreSelectionChange = false;
}

void PresetBrowserComponent::loadVisiblePreset(int row)
{
    if (! juce::isPositiveAndBelow(row, visiblePresets.size()))
        return;

    const auto& preset = visiblePresets.getReference(row);
    const auto& library = preset.isFactory ? PluginProcessor::getFactoryPresets() : processor.getUserPresets();

    for (int index = 0; index < library.size(); ++index)
    {
        if (library.getReference(index).id != preset.id)
            continue;

        if (preset.isFactory)
            processor.loadFactoryPreset(index);
        else
            processor.loadUserPreset(index);

        presetListBox.repaint();
        return;
    }
}

void PresetBrowserComponent::updateFilterButtons()
{
    allFilterButton.setToggleState(filterMode == FilterMode::all, juce::dontSendNotification);
    factoryFilterButton.setToggleState(filterMode == FilterMode::factoryOnly, juce::dontSendNotification);
    userFilterButton.setToggleState(filterMode == FilterMode::userOnly, juce::dontSendNotification);
}

void PresetBrowserComponent::updateSaveModeVisibility()
{
    saveHintLabel.setVisible(saveMode);
    presetNameEditor.setVisible(saveMode);
    categoryBox.setVisible(saveMode);
    confirmSaveButton.setVisible(saveMode);
    cancelSaveButton.setVisible(saveMode);

    saveAsButton.setVisible(! saveMode);
    deleteButton.setVisible(! saveMode);
    importButton.setVisible(! saveMode);
    exportButton.setVisible(! saveMode);
    resetButton.setVisible(! saveMode);

    resized();
}

void PresetBrowserComponent::beginSaveMode()
{
    saveMode = true;
    presetNameEditor.setText(processor.getCurrentPresetName(), juce::dontSendNotification);
    categoryBox.clear(juce::dontSendNotification);
    for (const auto& category : processor.getAvailablePresetCategories())
        categoryBox.addItem(category, categoryBox.getNumItems() + 1);
    categoryBox.setText(processor.getCurrentPresetCategory(), juce::dontSendNotification);
    updateSaveModeVisibility();
    presetNameEditor.grabKeyboardFocus();
    presetNameEditor.selectAll();
}

void PresetBrowserComponent::cancelSaveMode()
{
    saveMode = false;
    updateSaveModeVisibility();
    grabKeyboardFocus();
}

void PresetBrowserComponent::confirmSavePreset()
{
    if (! saveMode)
        return;

    if (processor.saveUserPreset(presetNameEditor.getText().trim(), categoryBox.getText().trim()))
    {
        cancelSaveMode();
        rebuildVisiblePresets(false);
    }
}

void PresetBrowserComponent::deleteSelectedPreset()
{
    const auto selectedRow = presetListBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(selectedRow, visiblePresets.size()))
        return;

    const auto& selected = visiblePresets.getReference(selectedRow);
    if (selected.isFactory)
        return;

    const auto& userPresets = processor.getUserPresets();
    for (int index = 0; index < userPresets.size(); ++index)
    {
        if (userPresets.getReference(index).id == selected.id)
        {
            processor.deleteUserPreset(index);
            rebuildVisiblePresets(false);
            return;
        }
    }
}

void PresetBrowserComponent::launchImportChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import SSP Imager Preset", juce::File{}, "*.sspimagerpreset");
    auto* chooser = fileChooser.get();
    juce::Component::SafePointer<PresetBrowserComponent> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safeThis, chooser](const juce::FileChooser&)
                         {
                             if (safeThis == nullptr || chooser == nullptr)
                                 return;

                             const auto file = chooser->getResult();
                             safeThis->fileChooser.reset();
                             if (file == juce::File{})
                                 return;

                             safeThis->processor.importPresetFromFile(file, true);
                             safeThis->rebuildVisiblePresets(false);
                         });
}

void PresetBrowserComponent::launchExportChooser()
{
    const auto baseName = processor.getCurrentPresetName().isNotEmpty() ? processor.getCurrentPresetName() : "Current Settings";
    fileChooser = std::make_unique<juce::FileChooser>("Export SSP Imager Preset",
                                                      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                          .getChildFile(baseName + ".sspimagerpreset"),
                                                      "*.sspimagerpreset");
    auto* chooser = fileChooser.get();
    juce::Component::SafePointer<PresetBrowserComponent> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [safeThis, chooser](const juce::FileChooser&)
                         {
                             if (safeThis == nullptr || chooser == nullptr)
                                 return;

                             const auto file = chooser->getResult();
                             safeThis->fileChooser.reset();
                             if (file == juce::File{})
                                 return;

                             safeThis->processor.exportCurrentPresetToFile(file);
                         });
}

juce::String PresetBrowserComponent::describePreset(const PluginProcessor::PresetRecord& preset) const
{
    juce::String description = preset.category.isNotEmpty() ? preset.category : "Uncategorised";
    if (preset.author.isNotEmpty())
        description << "  |  " << preset.author;
    return description;
}

bool PresetBrowserComponent::matchesSearch(const PluginProcessor::PresetRecord& preset) const
{
    const auto needle = searchEditor.getText().trim().toLowerCase();
    if (needle.isEmpty())
        return true;

    const auto haystack = (preset.name + " " + preset.category + " " + preset.author).toLowerCase();
    return haystack.contains(needle);
}

bool PresetBrowserComponent::matchesFilter(const PluginProcessor::PresetRecord& preset) const
{
    if (filterMode == FilterMode::factoryOnly)
        return preset.isFactory;
    if (filterMode == FilterMode::userOnly)
        return ! preset.isFactory;
    return true;
}
