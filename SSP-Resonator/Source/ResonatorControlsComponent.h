#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class ResonatorControlsComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit ResonatorControlsComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void styleLabel(juce::Label&, const juce::String& text);
    void styleKnob(ssp::ui::SSPKnob&, juce::Colour accent);

    PluginProcessor& processor;

    juce::Rectangle<int> harmonyPanel;
    juce::Rectangle<int> tonePanel;
    juce::Rectangle<int> mixPanel;
    juce::Rectangle<int> statusPanel;

    juce::Label sectionLabel;
    juce::Label helperLabel;
    juce::Label harmonySummaryLabel;
    juce::Label voiceSummaryLabel;
    juce::Label statusCopyLabel;
    juce::Label rootLabel;
    juce::Label octaveLabel;
    juce::Label chordLabel;
    juce::Label voicingLabel;
    juce::Label driveLabel;
    juce::Label resonanceLabel;
    juce::Label brightnessLabel;
    juce::Label detuneLabel;
    juce::Label widthLabel;
    juce::Label mixLabel;
    juce::Label outputLabel;

    ssp::ui::SSPDropdown rootBox;
    ssp::ui::SSPDropdown octaveBox;
    ssp::ui::SSPDropdown chordBox;
    ssp::ui::SSPDropdown voicingBox;
    ssp::ui::SSPKnob driveKnob;
    ssp::ui::SSPKnob resonanceKnob;
    ssp::ui::SSPKnob brightnessKnob;
    ssp::ui::SSPKnob detuneKnob;
    ssp::ui::SSPKnob widthKnob;
    ssp::ui::SSPKnob mixKnob;
    ssp::ui::SSPKnob outputKnob;

    std::unique_ptr<ComboAttachment> rootAttachment;
    std::unique_ptr<ComboAttachment> octaveAttachment;
    std::unique_ptr<ComboAttachment> chordAttachment;
    std::unique_ptr<ComboAttachment> voicingAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> brightnessAttachment;
    std::unique_ptr<SliderAttachment> detuneAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorControlsComponent)
};
