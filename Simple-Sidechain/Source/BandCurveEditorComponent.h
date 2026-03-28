#pragma once

#include <JuceHeader.h>
#include "CurveInterp.h"
#include "PluginProcessor.h"

class BandCurveEditorComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit BandCurveEditorComponent(PluginProcessor&);
    ~BandCurveEditorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void updatePresetStyles();
    void pushCurveToProcessor();
    void loadCurveFromProcessor();
    void applyPreset(int presetIndex);

    [[nodiscard]] juce::Rectangle<float> getPlotBounds() const;
    [[nodiscard]] juce::Point<float> normToPlot(float nx, float ny) const;
    [[nodiscard]] juce::Point<float> plotToNorm(juce::Point<int> pix) const;
    [[nodiscard]] int hitTestAnchor(juce::Point<int> pix) const;
    [[nodiscard]] int hitTestSegment(juce::Point<int> pix) const;
    [[nodiscard]] juce::Point<float> getSegmentHandlePosition(int segmentIndex) const;
    [[nodiscard]] int findMatchingPreset() const;

    PluginProcessor& processor;

    juce::TextButton presetDefault{"Default"};
    juce::TextButton presetPunch{"Punch"};
    juce::TextButton presetHold{"Hold"};
    juce::TextButton presetBreathe{"Breathe"};

    int selectedPreset = -1;
    std::vector<CurvePoint> editPoints;

    int dragIndex = -1;
    int dragSegmentIndex = -1;
    int hoverIndex = -1;
    int hoverSegmentIndex = -1;
    float dragSegmentStartCurve = 0.0f;
    juce::Point<int> dragStartPos;

    static constexpr int maxAnchors = 48;
    static constexpr float minGapX = 0.02f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandCurveEditorComponent)
};
