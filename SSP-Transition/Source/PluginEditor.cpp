#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 760;
constexpr int editorHeight = 560;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Transition", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Preset-led fades, wide washes, and one big macro move to get out of a section fast.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff92a0ae));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff15161c), 0.0f, 0.0f, juce::Colour(0xff0f1115), 0.0f, (float) getHeight(), false);
    bg.addColour(0.48, juce::Colour(0xff181a22));
    g.setGradientFill(bg);
    g.fillAll();

    auto glowArea = getLocalBounds().toFloat().reduced(18.0f, 80.0f);
    juce::ColourGradient glow(juce::Colour(0x00ffb347), glowArea.getX(), glowArea.getCentreY(),
                              juce::Colour(0x00ffb347), glowArea.getRight(), glowArea.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x22ffb347));
    g.setGradientFill(glow);
    g.fillEllipse(glowArea.expanded(24.0f, 40.0f));

    g.setColour(juce::Colour(0xff2b3138));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(18, 16);

    auto header = area.removeFromTop(62);
    titleLabel.setBounds(header.removeFromTop(32));
    hintLabel.setBounds(header);

    area.removeFromTop(10);
    controls.setBounds(area);
}
