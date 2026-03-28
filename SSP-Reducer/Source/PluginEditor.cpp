#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 900;
constexpr int editorHeight = 610;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Reducer", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(29.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Focused bit reduction with mode-driven character, sample-rate crush, and fast dry/wet blending.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb39b92));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff171110), 0.0f, 0.0f, juce::Colour(0xff0f0b0b), 0.0f, (float) getHeight(), false);
    bg.addColour(0.44, juce::Colour(0xff201615));
    g.setGradientFill(bg);
    g.fillAll();

    auto glowArea = getLocalBounds().toFloat().reduced(18.0f, 76.0f);
    juce::ColourGradient glow(juce::Colour(0x00ff7a4d), glowArea.getX(), glowArea.getCentreY(),
                              juce::Colour(0x00ff7a4d), glowArea.getRight(), glowArea.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x26ff7a4d));
    g.setGradientFill(glow);
    g.fillEllipse(glowArea.expanded(32.0f, 48.0f));

    g.setColour(juce::Colour(0xff3a2c28));
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
