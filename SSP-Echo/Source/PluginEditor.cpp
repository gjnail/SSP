#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1120;
constexpr int editorHeight = 840;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts),
      presetBrowser(p)
{
    setSize(editorWidth, editorHeight);
    setResizable(true, true);
    setResizeLimits(980, 760, 1600, 1200);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP Echo", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1e8d6));
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Preset browser, vector front panel, and character view tuned for worn, moving repeats.",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(12.5f));
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff99a7b8));
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
    settingsButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };

    for (auto* button : { static_cast<juce::Button*>(&previousPresetButton),
                          static_cast<juce::Button*>(&presetButton),
                          static_cast<juce::Button*>(&nextPresetButton),
                          static_cast<juce::Button*>(&settingsButton) })
        addAndMakeVisible(*button);

    addAndMakeVisible(controls);
    addAndMakeVisible(presetBrowser);

    startTimerHz(12);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(4.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(70);
    auto titleRow = header.removeFromTop(36);

    titleLabel.setBounds(titleRow.removeFromLeft(150));
    titleRow.removeFromLeft(12);
    previousPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(6);
    presetButton.setBounds(titleRow.removeFromLeft(270).reduced(0, 2));
    titleRow.removeFromLeft(6);
    nextPresetButton.setBounds(titleRow.removeFromLeft(34).reduced(0, 2));
    titleRow.removeFromLeft(12);
    settingsButton.setBounds(titleRow.removeFromLeft(60).reduced(0, 2));
    hintLabel.setBounds(header);

    area.removeFromTop(10);
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
