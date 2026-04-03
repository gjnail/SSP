#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzerComponent.h"
#include "SpectrumControlsComponent.h"
#include "SpectrumVectorUI.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshHeaderColours();

    PluginProcessor& processorRef;
    spectrumui::LookAndFeel lookAndFeel;
    SpectrumAnalyzerComponent analyzer;
    SpectrumControlsComponent controls;

    juce::Label titleLabel;
    juce::Label tagLabel;
    juce::Label hintLabel;

    spectrumui::SSPButton captureAButton{ "REF A" };
    spectrumui::SSPButton captureBButton{ "REF B" };
    spectrumui::SSPButton clearRefButton{ "CLEAR REFS" };

    int lastThemeIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
