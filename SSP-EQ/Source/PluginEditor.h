#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EqGraphComponent.h"
#include "EqControlsComponent.h"
#include "PresetBrowserComponent.h"

class ModAwareSSPButton final : public juce::TextButton
{
public:
    explicit ModAwareSSPButton(const juce::String& text = {})
        : juce::TextButton(text)
    {
        setLookAndFeel(&ssp::ui::getVectorLookAndFeel());
    }

    std::function<void(const juce::ModifierKeys&)> onClickWithMods;

private:
    void clicked(const juce::ModifierKeys& mods) override
    {
        if (onClickWithMods)
            onClickWithMods(mods);
    }
};

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;

    EqGraphComponent graph;
    EqControlsComponent controls;
    PresetBrowserComponent presetBrowser;
    juce::Label titleLabel;
    juce::Label phaseBadgeLabel;
    juce::Label hintLabel;
    ssp::ui::SSPButton compareAButton{"A"};
    ssp::ui::SSPButton compareCopyButton{"->"};
    ssp::ui::SSPButton compareBButton{"B"};
    ssp::ui::SSPButton previousPresetButton{"<"};
    ssp::ui::SSPButton presetButton{"Default Setting"};
    ssp::ui::SSPButton nextPresetButton{">"};
    ModAwareSSPButton randomizeButton{"RND"};
    ssp::ui::SSPButton settingsButton{"SET"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
