#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1160;
constexpr int editorHeight = 940;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP 3D Waveshaper", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(33.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Morph through internal shaper tables with phase, bias, smoothing, overflow control, DC filtering, and dry/wet blend.",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(13.0f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb8d0dc));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff091119), 0.0f, 0.0f, juce::Colour(0xff06090d), 0.0f, (float) getHeight(), false);
    bg.addColour(0.42, juce::Colour(0xff10202a));
    bg.addColour(0.72, juce::Colour(0xff0c141c));
    g.setGradientFill(bg);
    g.fillAll();

    auto topGlow = getLocalBounds().toFloat().reduced(40.0f, 70.0f);
    juce::ColourGradient upper(juce::Colour(0x245ae6ff), topGlow.getX(), topGlow.getY(),
                               juce::Colour(0x149ffccf), topGlow.getCentreX(), topGlow.getBottom(), false);
    g.setGradientFill(upper);
    g.fillEllipse(topGlow.removeFromTop(topGlow.getHeight() * 0.56f));

    auto bottomGlow = getLocalBounds().toFloat().reduced(90.0f, 260.0f);
    juce::ColourGradient lower(juce::Colour(0x18ff8766), bottomGlow.getCentreX(), bottomGlow.getBottom(),
                               juce::Colour(0x005ae6ff), bottomGlow.getCentreX(), bottomGlow.getY(), false);
    g.setGradientFill(lower);
    g.fillEllipse(bottomGlow.translated(0.0f, 48.0f));

    g.setColour(juce::Colour(0xff314655));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(24, 20);

    auto header = area.removeFromTop(74);
    titleLabel.setBounds(header.removeFromTop(38));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    controls.setBounds(area);
}
