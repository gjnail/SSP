#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 960;
constexpr int editorHeight = 660;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Hyper Resonance", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Disperser-style phase shaping with SSP seasoning: focus a band, stack the phase shift, then smear or snap transients without leaning on EQ gain.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd2d2c4));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff121316), 0.0f, 0.0f, juce::Colour(0xff090b0e), 0.0f, (float) getHeight(), false);
    bg.addColour(0.42, juce::Colour(0xff161b22));
    bg.addColour(0.78, juce::Colour(0xff11161b));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(24.0f, 70.0f);
    juce::ColourGradient topGlow(juce::Colour(0x24ffb15a), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x1648d7ff), upperGlow.getRight(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.55f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(76.0f, 160.0f).translated(0.0f, 32.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x1eff7e52), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x0048d7ff), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow);

    g.setColour(juce::Colour(0xff3d454e));
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
