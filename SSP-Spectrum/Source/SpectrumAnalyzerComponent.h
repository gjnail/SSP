#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectrumAnalyzerComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    explicit SpectrumAnalyzerComponent(PluginProcessor&);
    ~SpectrumAnalyzerComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    void handleReferenceA(const juce::ModifierKeys&);
    void handleReferenceB(const juce::ModifierKeys&);
    void clearReferences();
    bool isReferenceAVisible() const noexcept { return referenceA.captured && referenceA.visible; }
    bool isReferenceBVisible() const noexcept { return referenceB.captured && referenceB.visible; }
    bool hasReferenceA() const noexcept { return referenceA.captured; }
    bool hasReferenceB() const noexcept { return referenceB.captured; }

private:
    struct ReferenceTrace
    {
        bool captured = false;
        bool visible = false;
        juce::String label;
        std::array<float, PluginProcessor::displayPointCount> values{};
    };

    void recallOrCaptureReference(ReferenceTrace& target, ReferenceTrace& other, const juce::ModifierKeys& mods);
    void timerCallback() override;
    void ensureWaterfallImage(int width, int height);
    void pushWaterfallRow();
    static juce::Path makeTracePath(const std::array<float, PluginProcessor::displayPointCount>& values,
                                    juce::Rectangle<float> plot, float rangeDb);
    static float mapDbToY(float db, juce::Rectangle<float> plot, float rangeDb);
    static juce::String formatFrequency(float frequency);
    static juce::String formatNote(float frequency);
    void drawSidebar(juce::Graphics&, juce::Rectangle<float> area, const juce::Colour& accent);
    void drawWaveformMeter(juce::Graphics&, juce::Rectangle<float> area);
    void drawVectorscopeMeter(juce::Graphics&, juce::Rectangle<float> area);
    void drawHoverReadout(juce::Graphics&, juce::Rectangle<float> plot);

    PluginProcessor& processor;
    PluginProcessor::AnalyzerSnapshot snapshot;

    juce::Image waterfallImage;
    bool waterfallFramePending = false;

    ReferenceTrace referenceA{ false, false, "A", {} };
    ReferenceTrace referenceB{ false, false, "B", {} };

    juce::Rectangle<float> lastPlotArea;
    int hoverIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzerComponent)
};
