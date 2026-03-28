#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 450;
constexpr int editorHeight = 390;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      pluginProcessor(p),
      graph(p),
      controls(p.apvts)
{
    setLookAndFeel(&eq3ui::getLookAndFeel());
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP EQ3", juce::dontSendNotification);
    titleLabel.setFont(eq3ui::titleFont(18.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, eq3ui::textStrong());
    addAndMakeVisible(titleLabel);

    tagLabel.setText("THREE KNOBS. QUICK TONE.", juce::dontSendNotification);
    tagLabel.setFont(eq3ui::bodyFont(9.8f));
    tagLabel.setJustificationType(juce::Justification::centredLeft);
    tagLabel.setColour(juce::Label::textColourId, eq3ui::textMuted());
    addAndMakeVisible(tagLabel);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
    addAndMakeVisible(powerToggle);

    powerAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.apvts, "power", powerToggle);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    eq3ui::drawEditorBackdrop(g, bounds);

    auto content = bounds.reduced(12.0f, 10.0f);
    auto header = content.removeFromTop(34.0f);

    auto divider = header.withTrimmedLeft(166.0f).withTrimmedRight(88.0f).withHeight(1.0f).withCentre({ header.getCentreX(), header.getBottom() - 1.0f });
    g.setColour(eq3ui::outlineSoft().withAlpha(0.65f));
    g.fillRect(divider);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20, 18);
    auto header = bounds.removeFromTop(34);

    auto titleArea = header.removeFromLeft(220);
    titleLabel.setBounds(titleArea.removeFromTop(18));
    tagLabel.setBounds(titleArea.removeFromTop(12));
    powerToggle.setBounds(header.removeFromRight(94));

    bounds.removeFromTop(8);

    const int controlsHeight = juce::jmax(118, juce::roundToInt((float) bounds.getHeight() * 0.34f));
    graph.setBounds(bounds.removeFromTop(bounds.getHeight() - controlsHeight));
    bounds.removeFromTop(8);
    controls.setBounds(bounds);
}
