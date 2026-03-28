#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1020;
constexpr int editorHeight = 720;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      meterComponent(p)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Loud", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Pass-through loudness metering with momentary, short-term, integrated, peak, and resettable session history.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa9bbc8));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(meterComponent);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff0c1219), 0.0f, 0.0f, juce::Colour(0xff081017), 0.0f, (float) getHeight(), false);
    bg.addColour(0.46, juce::Colour(0xff0e151d));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(36.0f, 86.0f);
    juce::ColourGradient topGlow(juce::Colour(0x2467d5ff), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x1273f2c9), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.62f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(86.0f, 210.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x18ffc771), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x00ffc771), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 58.0f));

    g.setColour(juce::Colour(0xff31424f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(68);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    meterComponent.setBounds(area);
}
