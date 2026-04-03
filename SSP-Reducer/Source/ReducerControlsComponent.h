#pragma once

#include <JuceHeader.h>
#include "ReducerDSP.h"
#include "ReducerVectorUI.h"

class ReducerControlsComponent final : public juce::Component
{
public:
    explicit ReducerControlsComponent(juce::AudioProcessorValueTreeState&);
    ~ReducerControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ReducerKnob;
    class ReducerToggle;

    std::unique_ptr<ReducerKnob> mixKnob;
    std::unique_ptr<ReducerKnob> bitsKnob;
    std::unique_ptr<ReducerKnob> rateKnob;
    std::unique_ptr<ReducerKnob> shapeKnob;
    std::unique_ptr<ReducerKnob> jitterKnob;
    std::unique_ptr<ReducerKnob> filterKnob;
    std::unique_ptr<ReducerToggle> preToggle;
    std::unique_ptr<ReducerToggle> postToggle;
    std::unique_ptr<ReducerToggle> dcShiftToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReducerControlsComponent)
};
