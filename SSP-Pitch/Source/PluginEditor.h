#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class PitchRootComponent;

class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           public juce::FileDragAndDropTarget
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;

    PluginProcessor& pluginProcessor;
    std::unique_ptr<PitchRootComponent> root;
    std::uint32_t lastRevision = 0;
    double lastTimerSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
