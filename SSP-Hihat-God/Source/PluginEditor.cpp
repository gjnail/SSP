#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 980;
constexpr int editorHeight = 680;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Hihat God", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Host-synced sine-LFO gain and pan movement for hats so repeated sequences feel wider, looser, and less machine-perfect.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc4c7d8));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff0d1017), 0.0f, 0.0f, juce::Colour(0xff080b10), 0.0f, (float) getHeight(), false);
    bg.addColour(0.46, juce::Colour(0xff111520));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(36.0f, 84.0f);
    juce::ColourGradient topGlow(juce::Colour(0x247ad2ff), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x1467f0d5), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.62f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(84.0f, 206.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x18ffc97a), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x00ffc97a), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 58.0f));

    g.setColour(juce::Colour(0xff3e3f54));
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
