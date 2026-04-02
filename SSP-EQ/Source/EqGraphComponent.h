#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SSPVectorUI.h"

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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void resized() override;

    void setSelectedPoint(int index);
    int getSelectedPoint() const noexcept { return selectedPoint; }
    const juce::Array<int>& getSelectedPoints() const noexcept { return selectedPoints; }

    std::function<void(int)> onSelectionChanged;

private:
    void timerCallback() override;
    juce::Rectangle<int> getPlotBounds() const;
    juce::Point<float> pointToScreen(int pointIndex, const PluginProcessor::EqPoint& point) const;
    juce::Point<float> pointToDisplayScreen(int pointIndex, const PluginProcessor::EqPoint& point) const;
    juce::Point<float> pointToScreenForSnapshot(int pointIndex, const PluginProcessor::EqPoint& point,
                                                const PluginProcessor::PointArray& sourcePoints) const;
    PluginProcessor::EqPoint screenToPoint(juce::Point<float> position, const PluginProcessor::EqPoint& source) const;
    int hitTestPoint(juce::Point<float> position) const;
    int hitTestSoloButton(juce::Point<float> position) const;
    juce::Rectangle<float> getSoloButtonBounds(int pointIndex) const;
    bool isPointSelected(int index) const;
    void clearSelection();
    void addPointToSelection(int index, bool makePrimary);
    void updateMarqueeSelection(bool additive);
    void selectPoint(int index);

    PluginProcessor& processor;
    PluginProcessor::PointArray transitionFromPoints{};
    PluginProcessor::PointArray transitionToPoints{};
    juce::uint32 transitionStartMs = 0;
    int transitionDurationMs = 90;
    bool transitionActive = false;
    int selectedPoint = -1;
    int dragPoint = -1;
    int hoverPoint = -1;
    juce::Array<int> selectedPoints;
    PluginProcessor::PointArray dragStartPoints{};
    juce::Point<float> dragStartPosition;
    bool marqueeSelecting = false;
    bool marqueeAdditive = false;
    juce::Rectangle<float> marqueeBounds;
    bool showNoteAxis = false;
    ssp::ui::SSPButton notesButton{"Notes"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqGraphComponent)
};
