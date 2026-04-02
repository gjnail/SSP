#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ReactorUI.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void setEditorScale(float newScale);
    void updateContentScale();

    PluginProcessor& pluginProcessor;
    reactorui::LookAndFeel lookAndFeel;
    std::unique_ptr<juce::Component> content;
    float editorScale = 0.85f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
