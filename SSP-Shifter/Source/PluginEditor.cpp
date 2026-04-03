#include "PluginEditor.h"

#include "ShifterVectorUI.h"

namespace
{
constexpr int editorWidth = 1280;
constexpr int editorHeight = 860;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor),
      controls(processor, processor.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Shifter", juce::dontSendNotification);
    titleLabel.setFont(shifterui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, shifterui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Dual-engine pitch and frequency shifting with the same vector front-panel language as the SSP mixing tools.",
                      juce::dontSendNotification);
    hintLabel.setFont(shifterui::bodyFont(10.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, shifterui::textMuted());
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    shifterui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(2.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(64);
    titleLabel.setBounds(header.removeFromTop(30));
    hintLabel.setBounds(header);
    area.removeFromTop(12);
    controls.setBounds(area);
}
