#include "PluginEditor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(1240, 760);
    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080808));
}

void PluginEditor::resized()
{
    controls.setBounds(getLocalBounds());
}
