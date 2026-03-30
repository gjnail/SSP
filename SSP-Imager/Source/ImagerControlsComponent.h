#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class ImagerControlsComponent final : public juce::Component
{
public:
    explicit ImagerControlsComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;

    struct BandControls
    {
        ssp::ui::SSPKnob widthKnob;
        ssp::ui::SSPKnob panKnob;
        ssp::ui::SSPButton soloButton{"S"};
        ssp::ui::SSPButton muteButton{"M"};
        juce::Label nameLabel;
        juce::Label rangeLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    };

    std::array<BandControls, PluginProcessor::numBands> bands;

    static constexpr juce::uint32 bandColourValues[PluginProcessor::numBands] = {
        0xff4da6ff,
        0xff3ecfff,
        0xffffcf4d,
        0xffff8f4d
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImagerControlsComponent)
};
