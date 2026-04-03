#include "PluginEditor.h"
#include "MixerVectorUI.h"

namespace
{
constexpr int editorWidth = 900;
constexpr int editorHeight = 744;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Mixer", juce::dontSendNotification);
    titleLabel.setFont(mixerui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, mixerui::textStrong());
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    mixerui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(4.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);
    auto header = area.removeFromTop(46);
    titleLabel.setBounds(header.removeFromLeft(220));
    area.removeFromTop(10);
    controls.setBounds(area);
}
