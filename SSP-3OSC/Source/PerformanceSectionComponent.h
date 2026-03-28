#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PerformanceSectionComponent final : public juce::Component
{
public:
    PerformanceSectionComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~PerformanceSectionComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class DragValueBox;

    juce::Label titleLabel;
    juce::Label bendTitleLabel;
    juce::Label bendUpLabel;
    juce::Label bendDownLabel;
    std::unique_ptr<DragValueBox> bendUpBox;
    std::unique_ptr<DragValueBox> bendDownBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceSectionComponent)
};
