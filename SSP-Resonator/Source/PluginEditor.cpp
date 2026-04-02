#include "PluginEditor.h"
#include "SSPVectorUI.h"

namespace
{
constexpr int editorWidth = 1360;
constexpr int editorHeight = 1160;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      graph(p),
      controls(p)
{
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP Resonator", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Bright chord-driven resonance for drums, vocals, synths and textures. Dial the harmony below, then feed it audio to light up the visualiser.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.8f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff98aec7));
    addAndMakeVisible(hintLabel);

    previousPresetButton.onClick = [this] { processor.stepFactoryPreset(-1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    nextPresetButton.onClick = [this] { processor.stepFactoryPreset(1); };
    differenceButton.setClickingTogglesState(true);
    differenceButton.onClick = [this]
    {
        processor.setDifferenceSoloEnabled(differenceButton.getToggleState());
    };

    for (auto* button : { static_cast<juce::Button*>(&previousPresetButton),
                          static_cast<juce::Button*>(&presetButton),
                          static_cast<juce::Button*>(&nextPresetButton),
                          static_cast<juce::Button*>(&differenceButton) })
        addAndMakeVisible(*button);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
    startTimerHz(12);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(78);
    auto titleRow = header.removeFromTop(36);
    titleLabel.setBounds(titleRow.removeFromLeft(230));
    titleRow.removeFromLeft(12);
    previousPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(6);
    presetButton.setBounds(titleRow.removeFromLeft(320).reduced(0, 2));
    titleRow.removeFromLeft(6);
    nextPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(10);
    differenceButton.setBounds(titleRow.removeFromLeft(68).reduced(0, 2));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    graph.setBounds(area.removeFromTop(470));
    area.removeFromTop(14);
    controls.setBounds(area);
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)
    {
        processor.stepFactoryPreset(-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        processor.stepFactoryPreset(1);
        return true;
    }

    if (key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D')
    {
        processor.setDifferenceSoloEnabled(! processor.isDifferenceSoloEnabled());
        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void PluginEditor::timerCallback()
{
    auto presetName = processor.getCurrentFactoryPresetName();
    if (presetName.isEmpty())
        presetName = "Factory Preset";

    presetButton.setButtonText(presetName);
    differenceButton.setToggleState(processor.isDifferenceSoloEnabled(), juce::dontSendNotification);
}

void PluginEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto presetNames = PluginProcessor::getFactoryPresetNames();
    const int currentIndex = processor.getCurrentFactoryPresetIndex();

    for (int i = 0; i < presetNames.size(); ++i)
        menu.addItem(i + 1, presetNames[i], true, i == currentIndex);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
                       [this](int result)
                       {
                           if (result > 0)
                               processor.loadFactoryPreset(result - 1);
                       });
}
