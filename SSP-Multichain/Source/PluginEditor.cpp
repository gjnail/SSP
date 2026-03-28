#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1040;
constexpr int editorHeight = 900;
constexpr int eqHeight = 220;
constexpr int shapeHeight = 320;
constexpr int controlsHeight = 220;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      crossoverView(p.apvts),
      bandCurveEditor(p),
      controlsSection(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    addAndMakeVisible(crossoverView);
    addAndMakeVisible(bandCurveEditor);
    addAndMakeVisible(controlsSection);

    titleLabel.setText("SSP Multiband Sidechain", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText(
        "Shape the movement in the graph above, choose the trigger mode below, and use the sliders to balance the bands.",
        juce::dontSendNotification);
    hintLabel.setFont(12.0f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ba8b5));
    addAndMakeVisible(hintLabel);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff171d24), 0.0f, 0.0f, juce::Colour(0xff0f1217), 0.0f, (float)getHeight(), false);
    bg.addColour(0.45, juce::Colour(0xff121923));
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(juce::Colour(0xff2a3038));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced(14);

    crossoverView.setBounds(r.removeFromTop(eqHeight));
    r.removeFromTop(8);

    bandCurveEditor.setBounds(r.removeFromTop(shapeHeight));
    r.removeFromTop(10);

    auto headerArea = r.removeFromTop(54);
    titleLabel.setBounds(headerArea.removeFromTop(26));
    hintLabel.setBounds(headerArea);
    r.removeFromTop(8);

    controlsSection.setBounds(r.removeFromTop(controlsHeight));
}
