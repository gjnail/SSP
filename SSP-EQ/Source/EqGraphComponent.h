#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EqGraphComponent final : public juce::Component,
                               private juce::Timer
{
public:
    explicit EqGraphComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setSelectedPoint(int index);
    int getSelectedPoint() const noexcept { return selectedPoint; }

    std::function<void(int)> onSelectionChanged;

private:
    void timerCallback() override;
    juce::Rectangle<int> getPlotBounds() const;
    juce::Point<float> pointToScreen(float frequency, float gainDb) const;
    PluginProcessor::EqPoint screenToPoint(juce::Point<float> position, const PluginProcessor::EqPoint& source) const;
    int hitTestPoint(juce::Point<float> position) const;
    void selectPoint(int index);

    PluginProcessor& processor;
    int selectedPoint = -1;
    int dragPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqGraphComponent)
};
