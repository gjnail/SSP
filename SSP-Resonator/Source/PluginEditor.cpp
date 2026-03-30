#include "PluginEditor.h"
#include "SSPVectorUI.h"

namespace
{
constexpr int editorWidth = 1080;
constexpr int editorHeight = 820;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      graph(p),
      controls(p)
{
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);

    titleLabel.setText("SSP Resonator", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(31.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Bright chord-driven resonance for drums, vocals, synths and textures. Dial the harmony below, then feed it audio to light up the visualiser.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.8f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff98aec7));
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(62);
    titleLabel.setBounds(header.removeFromTop(34));
    hintLabel.setBounds(header);

    area.removeFromTop(12);
    graph.setBounds(area.removeFromTop(452));
    area.removeFromTop(14);
    controls.setBounds(area);
}
