#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1080;
constexpr int editorHeight = 1180;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      graph(p),
      controls(p),
      presetBrowser(p)
{
    setSize(editorWidth, editorHeight);
    setResizable(true, true);
    setResizeLimits(960, 900, 1800, 1600);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP EQ", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    phaseBadgeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    phaseBadgeLabel.setJustificationType(juce::Justification::centred);
    phaseBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0b1118));
    addAndMakeVisible(phaseBadgeLabel);

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
    phaseBadgeLabel.setBounds(titleRow.removeFromLeft(44).reduced(0, 4));
    titleRow.removeFromLeft(10);
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
    graph.setBounds(area.removeFromTop(390));
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

        if (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D')
        {
            auto targets = graph.getSelectedPoints();
            if (targets.isEmpty() && graph.getSelectedPoint() >= 0)
                targets.add(graph.getSelectedPoint());

            if (! targets.isEmpty())
            {
                const bool enableDynamic = ! processor->getPoint(targets.getFirst()).dynamicEnabled;
                for (auto index : targets)
                {
                    auto point = processor->getPoint(index);
                    if (! point.enabled)
                        continue;
                    point.dynamicEnabled = enableDynamic;
                    processor->setPoint(index, point);
                }
                return true;
            }
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

    const auto processingMode = processor->getProcessingMode();
    if (processingMode == PluginProcessor::naturalPhase)
    {
        phaseBadgeLabel.setVisible(true);
        phaseBadgeLabel.setText("NP", juce::dontSendNotification);
        phaseBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xfff0b661));
        phaseBadgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xfff5d293));
    }
    else if (processingMode == PluginProcessor::linearPhase)
    {
        phaseBadgeLabel.setVisible(true);
        phaseBadgeLabel.setText("LP", juce::dontSendNotification);
        phaseBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff63d0ff));
        phaseBadgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xffa9ebff));
    }
    else
    {
        phaseBadgeLabel.setVisible(false);
    }
}
