#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 760;
constexpr int editorHeight = 540;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Beef", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("One big enhancer knob for instant weight, gloss, glue, and attitude.", juce::dontSendNotification);
    hintLabel.setFont(12.8f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcda98e));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff1b1110), 0.0f, 0.0f,
                            juce::Colour(0xff0d0a0a), 0.0f, (float) getHeight(), false);
    bg.addColour(0.38, juce::Colour(0xff241413));
    bg.addColour(0.72, juce::Colour(0xff151012));
    g.setGradientFill(bg);
    g.fillAll();

    auto glowArea = getLocalBounds().toFloat().reduced(24.0f, 86.0f);
    juce::ColourGradient glow(juce::Colour(0x00ff9242), glowArea.getX(), glowArea.getCentreY(),
                              juce::Colour(0x00ff9242), glowArea.getRight(), glowArea.getCentreY(), false);
    glow.addColour(0.35, juce::Colour(0x24ff9242));
    glow.addColour(0.65, juce::Colour(0x18ffcf58));
    g.setGradientFill(glow);
    g.fillEllipse(glowArea.expanded(46.0f, 54.0f));

    g.setColour(juce::Colour(0xff44302b));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(18, 16);

    auto header = area.removeFromTop(64);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(10);
    controls.setBounds(area);
}
