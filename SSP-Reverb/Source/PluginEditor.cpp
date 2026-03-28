#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 860;
constexpr int editorHeight = 590;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Reverb", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(29.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Modern algorithmic reverb with lush modes, quick tone control, and a clean mix-focused front panel.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff98a8b8));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff11161d), 0.0f, 0.0f, juce::Colour(0xff0c1015), 0.0f, (float) getHeight(), false);
    bg.addColour(0.44, juce::Colour(0xff151b24));
    g.setGradientFill(bg);
    g.fillAll();

    auto glowArea = getLocalBounds().toFloat().reduced(18.0f, 76.0f);
    juce::ColourGradient glow(juce::Colour(0x0028c8ff), glowArea.getX(), glowArea.getCentreY(),
                              juce::Colour(0x0028c8ff), glowArea.getRight(), glowArea.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x2628c8ff));
    g.setGradientFill(glow);
    g.fillEllipse(glowArea.expanded(28.0f, 46.0f));

    g.setColour(juce::Colour(0xff28313a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(18, 16);
    auto header = area.removeFromTop(66);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);
    area.removeFromTop(10);
    controls.setBounds(area);
}
