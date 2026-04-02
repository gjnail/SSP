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
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void styleLabel(juce::Label&, const juce::String& text);
    void styleKnob(ssp::ui::SSPKnob&, juce::Colour accent);
    void styleStatusLabel(juce::Label&);

    PluginProcessor& processor;

    juce::Rectangle<int> harmonyPanel;
    juce::Rectangle<int> tonePanel;
    juce::Rectangle<int> motionPanel;
    juce::Rectangle<int> protectPanel;
    juce::Rectangle<int> statusPanel;
    juce::Label sectionLabel;
    juce::Label helperLabel;
    juce::Label harmonySummaryLabel;
    juce::Label voiceSummaryLabel;
    juce::Label sourceStatusLabel;
    juce::Label motionStatusLabel;
    juce::Label protectStatusLabel;

    juce::Label rootLabel;
    juce::Label octaveLabel;
    juce::Label chordLabel;
    juce::Label voicingLabel;
    juce::Label vowelLabel;
    juce::Label driveLabel;
    juce::Label resonanceLabel;
    juce::Label brightnessLabel;
    juce::Label formantLabel;
    juce::Label detuneLabel;
    juce::Label widthLabel;
    juce::Label motionDepthLabel;
    juce::Label motionRateLabel;
    juce::Label divisionLabel;
    juce::Label protectLabel;
    juce::Label crossoverLabel;
    juce::Label transientLabel;
    juce::Label mixLabel;
    juce::Label outputLabel;

    ssp::ui::SSPDropdown rootBox;
    ssp::ui::SSPDropdown octaveBox;
    ssp::ui::SSPDropdown chordBox;
    ssp::ui::SSPDropdown voicingBox;
    ssp::ui::SSPDropdown vowelBox;
    ssp::ui::SSPDropdown divisionBox;
    ssp::ui::SSPKnob driveKnob;
    ssp::ui::SSPKnob resonanceKnob;
    ssp::ui::SSPKnob brightnessKnob;
    ssp::ui::SSPKnob formantKnob;
    ssp::ui::SSPKnob detuneKnob;
    ssp::ui::SSPKnob widthKnob;
    ssp::ui::SSPKnob motionDepthKnob;
    ssp::ui::SSPKnob motionRateKnob;
    ssp::ui::SSPKnob protectKnob;
    ssp::ui::SSPKnob crossoverKnob;
    ssp::ui::SSPKnob transientKnob;
    ssp::ui::SSPKnob mixKnob;
    ssp::ui::SSPKnob outputKnob;
    ssp::ui::SSPToggle midiFollowToggle{"MIDI Root"};
    ssp::ui::SSPToggle motionSyncToggle{"Sync"};

    std::unique_ptr<ComboAttachment> rootAttachment;
    std::unique_ptr<ComboAttachment> octaveAttachment;
    std::unique_ptr<ComboAttachment> chordAttachment;
    std::unique_ptr<ComboAttachment> voicingAttachment;
    std::unique_ptr<ComboAttachment> vowelAttachment;
    std::unique_ptr<ComboAttachment> divisionAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> brightnessAttachment;
    std::unique_ptr<SliderAttachment> formantAttachment;
    std::unique_ptr<SliderAttachment> detuneAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> motionDepthAttachment;
    std::unique_ptr<SliderAttachment> motionRateAttachment;
    std::unique_ptr<SliderAttachment> protectAttachment;
    std::unique_ptr<SliderAttachment> crossoverAttachment;
    std::unique_ptr<SliderAttachment> transientAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ButtonAttachment> midiFollowAttachment;
    std::unique_ptr<ButtonAttachment> motionSyncAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorControlsComponent)
};
