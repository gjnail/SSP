#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1020;
constexpr int editorHeight = 720;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      validatorComponent(p)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Sub Validator", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Simple low-end validation for the 20 Hz to 200 Hz range, with a freezeable spectrum view and a fast pass/fail status.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc8becb));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(validatorComponent);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff120f17), 0.0f, 0.0f, juce::Colour(0xff09090f), 0.0f, (float) getHeight(), false);
    bg.addColour(0.46, juce::Colour(0xff15121a));
    g.setGradientFill(bg);
    g.fillAll();

    auto upperGlow = getLocalBounds().toFloat().reduced(34.0f, 82.0f);
    juce::ColourGradient topGlow(juce::Colour(0x2474d7ff), upperGlow.getX(), upperGlow.getY(),
                                 juce::Colour(0x1479f0b7), upperGlow.getCentreX(), upperGlow.getBottom(), false);
    g.setGradientFill(topGlow);
    g.fillEllipse(upperGlow.removeFromTop(upperGlow.getHeight() * 0.62f));

    auto lowerGlow = getLocalBounds().toFloat().reduced(84.0f, 208.0f);
    juce::ColourGradient bottomGlow(juce::Colour(0x18ffc978), lowerGlow.getCentreX(), lowerGlow.getBottom(),
                                    juce::Colour(0x00ffc978), lowerGlow.getCentreX(), lowerGlow.getY(), false);
    g.setGradientFill(bottomGlow);
    g.fillEllipse(lowerGlow.translated(0.0f, 58.0f));

    g.setColour(juce::Colour(0xff3e3842));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(68);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    validatorComponent.setBounds(area);
}
