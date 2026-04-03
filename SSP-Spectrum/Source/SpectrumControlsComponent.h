#pragma once

#include <memory>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SpectrumVectorUI.h"

class SpectrumControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    explicit SpectrumControlsComponent(PluginProcessor&);
    ~SpectrumControlsComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void styleLabel(juce::Label&);
    void refreshPresetUi();
    void timerCallback() override;

    PluginProcessor& processor;

    spectrumui::SSPButton previousPresetButton{ "<" };
    spectrumui::SSPDropdown presetBox;
    spectrumui::SSPButton nextPresetButton{ ">" };
    juce::Label presetLabel;
    juce::Label presetMetaLabel;

    spectrumui::SSPDropdown displayModeBox;
    spectrumui::SSPDropdown fftSizeBox;
    spectrumui::SSPDropdown overlapBox;
    spectrumui::SSPDropdown windowBox;
    spectrumui::SSPDropdown themeBox;

    juce::Label displayModeLabel;
    juce::Label fftSizeLabel;
    juce::Label overlapLabel;
    juce::Label windowLabel;
    juce::Label themeLabel;

    spectrumui::SSPKnob averageKnob;
    spectrumui::SSPKnob smoothKnob;
    spectrumui::SSPKnob slopeKnob;
    spectrumui::SSPKnob rangeKnob;
    spectrumui::SSPKnob holdKnob;

    juce::Label averageLabel;
    juce::Label smoothLabel;
    juce::Label slopeLabel;
    juce::Label rangeLabel;
    juce::Label holdLabel;

    spectrumui::SSPToggle freezeToggle{ "Pause" };
    spectrumui::SSPToggle waterfallToggle{ "Waterfall" };
    spectrumui::SSPButton clearMaxButton{ "Reset Max" };
    spectrumui::SSPButton clearClipButton{ "Clear Clip" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> displayModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> overlapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> themeAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> averageAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> waterfallAttachment;

    bool updatingPresetUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumControlsComponent)
};
