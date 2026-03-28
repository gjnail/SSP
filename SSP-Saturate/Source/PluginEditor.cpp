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

    titleLabel.setText("SSP Saturate", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Warm stereo saturation with drive, color, asymmetry, output trim, and dry/wet blend.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdac9bf));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff130d0b), 0.0f, 0.0f, juce::Colour(0xff090707), 0.0f, (float) getHeight(), false);
    bg.addColour(0.45, juce::Colour(0xff1d1411));
    g.setGradientFill(bg);
    g.fillAll();

    auto topGlow = getLocalBounds().toFloat().reduced(34.0f, 78.0f);
    juce::ColourGradient upper(juce::Colour(0x24ff9a63), topGlow.getX(), topGlow.getY(),
                               juce::Colour(0x18ffcf72), topGlow.getCentreX(), topGlow.getBottom(), false);
    g.setGradientFill(upper);
    g.fillEllipse(topGlow.removeFromTop(topGlow.getHeight() * 0.6f));

    auto bottomGlow = getLocalBounds().toFloat().reduced(90.0f, 230.0f);
    juce::ColourGradient lower(juce::Colour(0x1aff6f5f), bottomGlow.getCentreX(), bottomGlow.getBottom(),
                               juce::Colour(0x00ff6f5f), bottomGlow.getCentreX(), bottomGlow.getY(), false);
    g.setGradientFill(lower);
    g.fillEllipse(bottomGlow.translated(0.0f, 56.0f));

    g.setColour(juce::Colour(0xff4b3f38));
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
