#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1240;
constexpr int editorHeight = 860;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);
    setResizable(true, true);
    setResizeLimits(1080, 760, 1680, 1200);
    setWantsKeyboardFocus(true);
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

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    if (controls.handleKeyPress(key))
        return true;

    return juce::AudioProcessorEditor::keyPressed(key);
}
