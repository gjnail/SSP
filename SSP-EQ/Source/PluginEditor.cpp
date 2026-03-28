#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1080;
constexpr int editorHeight = 820;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      graph(p),
      controls(p),
      presetBrowser(p)
{
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP EQ", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Double-click to add up to 24 bands, drag nodes to place them, use the wheel for Q, and shape each band below without changing the SSP EQ look.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8aa2bf));
    addAndMakeVisible(hintLabel);

    compareAButton.onClick = [this]
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->setActiveCompareSlot(0);
    };
    compareCopyButton.onClick = [this]
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->copyActiveCompareSlotToOther();
    };
    compareBButton.onClick = [this]
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->setActiveCompareSlot(1);
    };
    previousPresetButton.onClick = [this]
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->stepPreset(-1);
    };
    presetButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };
    nextPresetButton.onClick = [this]
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->stepPreset(1);
    };
    randomizeButton.onClickWithMods = [this](const juce::ModifierKeys& mods)
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
        {
            const auto mode = mods.isShiftDown() ? PluginProcessor::RandomizeMode::subtle
                : ((mods.isCommandDown() || mods.isCtrlDown()) ? PluginProcessor::RandomizeMode::extreme
                                                               : PluginProcessor::RandomizeMode::standard);
            processor->randomizeCurrentSlot(mode);
        }
    };
    settingsButton.onClick = [this]
    {
        if (! presetBrowser.isOpen())
            presetBrowser.open();
        else
            presetBrowser.close();
    };

    for (auto* button : { static_cast<juce::Button*>(&compareAButton),
                          static_cast<juce::Button*>(&compareCopyButton),
                          static_cast<juce::Button*>(&compareBButton),
                          static_cast<juce::Button*>(&previousPresetButton),
                          static_cast<juce::Button*>(&presetButton),
                          static_cast<juce::Button*>(&nextPresetButton),
                          static_cast<juce::Button*>(&randomizeButton),
                          static_cast<juce::Button*>(&settingsButton) })
        addAndMakeVisible(*button);

    graph.onSelectionChanged = [this](int index)
    {
        controls.setSelectedPoint(index);
    };

    controls.onSelectionChanged = [this](int index)
    {
        graph.setSelectedPoint(index);
    };

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
    addAndMakeVisible(presetBrowser);
    startTimerHz(12);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(68);
    auto titleRow = header.removeFromTop(36);
    titleLabel.setBounds(titleRow.removeFromLeft(140));
    titleRow.removeFromLeft(12);
    compareAButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(6);
    compareCopyButton.setBounds(titleRow.removeFromLeft(38).reduced(0, 2));
    titleRow.removeFromLeft(6);
    compareBButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(12);
    previousPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(6);
    presetButton.setBounds(titleRow.removeFromLeft(260).reduced(0, 2));
    titleRow.removeFromLeft(6);
    nextPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(12);
    randomizeButton.setBounds(titleRow.removeFromLeft(56).reduced(0, 2));
    titleRow.removeFromLeft(8);
    settingsButton.setBounds(titleRow.removeFromLeft(56).reduced(0, 2));
    hintLabel.setBounds(header);

    area.removeFromTop(10);
    graph.setBounds(area.removeFromTop(460));
    area.removeFromTop(14);
    controls.setBounds(area);
    presetBrowser.setBounds(getLocalBounds());
    presetBrowser.setAnchorBounds(presetButton.getBounds(), graph.getBounds());
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    if (presetBrowser.handleKeyPress(key))
        return true;

    if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
    {
        if (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S')
        {
            const auto selected = graph.getSelectedPoint();
            if (selected >= 0)
            {
                processor->toggleSoloPoint(selected);
                return true;
            }
        }

        if (key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C')
        {
            processor->toggleABComparison();
            return true;
        }

        if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
            && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z'))
        {
            if (processor->undoRandomizeCurrentSlot())
                return true;
        }

        if (key == juce::KeyPress::leftKey)
        {
            processor->stepPreset(-1);
            return true;
        }

        if (key == juce::KeyPress::rightKey)
        {
            processor->stepPreset(1);
            return true;
        }
    }

    if (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N')
    {
        controls.focusFrequencyInput();
        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void PluginEditor::timerCallback()
{
    auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor());
    if (processor == nullptr)
        return;

    auto name = processor->getCurrentPresetName();
    if (name.isEmpty())
        name = "Default Setting";
    if (processor->isCurrentPresetDirty())
        name << " *";
    presetButton.setButtonText(name);
    compareAButton.setToggleState(processor->getActiveCompareSlot() == 0, juce::dontSendNotification);
    compareBButton.setToggleState(processor->getActiveCompareSlot() == 1, juce::dontSendNotification);
}
