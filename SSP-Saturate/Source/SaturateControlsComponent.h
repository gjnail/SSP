#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class SaturateControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    SaturateControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~SaturateControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SaturateKnob;
    class SpectrumDisplay;
    class BandPanel;
    class GlobalPanel;

    void timerCallback() override;
    void setSelectedBand(int newBandIndex);
    void updateHeaderText();
    void applyPreset(int presetIndex);
    void stepPreset(int delta);
    void showPresetBrowser();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    int selectedBand = 0;
    int currentPresetIndex = 0;

    juce::Label eyebrowLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetLabel;
    juce::Label searchLabel;
    juce::Label selectedBandLabel;
    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton presetButton{"Gentle Warmth"};
    ssp::ui::SSPButton nextPresetButton{">"};

    std::unique_ptr<SpectrumDisplay> spectrumDisplay;
    std::unique_ptr<GlobalPanel> globalPanel;
    std::array<std::unique_ptr<BandPanel>, PluginProcessor::maxBands> bandPanels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturateControlsComponent)
};
