#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VintageKnobComponent.h"
#include "VintageMeterComponent.h"

class VintageCompControlsComponent final : public juce::Component,
                                           private juce::Timer
{
public:
    VintageCompControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~VintageCompControlsComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateRatioButtons();
    void updateMeterModeButtons();
    void selectRatio(int index);
    void selectMeterMode(int index);
    void drawFaceplate(juce::Graphics&, juce::Rectangle<float>) const;
    void drawRackScrews(juce::Graphics&, juce::Rectangle<int>) const;
    void drawSectionCaption(juce::Graphics&, juce::Rectangle<int>, const juce::String&) const;

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    VintageMeterComponent meter;
    VintageKnobComponent inputKnob{"Input", juce::Colour(0xfff1ca6c), true};
    VintageKnobComponent attackKnob{"Attack", juce::Colour(0xffc8d5e6)};
    VintageKnobComponent releaseKnob{"Release", juce::Colour(0xffc8d5e6)};
    VintageKnobComponent mixKnob{"Mix", juce::Colour(0xfff1ca6c)};
    VintageKnobComponent outputKnob{"Output", juce::Colour(0xfff1ca6c), true};

    juce::TextButton ratio4Button{"4"};
    juce::TextButton ratio8Button{"8"};
    juce::TextButton ratio12Button{"12"};
    juce::TextButton ratio20Button{"20"};
    juce::TextButton ratioAllButton{"ALL"};
    juce::TextButton meterGrButton{"GR"};
    juce::TextButton meter8Button{"+8"};
    juce::TextButton meter4Button{"+4"};
    juce::TextButton meterOffButton{"OFF"};

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageCompControlsComponent)
};
