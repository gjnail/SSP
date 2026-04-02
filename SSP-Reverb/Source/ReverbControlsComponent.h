#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ReverbVectorUI.h"

class ReverbControlsComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    ReverbControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~ReverbControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ParameterKnob;
    class InputFilterGraph;
    class DiffusionGraph;

    void timerCallback() override;
    void applyTypePreset(int presetIndex);

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Rectangle<int> typeSelectorBounds;
    juce::Rectangle<int> inputSectionBounds;
    juce::Rectangle<int> earlySectionBounds;
    juce::Rectangle<int> modifiersSectionBounds;
    juce::Rectangle<int> globalSectionBounds;
    juce::Rectangle<int> diffusionSectionBounds;
    juce::Rectangle<int> diffusionLowBandBounds;
    juce::Rectangle<int> diffusionHighBandBounds;

    InputFilterGraph* inputFilterGraphPtr = nullptr;
    DiffusionGraph* diffusionGraphPtr = nullptr;
    std::unique_ptr<InputFilterGraph> inputFilterGraph;
    std::unique_ptr<DiffusionGraph> diffusionGraph;

    ParameterKnob* loCutKnobPtr = nullptr;
    ParameterKnob* hiCutKnobPtr = nullptr;
    ParameterKnob* earlyAmountKnobPtr = nullptr;
    ParameterKnob* earlyRateKnobPtr = nullptr;
    ParameterKnob* earlyShapeKnobPtr = nullptr;
    ParameterKnob* diffCrossoverKnobPtr = nullptr;
    ParameterKnob* lowDiffAmountKnobPtr = nullptr;
    ParameterKnob* lowDiffScaleKnobPtr = nullptr;
    ParameterKnob* lowDiffRateKnobPtr = nullptr;
    ParameterKnob* highDiffAmountKnobPtr = nullptr;
    ParameterKnob* highDiffScaleKnobPtr = nullptr;
    ParameterKnob* highDiffRateKnobPtr = nullptr;
    ParameterKnob* sizeKnobPtr = nullptr;
    ParameterKnob* decayKnobPtr = nullptr;
    ParameterKnob* predelayKnobPtr = nullptr;
    ParameterKnob* widthKnobPtr = nullptr;
    ParameterKnob* densityKnobPtr = nullptr;
    ParameterKnob* dryWetKnobPtr = nullptr;
    ParameterKnob* chorusAmountKnobPtr = nullptr;
    ParameterKnob* chorusRateKnobPtr = nullptr;
    ParameterKnob* reflectKnobPtr = nullptr;

    std::unique_ptr<ParameterKnob> loCutKnob;
    std::unique_ptr<ParameterKnob> hiCutKnob;
    std::unique_ptr<ParameterKnob> earlyAmountKnob;
    std::unique_ptr<ParameterKnob> earlyRateKnob;
    std::unique_ptr<ParameterKnob> earlyShapeKnob;
    std::unique_ptr<ParameterKnob> diffCrossoverKnob;
    std::unique_ptr<ParameterKnob> lowDiffAmountKnob;
    std::unique_ptr<ParameterKnob> lowDiffScaleKnob;
    std::unique_ptr<ParameterKnob> lowDiffRateKnob;
    std::unique_ptr<ParameterKnob> highDiffAmountKnob;
    std::unique_ptr<ParameterKnob> highDiffScaleKnob;
    std::unique_ptr<ParameterKnob> highDiffRateKnob;
    std::unique_ptr<ParameterKnob> sizeKnob;
    std::unique_ptr<ParameterKnob> decayKnob;
    std::unique_ptr<ParameterKnob> predelayKnob;
    std::unique_ptr<ParameterKnob> widthKnob;
    std::unique_ptr<ParameterKnob> densityKnob;
    std::unique_ptr<ParameterKnob> dryWetKnob;
    std::unique_ptr<ParameterKnob> chorusAmountKnob;
    std::unique_ptr<ParameterKnob> chorusRateKnob;
    std::unique_ptr<ParameterKnob> reflectKnob;

    reverbui::SSPToggle earlySpinToggle { "Spin" };
    reverbui::SSPToggle freezeToggle { "Freeze" };
    reverbui::SSPToggle flatCutToggle { "Flat / Cut" };
    juce::Label typeLabel;
    juce::Label typeHintLabel;
    reverbui::SSPDropdown typeDropdown;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> earlySpinAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> flatCutAttachment;
    bool ignoreTypeSelection = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbControlsComponent)
};
