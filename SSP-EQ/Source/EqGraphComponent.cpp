#include "EqGraphComponent.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGainDb = -24.0f;
constexpr float maxGainDb = 24.0f;
constexpr float pointRadius = 7.0f;

float frequencyToNormalised(float frequency)
{
    const auto clamped = juce::jlimit(minFrequency, maxFrequency, frequency);
    return (std::log10(clamped) - std::log10(minFrequency)) / (std::log10(maxFrequency) - std::log10(minFrequency));
}

float normalisedToFrequency(float normalised)
{
    const auto clamped = juce::jlimit(0.0f, 1.0f, normalised);
    return std::pow(10.0f, std::log10(minFrequency) + clamped * (std::log10(maxFrequency) - std::log10(minFrequency)));
}
}

EqGraphComponent::EqGraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
}

void EqGraphComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xff111822));
    g.fillRoundedRectangle(bounds, 14.0f);

    g.setColour(juce::Colour(0xff1b2734));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Interactive EQ", 16, 12, 220, 24, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff7f92a9));
    g.setFont(12.0f);
    g.drawText("Double-click to add a point. Right-click a point to remove it.",
               16, 36, getWidth() - 32, 18, juce::Justification::centredLeft, false);

    auto plot = getPlotBounds();
    g.setColour(juce::Colour(0xff171f2b));
    g.fillRoundedRectangle(plot.toFloat(), 12.0f);

    g.saveState();
    g.reduceClipRegion(plot);

    const auto points = processor.getPointsSnapshot();
    const auto spectrum = processor.getSpectrumDataCopy();
    const double sampleRate = juce::jmax(1.0, processor.getCurrentSampleRate());

    static constexpr float gridFrequencies[] = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
    for (float frequency : gridFrequencies)
    {
        const float x = plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
        g.setColour(frequency == 1000.0f ? juce::Colour(0xff365d87) : juce::Colour(0xff24384d));
        g.drawVerticalLine(juce::roundToInt(x), (float) plot.getY(), (float) plot.getBottom());
    }

    for (int i = 0; i < 5; ++i)
    {
        const float gain = juce::jmap((float) i, 0.0f, 4.0f, maxGainDb, minGainDb);
        const float y = juce::jmap(gain, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        g.setColour(std::abs(gain) < 0.001f ? juce::Colour(0xff3e6fa5) : juce::Colour(0xff24384d));
        g.drawHorizontalLine(juce::roundToInt(y), (float) plot.getX(), (float) plot.getRight());
    }

    juce::Path spectrumPath;
    bool startedSpectrum = false;
    for (int i = 1; i < (int) spectrum.size(); ++i)
    {
        const float frequency = (float) i * (float) sampleRate / (float) PluginProcessor::fftSize;
        if (frequency < minFrequency || frequency > maxFrequency)
            continue;

        const float x = plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
        const float y = juce::jmap(juce::jlimit(-96.0f, 0.0f, spectrum[(size_t) i]), -96.0f, 0.0f, (float) plot.getBottom(), (float) plot.getY());

        if (!startedSpectrum)
        {
            spectrumPath.startNewSubPath(x, y);
            startedSpectrum = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    if (startedSpectrum)
    {
        juce::Path spectrumFill(spectrumPath);
        spectrumFill.lineTo((float) plot.getRight(), (float) plot.getBottom());
        spectrumFill.lineTo((float) plot.getX(), (float) plot.getBottom());
        spectrumFill.closeSubPath();

        g.setColour(juce::Colour(0x1438d8ff));
        g.fillPath(spectrumFill);
        g.setColour(juce::Colour(0x7864cfff));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Path responsePath;
    bool started = false;
    for (int x = 0; x < plot.getWidth(); ++x)
    {
        const float frequency = normalisedToFrequency((float) x / (float) juce::jmax(1, plot.getWidth() - 1));
        const float gainDb = juce::jlimit(minGainDb, maxGainDb, processor.getResponseForFrequency(frequency));
        const float y = juce::jmap(gainDb, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        const float px = (float) plot.getX() + (float) x;
        if (!started)
        {
            responsePath.startNewSubPath(px, y);
            started = true;
        }
        else
        {
            responsePath.lineTo(px, y);
        }
    }

    juce::Path fillPath(responsePath);
    fillPath.lineTo((float) plot.getRight(), juce::jmap(0.0f, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom()));
    fillPath.lineTo((float) plot.getX(), juce::jmap(0.0f, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom()));
    fillPath.closeSubPath();

    g.setColour(juce::Colour(0x2034b9ff));
    g.fillPath(fillPath);
    g.setColour(juce::Colour(0xff63d0ff));
    g.strokePath(responsePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        const auto point = points[(size_t) i];
        if (!point.enabled)
            continue;

        const auto position = pointToScreen(point.frequency, point.gainDb);
        const bool isSelected = (i == selectedPoint);

        g.setColour(isSelected ? juce::Colour(0xff111822) : juce::Colour(0xff17202b));
        g.fillEllipse(position.x - pointRadius, position.y - pointRadius, pointRadius * 2.0f, pointRadius * 2.0f);

        g.setColour(isSelected ? juce::Colour(0xff9de7ff) : juce::Colour(0xff63d0ff));
        g.drawEllipse(position.x - pointRadius, position.y - pointRadius, pointRadius * 2.0f, pointRadius * 2.0f, isSelected ? 2.6f : 1.6f);
    }

    g.restoreState();

    g.setColour(juce::Colour(0xff92a6bb));
    g.setFont(11.5f);
    g.drawText("+24 dB", plot.getX(), plot.getY() - 18, 60, 16, juce::Justification::centredLeft, false);
    g.drawText("0 dB", plot.getX(), plot.getCentreY() - 8, 50, 16, juce::Justification::centredLeft, false);
    g.drawText("-24 dB", plot.getX(), plot.getBottom() + 4, 60, 16, juce::Justification::centredLeft, false);

    g.drawText("20 Hz", plot.getX() - 4, plot.getBottom() + 4, 60, 16, juce::Justification::centredLeft, false);
    g.drawText("1 kHz", plot.getCentreX() - 30, plot.getBottom() + 4, 60, 16, juce::Justification::centred, false);
    g.drawText("20 kHz", plot.getRight() - 60, plot.getBottom() + 4, 60, 16, juce::Justification::centredRight, false);
}

void EqGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
    {
        if (const int hit = hitTestPoint(event.position); hit >= 0)
        {
            processor.removePoint(hit);
            dragPoint = -1;
            if (hit == selectedPoint)
                selectPoint(-1);
            repaint();
        }

        return;
    }

    dragPoint = hitTestPoint(event.position);
    selectPoint(dragPoint);
}

void EqGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragPoint < 0)
        return;

    auto point = processor.getPoint(dragPoint);
    if (!point.enabled)
        return;

    auto movedPoint = screenToPoint(event.position, point);
    movedPoint.enabled = true;
    processor.setPointPosition(dragPoint, movedPoint.frequency, movedPoint.gainDb);
    repaint();
}

void EqGraphComponent::mouseUp(const juce::MouseEvent&)
{
    dragPoint = -1;
}

void EqGraphComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!getPlotBounds().contains(event.getPosition()))
        return;

    if (processor.getEnabledPointCount() >= PluginProcessor::maxPoints)
        return;

    auto point = screenToPoint(event.position, {});
    const int newIndex = processor.addPoint(point.frequency, point.gainDb);
    if (newIndex >= 0)
        selectPoint(newIndex);
}

void EqGraphComponent::setSelectedPoint(int index)
{
    if (selectedPoint == index)
        return;

    selectedPoint = index;
    repaint();
}

void EqGraphComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<int> EqGraphComponent::getPlotBounds() const
{
    auto bounds = getLocalBounds().reduced(16);
    bounds.removeFromTop(54);
    bounds.removeFromBottom(28);
    bounds.removeFromLeft(12);
    bounds.removeFromRight(12);
    return bounds;
}

juce::Point<float> EqGraphComponent::pointToScreen(float frequency, float gainDb) const
{
    const auto plot = getPlotBounds().toFloat();
    const float x = plot.getX() + frequencyToNormalised(frequency) * plot.getWidth();
    const float y = juce::jmap(juce::jlimit(minGainDb, maxGainDb, gainDb), maxGainDb, minGainDb, plot.getY(), plot.getBottom());
    return {x, y};
}

PluginProcessor::EqPoint EqGraphComponent::screenToPoint(juce::Point<float> position, const PluginProcessor::EqPoint& source) const
{
    auto point = source;
    const auto plot = getPlotBounds().toFloat();
    const float normalisedX = (position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth());
    const float normalisedY = (position.y - plot.getY()) / juce::jmax(1.0f, plot.getHeight());

    point.frequency = normalisedToFrequency(normalisedX);
    point.gainDb = juce::jmap(juce::jlimit(0.0f, 1.0f, normalisedY), 1.0f, 0.0f, minGainDb, maxGainDb);
    point.q = source.q <= 0.0f ? 1.0f : source.q;
    return point;
}

int EqGraphComponent::hitTestPoint(juce::Point<float> position) const
{
    const auto points = processor.getPointsSnapshot();
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        const auto point = points[(size_t) i];
        if (!point.enabled)
            continue;

        if (pointToScreen(point.frequency, point.gainDb).getDistanceFrom(position) <= pointRadius + 4.0f)
            return i;
    }

    return -1;
}

void EqGraphComponent::selectPoint(int index)
{
    if (selectedPoint == index)
        return;

    selectedPoint = index;
    if (onSelectionChanged)
        onSelectionChanged(index);
    repaint();
}
