#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

class ImagerGraphComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    enum class VisualiserMode
    {
        goniometer = 0,
        polarSample,
        stereoTrail
    };

    explicit ImagerGraphComponent(PluginProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void setVisualiserMode(VisualiserMode newMode);
    VisualiserMode getVisualiserMode() const noexcept { return visualiserMode; }

private:
    void timerCallback() override;
    juce::Rectangle<int> getScopeBounds() const;
    juce::Rectangle<int> getAnalysisBounds() const;
    juce::Rectangle<int> getPlotBounds() const;
    juce::Rectangle<int> getStatsBounds() const;
    float frequencyToX(float freq) const;
    float xToFrequency(float x) const;
    int findHandleAt(juce::Point<float> pos) const;

    void drawSectionLabel(juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, const juce::String& subtitle) const;
    void drawScope(juce::Graphics&, const juce::Rectangle<int>& scopeBounds) const;
    void drawCorrelationMeter(juce::Graphics&, juce::Rectangle<float> bounds) const;
    void drawGoniometerScope(juce::Graphics&, juce::Rectangle<float> scopeArea) const;
    void drawPolarScope(juce::Graphics&, juce::Rectangle<float> scopeArea) const;
    void drawStereoTrailScope(juce::Graphics&, juce::Rectangle<float> scopeArea) const;
    void drawGrid(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandFills(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandMeters(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawCrossoverHandles(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandLabels(juce::Graphics&, const juce::Rectangle<int>& plot) const;
    void drawBandStats(juce::Graphics&, const juce::Rectangle<int>& statsBounds) const;

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
    std::array<float, PluginProcessor::numBands> displayLevelL{};
    std::array<float, PluginProcessor::numBands> displayLevelR{};
    std::array<PluginProcessor::VectorscopePoint, PluginProcessor::vectorscopeSize> scopeSnapshot{};
    int scopeWriteIndex = 0;
    float overallCorrelation = 1.0f;
    VisualiserMode visualiserMode = VisualiserMode::goniometer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImagerGraphComponent)
};
