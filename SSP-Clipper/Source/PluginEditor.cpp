#include "PluginEditor.h"
#include "../../SSP-Reverb/Source/ReverbVectorUI.h"

namespace
{
constexpr int editorWidth = 1240;
constexpr int editorHeight = 760;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts),
      presetBrowser(p)
{
    setOpaque(true);
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP Clipper", juce::dontSendNotification);
    titleLabel.setFont(reverbui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reverbui::textStrong());
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Large waveform clip view, minimal control row, and SSP Reverb theming tuned toward a ClipperX-style workflow.",
                      juce::dontSendNotification);
    hintLabel.setFont(reverbui::bodyFont(10.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    hintLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(hintLabel);

    previousPresetButton.onClick = [this, &p] { p.stepPreset(-1); };
    presetButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };
    nextPresetButton.onClick = [this, &p] { p.stepPreset(1); };

    addAndMakeVisible(previousPresetButton);
    addAndMakeVisible(presetButton);
    addAndMakeVisible(nextPresetButton);
    addAndMakeVisible(controls);
    addChildComponent(presetBrowser);
    startTimerHz(10);
}

void PluginEditor::paint(juce::Graphics& g)
{
    if (backgroundCache.isNull()
        || backgroundCache.getWidth() != getWidth()
        || backgroundCache.getHeight() != getHeight())
        refreshBackgroundCache();

    g.drawImageAt(backgroundCache, 0, 0);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(66);
    auto titleRow = header.removeFromTop(30);
    titleLabel.setBounds(titleRow.removeFromLeft(220));
    titleRow.removeFromLeft(12);
    previousPresetButton.setBounds(titleRow.removeFromLeft(34));
    titleRow.removeFromLeft(6);
    presetButton.setBounds(titleRow.removeFromLeft(240));
    titleRow.removeFromLeft(6);
    nextPresetButton.setBounds(titleRow.removeFromLeft(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    controls.setBounds(area);
    const int browserWidth = juce::jmin(560, getWidth() - 48);
    const int browserX = juce::jlimit(24, getWidth() - browserWidth - 24,
                                      presetButton.getX() + presetButton.getWidth() / 2 - browserWidth / 2);
    presetBrowser.setBounds(browserX, presetButton.getBottom() + 10, browserWidth, 320);
    presetBrowser.setAnchorBounds(presetButton.getBounds());

    refreshBackgroundCache();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    if (presetBrowser.isOpen() && presetBrowser.keyPressed(key))
        return true;

    if (key == juce::KeyPress::leftKey)
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->stepPreset(-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
            processor->stepPreset(1);
        return true;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void PluginEditor::refreshBackgroundCache()
{
    if (getWidth() <= 0 || getHeight() <= 0)
    {
        backgroundCache = {};
        return;
    }

    backgroundCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(backgroundCache);
    reverbui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(2.0f));
}

void PluginEditor::timerCallback()
{
    if (auto* processor = dynamic_cast<PluginProcessor*>(getAudioProcessor()))
    {
        auto name = processor->getCurrentPresetName();
        if (name.isEmpty())
            name = "Default Setting";
        if (processor->isCurrentPresetDirty())
            name << " *";

        if (presetButton.getButtonText() != name)
            presetButton.setButtonText(name);
    }
}
