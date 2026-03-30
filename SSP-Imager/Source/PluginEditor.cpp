#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 900;
constexpr int editorHeight = 700;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      graph(p),
      controls(p)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Imager", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Drag crossover handles to split frequencies. Adjust width per band: 0% = mono, 100% = stereo, 200% = extra wide.",
                      juce::dontSendNotification);
    hintLabel.setFont(12.0f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8aa2bf));
    addAndMakeVisible(hintLabel);

    // Output gain knob
    outputGainKnob.setRange(-24.0, 12.0, 0.1);
    outputGainKnob.setTextValueSuffix(" dB");
    outputGainKnob.setDoubleClickReturnValue(true, 0.0);
    outputGainKnob.setColour(juce::Slider::rotarySliderFillColourId, ssp::ui::brandGold());
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "outputGain", outputGainKnob);
    addAndMakeVisible(outputGainKnob);

    outputGainLabel.setText("OUTPUT", juce::dontSendNotification);
    outputGainLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    outputGainLabel.setJustificationType(juce::Justification::centred);
    outputGainLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
    addAndMakeVisible(outputGainLabel);

    // Bypass button
    bypassButton.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, "bypass", bypassButton);
    addAndMakeVisible(bypassButton);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);

    startTimerHz(12);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 18);

    // Header row
    auto header = area.removeFromTop(68);
    auto titleRow = header.removeFromTop(36);
    titleLabel.setBounds(titleRow.removeFromLeft(180));

    // Right side: output gain knob and bypass
    auto rightControls = titleRow.removeFromRight(160);
    bypassButton.setBounds(rightControls.removeFromRight(48).reduced(0, 2));
    rightControls.removeFromRight(8);
    outputGainLabel.setBounds(rightControls.removeFromBottom(14));
    outputGainKnob.setBounds(rightControls);

    hintLabel.setBounds(header);

    area.removeFromTop(10);

    // Graph (crossover visualization)
    graph.setBounds(area.removeFromTop(320));

    area.removeFromTop(14);

    // Band controls
    controls.setBounds(area);
}

void PluginEditor::timerCallback()
{
    // Trigger range label updates
    controls.resized();
}
