#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1240;
constexpr int editorHeight = 860;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);
    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void PluginEditor::resized()
{
    controls.setBounds(getLocalBounds());
}
