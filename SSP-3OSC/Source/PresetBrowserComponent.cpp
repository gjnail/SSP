#include "PresetBrowserComponent.h"
#include "ReactorUI.h"

namespace
{
void stylePresetLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}
}

PresetBrowserComponent::PresetBrowserComponent(PluginProcessor& p)
    : processor(p)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(previousPresetButton);
    addAndMakeVisible(nextPresetButton);
    addAndMakeVisible(initButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(refreshButton);

    titleLabel.setText("PRESETS", juce::dontSendNotification);
    stylePresetLabel(titleLabel, 15.0f, reactorui::textStrong());

    previousPresetButton.setButtonText("<");
    previousPresetButton.setTooltip("Load previous preset");
    nextPresetButton.setButtonText(">");
    nextPresetButton.setTooltip("Load next preset");

    initButton.setButtonText("Init");
    saveButton.setButtonText("Save Preset");
    refreshButton.setButtonText("Rescan");

    presetBox.onChange = [this] { handlePresetSelection(); };
    previousPresetButton.onClick = [this] { navigatePreset(-1); };
    nextPresetButton.onClick = [this] { navigatePreset(1); };
    initButton.onClick = [this]
    {
        processor.loadBasicInitPatch();
        rebuildPresetList();
    };
    saveButton.onClick = [this] { promptSavePreset(); };
    refreshButton.onClick = [this]
    {
        processor.refreshUserPresets();
        rebuildPresetList();
    };

    rebuildPresetList();
}

void PresetBrowserComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffffc95b), 12.0f);
}

void PresetBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);
    auto left = area.removeFromLeft(310);
    titleLabel.setBounds(left.removeFromTop(22));

    area.removeFromLeft(14);
    auto buttons = area.removeFromRight(326);
    auto presetRow = area.removeFromTop(34);
    previousPresetButton.setBounds(presetRow.removeFromLeft(32));
    presetRow.removeFromLeft(8);
    nextPresetButton.setBounds(presetRow.removeFromLeft(32));
    presetRow.removeFromLeft(10);
    presetBox.setBounds(presetRow);

    auto saveArea = buttons.removeFromTop(34);
    initButton.setBounds(saveArea.removeFromLeft(78));
    saveArea.removeFromLeft(10);
    saveButton.setBounds(saveArea.removeFromLeft(128));
    saveArea.removeFromLeft(10);
    refreshButton.setBounds(saveArea);
}

void PresetBrowserComponent::rebuildPresetList()
{
    suppressPresetChange = true;
    presetBox.clear(juce::dontSendNotification);
    navigablePresetIds.clear();

    presetBox.addSectionHeading("Factory Packs");
    const auto& factoryPresets = PluginProcessor::getFactoryPresetNames();
    const auto& factoryLibrary = PluginProcessor::getFactoryPresetLibrary();
    juce::String currentPack;
    for (const auto& preset : factoryLibrary)
    {
        if (preset.pack != currentPack)
        {
            currentPack = preset.pack;
            presetBox.addSectionHeading(currentPack);
        }

        presetBox.addItem(preset.name, 1000 + preset.index);
        navigablePresetIds.add(1000 + preset.index);
    }

    processor.refreshUserPresets();
    const auto& userPresets = processor.getUserPresetNames();
    if (! userPresets.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading("User Presets");
        for (int i = 0; i < userPresets.size(); ++i)
        {
            presetBox.addItem(userPresets[i], 2000 + i);
            navigablePresetIds.add(2000 + i);
        }
    }

    int selectedId = 0;
    const auto currentName = processor.getCurrentPresetName();
    if (processor.isCurrentPresetFactory())
    {
        const int factoryIndex = factoryPresets.indexOf(currentName);
        if (factoryIndex >= 0)
            selectedId = 1000 + factoryIndex;
    }
    else
    {
        const int userIndex = userPresets.indexOf(currentName);
        if (userIndex >= 0)
            selectedId = 2000 + userIndex;
    }

    if (selectedId == 0)
    {
        presetBox.addSeparator();
        presetBox.addItem(currentName.isNotEmpty() ? currentName : "Current Patch", 9000);
        selectedId = 9000;
    }

    presetBox.setSelectedId(selectedId, juce::dontSendNotification);
    suppressPresetChange = false;
    updateNavigationButtons();
}

void PresetBrowserComponent::handlePresetSelection()
{
    if (suppressPresetChange)
        return;

    const int selectedId = presetBox.getSelectedId();
    if (selectedId >= 1000 && selectedId < 2000)
        processor.loadFactoryPreset(selectedId - 1000);
    else if (selectedId >= 2000 && selectedId < 3000)
        processor.loadUserPreset(selectedId - 2000);

    rebuildPresetList();
}

void PresetBrowserComponent::navigatePreset(int direction)
{
    if (navigablePresetIds.isEmpty())
        return;

    const int currentId = presetBox.getSelectedId();
    int currentIndex = navigablePresetIds.indexOf(currentId);

    if (currentIndex < 0)
        currentIndex = direction > 0 ? -1 : 0;

    const int nextIndex = (currentIndex + direction + navigablePresetIds.size()) % navigablePresetIds.size();
    presetBox.setSelectedId(navigablePresetIds[nextIndex], juce::sendNotificationSync);
}

void PresetBrowserComponent::updateNavigationButtons()
{
    const bool enabled = ! navigablePresetIds.isEmpty();
    previousPresetButton.setEnabled(enabled);
    nextPresetButton.setEnabled(enabled);
}

void PresetBrowserComponent::promptSavePreset()
{
    auto suggestedFile = processor.getUserPresetDirectory().getChildFile(processor.getCurrentPresetName().isNotEmpty()
                                                                             ? processor.getCurrentPresetName() + ".sspreset"
                                                                             : "New Reactor Patch.sspreset");
    saveChooser = std::make_unique<juce::FileChooser>("Save SSP Reactor Preset", suggestedFile, "*.sspreset");
    auto* chooserPtr = saveChooser.get();
    juce::Component::SafePointer<PresetBrowserComponent> safeThis(this);
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

                                 const auto presetName = result.getFileNameWithoutExtension();
                                 if (! safeThis->processor.saveUserPreset(presetName))
                                 {
                                     juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                                 "Preset Save Failed",
                                                                                 "Reactor couldn't save that preset. Try a different name.");
                                     return;
                                 }

                                 safeThis->rebuildPresetList();
                             });
}
