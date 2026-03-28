#pragma once

#include <JuceHeader.h>

class HeaderSectionComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    HeaderSectionComponent();
    ~HeaderSectionComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawTube(juce::Graphics&, juce::Rectangle<float>, float glow, int tubeIndex);

    juce::Label titleLabel;
    std::array<float, 4> tubePhases{};
    std::array<float, 4> tubeGlow{};
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderSectionComponent)
};
