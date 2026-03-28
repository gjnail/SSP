#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 980;
constexpr int editorHeight = 780;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Transient Control", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Simple attack and sustain shaping with optional clipping, output trim, and dry/wet blend.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd5cfc4));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff15100d), 0.0f, 0.0f, juce::Colour(0xff0b0908), 0.0f, (float) getHeight(), false);
    bg.addColour(0.48, juce::Colour(0xff1e1713));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(36.0f, 84.0f);
    juce::ColourGradient topGlow(juce::Colour(0x28ff8b5c), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x16ffd06a), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.58f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(92.0f, 230.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x18ff6b5f), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x00ff6b5f), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 46.0f));

    g.setColour(juce::Colour(0xff4a403c));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(68);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    controls.setBounds(area);
}
