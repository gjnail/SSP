#include "EchoPresetBrowserComponent.h"

namespace
{
constexpr float rowHeight = 28.0f;
constexpr float dividerHeight = 10.0f;
constexpr float panelWidth = 312.0f;
constexpr float submenuWidth = 284.0f;
constexpr float panelMaxHeight = 420.0f;
constexpr float panelOverlap = 12.0f;
constexpr float searchHeight = 32.0f;
constexpr float dialogWidth = 420.0f;
constexpr float dialogHeight = 250.0f;

juce::Colour backgroundScrim() { return juce::Colours::black.withAlpha(0.36f); }
juce::Colour panelFill() { return juce::Colour(0xff0d1822); }
juce::Colour panelText() { return juce::Colours::white; }
juce::Colour mutedText() { return juce::Colour(0xff86a3b5); }
juce::Colour highlightFill() { return juce::Colour(0xff123842); }
juce::Colour userTagFill() { return juce::Colour(0xff154b46); }

void styleDialogLabel(juce::Label& label, float size, juce::Colour colour)
{
    label.setFont(juce::Font(size, juce::Font::bold));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

EchoPresetBrowserComponent::EchoPresetBrowserComponent(PluginProcessor& p)
    : processor(p)
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);
    setVisible(false);

    styleDialogLabel(dialogTitleLabel, 18.0f, panelText());
    styleDialogLabel(dialogMessageLabel, 12.0f, mutedText());
    dialogMessageLabel.setJustificationType(juce::Justification::topLeft);

    searchEditor.setFont(juce::Font(13.0f, juce::Font::bold));
    searchEditor.setTextToShowWhenEmpty("Search presets", mutedText());
    searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xcc10202b));
    searchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff31475d));
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentColour());
    searchEditor.setColour(juce::TextEditor::textColourId, panelText());
    searchEditor.onTextChange = [this]
    {
        searchText = searchEditor.getText();
        rebuildMenu();
    };

    presetNameEditor.setFont(juce::Font(14.0f, juce::Font::bold));
    presetNameEditor.setTextToShowWhenEmpty("Preset name", mutedText());
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111c27));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff31475d));
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentColour());
    presetNameEditor.setColour(juce::TextEditor::textColourId, panelText());
    presetNameEditor.onReturnKey = [this] { confirmSaveDialog(); };

    categoryBox.setEditableText(true);
    categoryBox.setTextWhenNothingSelected("Choose or type a category");

    primaryDialogButton.onClick = [this]
    {
        if (dialogMode == DialogMode::saveAs)
            confirmSaveDialog();
        else if (dialogMode == DialogMode::confirmReset)
            confirmResetDialog();
        else if (dialogMode == DialogMode::deleteUserPreset)
            hideDialog();
    };

    secondaryDialogButton.onClick = [this] { hideDialog(); };
    clearSearchButton.onClick = [this]
    {
        searchEditor.clear();
        searchEditor.grabKeyboardFocus();
    };

    deleteListBox.setModel(this);
    deleteListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0x00000000));
    deleteListBox.setRowHeight((int) rowHeight);

    for (auto* component : { static_cast<juce::Component*>(&dialogTitleLabel),
                             static_cast<juce::Component*>(&dialogMessageLabel),
                             static_cast<juce::Component*>(&searchEditor),
                             static_cast<juce::Component*>(&presetNameEditor),
                             static_cast<juce::Component*>(&categoryBox),
                             static_cast<juce::Component*>(&clearSearchButton),
                             static_cast<juce::Component*>(&primaryDialogButton),
                             static_cast<juce::Component*>(&secondaryDialogButton),
                             static_cast<juce::Component*>(&deleteListBox) })
    {
        addChildComponent(*component);
    }
}

void EchoPresetBrowserComponent::setAnchorBounds(juce::Rectangle<int> button, juce::Rectangle<int> graph)
{
    anchorBounds = button;
    graphBounds = graph;
    updateMenuLayout();
    updateDialogLayout();
}

void EchoPresetBrowserComponent::open()
{
    visibleOrAnimating = true;
    isClosing = false;
    openProgress = 1.0f;
    setVisible(true);
    searchEditor.setVisible(true);
    clearSearchButton.setVisible(true);
    rebuildMenu();
    grabKeyboardFocus();
    startTimerHz(20);
}

void EchoPresetBrowserComponent::close()
{
    endPreviewIfNeeded();
    hideDialog();
    visibleOrAnimating = false;
    isClosing = false;
    openProgress = 0.0f;
    setVisible(false);
    searchEditor.setVisible(false);
    clearSearchButton.setVisible(false);
    panels.clear();
    stopTimer();
    repaint();
}

bool EchoPresetBrowserComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (! visibleOrAnimating)
        return false;

    if (dialogMode == DialogMode::saveAs)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            hideDialog();
            return true;
        }
        return false;
    }

    if (dialogMode != DialogMode::none)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            hideDialog();
            return true;
        }

        if (key == juce::KeyPress::returnKey)
        {
            if (dialogMode == DialogMode::confirmReset)
                confirmResetDialog();
            return true;
        }

        return false;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    if (key == juce::KeyPress::upKey)
    {
        moveHighlight(-1);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        moveHighlight(1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        openHighlightedSubmenu();
        return true;
    }

    if (key == juce::KeyPress::leftKey)
    {
        closeLastSubmenu();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        activateHighlightedItem();
        return true;
    }

    return false;
}

void EchoPresetBrowserComponent::paint(juce::Graphics& g)
{
    if (! visibleOrAnimating)
        return;

    auto graphArea = graphBounds.isEmpty() ? getLocalBounds().toFloat() : graphBounds.toFloat();
    g.setColour(juce::Colour(0xaa0a121a).withAlpha(0.28f * openProgress));
    g.fillRoundedRectangle(graphArea, 14.0f);
    g.setColour(juce::Colour(0x401e4552).withAlpha(0.35f * openProgress));
    g.fillRoundedRectangle(graphArea.reduced(10.0f, 14.0f), 12.0f);

    const float translateY = (1.0f - openProgress) * 6.0f;
    const float translateX = (1.0f - openProgress) * -28.0f;

    for (const auto& panel : panels)
    {
        auto panelBounds = panel.bounds.translated(translateX, translateY);
        juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(openProgress);
        ssp::ui::drawPanelBackground(g, panelBounds, accentColour(), 16.0f);

        auto contentBounds = getPanelContentBounds(panel).translated(translateX, translateY);
        g.reduceClipRegion(contentBounds.toNearestInt());

        float y = contentBounds.getY() - panel.scrollOffset;
        for (int i = 0; i < (int) panel.items.size(); ++i)
        {
            const auto& item = panel.items[(size_t) i];
            const float height = getItemHeight(item);
            auto row = juce::Rectangle<float>(contentBounds.getX(), y, contentBounds.getWidth(), height);

            if (row.getBottom() >= contentBounds.getY() && row.getY() <= contentBounds.getBottom())
            {
                if (item.kind == MenuItem::Kind::divider)
                {
                    g.setColour(juce::Colour(0xff2a3948));
                    g.drawHorizontalLine((int) row.getCentreY(), row.getX() + 10.0f, row.getRight() - 10.0f);
                }
                else
                {
                    const bool highlighted = panel.highlightedIndex == i;
                    if (highlighted)
                    {
                        g.setColour(highlightFill().withAlpha(item.enabled ? 0.95f : 0.45f));
                        g.fillRoundedRectangle(row.reduced(4.0f, 2.0f), 8.0f);
                    }

                    if (item.checked)
                    {
                        juce::Path tick;
                        tick.startNewSubPath(row.getX() + 12.0f, row.getCentreY());
                        tick.lineTo(row.getX() + 16.5f, row.getBottom() - 8.5f);
                        tick.lineTo(row.getX() + 24.0f, row.getY() + 8.0f);
                        g.setColour(accentColour());
                        g.strokePath(tick, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }

                    auto textBounds = row.reduced(14.0f, 0.0f).toNearestInt();
                    textBounds.removeFromLeft(item.checked ? 18 : 6);
                    const auto textColour = item.enabled ? (item.isUserPreset ? juce::Colour(0xffabf5e9) : panelText()) : mutedText().withAlpha(0.65f);
                    g.setColour(textColour);
                    g.setFont(juce::Font(13.0f, juce::Font::bold));
                    g.drawFittedText(item.label, textBounds.removeFromLeft(textBounds.getWidth() - 54), juce::Justification::centredLeft, 1);

                    if (item.isUserPreset)
                    {
                        auto tag = juce::Rectangle<float>((float) textBounds.getX() + 2.0f, row.getY() + 6.0f, 40.0f, rowHeight - 12.0f);
                        g.setColour(userTagFill());
                        g.fillRoundedRectangle(tag, 7.0f);
                        g.setColour(juce::Colour(0xff9cf2e2));
                        g.setFont(juce::Font(10.0f, juce::Font::bold));
                        g.drawText("USER", tag.toNearestInt(), juce::Justification::centred, false);
                    }

                    if (! item.children.empty())
                    {
                        juce::Path arrow;
                        const float x = row.getRight() - 18.0f;
                        const float cy = row.getCentreY();
                        arrow.startNewSubPath(x - 4.0f, cy - 5.0f);
                        arrow.lineTo(x + 1.0f, cy);
                        arrow.lineTo(x - 4.0f, cy + 5.0f);
                        g.setColour(item.enabled ? panelText() : mutedText());
                        g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }
                }
            }

            y += height;
        }
    }

    if (dialogMode != DialogMode::none)
    {
        auto dialogBounds = juce::Rectangle<float>(dialogWidth, dialogMode == DialogMode::deleteUserPreset ? 340.0f : dialogHeight)
                                .withCentre(getLocalBounds().toFloat().getCentre());
        g.setOpacity(openProgress);
        ssp::ui::drawPanelBackground(g, dialogBounds, accentColour(), 18.0f);
    }
}

void EchoPresetBrowserComponent::resized()
{
    updateMenuLayout();
    updateDialogLayout();

    auto searchBounds = (graphBounds.isEmpty() ? getLocalBounds().reduced(18) : graphBounds.reduced(18)).removeFromTop((int) searchHeight);
    searchEditor.setBounds(searchBounds.removeFromLeft(220));
    searchBounds.removeFromLeft(8);
    clearSearchButton.setBounds(searchBounds.removeFromLeft(64));
}

void EchoPresetBrowserComponent::mouseMove(const juce::MouseEvent& event)
{
    if (dialogMode != DialogMode::none)
        return;

    updateHoverState(event.position);
}

void EchoPresetBrowserComponent::mouseExit(const juce::MouseEvent&)
{
    clearHoverPreviewPending();
    endPreviewIfNeeded();
}

void EchoPresetBrowserComponent::mouseDown(const juce::MouseEvent& event)
{
    if (dialogMode != DialogMode::none)
    {
        auto dialogBounds = juce::Rectangle<float>(dialogWidth, dialogMode == DialogMode::deleteUserPreset ? 340.0f : dialogHeight)
                                .withCentre(getLocalBounds().toFloat().getCentre());
        if (! dialogBounds.contains(event.position))
            hideDialog();
        return;
    }

    const auto [panelIndex, itemIndex] = hitTestItem(event.position);
    if (panelIndex >= 0 && itemIndex >= 0)
    {
        setHighlightedItem(panelIndex, itemIndex, true);
        const auto& item = panels[(size_t) panelIndex].items[(size_t) itemIndex];
        if (item.children.empty())
            executeMenuItem(item);
        return;
    }

    if (! isPointInsideAnyPanel(event.position))
        close();
}

void EchoPresetBrowserComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    for (auto& panel : panels)
    {
        if (! panel.bounds.contains(event.position))
            continue;

        const auto contentBounds = getPanelContentBounds(panel);
        const float maxScroll = juce::jmax(0.0f, getPanelContentHeight(panel) - contentBounds.getHeight());
        if (maxScroll <= 0.0f)
            return;

        panel.scrollOffset = juce::jlimit(0.0f, maxScroll, panel.scrollOffset - wheel.deltaY * 56.0f);
        repaint();
        return;
    }
}

int EchoPresetBrowserComponent::getNumRows()
{
    return deletableUserPresets.size();
}

void EchoPresetBrowserComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
    g.setColour(rowIsSelected ? highlightFill() : panelFill().brighter(0.04f));
    g.fillRoundedRectangle(bounds.reduced(4.0f, 2.0f), 8.0f);

    if (! juce::isPositiveAndBelow(rowNumber, deletableUserPresets.size()))
        return;

    const auto& preset = deletableUserPresets.getReference(rowNumber);
    g.setColour(panelText());
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(preset.name, 14, 0, width - 110, height, juce::Justification::centredLeft, false);
    g.setColour(mutedText());
    g.setFont(11.0f);
    g.drawText(preset.category.isNotEmpty() ? preset.category : "Uncategorised", 14, 12, width - 110, height - 12, juce::Justification::centredLeft, false);

    auto deleteBounds = juce::Rectangle<float>((float) width - 84.0f, 6.0f, 68.0f, (float) height - 12.0f);
    g.setColour(juce::Colour(0xff4b1c27));
    g.fillRoundedRectangle(deleteBounds, 8.0f);
    g.setColour(juce::Colour(0xfff08b9e));
    g.drawRoundedRectangle(deleteBounds, 8.0f, 1.0f);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("Delete", deleteBounds.toNearestInt(), juce::Justification::centred, false);
}

void EchoPresetBrowserComponent::listBoxItemClicked(int row, const juce::MouseEvent& event)
{
    if (event.x >= deleteListBox.getWidth() - 84)
        deletePresetAtRow(row);
}

void EchoPresetBrowserComponent::timerCallback()
{
    if (! visibleOrAnimating)
    {
        stopTimer();
        return;
    }

    maybeBeginHoverPreview();
}

void EchoPresetBrowserComponent::rebuildMenu()
{
    panels.clear();
    MenuPanelState root;
    root.items = buildRootMenuItems();
    root.highlightedIndex = findNextSelectableIndex(root, -1, 1);
    panels.push_back(root);
    updateMenuLayout();
    repaint();
}

std::vector<EchoPresetBrowserComponent::MenuItem> EchoPresetBrowserComponent::buildRootMenuItems() const
{
    std::vector<MenuItem> items;

    if (searchText.trim().isNotEmpty())
    {
        for (const auto& preset : processor.getSequentialPresetList())
        {
            if (! presetMatchesFilter(preset) || ! presetMatchesSearch(preset))
                continue;
            MenuItem item;
            item.kind = MenuItem::Kind::preset;
            item.label = preset.category.isNotEmpty() ? preset.name + "  [" + preset.category + "]" : preset.name;
            item.checked = processor.matchesCurrentPreset(preset);
            item.isUserPreset = ! preset.isFactory;
            item.preset = preset;
            items.push_back(std::move(item));
        }

        if (items.empty())
            items.push_back(MenuItem{ MenuItem::Kind::placeholder, "No matching presets", {}, false, false, false, {}, {} });
    }
    else
    {
        juce::Array<PluginProcessor::PresetRecord> factoryStandalone;
        juce::Array<PluginProcessor::PresetRecord> factoryCategorised;
        juce::Array<PluginProcessor::PresetRecord> userOrdered;

        for (const auto& preset : PluginProcessor::getFactoryPresets())
        {
            if (! presetMatchesFilter(preset))
                continue;
            if (preset.category.isEmpty())
                factoryStandalone.add(preset);
            else
                factoryCategorised.add(preset);
        }

        for (const auto& preset : processor.getUserPresets())
        {
            if (presetMatchesFilter(preset))
                userOrdered.add(preset);
        }

        for (const auto& preset : factoryStandalone)
        {
            MenuItem item;
            item.kind = MenuItem::Kind::preset;
            item.label = preset.name;
            item.checked = processor.matchesCurrentPreset(preset);
            item.preset = preset;
            items.push_back(std::move(item));
        }

        if (! factoryCategorised.isEmpty())
        {
            items.push_back(MenuItem{ MenuItem::Kind::divider, {}, {}, false, false, false, {}, {} });
            auto categoryItems = buildCategoryItems(factoryCategorised, 0);
            items.insert(items.end(), categoryItems.begin(), categoryItems.end());
        }

        if (! userOrdered.isEmpty())
        {
            items.push_back(MenuItem{ MenuItem::Kind::divider, {}, {}, false, false, false, {}, {} });
            MenuItem userSection;
            userSection.kind = MenuItem::Kind::action;
            userSection.label = "User Presets";
            for (const auto& preset : userOrdered)
            {
                MenuItem item;
                item.kind = MenuItem::Kind::preset;
                item.label = preset.category.isNotEmpty() ? preset.name + "  [" + preset.category + "]" : preset.name;
                item.checked = processor.matchesCurrentPreset(preset);
                item.isUserPreset = true;
                item.preset = preset;
                userSection.children.push_back(std::move(item));
            }
            items.push_back(std::move(userSection));
        }
    }

    items.push_back(MenuItem{ MenuItem::Kind::divider, {}, {}, false, false, false, {}, {} });
    items.push_back(MenuItem{ MenuItem::Kind::action, "Save As...", "save-as", false, true, false, {}, {} });

    MenuItem options;
    options.kind = MenuItem::Kind::action;
    options.label = "Options";
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Show User Presets Only", "filter-user", filterMode == FilterMode::userOnly, true, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Show Factory Presets Only", "filter-factory", filterMode == FilterMode::factoryOnly, true, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Hover Preview", "hover-preview", hoverPreviewEnabled, true, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::divider, {}, {}, false, false, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Delete User Preset...", "delete-user", false, ! processor.getUserPresets().isEmpty(), false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Import Preset...", "import-preset", false, true, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Export Preset...", "export-preset", false, true, false, {}, {} });
    options.children.push_back(MenuItem{ MenuItem::Kind::action, "Reset to Factory Defaults", "reset-factory", false, true, false, {}, {} });
    items.push_back(std::move(options));

    return items;
}

std::vector<EchoPresetBrowserComponent::MenuItem> EchoPresetBrowserComponent::buildCategoryItems(const juce::Array<PluginProcessor::PresetRecord>& presets, int depth) const
{
    std::vector<MenuItem> items;
    juce::StringArray names;

    for (const auto& preset : presets)
    {
        const auto segment = categorySegment(preset.category, depth);
        if (segment.isNotEmpty())
            names.addIfNotAlreadyThere(segment);
    }

    static const juce::StringArray preferredOrder{ "Dynamic", "Stereo" };
    juce::StringArray orderedNames;
    for (const auto& preferred : preferredOrder)
        if (names.contains(preferred))
            orderedNames.add(preferred);

    juce::StringArray extras;
    for (const auto& name : names)
        if (! orderedNames.contains(name))
            extras.addIfNotAlreadyThere(name);
    extras.sort(true);
    for (const auto& name : extras)
        orderedNames.add(name);

    for (const auto& name : orderedNames)
    {
        juce::Array<PluginProcessor::PresetRecord> branchPresets;
        juce::Array<PluginProcessor::PresetRecord> directPresets;

        for (const auto& preset : presets)
        {
            if (categorySegment(preset.category, depth) != name)
                continue;

            branchPresets.add(preset);
            if (categorySegment(preset.category, depth + 1).isEmpty())
                directPresets.add(preset);
        }

        MenuItem categoryItem;
        categoryItem.kind = MenuItem::Kind::action;
        categoryItem.label = name;

        const auto childCategories = buildCategoryItems(branchPresets, depth + 1);
        categoryItem.children.insert(categoryItem.children.end(), childCategories.begin(), childCategories.end());

        for (const auto& preset : directPresets)
        {
            MenuItem item;
            item.kind = MenuItem::Kind::preset;
            item.label = preset.name;
            item.checked = processor.matchesCurrentPreset(preset);
            item.isUserPreset = ! preset.isFactory;
            item.preset = preset;
            categoryItem.children.push_back(std::move(item));
        }

        items.push_back(std::move(categoryItem));
    }

    return items;
}

void EchoPresetBrowserComponent::updateMenuLayout()
{
    if (panels.empty())
        return;

    const auto availableBounds = (graphBounds.isEmpty() ? getLocalBounds() : graphBounds).reduced(8).toFloat();
    float x = availableBounds.getX() + 10.0f;
    float y = availableBounds.getY() + searchHeight + 16.0f;
    x = juce::jlimit(availableBounds.getX(), availableBounds.getRight() - panelWidth, x);

    for (int panelIndex = 0; panelIndex < (int) panels.size(); ++panelIndex)
    {
        auto& panel = panels[(size_t) panelIndex];
        const float width = panelIndex == 0 ? panelWidth : submenuWidth;
        const float height = juce::jmin(panelMaxHeight, getPanelContentHeight(panel) + 18.0f);

        if (panelIndex == 0)
        {
            panel.bounds = { x, y, width, height };
        }
        else
        {
            const auto& parent = panels[(size_t) (panelIndex - 1)];
            const auto row = getItemBounds(parent, parent.highlightedIndex);
            auto panelY = juce::jlimit(availableBounds.getY(), availableBounds.getBottom() - height, row.getY() - 8.0f);
            auto panelX = juce::jmin(parent.bounds.getRight() - panelOverlap, availableBounds.getRight() - width);
            panel.bounds = { panelX, panelY, width, height };
        }

        const auto content = getPanelContentBounds(panel);
        const float maxScroll = juce::jmax(0.0f, getPanelContentHeight(panel) - content.getHeight());
        panel.scrollOffset = juce::jlimit(0.0f, maxScroll, panel.scrollOffset);
    }
}

void EchoPresetBrowserComponent::updateDialogLayout()
{
    const auto dialogBounds = juce::Rectangle<int>((int) dialogWidth, dialogMode == DialogMode::deleteUserPreset ? 340 : (int) dialogHeight)
                                  .withCentre(getLocalBounds().getCentre());
    auto area = dialogBounds.reduced(22, 20);

    dialogTitleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    dialogMessageLabel.setBounds(area.removeFromTop(dialogMode == DialogMode::confirmReset ? 54 : 36));

    presetNameEditor.setVisible(dialogMode == DialogMode::saveAs);
    categoryBox.setVisible(dialogMode == DialogMode::saveAs);
    deleteListBox.setVisible(dialogMode == DialogMode::deleteUserPreset);
    dialogTitleLabel.setVisible(dialogMode != DialogMode::none);
    dialogMessageLabel.setVisible(dialogMode != DialogMode::none);
    primaryDialogButton.setVisible(dialogMode != DialogMode::none);
    secondaryDialogButton.setVisible(dialogMode != DialogMode::none);

    if (dialogMode == DialogMode::saveAs)
    {
        presetNameEditor.setBounds(area.removeFromTop(30));
        area.removeFromTop(10);
        categoryBox.setBounds(area.removeFromTop(30));
        area.removeFromTop(16);
    }

    if (dialogMode == DialogMode::deleteUserPreset)
    {
        deleteListBox.setBounds(area.removeFromTop(210));
        area.removeFromTop(14);
    }

    auto buttons = area.removeFromBottom(32);
    secondaryDialogButton.setBounds(buttons.removeFromRight(98));
    buttons.removeFromRight(10);
    primaryDialogButton.setBounds(buttons.removeFromRight(108));
}

void EchoPresetBrowserComponent::setHighlightedItem(int panelIndex, int itemIndex, bool openChildren)
{
    if (! juce::isPositiveAndBelow(panelIndex, (int) panels.size()))
        return;

    auto& panel = panels[(size_t) panelIndex];
    const bool validItem = juce::isPositiveAndBelow(itemIndex, (int) panel.items.size());
    const bool itemHasChildren = validItem && openChildren && ! panel.items[(size_t) itemIndex].children.empty();
    const bool childPanelAlreadyOpen = panels.size() > (size_t) panelIndex + 1;

    if (panel.highlightedIndex == itemIndex
        && childPanelAlreadyOpen == itemHasChildren)
        return;

    panel.highlightedIndex = itemIndex;
    panels.resize((size_t) panelIndex + 1);

    if (itemHasChildren)
    {
        const auto& item = panel.items[(size_t) itemIndex];
        MenuPanelState child;
        child.items = item.children;
        child.highlightedIndex = findNextSelectableIndex(child, -1, 1);
        panels.push_back(std::move(child));
    }

    updateMenuLayout();
    repaint();
}

void EchoPresetBrowserComponent::moveHighlight(int delta)
{
    if (panels.empty())
        return;

    auto& panel = panels.back();
    panel.highlightedIndex = findNextSelectableIndex(panel, panel.highlightedIndex, delta > 0 ? 1 : -1);

    const auto content = getPanelContentBounds(panel);
    const auto row = getItemBounds(panel, panel.highlightedIndex);
    const float maxScroll = juce::jmax(0.0f, getPanelContentHeight(panel) - content.getHeight());
    if (row.getY() < content.getY())
        panel.scrollOffset = juce::jlimit(0.0f, maxScroll, panel.scrollOffset - (content.getY() - row.getY()) - 4.0f);
    else if (row.getBottom() > content.getBottom())
        panel.scrollOffset = juce::jlimit(0.0f, maxScroll, panel.scrollOffset + (row.getBottom() - content.getBottom()) + 4.0f);

    updateMenuLayout();
    repaint();
}

void EchoPresetBrowserComponent::openHighlightedSubmenu()
{
    if (panels.empty())
        return;

    auto& panel = panels.back();
    if (! juce::isPositiveAndBelow(panel.highlightedIndex, (int) panel.items.size()))
        return;

    const auto& item = panel.items[(size_t) panel.highlightedIndex];
    if (! item.children.empty())
        setHighlightedItem((int) panels.size() - 1, panel.highlightedIndex, true);
}

void EchoPresetBrowserComponent::closeLastSubmenu()
{
    if (panels.size() > 1)
    {
        panels.pop_back();
        updateMenuLayout();
        repaint();
    }
}

void EchoPresetBrowserComponent::activateHighlightedItem()
{
    if (panels.empty())
        return;

    const auto& panel = panels.back();
    if (! juce::isPositiveAndBelow(panel.highlightedIndex, (int) panel.items.size()))
        return;

    const auto& item = panel.items[(size_t) panel.highlightedIndex];
    if (! item.children.empty())
        openHighlightedSubmenu();
    else
        executeMenuItem(item);
}

int EchoPresetBrowserComponent::findNextSelectableIndex(const MenuPanelState& panel, int startIndex, int direction) const
{
    if (panel.items.empty())
        return -1;

    int index = startIndex;
    for (int step = 0; step < (int) panel.items.size(); ++step)
    {
        index = (index + direction + (int) panel.items.size()) % (int) panel.items.size();
        const auto& item = panel.items[(size_t) index];
        if (item.kind != MenuItem::Kind::divider && item.enabled)
            return index;
    }

    return -1;
}

float EchoPresetBrowserComponent::getItemHeight(const MenuItem& item) const
{
    return item.kind == MenuItem::Kind::divider ? dividerHeight : rowHeight;
}

float EchoPresetBrowserComponent::getPanelContentHeight(const MenuPanelState& panel) const
{
    float total = 0.0f;
    for (const auto& item : panel.items)
        total += getItemHeight(item);
    return total;
}

juce::Rectangle<float> EchoPresetBrowserComponent::getPanelContentBounds(const MenuPanelState& panel) const
{
    return panel.bounds.reduced(10.0f, 10.0f);
}

juce::Rectangle<float> EchoPresetBrowserComponent::getItemBounds(const MenuPanelState& panel, int itemIndex) const
{
    auto content = getPanelContentBounds(panel);
    float y = content.getY() - panel.scrollOffset;
    for (int i = 0; i < (int) panel.items.size(); ++i)
    {
        const float height = getItemHeight(panel.items[(size_t) i]);
        if (i == itemIndex)
            return { content.getX(), y, content.getWidth(), height };
        y += height;
    }
    return {};
}

bool EchoPresetBrowserComponent::isPointInsideAnyPanel(juce::Point<float> point) const
{
    for (const auto& panel : panels)
        if (panel.bounds.contains(point))
            return true;
    return false;
}

std::pair<int, int> EchoPresetBrowserComponent::hitTestItem(juce::Point<float> point) const
{
    for (int panelIndex = (int) panels.size() - 1; panelIndex >= 0; --panelIndex)
    {
        const auto& panel = panels[(size_t) panelIndex];
        if (! panel.bounds.contains(point))
            continue;

        auto content = getPanelContentBounds(panel);
        float y = content.getY() - panel.scrollOffset;
        for (int itemIndex = 0; itemIndex < (int) panel.items.size(); ++itemIndex)
        {
            const auto height = getItemHeight(panel.items[(size_t) itemIndex]);
            const auto row = juce::Rectangle<float>(content.getX(), y, content.getWidth(), height);
            if (row.contains(point))
                return { panelIndex, itemIndex };
            y += height;
        }
    }

    return { -1, -1 };
}

void EchoPresetBrowserComponent::updateHoverState(juce::Point<float> point)
{
    const auto [panelIndex, itemIndex] = hitTestItem(point);
    if (panelIndex >= 0 && itemIndex >= 0)
    {
        setHighlightedItem(panelIndex, itemIndex, true);

        const auto& item = panels[(size_t) panelIndex].items[(size_t) itemIndex];
        if (hoverPreviewEnabled && item.kind == MenuItem::Kind::preset && item.children.empty())
        {
            if (pendingPreviewPresetKey != item.preset.id)
                endPreviewIfNeeded();
            pendingPreviewPresetKey = item.preset.id;
            pendingPreviewDeadline = juce::Time::getMillisecondCounter() + 300;
        }
        else
        {
            clearHoverPreviewPending();
            endPreviewIfNeeded();
        }
    }
    else
    {
        clearHoverPreviewPending();
        endPreviewIfNeeded();
    }
}

void EchoPresetBrowserComponent::clearHoverPreviewPending()
{
    pendingPreviewPresetKey.clear();
    pendingPreviewDeadline = 0;
}

void EchoPresetBrowserComponent::maybeBeginHoverPreview()
{
    if (! hoverPreviewEnabled || pendingPreviewPresetKey.isEmpty())
        return;

    if (juce::Time::getMillisecondCounter() < pendingPreviewDeadline)
        return;

    for (const auto& panel : panels)
    {
        if (! juce::isPositiveAndBelow(panel.highlightedIndex, (int) panel.items.size()))
            continue;

        const auto& item = panel.items[(size_t) panel.highlightedIndex];
        if (item.kind == MenuItem::Kind::preset && item.preset.id == pendingPreviewPresetKey)
        {
            processor.beginPresetPreview(item.preset);
            clearHoverPreviewPending();
            return;
        }
    }

    clearHoverPreviewPending();
}

void EchoPresetBrowserComponent::endPreviewIfNeeded()
{
    processor.endPresetPreview();
}

void EchoPresetBrowserComponent::executeMenuItem(const MenuItem& item)
{
    if (! item.enabled)
        return;

    if (item.kind == MenuItem::Kind::preset)
    {
        loadPreset(item.preset);
        return;
    }

    if (item.kind == MenuItem::Kind::action)
        executeAction(item.actionID);
}

void EchoPresetBrowserComponent::executeAction(const juce::String& actionID)
{
    if (actionID == "save-as")
    {
        showSaveDialog();
        return;
    }

    if (actionID == "filter-user")
    {
        filterMode = filterMode == FilterMode::userOnly ? FilterMode::all : FilterMode::userOnly;
        rebuildMenu();
        return;
    }

    if (actionID == "filter-factory")
    {
        filterMode = filterMode == FilterMode::factoryOnly ? FilterMode::all : FilterMode::factoryOnly;
        rebuildMenu();
        return;
    }

    if (actionID == "hover-preview")
    {
        hoverPreviewEnabled = ! hoverPreviewEnabled;
        endPreviewIfNeeded();
        rebuildMenu();
        return;
    }

    if (actionID == "delete-user")
    {
        showDeleteDialog();
        return;
    }

    if (actionID == "import-preset")
    {
        launchImportChooser();
        return;
    }

    if (actionID == "export-preset")
    {
        launchExportChooser();
        return;
    }

    if (actionID == "reset-factory")
    {
        showResetDialog();
        return;
    }

}

void EchoPresetBrowserComponent::loadPreset(const PluginProcessor::PresetRecord& preset)
{
    endPreviewIfNeeded();

    const auto& library = preset.isFactory ? PluginProcessor::getFactoryPresets() : processor.getUserPresets();
    for (int i = 0; i < library.size(); ++i)
    {
        if (library.getReference(i).id != preset.id)
            continue;

        if (preset.isFactory)
            processor.loadFactoryPreset(i);
        else
            processor.loadUserPreset(i);

        rebuildMenu();
        return;
    }
}

void EchoPresetBrowserComponent::showSaveDialog()
{
    dialogMode = DialogMode::saveAs;
    dialogTitleLabel.setText("Save Preset", juce::dontSendNotification);
    dialogMessageLabel.setText("Name the preset and choose an existing category or type a new category path.", juce::dontSendNotification);
    presetNameEditor.setText(processor.getCurrentPresetName(), juce::dontSendNotification);
    categoryBox.clear(juce::dontSendNotification);
    for (const auto& category : processor.getAvailablePresetCategories())
        categoryBox.addItem(category, categoryBox.getNumItems() + 1);
    categoryBox.setText(processor.getCurrentPresetCategory(), juce::dontSendNotification);
    primaryDialogButton.setButtonText("Save");
    secondaryDialogButton.setButtonText("Cancel");
    updateDialogLayout();
    presetNameEditor.grabKeyboardFocus();
    presetNameEditor.selectAll();
    repaint();
}

void EchoPresetBrowserComponent::showDeleteDialog()
{
    dialogMode = DialogMode::deleteUserPreset;
    dialogTitleLabel.setText("Delete User Presets", juce::dontSendNotification);
    dialogMessageLabel.setText("Select any user preset below to remove it from your local preset library.", juce::dontSendNotification);
    deletableUserPresets = processor.getUserPresets();
    deleteListBox.updateContent();
    primaryDialogButton.setButtonText("Done");
    secondaryDialogButton.setButtonText("Close");
    updateDialogLayout();
    repaint();
}

void EchoPresetBrowserComponent::showResetDialog()
{
    dialogMode = DialogMode::confirmReset;
    dialogTitleLabel.setText("Reset User Presets", juce::dontSendNotification);
    dialogMessageLabel.setText("This removes all user-saved SSP Echo presets from your local library. Factory presets stay intact.", juce::dontSendNotification);
    primaryDialogButton.setButtonText("Reset");
    secondaryDialogButton.setButtonText("Cancel");
    updateDialogLayout();
    repaint();
}

void EchoPresetBrowserComponent::hideDialog()
{
    dialogMode = DialogMode::none;
    updateDialogLayout();
    repaint();
}

void EchoPresetBrowserComponent::confirmSaveDialog()
{
    if (dialogMode != DialogMode::saveAs)
        return;

    if (processor.saveUserPreset(presetNameEditor.getText(), categoryBox.getText()))
    {
        hideDialog();
        rebuildMenu();
        close();
    }
}

void EchoPresetBrowserComponent::confirmResetDialog()
{
    if (dialogMode != DialogMode::confirmReset)
        return;

    processor.resetUserPresetsToFactoryDefaults();
    hideDialog();
    rebuildMenu();
}

void EchoPresetBrowserComponent::deletePresetAtRow(int row)
{
    if (! juce::isPositiveAndBelow(row, deletableUserPresets.size()))
        return;

    if (processor.deleteUserPreset(row))
    {
        deletableUserPresets = processor.getUserPresets();
        deleteListBox.updateContent();
        rebuildMenu();
    }
}

void EchoPresetBrowserComponent::launchImportChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import SSP Echo Preset", juce::File{}, "*.sspechopreset");
    auto* chooser = fileChooser.get();
    juce::Component::SafePointer<EchoPresetBrowserComponent> safeThis(this);
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
                             safeThis->rebuildMenu();
                             safeThis->close();
                         });
}

void EchoPresetBrowserComponent::launchExportChooser()
{
    const auto baseName = processor.getCurrentPresetName().isNotEmpty() ? processor.getCurrentPresetName() : "Current Settings";
    fileChooser = std::make_unique<juce::FileChooser>("Export SSP Echo Preset",
                                                      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(baseName + ".sspechopreset"),
                                                      "*.sspechopreset");
    auto* chooser = fileChooser.get();
    juce::Component::SafePointer<EchoPresetBrowserComponent> safeThis(this);
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
                             safeThis->close();
                         });
}

juce::Colour EchoPresetBrowserComponent::accentColour() const
{
    return juce::Colour(0xff48d8cb);
}

juce::String EchoPresetBrowserComponent::categorySegment(const juce::String& category, int depth) const
{
    auto tokens = juce::StringArray::fromTokens(category, "/", {});
    return juce::isPositiveAndBelow(depth, tokens.size()) ? tokens[depth].trim() : juce::String{};
}

bool EchoPresetBrowserComponent::presetMatchesFilter(const PluginProcessor::PresetRecord& preset) const
{
    if (filterMode == FilterMode::userOnly)
        return ! preset.isFactory;
    if (filterMode == FilterMode::factoryOnly)
        return preset.isFactory;
    return true;
}

bool EchoPresetBrowserComponent::presetMatchesSearch(const PluginProcessor::PresetRecord& preset) const
{
    const auto needle = searchText.trim().toLowerCase();
    if (needle.isEmpty())
        return true;

    const auto haystack = (preset.name + " " + preset.category).toLowerCase();
    return haystack.contains(needle);
}
