#include "PluginEditor.h"
#include "ChorusVectorUI.h"

namespace
{
constexpr int editorWidth = 1280;
constexpr int editorHeight = 880;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts),
      presetBrowser(p)
{
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP Chorus", juce::dontSendNotification);
    titleLabel.setFont(reverbui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reverbui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Preset browser, stereo chorus shaping, and factory motion scenes for quick width and modulation work.",
                      juce::dontSendNotification);
    hintLabel.setFont(reverbui::bodyFont(10.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    addAndMakeVisible(hintLabel);

    previousPresetButton.onClick = [this] { processor.stepPreset(-1); };
    presetButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };
    nextPresetButton.onClick = [this] { processor.stepPreset(1); };
    browserButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };

    for (auto* button : { static_cast<juce::Button*>(&previousPresetButton),
                          static_cast<juce::Button*>(&presetButton),
                          static_cast<juce::Button*>(&nextPresetButton),
                          static_cast<juce::Button*>(&browserButton) })
    {
        addAndMakeVisible(*button);
    }

    addAndMakeVisible(controls);
    addAndMakeVisible(presetBrowser);

    startTimerHz(12);
}

void PluginEditor::paint(juce::Graphics& g)
{
    reverbui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(2.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(64);
    auto titleRow = header.removeFromTop(30);

    titleLabel.setBounds(titleRow.removeFromLeft(164));
    titleRow.removeFromLeft(12);
    previousPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 1));
    titleRow.removeFromLeft(6);
    presetButton.setBounds(titleRow.removeFromLeft(280).reduced(0, 1));
    titleRow.removeFromLeft(6);
    nextPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 1));
    titleRow.removeFromLeft(10);
    browserButton.setBounds(titleRow.removeFromLeft(84).reduced(0, 1));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    controls.setBounds(area);
    presetBrowser.setBounds(getLocalBounds());
    presetBrowser.setAnchorBounds(presetButton.getBounds(), controls.getBounds());
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    if (presetBrowser.handleKeyPress(key))
        return true;

    if (key == juce::KeyPress::leftKey)
    {
        processor.stepPreset(-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        processor.stepPreset(1);
        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void PluginEditor::timerCallback()
{
    auto name = processor.getCurrentPresetName();
    if (name.isEmpty())
        name = "Current Settings";
    if (processor.isCurrentPresetDirty())
        name << " *";
    presetButton.setButtonText(name);
}
