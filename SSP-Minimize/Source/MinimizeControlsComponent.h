#pragma once

#include <JuceHeader.h>

class MinimizeControlsComponent final : public juce::Component
{
public:
    explicit MinimizeControlsComponent(juce::AudioProcessorValueTreeState&);
    ~MinimizeControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class MinimizeKnob;

    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<MinimizeKnob> depthKnob;
    std::unique_ptr<MinimizeKnob> sensitivityKnob;
    std::unique_ptr<MinimizeKnob> sharpnessKnob;
    std::unique_ptr<MinimizeKnob> focusKnob;
    std::unique_ptr<MinimizeKnob> attackKnob;
    std::unique_ptr<MinimizeKnob> releaseKnob;
    std::unique_ptr<MinimizeKnob> mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MinimizeControlsComponent)
};
