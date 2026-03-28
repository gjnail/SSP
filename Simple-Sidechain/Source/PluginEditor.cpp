#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 820;
constexpr int editorHeight = 790;
constexpr int shapeHeight = 430;
constexpr int controlsHeight = 250;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      bandCurveEditor(p),
      controlsSection(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    addAndMakeVisible(bandCurveEditor);
    addAndMakeVisible(controlsSection);

    titleLabel.setText("SSP Simple Sidechain", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Draw the shape above, choose how it triggers below, then blend it with the Mix knob.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.0f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ba8b5));
    addAndMakeVisible(hintLabel);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff171d24), 0.0f, 0.0f, juce::Colour(0xff0f1217), 0.0f, (float) getHeight(), false);
    bg.addColour(0.45, juce::Colour(0xff121923));
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(juce::Colour(0xff2a3038));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(14);

    auto header = area.removeFromTop(54);
    titleLabel.setBounds(header.removeFromTop(28));
    hintLabel.setBounds(header);

    area.removeFromTop(8);
    bandCurveEditor.setBounds(area.removeFromTop(shapeHeight));
    area.removeFromTop(10);
    controlsSection.setBounds(area.removeFromTop(controlsHeight));
}
