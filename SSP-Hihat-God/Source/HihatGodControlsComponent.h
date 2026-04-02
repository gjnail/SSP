#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"
#include "HihatPresetBrowserComponent.h"

class HihatGodControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    HihatGodControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~HihatGodControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool handleKeyPress(const juce::KeyPress&);

private:
    class HihatKnobPanel;
    class MotionVisualizer;

    void timerCallback() override;
    void updateDynamicText();

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label eyebrowLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetLabel;
    juce::Label presetMetaLabel;
    juce::Label statusLabel;
    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton presetButton{"Subtle Humanize"};
    ssp::ui::SSPButton nextPresetButton{">"};
    std::unique_ptr<MotionVisualizer> visualizer;
    std::unique_ptr<HihatKnobPanel> variationKnob;
    std::unique_ptr<HihatKnobPanel> rateKnob;
    std::unique_ptr<HihatKnobPanel> volumeRangeKnob;
    std::unique_ptr<HihatKnobPanel> panRangeKnob;
    HihatPresetBrowserComponent presetBrowser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HihatGodControlsComponent)
};
