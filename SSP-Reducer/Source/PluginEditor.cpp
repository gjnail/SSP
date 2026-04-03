#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 560;
constexpr int editorHeight = 520;
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      graph(p),
      controls(p.apvts)
{
    setLookAndFeel(&reducerui::getVectorLookAndFeel());
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Reducer", juce::dontSendNotification);
    titleLabel.setFont(reducerui::titleFont(18.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reducerui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Kilohearts-leaning bitcrush with dither, ADC/DAC quality, filters, and spectrum preview.",
                      juce::dontSendNotification);
    hintLabel.setFont(reducerui::bodyFont(9.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, reducerui::textMuted());
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    reducerui::drawEditorBackdrop(g, bounds);

    auto content = bounds.reduced(12.0f, 10.0f);
    auto header = content.removeFromTop(34.0f);
    auto divider = header.withTrimmedLeft(182.0f).withTrimmedRight(16.0f).withHeight(1.0f).withCentre({ header.getCentreX(), header.getBottom() - 1.0f });
    g.setColour(reducerui::outlineSoft().withAlpha(0.65f));
    g.fillRect(divider);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20, 18);
    auto header = bounds.removeFromTop(34);

    auto titleArea = header.removeFromLeft(290);
    titleLabel.setBounds(titleArea.removeFromTop(18));
    hintLabel.setBounds(titleArea.removeFromTop(12));

    bounds.removeFromTop(8);
    const int graphHeight = 206;
    graph.setBounds(bounds.removeFromTop(graphHeight));
    bounds.removeFromTop(8);
    controls.setBounds(bounds);
}
