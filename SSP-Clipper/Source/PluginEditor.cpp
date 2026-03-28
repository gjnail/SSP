#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 920;
constexpr int editorHeight = 640;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Clipper", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("4x oversampled peak shaving with just enough shape control to move from invisible cleanup to obvious bite.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa6b1bc));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff181214), 0.0f, 0.0f, juce::Colour(0xff0e1014), 0.0f, (float) getHeight(), false);
    bg.addColour(0.46, juce::Colour(0xff19161b));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(34.0f, 90.0f);
    juce::ColourGradient topGlow(juce::Colour(0x28ff9a3d), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x00ff9a3d), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.66f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(60.0f, 180.0f);
    juce::ColourGradient redGlow(juce::Colour(0x1fff5e3a), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                 juce::Colour(0x00ff5e3a), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(redGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 56.0f));

    g.setColour(juce::Colour(0xff302d34));
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
