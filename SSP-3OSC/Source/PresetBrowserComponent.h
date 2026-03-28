#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PresetBrowserComponent final : public juce::Component
{
public:
    explicit PresetBrowserComponent(PluginProcessor&);
    ~PresetBrowserComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onRandomizeAll;

private:
    void rebuildPresetList();
    void handlePresetSelection();
    void promptSavePreset();
    void navigatePreset(int direction);
    void updateNavigationButtons();

    PluginProcessor& processor;
    bool suppressPresetChange = false;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ComboBox presetBox;
    juce::TextButton previousPresetButton;
    juce::TextButton nextPresetButton;
    juce::TextButton randomizeAllButton;
    juce::TextButton initButton;
    juce::TextButton saveButton;
    juce::TextButton refreshButton;
    juce::Array<int> navigablePresetIds;
    std::unique_ptr<juce::FileChooser> saveChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserComponent)
};
