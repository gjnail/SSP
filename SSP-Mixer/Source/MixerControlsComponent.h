#pragma once

#include <JuceHeader.h>
#include <array>
#include "MixerVectorUI.h"
#include "PluginProcessor.h"

class MixerControlsComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    MixerControlsComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~MixerControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ChannelKnob;
    class SvgToggleButton;
    class RouterDisplay;
    class MiniActionButton;

    void timerCallback() override;

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::unique_ptr<ChannelKnob> leftFromLeftKnob;
    std::unique_ptr<ChannelKnob> leftFromRightKnob;
    std::unique_ptr<ChannelKnob> rightFromLeftKnob;
    std::unique_ptr<ChannelKnob> rightFromRightKnob;
    std::unique_ptr<ChannelKnob> outputTrimKnob;
    std::unique_ptr<RouterDisplay> routerDisplay;

    std::unique_ptr<SvgToggleButton> swapToggle;
    std::unique_ptr<SvgToggleButton> monoToggle;
    std::unique_ptr<SvgToggleButton> invertLeftToggle;
    std::unique_ptr<SvgToggleButton> invertRightToggle;
    std::unique_ptr<SvgToggleButton> autoCompToggle;

    std::unique_ptr<MiniActionButton> resetButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> swapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> invertLeftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> invertRightAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoCompAttachment;

    juce::Rectangle<int> knobsPanelBounds;
    juce::Rectangle<int> visualizerPanelBounds;
    std::array<float, 4> displayedRoutes{{1.0f, 0.0f, 0.0f, 1.0f}};
    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerControlsComponent)
};
