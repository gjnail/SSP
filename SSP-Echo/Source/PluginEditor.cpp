#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 960;
constexpr int editorHeight = 660;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Echo", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Short-to-mid repeats with worn tone, repeat saturation, and flutter for movement instead of perfectly clinical copies.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc6b8a4));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff18130f), 0.0f, 0.0f, juce::Colour(0xff0f1012), 0.0f, (float) getHeight(), false);
    bg.addColour(0.46, juce::Colour(0xff1b1611));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(36.0f, 88.0f);
    juce::ColourGradient topGlow(juce::Colour(0x24ff9a3d), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x16a2c26c), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.64f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(72.0f, 200.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x1cffb15c), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x00ffb15c), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 60.0f));

    g.setColour(juce::Colour(0xff312821));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(66);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    controls.setBounds(area);
}
