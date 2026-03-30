#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1080;
constexpr int editorHeight = 780;
const juce::String defaultHintText = "Stereo sculpting with a live field view. Drag the crossover handles to re-split the spectrum.";
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      graph(p),
      controls(p),
      presetBrowser(p)
{
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Imager", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText(defaultHintText, juce::dontSendNotification);
    hintLabel.setFont(12.0f);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8aa2bf));
    addAndMakeVisible(hintLabel);

    presetNameLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    presetNameLabel.setColour(juce::Label::textColourId, ssp::ui::textStrong());
    addAndMakeVisible(presetNameLabel);

    presetMetaLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);
    presetMetaLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
    addAndMakeVisible(presetMetaLabel);

    visualiserModeLabel.setText("VIEW", juce::dontSendNotification);
    visualiserModeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    visualiserModeLabel.setJustificationType(juce::Justification::centredLeft);
    visualiserModeLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
    addAndMakeVisible(visualiserModeLabel);

    previousPresetButton.onClick = [this]
    {
        processorRef.stepPreset(-1);
        refreshPresetHeader();
    };
    addAndMakeVisible(previousPresetButton);

    nextPresetButton.onClick = [this]
    {
        processorRef.stepPreset(1);
        refreshPresetHeader();
    };
    addAndMakeVisible(nextPresetButton);

    browsePresetsButton.onClick = [this]
    {
        presetBrowser.toggle();
        refreshPresetHeader();
    };
    addAndMakeVisible(browsePresetsButton);

    learnButton.onClick = [this]
    {
        if (processorRef.isAutoLearnActive())
            processorRef.cancelAutoLearn();
        else
            processorRef.startAutoLearn();

        refreshPresetHeader();
    };
    addAndMakeVisible(learnButton);

    visualiserModeBox.addItem("Goniometer", 1);
    visualiserModeBox.addItem("Polar Sample", 2);
    visualiserModeBox.addItem("Stereo Trail", 3);
    visualiserModeBox.setSelectedId(1, juce::dontSendNotification);
    visualiserModeBox.onChange = [this]
    {
        graph.setVisualiserMode(static_cast<ImagerGraphComponent::VisualiserMode>(visualiserModeBox.getSelectedItemIndex()));
    };
    addAndMakeVisible(visualiserModeBox);

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

    bypassButton.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, "bypass", bypassButton);
    addAndMakeVisible(bypassButton);

    addAndMakeVisible(graph);
    addAndMakeVisible(controls);
    addAndMakeVisible(presetBrowser);
    presetBrowser.setVisible(false);

    refreshPresetHeader();
    startTimerHz(24);
}

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto header = area.removeFromTop(90);
    auto titleRow = header.removeFromTop(42);
    titleLabel.setBounds(titleRow.removeFromLeft(180));

    auto rightControls = titleRow.removeFromRight(170);
    bypassButton.setBounds(rightControls.removeFromRight(48).reduced(0, 2));
    rightControls.removeFromRight(8);
    outputGainLabel.setBounds(rightControls.removeFromBottom(14));
    outputGainKnob.setBounds(rightControls);

    auto presetStrip = titleRow.reduced(8, 0);
    previousPresetButton.setBounds(presetStrip.removeFromLeft(44));
    presetStrip.removeFromLeft(10);
    browsePresetsButton.setBounds(presetStrip.removeFromRight(96));
    presetStrip.removeFromRight(10);
    nextPresetButton.setBounds(presetStrip.removeFromRight(44));
    presetStrip.removeFromRight(12);

    auto presetTextArea = presetStrip;
    presetMetaLabel.setBounds(presetTextArea.removeFromBottom(14));
    presetNameLabel.setBounds(presetTextArea);

    auto utilityRow = header;
    auto utilityControls = utilityRow.removeFromRight(350);
    learnButton.setBounds(utilityControls.removeFromRight(112));
    utilityControls.removeFromRight(10);
    visualiserModeBox.setBounds(utilityControls.removeFromRight(150));
    utilityControls.removeFromRight(8);
    visualiserModeLabel.setBounds(utilityControls.removeFromRight(56));
    hintLabel.setBounds(utilityRow);

    area.removeFromTop(12);
    graph.setBounds(area.removeFromTop(376));

    area.removeFromTop(14);
    controls.setBounds(area);
    presetBrowser.setBounds(getLocalBounds());
}

void PluginEditor::timerCallback()
{
    controls.resized();

    if (processorRef.applyPendingAutoLearnResult())
        hintLabel.setText("Learn finished. The crossover points were rebalanced from the incoming material.", juce::dontSendNotification);

    refreshPresetHeader();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    return presetBrowser.handleKeyPress(key) || juce::AudioProcessorEditor::keyPressed(key);
}

void PluginEditor::refreshPresetHeader()
{
    auto presetName = processorRef.getCurrentPresetName();
    if (presetName.isEmpty())
        presetName = "Current Settings";

    presetNameLabel.setText(presetName, juce::dontSendNotification);

    juce::String meta = processorRef.isCurrentPresetFactory() ? "Factory" : "User";
    const auto category = processorRef.getCurrentPresetCategory().trim();
    if (category.isNotEmpty())
        meta = category + "  |  " + meta;
    if (processorRef.isCurrentPresetDirty())
        meta << "  |  edited";
    if (processorRef.isAutoLearnActive())
        meta << "  |  learning";

    presetMetaLabel.setText(meta, juce::dontSendNotification);

    if (processorRef.isAutoLearnActive())
    {
        const auto percent = (int) std::round(processorRef.getAutoLearnProgress() * 100.0f);
        learnButton.setButtonText("LEARN " + juce::String(percent) + "%");
        hintLabel.setText("Listening to the input and redistributing crossover points by spectral balance...", juce::dontSendNotification);
    }
    else
    {
        learnButton.setButtonText("LEARN");
        if (hintLabel.getText().isEmpty() || hintLabel.getText().containsIgnoreCase("Listening to the input"))
            hintLabel.setText(defaultHintText, juce::dontSendNotification);
    }
}
