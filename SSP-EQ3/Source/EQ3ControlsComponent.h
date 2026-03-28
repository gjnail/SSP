#pragma once

#include <memory>
#include <JuceHeader.h>
#include "EQ3VectorUI.h"

class EQ3ControlsComponent final : public juce::Component
{
public:
    explicit EQ3ControlsComponent(juce::AudioProcessorValueTreeState&);
    ~EQ3ControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class BandKnob;

    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<BandKnob> lowKnob;
    std::unique_ptr<BandKnob> midKnob;
    std::unique_ptr<BandKnob> highKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ3ControlsComponent)
};
