#include "HihatPresetBrowserComponent.h"

namespace
{
constexpr float panelWidth = 488.0f;
constexpr float panelHeight = 540.0f;
constexpr int rowHeight = 62;

juce::Colour scrimColour() { return juce::Colours::black.withAlpha(0.36f); }
juce::Colour rowFill() { return juce::Colour(0xff101a24); }
juce::Colour rowHighlight() { return juce::Colour(0xff153743); }
juce::Colour chipFill() { return juce::Colour(0xff183f48); }
juce::Colour dirtyChipFill() { return juce::Colour(0xff4a3620); }

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just)
{
    label.setColour(juce::Label::textColourId, colour);
    label.setFont(juce::Font(size, size >= 14.0f ? juce::Font::bold : juce::Font::plain));
    label.setJustificationType(just);
}
} // namespace

HihatPresetBrowserComponent::HihatPresetBrowserComponent(PluginProcessor& p)
    : processor(p)
{
    setVisible(false);
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    styleLabel(titleLabel, 18.0f, ssp::ui::textStrong(), juce::Justification::centredLeft);
    styleLabel(summaryLabel, 12.0f, ssp::ui::textMuted(), juce::Justification::centredLeft);
    styleLabel(resultsLabel, 11.0f, ssp::ui::brandMint(), juce::Justification::centredLeft);

    titleLabel.setText("Preset Library", juce::dontSendNotification);
    summaryLabel.setText("Search 30 factory motion presets by name, category, or tags.", juce::dontSendNotification);

    searchEditor.setTextToShowWhenEmpty("Search presets", ssp::ui::textMuted());
    searchEditor.setFont(juce::Font(13.0f, juce::Font::bold));
    searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xcc10202b));
    searchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff31475d));
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, ssp::ui::brandCyan());
    searchEditor.setColour(juce::TextEditor::textColourId, ssp::ui::textStrong());
    searchEditor.onTextChange = [this]
    {
        searchText = searchEditor.getText();
        rebuildFilteredPresets();
    };

    categoryBox.addItem("All Categories", 1);
    const auto categories = PluginProcessor::getFactoryPresetCategories();
    for (int i = 0; i < categories.size(); ++i)
        categoryBox.addItem(categories[i], i + 2);
    categoryBox.setSelectedId(1, juce::dontSendNotification);
    categoryBox.onChange = [this] { rebuildFilteredPresets(); };

    clearSearchButton.onClick = [this]
    {
        searchEditor.clear();
        grabKeyboardFocus();
    };

    presetList.setModel(this);
    presetList.setOutlineThickness(0);
    presetList.setRowHeight(rowHeight);
    presetList.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    presetList.setMultipleSelectionEnabled(false);

    for (auto* child : { static_cast<juce::Component*>(&titleLabel),
                         static_cast<juce::Component*>(&summaryLabel),
                         static_cast<juce::Component*>(&resultsLabel),
                         static_cast<juce::Component*>(&searchEditor),
                         static_cast<juce::Component*>(&categoryBox),
                         static_cast<juce::Component*>(&clearSearchButton),
                         static_cast<juce::Component*>(&presetList) })
    {
        addAndMakeVisible(*child);
        child->setAlpha(0.0f);
    }
}

void HihatPresetBrowserComponent::paint(juce::Graphics& g)
{
    if (! visibleOrAnimating)
        return;

    g.setColour(scrimColour().withAlpha(openProgress));
    g.fillAll();

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(openProgress);
    ssp::ui::drawPanelBackground(g, getPanelBounds(), ssp::ui::brandCyan(), 18.0f);
}

void HihatPresetBrowserComponent::resized()
{
    updateChildLayout();
}

void HihatPresetBrowserComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! getPanelBounds().contains(event.position))
        close();
}

void HihatPresetBrowserComponent::setAnchorBounds(juce::Rectangle<int> presetButtonBounds)
{
    anchorBounds = presetButtonBounds;
    updateChildLayout();
}

void HihatPresetBrowserComponent::open()
{
    visibleOrAnimating = true;
    isClosing = false;
    setVisible(true);
    rebuildFilteredPresets();
    grabKeyboardFocus();
    startTimerHz(60);
}

void HihatPresetBrowserComponent::close()
{
    isClosing = true;
    startTimerHz(60);
}

bool HihatPresetBrowserComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (! visibleOrAnimating)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    if (searchEditor.hasKeyboardFocus(true))
        return false;

    if (key == juce::KeyPress::upKey)
    {
        if (filteredPresetIndices.isEmpty())
            return true;

        const int currentRow = presetList.getSelectedRow();
        const int nextRow = juce::jlimit(0, filteredPresetIndices.size() - 1, currentRow <= 0 ? 0 : currentRow - 1);
        presetList.selectRow(nextRow);
        presetList.scrollToEnsureRowIsOnscreen(nextRow);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        if (filteredPresetIndices.isEmpty())
            return true;

        const int currentRow = presetList.getSelectedRow();
        const int nextRow = juce::jlimit(0, filteredPresetIndices.size() - 1, currentRow < 0 ? 0 : currentRow + 1);
        presetList.selectRow(nextRow);
        presetList.scrollToEnsureRowIsOnscreen(nextRow);
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        applyPresetForSelectedRow();
        return true;
    }

    return false;
}

int HihatPresetBrowserComponent::getNumRows()
{
    return filteredPresetIndices.size();
}

void HihatPresetBrowserComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(5.0f, 4.0f);
    g.setColour(rowIsSelected ? rowHighlight() : rowFill());
    g.fillRoundedRectangle(bounds, 10.0f);

    if (! juce::isPositiveAndBelow(rowNumber, filteredPresetIndices.size()))
        return;

    const int presetIndex = filteredPresetIndices[rowNumber];
    const bool active = presetIndex == processor.getCurrentFactoryPresetIndex();
    const bool dirty = active && processor.isCurrentPresetDirty();

    auto content = bounds.reduced(14.0f, 10.0f).toNearestInt();
    auto right = content.removeFromRight(118);

    if (active)
    {
        auto chipBounds = right.removeFromTop(22).toFloat().withTrimmedLeft(18.0f);
        g.setColour(dirty ? dirtyChipFill() : chipFill());
        g.fillRoundedRectangle(chipBounds, 8.0f);
        g.setColour(dirty ? ssp::ui::brandGold() : ssp::ui::brandMint());
        g.drawRoundedRectangle(chipBounds, 8.0f, 1.0f);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(dirty ? "ACTIVE *" : "ACTIVE", chipBounds.toNearestInt(), juce::Justification::centred, false);
    }

    g.setColour(ssp::ui::textStrong());
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawFittedText(PluginProcessor::getFactoryPresetName(presetIndex),
                     content.removeFromTop(20), juce::Justification::centredLeft, 1);

    g.setColour(ssp::ui::textMuted());
    g.setFont(juce::Font(11.5f));
    const auto meta = PluginProcessor::getFactoryPresetCategory(presetIndex)
                      + "    "
                      + PluginProcessor::getFactoryPresetTags(presetIndex);
    g.drawFittedText(meta, content, juce::Justification::centredLeft, 1);
}

void HihatPresetBrowserComponent::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    presetList.selectRow(row);
    applyPresetForSelectedRow();
}

void HihatPresetBrowserComponent::selectedRowsChanged(int)
{
    repaint();
}

void HihatPresetBrowserComponent::timerCallback()
{
    const float target = isClosing ? 0.0f : 1.0f;
    openProgress += (target - openProgress) * 0.25f;

    if (std::abs(openProgress - target) < 0.02f)
        openProgress = target;

    for (auto* child : { static_cast<juce::Component*>(&titleLabel),
                         static_cast<juce::Component*>(&summaryLabel),
                         static_cast<juce::Component*>(&resultsLabel),
                         static_cast<juce::Component*>(&searchEditor),
                         static_cast<juce::Component*>(&categoryBox),
                         static_cast<juce::Component*>(&clearSearchButton),
                         static_cast<juce::Component*>(&presetList) })
    {
        child->setAlpha(openProgress);
    }

    repaint();

    if (isClosing && openProgress <= 0.0f)
    {
        visibleOrAnimating = false;
        isClosing = false;
        setVisible(false);
        stopTimer();
    }
}

void HihatPresetBrowserComponent::rebuildFilteredPresets()
{
    filteredPresetIndices.clear();

    const auto search = searchText.trim();
    const auto categoryFilter = categoryBox.getSelectedId() <= 1 ? juce::String() : categoryBox.getText();

    for (int i = 0; i < PluginProcessor::getFactoryPresetCount(); ++i)
    {
        if (categoryFilter.isNotEmpty() && PluginProcessor::getFactoryPresetCategory(i) != categoryFilter)
            continue;

        if (search.isNotEmpty())
        {
            const auto haystack = PluginProcessor::getFactoryPresetName(i)
                                  + " "
                                  + PluginProcessor::getFactoryPresetCategory(i)
                                  + " "
                                  + PluginProcessor::getFactoryPresetTags(i);
            if (! haystack.containsIgnoreCase(search))
                continue;
        }

        filteredPresetIndices.add(i);
    }

    presetList.updateContent();

    int rowToSelect = -1;
    const int currentPreset = processor.getCurrentFactoryPresetIndex();
    for (int row = 0; row < filteredPresetIndices.size(); ++row)
    {
        if (filteredPresetIndices[row] == currentPreset)
        {
            rowToSelect = row;
            break;
        }
    }

    if (rowToSelect < 0 && ! filteredPresetIndices.isEmpty())
        rowToSelect = 0;

    presetList.selectRow(rowToSelect);
    if (rowToSelect >= 0)
        presetList.scrollToEnsureRowIsOnscreen(rowToSelect);

    resultsLabel.setText(juce::String(filteredPresetIndices.size()) + " presets shown", juce::dontSendNotification);
    repaint();
}

juce::Rectangle<float> HihatPresetBrowserComponent::getPanelBounds() const
{
    const float width = panelWidth;
    const float height = juce::jmin(panelHeight, (float) getHeight() - 76.0f);

    float x = anchorBounds.isEmpty() ? (float) getWidth() - width - 28.0f
                                     : (float) anchorBounds.getCentreX() - width * 0.48f;
    float y = anchorBounds.isEmpty() ? 88.0f
                                     : (float) anchorBounds.getBottom() + 14.0f;

    x = juce::jlimit(20.0f, (float) getWidth() - width - 20.0f, x);
    y = juce::jlimit(64.0f, (float) getHeight() - height - 20.0f, y);
    return { x, y, width, height };
}

void HihatPresetBrowserComponent::updateChildLayout()
{
    auto area = getPanelBounds().toNearestInt().reduced(20, 18);
    titleLabel.setBounds(area.removeFromTop(24));
    summaryLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    auto filters = area.removeFromTop(32);
    searchEditor.setBounds(filters.removeFromLeft(filters.getWidth() - 170));
    filters.removeFromLeft(8);
    categoryBox.setBounds(filters.removeFromLeft(102));
    filters.removeFromLeft(8);
    clearSearchButton.setBounds(filters);

    area.removeFromTop(10);
    resultsLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);
    presetList.setBounds(area);
}

void HihatPresetBrowserComponent::applyPresetForSelectedRow()
{
    const int selectedRow = presetList.getSelectedRow();
    if (! juce::isPositiveAndBelow(selectedRow, filteredPresetIndices.size()))
        return;

    processor.applyFactoryPreset(filteredPresetIndices[selectedRow]);
    close();
}
