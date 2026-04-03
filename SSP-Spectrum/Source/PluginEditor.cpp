#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1320;
constexpr int editorHeight = 930;
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      lookAndFeel([this] { return processorRef.getThemeIndex(); }),
      analyzer(p),
      controls(p)
{
    setLookAndFeel(&lookAndFeel);
    setSize(editorWidth, editorHeight);
    setResizable(true, true);
    setResizeLimits(1120, 820, 1800, 1300);

    titleLabel.setText("SSP Spectrum", juce::dontSendNotification);
    titleLabel.setFont(spectrumui::titleFont(28.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    tagLabel.setText("PREMIUM FFT WORKFLOW. STATIC REFERENCES. MID/SIDE. WATERFALL.", juce::dontSendNotification);
    tagLabel.setFont(spectrumui::smallCapsFont(10.5f));
    tagLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(tagLabel);

    hintLabel.setText("Capture A/B references, switch into producer presets like Kick Tuner or Mastering Inspect, and use Shift-click on A or B whenever you want to refresh that stored trace.",
                      juce::dontSendNotification);
    hintLabel.setFont(spectrumui::bodyFont(11.2f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hintLabel);

    captureAButton.onClick = [this] { analyzer.handleReferenceA(juce::ModifierKeys::getCurrentModifiersRealtime()); };
    captureBButton.onClick = [this] { analyzer.handleReferenceB(juce::ModifierKeys::getCurrentModifiersRealtime()); };
    clearRefButton.onClick = [this] { analyzer.clearReferences(); };

    addAndMakeVisible(captureAButton);
    addAndMakeVisible(captureBButton);
    addAndMakeVisible(clearRefButton);

    addAndMakeVisible(analyzer);
    addAndMakeVisible(controls);

    refreshHeaderColours();
    startTimerHz(12);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditor::refreshHeaderColours()
{
    const auto palette = spectrumui::getPalette(processorRef.getThemeIndex());
    titleLabel.setColour(juce::Label::textColourId, palette.textStrong);
    tagLabel.setColour(juce::Label::textColourId, palette.textMuted);
    hintLabel.setColour(juce::Label::textColourId, palette.textMuted);
}

void PluginEditor::timerCallback()
{
    const int theme = processorRef.getThemeIndex();
    if (theme != lastThemeIndex)
    {
        lastThemeIndex = theme;
        refreshHeaderColours();
        repaint();
        analyzer.repaint();
        controls.repaint();
    }

    captureAButton.setToggleState(analyzer.isReferenceAVisible(), juce::dontSendNotification);
    captureBButton.setToggleState(analyzer.isReferenceBVisible(), juce::dontSendNotification);
    clearRefButton.setEnabled(analyzer.hasReferenceA() || analyzer.hasReferenceB());
}

void PluginEditor::paint(juce::Graphics& g)
{
    const auto palette = spectrumui::getPalette(processorRef.getThemeIndex());
    spectrumui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(8.0f), palette);

    auto content = getLocalBounds().toFloat().reduced(24.0f, 18.0f);
    auto header = content.removeFromTop(56.0f);
    auto divider = header.withTrimmedLeft(420.0f).withTrimmedRight(12.0f).withHeight(1.0f).withCentre({ header.getCentreX(), header.getBottom() - 2.0f });
    g.setColour(palette.outlineSoft.withAlpha(0.7f));
    g.fillRect(divider);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(24, 18);
    auto header = area.removeFromTop(74);

    auto titleArea = header.removeFromLeft(560);
    titleLabel.setBounds(titleArea.removeFromTop(32));
    tagLabel.setBounds(titleArea.removeFromTop(14));
    titleArea.removeFromTop(4);
    hintLabel.setBounds(titleArea);

    auto buttonArea = header.removeFromRight(360);
    captureAButton.setBounds(buttonArea.removeFromLeft(86).reduced(0, 6));
    buttonArea.removeFromLeft(8);
    captureBButton.setBounds(buttonArea.removeFromLeft(86).reduced(0, 6));
    buttonArea.removeFromLeft(8);
    clearRefButton.setBounds(buttonArea.removeFromLeft(132).reduced(0, 6));

    area.removeFromTop(8);

    auto controlsHeight = juce::jmax(360, (int) std::round(area.getHeight() * 0.38f));
    analyzer.setBounds(area.removeFromTop(area.getHeight() - controlsHeight));
    area.removeFromTop(10);
    controls.setBounds(area);
}
