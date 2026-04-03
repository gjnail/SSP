#include "ClipperPresetBrowserComponent.h"

namespace
{
juce::Colour panelFill() { return juce::Colour(0xf2141a21); }
juce::Colour rowFill() { return juce::Colour(0xff1a222b); }
juce::Colour rowHighlight() { return juce::Colour(0xff23313d); }
juce::Colour rowFactory() { return juce::Colour(0xffe6e0d3); }
juce::Colour rowUser() { return reverbui::brandCyan().brighter(0.2f); }
}

ClipperPresetBrowserComponent::ClipperPresetBrowserComponent(PluginProcessor& processorToUse)
    : processor(processorToUse)
{
    setVisible(false);
    setWantsKeyboardFocus(true);

    searchEditor.setTextToShowWhenEmpty("Search presets", reverbui::textMuted());
    searchEditor.onTextChange = [this] { refreshRows(); };
    presetNameEditor.setTextToShowWhenEmpty("Preset name", reverbui::textMuted());
    categoryBox.setEditableText(true);
    categoryBox.setTextWhenNothingSelected("Category");

    saveButton.onClick = [this] { saveCurrentPreset(); };
    deleteButton.onClick = [this] { deleteSelectedPreset(); };
    importButton.onClick = [this] { importPreset(); };
    exportButton.onClick = [this] { exportPreset(); };
    closeButton.onClick = [this] { close(); };

    listBox.setModel(this);
    listBox.setRowHeight(28);

    for (auto* component : { static_cast<juce::Component*>(&searchEditor),
                             static_cast<juce::Component*>(&presetNameEditor),
                             static_cast<juce::Component*>(&categoryBox),
                             static_cast<juce::Component*>(&saveButton),
                             static_cast<juce::Component*>(&deleteButton),
                             static_cast<juce::Component*>(&importButton),
                             static_cast<juce::Component*>(&exportButton),
                             static_cast<juce::Component*>(&closeButton),
                             static_cast<juce::Component*>(&listBox) })
        addAndMakeVisible(*component);
}

void ClipperPresetBrowserComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
    const auto panel = getLocalBounds().toFloat().reduced(2.0f, 2.0f);
    reverbui::drawPanelBackground(g, panel, reverbui::brandGold(), 16.0f);
    g.setColour(reverbui::textStrong());
    g.setFont(reverbui::smallCapsFont(12.0f));
    g.drawText("PRESET BROWSER", panel.toNearestInt().removeFromTop(22).reduced(10, 0), juce::Justification::centredLeft, false);
}

void ClipperPresetBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 14);
    area.removeFromTop(16);

    auto topRow = area.removeFromTop(30);
    searchEditor.setBounds(topRow.removeFromLeft(180));
    topRow.removeFromLeft(8);
    presetNameEditor.setBounds(topRow.removeFromLeft(160));
    topRow.removeFromLeft(8);
    categoryBox.setBounds(topRow.removeFromLeft(120));
    topRow.removeFromLeft(8);
    saveButton.setBounds(topRow.removeFromLeft(72));
    topRow.removeFromLeft(6);
    deleteButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(6);
    importButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(6);
    exportButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(6);
    closeButton.setBounds(topRow);

    area.removeFromTop(10);
    listBox.setBounds(area);
}

bool ClipperPresetBrowserComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void ClipperPresetBrowserComponent::setAnchorBounds(juce::Rectangle<int> buttonBounds)
{
    anchorBounds = buttonBounds;
}

void ClipperPresetBrowserComponent::open()
{
    categoryBox.clear();
    for (const auto& category : processor.getAvailablePresetCategories())
        categoryBox.addItem(category, categoryBox.getNumItems() + 1);

    presetNameEditor.setText(processor.getCurrentPresetName(), juce::dontSendNotification);
    categoryBox.setText(processor.getCurrentPresetCategory(), juce::dontSendNotification);
    refreshRows();
    setVisible(true);
    grabKeyboardFocus();
    toFront(true);
}

void ClipperPresetBrowserComponent::close()
{
    setVisible(false);
}

int ClipperPresetBrowserComponent::getNumRows()
{
    return rows.size();
}

void ClipperPresetBrowserComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow(rowNumber, rows.size()))
        return;

    const auto& item = rows.getReference(rowNumber);
    g.setColour(rowIsSelected ? rowHighlight() : rowFill());
    g.fillRoundedRectangle(juce::Rectangle<float>(2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f), 8.0f);

    g.setColour(item.isFactory ? rowFactory() : rowUser());
    g.setFont(reverbui::bodyFont(12.0f));
    g.drawText(item.preset.name, 12, 0, width - 90, height, juce::Justification::centredLeft, false);
    g.setColour(reverbui::textMuted());
    g.drawText(item.preset.category, width - 110, 0, 100, height, juce::Justification::centredRight, false);
}

void ClipperPresetBrowserComponent::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    listBox.selectRow(row);
    loadSelectedPreset();
}

void ClipperPresetBrowserComponent::refreshRows()
{
    rows.clearQuick();
    const auto search = searchEditor.getText().trim().toLowerCase();
    const auto addPresetRows = [this, &search](const auto& presets, bool isFactory)
    {
        for (int i = 0; i < presets.size(); ++i)
        {
            const auto& preset = presets.getReference(i);
            const auto haystack = (preset.name + " " + preset.category).toLowerCase();
            if (search.isNotEmpty() && ! haystack.contains(search))
                continue;

            RowItem item;
            item.isFactory = isFactory;
            item.index = i;
            item.preset = preset;
            rows.add(item);
        }
    };

    addPresetRows(PluginProcessor::getFactoryPresets(), true);
    addPresetRows(processor.getUserPresets(), false);
    listBox.updateContent();
    listBox.repaint();
}

void ClipperPresetBrowserComponent::loadSelectedPreset()
{
    const int selected = listBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(selected, rows.size()))
        return;

    const auto& item = rows.getReference(selected);
    if (item.isFactory)
        processor.loadFactoryPreset(item.index);
    else
        processor.loadUserPreset(item.index);

    close();
}

void ClipperPresetBrowserComponent::saveCurrentPreset()
{
    processor.saveUserPreset(presetNameEditor.getText(), categoryBox.getText());
    refreshRows();
}

void ClipperPresetBrowserComponent::deleteSelectedPreset()
{
    const int selected = listBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(selected, rows.size()))
        return;

    const auto& item = rows.getReference(selected);
    if (item.isFactory)
        return;

    processor.deleteUserPreset(item.index);
    refreshRows();
}

void ClipperPresetBrowserComponent::importPreset()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import SSP Clipper Preset", juce::File{}, "*.sspclipperpreset");
    juce::Component::SafePointer<ClipperPresetBrowserComponent> safeThis(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safeThis](const juce::FileChooser& chooser)
                             {
                                 if (safeThis == nullptr)
                                     return;
                                 const auto file = chooser.getResult();
                                 if (file.existsAsFile())
                                 {
                                     safeThis->processor.importPresetFromFile(file, true);
                                     safeThis->refreshRows();
                                 }
                             });
}

void ClipperPresetBrowserComponent::exportPreset()
{
    const auto baseName = processor.getCurrentPresetName().isNotEmpty() ? processor.getCurrentPresetName() : "Current Settings";
    fileChooser = std::make_unique<juce::FileChooser>("Export SSP Clipper Preset", juce::File{}, "*.sspclipperpreset");
    juce::Component::SafePointer<ClipperPresetBrowserComponent> safeThis(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                             [safeThis, baseName](const juce::FileChooser& chooser)
                             {
                                 if (safeThis == nullptr)
                                     return;
                                 auto file = chooser.getResult();
                                 if (file == juce::File{})
                                     return;
                                 if (! file.hasFileExtension(".sspclipperpreset"))
                                     file = file.withFileExtension(".sspclipperpreset");
                                 safeThis->processor.exportCurrentPresetToFile(file);
                             });
}
