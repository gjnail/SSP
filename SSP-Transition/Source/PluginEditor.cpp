#include "PluginEditor.h"
#include "TransitionVectorUI.h"

namespace
{
constexpr int editorWidth = 980;
constexpr int editorHeight = 720;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      controls(p, p.apvts)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Transition", juce::dontSendNotification);
    titleLabel.setFont(transitionui::titleFont(29.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, transitionui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("One-knob builds and easy wash-outs with SSP's metal-panel vector UI.",
                      juce::dontSendNotification);
    hintLabel.setFont(transitionui::bodyFont(11.5f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, transitionui::textMuted());
    addAndMakeVisible(hintLabel);

    subHintLabel.setText("Preset the mood, automate one knob, and use the same control for a drop lift or a fast section wash.",
                         juce::dontSendNotification);
    subHintLabel.setFont(transitionui::bodyFont(10.5f));
    subHintLabel.setJustificationType(juce::Justification::centredLeft);
    subHintLabel.setColour(juce::Label::textColourId, transitionui::textMuted().withAlpha(0.88f));
    addAndMakeVisible(subHintLabel);

    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    transitionui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(1.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);

    auto header = area.removeFromTop(76);
    titleLabel.setBounds(header.removeFromTop(32));
    hintLabel.setBounds(header.removeFromTop(18));
    subHintLabel.setBounds(header);

    area.removeFromTop(10);
    controls.setBounds(area);
}
