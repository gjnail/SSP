#pragma once

#include <JuceHeader.h>
#include "ModSectionComponent.h"
#include "PluginProcessor.h"

class EffectsRackComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit EffectsRackComponent(PluginProcessor&);
    ~EffectsRackComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshRack();
    void refreshLfoSelector();
    void layoutOrderRows();
    void rebuildAddMenu();
    void styleActionButton(juce::Button& button, juce::Colour accent) const;

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label rackLabel;
    juce::Viewport rackViewport;
    juce::Component rackContent;
    juce::ComboBox addModuleBox;
    juce::TextButton addButton;
    juce::Label lfoLabel;
    juce::ComboBox lfoSelector;
    std::unique_ptr<juce::Component> lfoDragBadge;
    ModSectionComponent modSection;
    juce::OwnedArray<juce::Component> rackItems;
    juce::OwnedArray<juce::Component> orderItems;
    juce::Rectangle<int> rackPanelBounds;
    juce::Rectangle<int> orderPanelBounds;
    juce::Rectangle<int> orderRowsBounds;
    juce::Rectangle<int> lfoPanelBounds;
    juce::Rectangle<int> modPanelBounds;
    juce::Array<int> cachedOrder;
    juce::StringArray cachedLfoNames;
    bool orderDragInProgress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsRackComponent)
};
