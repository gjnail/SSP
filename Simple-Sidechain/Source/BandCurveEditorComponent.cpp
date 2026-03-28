#include "BandCurveEditorComponent.h"
#include <cmath>

namespace
{
constexpr int titleBarH = 24;
constexpr int presetRowH = 32;
constexpr float presetTolerance = 0.025f;

juce::Colour accentColour() { return juce::Colour(0xffffd83d); }

const std::array<std::vector<CurvePoint>, 4> presetCurves{{
    {{0.0f, 1.0f, 0.0f}, {0.012f, 0.0f, 0.78f}, {0.21f, 0.0f, 0.56f}, {0.27f, 0.08f, 0.22f}, {0.33f, 0.96f, -0.18f}, {0.38f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.010f, 0.0f, 0.88f}, {0.12f, 0.0f, 0.78f}, {0.19f, 0.14f, 0.24f}, {0.24f, 0.95f, -0.20f}, {0.28f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.012f, 0.0f, 0.58f}, {0.17f, 0.0f, 0.42f}, {0.28f, 0.58f, -0.10f}, {0.36f, 0.96f, -0.18f}, {0.42f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.014f, 0.0f, 0.24f}, {0.16f, 0.0f, 0.18f}, {0.32f, 0.26f, -0.28f}, {0.50f, 0.82f, -0.12f}, {0.66f, 0.96f, -0.10f}, {0.72f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
}};

bool curvesCloseEnough(const std::vector<CurvePoint>& a, const std::vector<CurvePoint>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
    {
        if (std::abs(a[i].x - b[i].x) > presetTolerance || std::abs(a[i].y - b[i].y) > presetTolerance
            || std::abs(a[i].curve - b[i].curve) > 0.08f)
            return false;
    }

    return true;
}

juce::String triggerRateLabel(juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("triggerRate")))
        return param->choices[param->getIndex()];

    return "1/4";
}
} // namespace

BandCurveEditorComponent::BandCurveEditorComponent(PluginProcessor& p)
    : processor(p)
{
    addAndMakeVisible(presetDefault);
    addAndMakeVisible(presetPunch);
    addAndMakeVisible(presetHold);
    addAndMakeVisible(presetBreathe);

    presetDefault.onClick = [this] { applyPreset(0); };
    presetPunch.onClick = [this] { applyPreset(1); };
    presetHold.onClick = [this] { applyPreset(2); };
    presetBreathe.onClick = [this] { applyPreset(3); };

    loadCurveFromProcessor();
    updatePresetStyles();
    startTimerHz(30);
}

BandCurveEditorComponent::~BandCurveEditorComponent()
{
    stopTimer();
    pushCurveToProcessor();
}

void BandCurveEditorComponent::timerCallback()
{
    updatePresetStyles();
    repaint();
}

void BandCurveEditorComponent::updatePresetStyles()
{
    selectedPreset = findMatchingPreset();

    auto style = [this](juce::TextButton& button, int index)
    {
        const bool active = selectedPreset == index;
        button.setColour(juce::TextButton::buttonColourId, active ? juce::Colour(0xff6e5c14) : juce::Colour(0xff1d232b));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8c7418));
        button.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : juce::Colour(0xffc7d0d8));
    };

    style(presetDefault, 0);
    style(presetPunch, 1);
    style(presetHold, 2);
    style(presetBreathe, 3);
}

void BandCurveEditorComponent::pushCurveToProcessor()
{
    if (editPoints.size() >= 2)
        processor.setBandCurve(0, editPoints);
}

void BandCurveEditorComponent::loadCurveFromProcessor()
{
    processor.getBandCurve(0, editPoints);
    if (editPoints.size() < 2)
        editPoints = presetCurves[0];

    CurveInterp::sortAndClamp(editPoints);
}

void BandCurveEditorComponent::applyPreset(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= (int) presetCurves.size())
        return;

    editPoints = presetCurves[(size_t) presetIndex];
    CurveInterp::sortAndClamp(editPoints);
    pushCurveToProcessor();
    loadCurveFromProcessor();
    updatePresetStyles();
    repaint();
}

juce::Rectangle<float> BandCurveEditorComponent::getPlotBounds() const
{
    auto r = getLocalBounds().toFloat().reduced(12.0f, 8.0f);
    r.removeFromTop((float) titleBarH + (float) presetRowH + 2.0f);
    r.removeFromLeft(20.0f);
    r.removeFromRight(8.0f);
    r.removeFromBottom(58.0f);
    return r;
}

juce::Point<float> BandCurveEditorComponent::normToPlot(float nx, float ny) const
{
    const auto plot = getPlotBounds();
    return {plot.getX() + nx * plot.getWidth(), plot.getBottom() - ny * plot.getHeight()};
}

juce::Point<float> BandCurveEditorComponent::plotToNorm(juce::Point<int> pix) const
{
    const auto plot = getPlotBounds();
    const float nx = juce::jlimit(0.0f, 1.0f, (float) (pix.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
    const float ny = juce::jlimit(0.0f, 1.0f, (plot.getBottom() - (float) pix.y) / juce::jmax(1.0f, plot.getHeight()));
    return {nx, ny};
}

int BandCurveEditorComponent::hitTestAnchor(juce::Point<int> pix) const
{
    const float hitR = 9.0f;
    for (int i = 0; i < (int) editPoints.size(); ++i)
    {
        const auto pt = normToPlot(editPoints[(size_t) i].x, editPoints[(size_t) i].y);
        if (juce::Point<float>(pt).getDistanceFrom(pix.toFloat()) <= hitR)
            return i;
    }
    return -1;
}

juce::Point<float> BandCurveEditorComponent::getSegmentHandlePosition(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= (int) editPoints.size() - 1)
        return {};

    const auto& left = editPoints[(size_t) segmentIndex];
    const auto& right = editPoints[(size_t) segmentIndex + 1];
    const float x = juce::jmap(0.5f, left.x, right.x);
    const float y = CurveInterp::eval(x, editPoints);
    return normToPlot(x, y);
}

int BandCurveEditorComponent::hitTestSegment(juce::Point<int> pix) const
{
    constexpr float hitDistance = 12.0f;
    int bestIndex = -1;
    float bestDistance = hitDistance;

    for (int i = 0; i < (int) editPoints.size() - 1; ++i)
    {
        const auto handle = getSegmentHandlePosition(i);
        const float distance = handle.getDistanceFrom(pix.toFloat());
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

int BandCurveEditorComponent::findMatchingPreset() const
{
    for (int i = 0; i < (int) presetCurves.size(); ++i)
        if (curvesCloseEnough(editPoints, presetCurves[(size_t) i]))
            return i;

    return -1;
}

void BandCurveEditorComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        CurveInterp::sortAndClamp(editPoints);
        const int hit = hitTestAnchor(e.getPosition());
        if (hit >= 0 && (int) editPoints.size() > 2)
        {
            editPoints.erase(editPoints.begin() + hit);
            CurveInterp::sortAndClamp(editPoints);
            pushCurveToProcessor();
            loadCurveFromProcessor();
            updatePresetStyles();
            repaint();
        }
        return;
    }

    CurveInterp::sortAndClamp(editPoints);
    pushCurveToProcessor();
    loadCurveFromProcessor();
    dragIndex = hitTestAnchor(e.getPosition());
    dragSegmentIndex = -1;
    dragStartPos = e.getPosition();

    if (dragIndex < 0)
    {
        dragSegmentIndex = hitTestSegment(e.getPosition());
        if (dragSegmentIndex >= 0)
            dragSegmentStartCurve = editPoints[(size_t) dragSegmentIndex].curve;
    }
}

void BandCurveEditorComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragIndex >= 0 && dragIndex < (int) editPoints.size())
    {
        const auto nm = plotToNorm(e.getPosition());
        float nx = nm.x;
        float ny = nm.y;

        if (dragIndex > 0)
            nx = juce::jmax(editPoints[(size_t) dragIndex - 1].x + minGapX, nx);
        if (dragIndex < (int) editPoints.size() - 1)
            nx = juce::jmin(editPoints[(size_t) dragIndex + 1].x - minGapX, nx);

        editPoints[(size_t) dragIndex].x = nx;
        editPoints[(size_t) dragIndex].y = ny;

        CurveInterp::sortAndClamp(editPoints);
        pushCurveToProcessor();
        loadCurveFromProcessor();
        updatePresetStyles();
        repaint();
        return;
    }

    if (dragSegmentIndex >= 0 && dragSegmentIndex < (int) editPoints.size() - 1)
    {
        const float delta = (float) (dragStartPos.y - e.y) / 110.0f;
        editPoints[(size_t) dragSegmentIndex].curve = juce::jlimit(-1.0f, 1.0f, dragSegmentStartCurve + delta);
        CurveInterp::sortAndClamp(editPoints);
        pushCurveToProcessor();
        loadCurveFromProcessor();
        updatePresetStyles();
        repaint();
    }
}

void BandCurveEditorComponent::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    CurveInterp::sortAndClamp(editPoints);
    pushCurveToProcessor();
    loadCurveFromProcessor();
    dragIndex = -1;
    dragSegmentIndex = -1;
    updatePresetStyles();
    repaint();
}

void BandCurveEditorComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
        return;

    const auto plot = getPlotBounds();
    if (!plot.contains(e.getPosition().toFloat()))
        return;

    if ((int) editPoints.size() >= maxAnchors)
        return;

    const auto nm = plotToNorm(e.getPosition());
    editPoints.push_back({nm.x, nm.y, 0.0f});
    CurveInterp::sortAndClamp(editPoints);
    pushCurveToProcessor();
    loadCurveFromProcessor();
    updatePresetStyles();
    repaint();
}

void BandCurveEditorComponent::mouseMove(const juce::MouseEvent& e)
{
    hoverIndex = hitTestAnchor(e.getPosition());
    hoverSegmentIndex = hoverIndex < 0 ? hitTestSegment(e.getPosition()) : -1;
    setMouseCursor((hoverIndex >= 0 || hoverSegmentIndex >= 0) ? juce::MouseCursor::DraggingHandCursor
                                                               : juce::MouseCursor::NormalCursor);
    repaint();
}

void BandCurveEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12151a));
    g.setColour(juce::Colour(0xff2a3038));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Volume shape", 12, 8, 200, titleBarH, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff7f8b98));
    g.setFont(11.0f);
    g.drawText("Single-band graph", getWidth() - 180, 8, 168, titleBarH, juce::Justification::centredRight);

    const auto plot = getPlotBounds();
    const float triggerPhase = processor.getBandSidechainLevel(0);
    const auto livePoint = normToPlot(triggerPhase, CurveInterp::eval(triggerPhase, editPoints));
    const auto rateText = triggerRateLabel(processor.apvts);

    g.setColour(juce::Colour(0xff1e242c));
    g.fillRoundedRectangle(plot, 4.0f);

    g.setColour(juce::Colour(0xff2a3544));
    for (int i = 1; i < 4; ++i)
    {
        const float ty = plot.getY() + plot.getHeight() * 0.25f * (float) i;
        g.drawHorizontalLine(juce::roundToInt(ty), plot.getX(), plot.getRight());
    }
    for (int i = 1; i < 6; ++i)
    {
        const float tx = plot.getX() + plot.getWidth() * (float) i / 6.0f;
        g.drawVerticalLine(juce::roundToInt(tx), plot.getY(), plot.getBottom());
    }

    juce::Path curvePath;
    juce::Path fillPath;
    constexpr int steps = 200;
    for (int s = 0; s <= steps; ++s)
    {
        const float x = (float) s / (float) steps;
        const float y = CurveInterp::eval(x, editPoints);
        const auto pt = normToPlot(x, y);
        if (s == 0)
        {
            curvePath.startNewSubPath(pt);
            fillPath.startNewSubPath(normToPlot(0.0f, 0.0f));
            fillPath.lineTo(pt);
        }
        else
        {
            curvePath.lineTo(pt);
            fillPath.lineTo(pt);
        }
    }
    fillPath.lineTo(normToPlot(1.0f, 0.0f));
    fillPath.closeSubPath();

    g.setColour(accentColour().withAlpha(0.20f));
    g.fillPath(fillPath);

    g.setColour(accentColour());
    g.strokePath(curvePath, juce::PathStrokeType(2.2f));

    g.setColour(juce::Colour(0x55ffffff));
    g.drawVerticalLine(juce::roundToInt(livePoint.x), plot.getY(), plot.getBottom());

    for (int i = 0; i < (int) editPoints.size() - 1; ++i)
    {
        const auto handle = getSegmentHandlePosition(i);
        const bool active = (i == hoverSegmentIndex || i == dragSegmentIndex);
        g.setColour(active ? accentColour().brighter(0.15f) : juce::Colour(0xff748290));
        g.fillEllipse(handle.x - 4.0f, handle.y - 4.0f, 8.0f, 8.0f);
    }

    for (int i = 0; i < (int) editPoints.size(); ++i)
    {
        const auto pt = normToPlot(editPoints[(size_t) i].x, editPoints[(size_t) i].y);
        g.setColour(juce::Colour(0xff1a2027));
        g.fillEllipse(pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);
        const bool hi = (i == hoverIndex || i == dragIndex);
        g.setColour(hi ? juce::Colours::white : accentColour());
        g.drawEllipse(pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, 2.0f);
    }

    g.setColour(accentColour());
    g.fillEllipse(livePoint.x - 5.0f, livePoint.y - 5.0f, 10.0f, 10.0f);

    g.setColour(juce::Colour(0xff8899aa));
    g.setFont(10.0f);
    g.drawText("0", juce::Rectangle<float>(plot.getX(), plot.getBottom() + 2.0f, 24.0f, 16.0f), juce::Justification::centredLeft);
    g.drawText(rateText, juce::Rectangle<float>(plot.getRight() - 48.0f, plot.getBottom() + 2.0f, 48.0f, 16.0f), juce::Justification::centredRight);
    g.drawText("Time", juce::Rectangle<float>(plot.getCentreX() - 30.0f, plot.getBottom() + 2.0f, 60.0f, 16.0f), juce::Justification::centred);
    g.drawText("100% volume", juce::Rectangle<float>(10.0f, plot.getY() - 2.0f, plot.getX() - 10.0f, 14.0f), juce::Justification::topLeft);
    g.drawText("0% volume", juce::Rectangle<float>(10.0f, plot.getBottom() - 14.0f, plot.getX() - 10.0f, 14.0f), juce::Justification::bottomLeft);

    g.setColour(juce::Colour(0xff667788));
    g.setFont(11.0f);
    g.drawText("Drag anchors to shape volume over time. Drag the small dots to bend the curve.",
               juce::Rectangle<float>(plot.getX(), plot.getBottom() + 18.0f, plot.getWidth(), 16.0f),
               juce::Justification::centred);
}

void BandCurveEditorComponent::resized()
{
    auto r = getLocalBounds().reduced(10, 8);
    r.removeFromTop(titleBarH);

    auto presetRow = r.removeFromTop(presetRowH);
    const int buttonWidth = presetRow.getWidth() / 4;
    presetDefault.setBounds(presetRow.removeFromLeft(buttonWidth).reduced(4, 3));
    presetPunch.setBounds(presetRow.removeFromLeft(buttonWidth).reduced(4, 3));
    presetHold.setBounds(presetRow.removeFromLeft(buttonWidth).reduced(4, 3));
    presetBreathe.setBounds(presetRow.removeFromLeft(buttonWidth).reduced(4, 3));
}
