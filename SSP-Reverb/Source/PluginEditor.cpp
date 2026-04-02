#include "PluginEditor.h"
#include "ReverbVectorUI.h"

namespace
{
constexpr int editorWidth = 1280;
constexpr int editorHeight = 880;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Reverb", juce::dontSendNotification);
    titleLabel.setFont(reverbui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reverbui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Live-inspired granular front panel for shaping filters, reflections, diffusion, modulation, and mix in one pass.",
                      juce::dontSendNotification);
    hintLabel.setFont(reverbui::bodyFont(10.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    reverbui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(2.0f));
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
