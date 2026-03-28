#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"
#include "PluginProcessor.h"

class WavetableOscillatorComponent final : public juce::Component,
                                           private juce::Timer
{
public:
    WavetableOscillatorComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int oscillatorIndex);
    ~WavetableOscillatorComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Rectangle<int> getPreviewBounds() const;

    int oscIndex = 1;
    juce::Label tableLabel;
    juce::Label positionLabel;
    juce::Label octaveLabel;
    juce::ComboBox tableBox;
    ModulationKnob positionSlider;
    ModulationKnob octaveSlider;
    std::atomic<float>* tableParam = nullptr;
    std::atomic<float>* positionParam = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> positionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableOscillatorComponent)
};
