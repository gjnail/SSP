#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class ImagerGraphComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit ImagerGraphComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> getPlotBounds() const;
    float frequencyToX(float freq) const;
    float xToFrequency(float x) const;
    int findHandleAt(juce::Point<float> pos) const;

    void drawGrid(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandFills(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawCorrelationMeters(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawWidthIndicators(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawCrossoverHandles(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandLabels(juce::Graphics&, const juce::Rectangle<int>& plot) const;

    PluginProcessor& processor;
    int dragHandle = -1;
    int hoverHandle = -1;

    // Band colours
    static constexpr juce::uint32 bandColourValues[PluginProcessor::numBands] = {
        0xff4da6ff, // blue (low)
        0xff3ecfff, // cyan (low mid)
        0xffffcf4d, // gold (high mid)
        0xffff8f4d  // orange (high)
    };

    juce::Colour getBandColour(int band) const
    {
        return juce::Colour(bandColourValues[juce::jlimit(0, PluginProcessor::numBands - 1, band)]);
    }

    // Cached meter values for smooth drawing
    std::array<float, PluginProcessor::numBands> displayCorrelation{};
    std::array<float, PluginProcessor::numBands> displayWidth{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImagerGraphComponent)
};
