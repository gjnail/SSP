#include "EqGraphComponent.h"
#include "MusicNoteUtils.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGainDb = -24.0f;
constexpr float maxGainDb = 24.0f;
constexpr float pointRadius = 7.0f;
constexpr float selectedPointRadius = 9.0f;

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

juce::String formatFrequencyLabel(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";
    return juce::String((int) frequency);
}

bool pointTypeUsesGain(int type)
{
    return type == PluginProcessor::bell
        || type == PluginProcessor::lowShelf
        || type == PluginProcessor::highShelf
        || type == PluginProcessor::tiltShelf;
}

void drawResponseDeltaFill(juce::Graphics& g,
                           const juce::Rectangle<int>& plot,
                           const std::vector<float>& responseValues,
                           juce::Colour colour)
{
    if (responseValues.empty())
        return;

    const float zeroY = juce::jmap(0.0f, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
    g.setColour(colour);

    for (int x = 0; x < (int) responseValues.size(); ++x)
    {
        const float y = juce::jmap(juce::jlimit(minGainDb, maxGainDb, responseValues[(size_t) x]),
                                   maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        g.drawVerticalLine(plot.getX() + x, juce::jmin(y, zeroY), juce::jmax(y, zeroY));
    }
}
} // namespace

EqGraphComponent::EqGraphComponent(PluginProcessor& p)
    : processor(p)
{
    notesButton.setClickingTogglesState(true);
    notesButton.onClick = [this]
    {
        showNoteAxis = notesButton.getToggleState();
        repaint();
    };
    addAndMakeVisible(notesButton);
    startTimerHz(60);
}

void EqGraphComponent::resized()
{
    notesButton.setBounds(getWidth() - 86, 16, 62, 24);
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
    g.drawText("Double-click to add a point, drag to move it, scroll to adjust Q, right-click to remove. Up to 24 bands.",
               16, 36, getWidth() - 120, 18, juce::Justification::centredLeft, false);

    auto plot = getPlotBounds();
    g.setColour(juce::Colour(0xff171f2b));
    g.fillRoundedRectangle(plot.toFloat(), 12.0f);

    const auto points = processor.getPointsSnapshot();
    const auto analyzerFrame = processor.getAnalyzerFrameCopy();
    const int analyzerMode = processor.getAnalyzerMode();
    const double sampleRate = juce::jmax(1.0, processor.getCurrentSampleRate());

    g.saveState();
    g.reduceClipRegion(plot);

    static constexpr float majorGridFrequencies[] = {20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f};
    static constexpr float minorGridFrequencies[] = {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
    static constexpr float gainLines[] = {-24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f, 24.0f};

    for (float frequency : minorGridFrequencies)
    {
        const float x = plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
        g.setColour(juce::Colour(0xff24384d));
        g.drawVerticalLine(juce::roundToInt(x), (float) plot.getY(), (float) plot.getBottom());
    }

    for (float frequency : majorGridFrequencies)
    {
        const float x = plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
        g.setColour(frequency == 1000.0f ? juce::Colour(0xff365d87) : juce::Colour(0xff314f6b));
        g.drawVerticalLine(juce::roundToInt(x), (float) plot.getY(), (float) plot.getBottom());
    }

    for (float gain : gainLines)
    {
        const float y = juce::jmap(gain, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        g.setColour(std::abs(gain) < 0.001f ? juce::Colour(0xff3e6fa5) : juce::Colour(0xff24384d));
        g.drawHorizontalLine(juce::roundToInt(y), (float) plot.getX(), (float) plot.getRight());
    }

    auto drawAnalyzer = [&](const PluginProcessor::SpectrumArray& spectrum, juce::Colour fillColour, juce::Colour strokeColour)
    {
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

        if (!startedSpectrum)
            return;

        juce::Path fill(spectrumPath);
        fill.lineTo((float) plot.getRight(), (float) plot.getBottom());
        fill.lineTo((float) plot.getX(), (float) plot.getBottom());
        fill.closeSubPath();

        juce::ColourGradient gradient(fillColour.brighter(0.1f), (float) plot.getCentreX(), (float) plot.getY(),
                                      fillColour.withAlpha(0.02f), (float) plot.getCentreX(), (float) plot.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillPath(fill);

        g.setColour(strokeColour);
        g.strokePath(spectrumPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (analyzerMode == PluginProcessor::analyzerPre || analyzerMode == PluginProcessor::analyzerPrePost)
        drawAnalyzer(analyzerFrame.pre, juce::Colour(0x2034c7a6), juce::Colour(0x8052cce8));

    if (analyzerMode == PluginProcessor::analyzerPost || analyzerMode == PluginProcessor::analyzerPrePost)
        drawAnalyzer(analyzerFrame.post, juce::Colour(0x1637e4ff), juce::Colour(0x9063d0ff));

    if (analyzerMode == PluginProcessor::analyzerSidechain)
        drawAnalyzer(analyzerFrame.sidechain, juce::Colour(0x1830b89f), juce::Colour(0x8084dfff));

    std::array<bool, 5> activeModes{};
    for (const auto& point : points)
        if (point.enabled && juce::isPositiveAndBelow(point.stereoMode, (int) activeModes.size()))
            activeModes[(size_t) point.stereoMode] = true;

    if (selectedPoint >= 0 && juce::isPositiveAndBelow(selectedPoint, PluginProcessor::maxPoints))
    {
        const auto point = points[(size_t) selectedPoint];
        if (point.enabled)
        {
            std::vector<float> contributionValues;
            contributionValues.reserve((size_t) plot.getWidth());
            for (int x = 0; x < plot.getWidth(); ++x)
            {
                const float frequency = normalisedToFrequency((float) x / (float) juce::jmax(1, plot.getWidth() - 1));
                contributionValues.push_back(juce::jlimit(minGainDb, maxGainDb,
                                                          processor.getBandResponseForFrequency(selectedPoint, frequency)));
            }

            drawResponseDeltaFill(g, plot, contributionValues, ssp::ui::getStereoModeColour(point.stereoMode).withAlpha(0.18f));
        }
    }

    for (int mode = 0; mode < (int) activeModes.size(); ++mode)
    {
        if (!activeModes[(size_t) mode])
            continue;

        juce::Path responsePath;
        bool started = false;
        std::vector<float> responseValues;
        responseValues.reserve((size_t) plot.getWidth());

        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const float frequency = normalisedToFrequency((float) x / (float) juce::jmax(1, plot.getWidth() - 1));
            const float gainDb = juce::jlimit(minGainDb, maxGainDb, processor.getResponseForFrequencyByStereoMode(frequency, mode));
            responseValues.push_back(gainDb);
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

        const auto modeColour = ssp::ui::getStereoModeColour(mode);
        drawResponseDeltaFill(g, plot, responseValues, modeColour.withAlpha(mode == PluginProcessor::stereo ? 0.16f : 0.12f));
        g.setColour(modeColour);
        g.strokePath(responsePath, juce::PathStrokeType(mode == PluginProcessor::stereo ? 2.6f : 2.2f,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }

    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        const auto point = points[(size_t) i];
        if (!point.enabled)
            continue;

        const auto position = pointToScreen(i, point);
        const bool isSelected = (i == selectedPoint);
        const bool isHovered = (i == hoverPoint);
        const float radius = isSelected ? selectedPointRadius : pointRadius + (isHovered ? 1.5f : 0.0f);
        const auto modeColour = ssp::ui::getStereoModeColour(point.stereoMode);

        g.setColour(modeColour.withAlpha(isSelected ? 0.45f : 0.22f));
        g.fillEllipse(position.x - radius - 4.0f, position.y - radius - 4.0f, (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);

        g.setColour(juce::Colour(0xff111822));
        g.fillEllipse(position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(isSelected ? modeColour.brighter(0.25f) : modeColour);
        g.drawEllipse(position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f, isSelected ? 2.8f : 1.8f);

        if (isHovered)
        {
            const auto noteText = ssp::notes::formatFrequencyWithNote(point.frequency);
            const juce::String info = noteText + " | " + (point.gainDb > 0.0f ? "+" : "") + juce::String(point.gainDb, 1) + " dB | Q " + juce::String(point.q, 2);
            auto tagBounds = juce::Rectangle<float>(position.x + 12.0f, position.y - 34.0f, juce::jlimit(180.0f, 280.0f, 8.0f * (float) info.length()), 22.0f);
            if (tagBounds.getRight() > (float) plot.getRight())
                tagBounds.setX(position.x - tagBounds.getWidth() - 12.0f);

            g.setColour(juce::Colour(0xe6111822));
            g.fillRoundedRectangle(tagBounds, 8.0f);
            g.setColour(modeColour.withAlpha(0.8f));
            g.drawRoundedRectangle(tagBounds, 8.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            g.drawText(ssp::ui::getStereoModeShortLabel(point.stereoMode) + " " + info,
                       tagBounds.toNearestInt().reduced(8, 2),
                       juce::Justification::centredLeft, false);
        }
    }

    g.restoreState();

    g.setColour(juce::Colour(0xff92a6bb));
    g.setFont(11.5f);
    for (float gain : gainLines)
    {
        const float y = juce::jmap(gain, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        const juce::String prefix = gain > 0.0f ? "+" : "";
        g.drawText(prefix + juce::String((int) gain) + " dB", plot.getX(), juce::roundToInt(y) - 8, 64, 16, juce::Justification::centredLeft, false);
    }

    for (float frequency : minorGridFrequencies)
    {
        const float x = plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
        g.drawText(formatFrequencyLabel(frequency), juce::roundToInt(x) - 24, plot.getBottom() + 4, 48, 16, juce::Justification::centred, false);
    }

    if (showNoteAxis)
    {
        g.setColour(juce::Colour(0xff6f849b));
        g.setFont(10.0f);
        for (int octave = 1; octave <= 7; ++octave)
        {
            const double frequency = 440.0 * std::pow(2.0, (((octave + 1) * 12) - 69.0) / 12.0);
            if (frequency < minFrequency || frequency > maxFrequency)
                continue;
            const float x = plot.getX() + frequencyToNormalised((float) frequency) * (float) plot.getWidth();
            g.drawText("C" + juce::String(octave), juce::roundToInt(x) - 20, plot.getBottom() + 18, 40, 14, juce::Justification::centred, false);
        }
    }

    int activeModeCount = 0;
    for (bool active : activeModes)
        activeModeCount += active ? 1 : 0;

    if (activeModeCount > 1)
    {
        auto legendBounds = plot.removeFromRight(132).removeFromTop(92).translated(-4, 4).toFloat();
        g.setColour(juce::Colour(0xe6111822));
        g.fillRoundedRectangle(legendBounds, 10.0f);
        g.setColour(juce::Colour(0xff2f465c));
        g.drawRoundedRectangle(legendBounds, 10.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText("Mode Curves", legendBounds.removeFromTop(18).toNearestInt().reduced(8, 0), juce::Justification::centredLeft, false);

        for (int mode = 0; mode < (int) activeModes.size(); ++mode)
        {
            if (!activeModes[(size_t) mode])
                continue;
            auto row = legendBounds.removeFromTop(14).reduced(8.0f, 0.0f);
            g.setColour(ssp::ui::getStereoModeColour(mode));
            g.drawLine(row.getX(), row.getCentreY(), row.getX() + 16.0f, row.getCentreY(), 2.0f);
            g.setColour(juce::Colour(0xffc7d8e9));
            g.drawText(PluginProcessor::getStereoModeNames()[mode], juce::Rectangle<int>((int) row.getX() + 22, (int) row.getY() - 4, 90, 14), juce::Justification::centredLeft, false);
        }
    }
}

void EqGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    if (notesButton.getBounds().contains(event.getPosition()))
        return;

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
    if (!pointTypeUsesGain(point.type))
        movedPoint.gainDb = point.gainDb;

    processor.setPoint(dragPoint, movedPoint);
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

void EqGraphComponent::mouseMove(const juce::MouseEvent& event)
{
    const int hit = hitTestPoint(event.position);
    if (hoverPoint != hit)
    {
        hoverPoint = hit;
        repaint();
    }
}

void EqGraphComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverPoint != -1)
    {
        hoverPoint = -1;
        repaint();
    }
}

void EqGraphComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const int hit = hitTestPoint(event.position);
    const int targetPoint = hit >= 0 ? hit : selectedPoint;
    if (targetPoint < 0)
        return;

    auto point = processor.getPoint(targetPoint);
    if (!point.enabled)
        return;

    point.q = juce::jlimit(0.2f, 18.0f, point.q + wheel.deltaY * (point.q < 3.0f ? 0.2f : 0.5f));
    processor.setPoint(targetPoint, point);
    selectPoint(targetPoint);
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
    bounds.removeFromBottom(showNoteAxis ? 50 : 34);
    bounds.removeFromLeft(12);
    bounds.removeFromRight(12);
    return bounds;
}

juce::Point<float> EqGraphComponent::pointToScreen(int pointIndex, const PluginProcessor::EqPoint& point) const
{
    const auto plot = getPlotBounds().toFloat();
    const float x = plot.getX() + frequencyToNormalised(point.frequency) * plot.getWidth();
    const float responseAtFrequency = juce::jlimit(minGainDb, maxGainDb, processor.getBandResponseForFrequency(pointIndex, point.frequency));
    const float y = juce::jmap(responseAtFrequency, maxGainDb, minGainDb, plot.getY(), plot.getBottom());
    return {x, y};
}

PluginProcessor::EqPoint EqGraphComponent::screenToPoint(juce::Point<float> position, const PluginProcessor::EqPoint& source) const
{
    auto point = source;
    const auto plot = getPlotBounds().toFloat();
    const float normalisedX = (position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth());
    const float normalisedY = (position.y - plot.getY()) / juce::jmax(1.0f, plot.getHeight());

    point.frequency = normalisedToFrequency(normalisedX);
    if (pointTypeUsesGain(source.type))
        point.gainDb = juce::jmap(juce::jlimit(0.0f, 1.0f, normalisedY), 1.0f, 0.0f, minGainDb, maxGainDb);
    point.q = source.q <= 0.0f ? 1.0f : source.q;
    point.slopeIndex = source.slopeIndex;
    point.stereoMode = source.stereoMode;
    point.type = source.type;
    return point;
}

int EqGraphComponent::hitTestPoint(juce::Point<float> position) const
{
    const auto points = processor.getPointsSnapshot();
    for (int i = PluginProcessor::maxPoints - 1; i >= 0; --i)
    {
        const auto point = points[(size_t) i];
        if (!point.enabled)
            continue;

        if (pointToScreen(i, point).getDistanceFrom(position) <= selectedPointRadius + 4.0f)
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
