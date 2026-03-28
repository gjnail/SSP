#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 980;
constexpr int editorHeight = 690;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      graph(p),
      controls(p)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP EQ", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Double-click the graph to add a point, drag to move it, and choose the selected point type below. Up to 8 points.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.5f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8aa2bf));
    addAndMakeVisible(hintLabel);

    graph.onSelectionChanged = [this](int index)
    {
        controls.setSelectedPoint(index);
    };

    controls.onSelectionChanged = [this](int index)
    {
        graph.setSelectedPoint(index);
    };

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient background(juce::Colour(0xff0b1118), 0.0f, 0.0f,
                                    juce::Colour(0xff121b27), 0.0f, (float) getHeight(), false);
    background.addColour(0.42, juce::Colour(0xff0e1824));
    background.addColour(0.82, juce::Colour(0xff0a1017));
    g.setGradientFill(background);
    g.fillAll();

    auto glowBounds = getLocalBounds().toFloat().reduced(24.0f, 90.0f);
    juce::ColourGradient glow(juce::Colour(0x0032c7ff), glowBounds.getX(), glowBounds.getCentreY(),
                              juce::Colour(0x0024a0ff), glowBounds.getRight(), glowBounds.getCentreY(), false);
    glow.addColour(0.35, juce::Colour(0x202bb5ff));
    glow.addColour(0.7, juce::Colour(0x1046d8ff));
    g.setGradientFill(glow);
    g.fillEllipse(glowBounds.expanded(36.0f, 52.0f));

    g.setColour(juce::Colour(0xff29384a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(68);
    titleLabel.setBounds(header.removeFromTop(36));
    hintLabel.setBounds(header);

    area.removeFromTop(10);
    graph.setBounds(area.removeFromTop(420));
    area.removeFromTop(14);
    controls.setBounds(area);
}
