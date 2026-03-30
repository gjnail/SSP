#include "ImagerGraphComponent.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float handleRadius = 8.0f;
constexpr float hitRadius = 16.0f;

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

// Approximate LR4 magnitude response for drawing crossover curves
float lr4Response(float freq, float cutoff, bool isLowpass)
{
    const float ratio = freq / cutoff;
    const float r2 = ratio * ratio;
    const float r4 = r2 * r2;
    // LR4 = squared Butterworth 2nd order
    const float magSq = 1.0f / (1.0f + r4);
    return isLowpass ? magSq : (1.0f - magSq);
}
} // namespace

ImagerGraphComponent::ImagerGraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
    displayCorrelation.fill(1.0f);
    displayWidth.fill(0.0f);
}

juce::Rectangle<int> ImagerGraphComponent::getPlotBounds() const
{
    return getLocalBounds().reduced(40, 20);
}

float ImagerGraphComponent::frequencyToX(float freq) const
{
    const auto plot = getPlotBounds();
    return (float) plot.getX() + frequencyToNormalised(freq) * (float) plot.getWidth();
}

float ImagerGraphComponent::xToFrequency(float x) const
{
    const auto plot = getPlotBounds();
    const float normalised = (x - (float) plot.getX()) / (float) juce::jmax(1, plot.getWidth());
    return normalisedToFrequency(normalised);
}

int ImagerGraphComponent::findHandleAt(juce::Point<float> pos) const
{
    const auto plot = getPlotBounds();
    const float handleY = (float) plot.getBottom() - 12.0f;

    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
    {
        const float hx = frequencyToX(processor.getCrossoverFrequency(i));
        if (pos.getDistanceFrom({ hx, handleY }) < hitRadius)
            return i;
    }
    return -1;
}

void ImagerGraphComponent::timerCallback()
{
    for (int b = 0; b < PluginProcessor::numBands; ++b)
    {
        displayCorrelation[b] = processor.getBandMeter(b).correlation.load();
        displayWidth[b] = processor.getBandMeter(b).width.load();
    }
    repaint();
}

void ImagerGraphComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    ssp::ui::drawPanelBackground(g, bounds, ssp::ui::brandCyan());

    const auto plot = getPlotBounds();
    // Dark inner area
    g.setColour(juce::Colour(0xff0a1017));
    g.fillRoundedRectangle(plot.toFloat(), 4.0f);

    drawBandFills(g, plot);
    drawGrid(g, plot);
    drawCorrelationMeters(g, plot);
    drawWidthIndicators(g, plot);
    drawCrossoverHandles(g, plot);
    drawBandLabels(g, plot);

    // Inner border
    g.setColour(ssp::ui::outlineSoft());
    g.drawRoundedRectangle(plot.toFloat(), 4.0f, 1.0f);
}

void ImagerGraphComponent::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    // Frequency grid lines
    const float freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    g.setColour(juce::Colour(0xff1a2535));

    for (auto freq : freqs)
    {
        const float x = frequencyToX(freq);
        if (x > (float) plot.getX() && x < (float) plot.getRight())
            g.drawVerticalLine((int) x, (float) plot.getY(), (float) plot.getBottom());
    }

    // Frequency labels
    g.setColour(ssp::ui::textMuted().withAlpha(0.6f));
    g.setFont(10.0f);
    const float labelFreqs[] = { 100, 1000, 10000 };
    const juce::String labelTexts[] = { "100", "1k", "10k" };
    for (int i = 0; i < 3; ++i)
    {
        const float x = frequencyToX(labelFreqs[i]);
        g.drawText(labelTexts[i], (int) x - 20, plot.getBottom() + 2, 40, 14, juce::Justification::centred);
    }
}

void ImagerGraphComponent::drawBandFills(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    // Get crossover x positions
    float xPositions[PluginProcessor::numCrossovers];
    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
        xPositions[i] = frequencyToX(processor.getCrossoverFrequency(i));

    // Define band boundaries
    float bandLeft[PluginProcessor::numBands];
    float bandRight[PluginProcessor::numBands];
    bandLeft[0] = (float) plot.getX();
    bandRight[0] = xPositions[0];
    bandLeft[1] = xPositions[0];
    bandRight[1] = xPositions[1];
    bandLeft[2] = xPositions[1];
    bandRight[2] = xPositions[2];
    bandLeft[3] = xPositions[2];
    bandRight[3] = (float) plot.getRight();

    for (int b = 0; b < PluginProcessor::numBands; ++b)
    {
        auto bandArea = juce::Rectangle<float>(bandLeft[b], (float) plot.getY(),
                                                bandRight[b] - bandLeft[b], (float) plot.getHeight());
        if (bandArea.getWidth() < 1.0f)
            continue;

        auto colour = getBandColour(b);

        // Fill with gradient
        juce::ColourGradient fill(colour.withAlpha(0.12f), bandArea.getTopLeft(),
                                  colour.withAlpha(0.04f), bandArea.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRect(bandArea);

        // Draw crossover transition (soft edge)
        if (b < PluginProcessor::numBands - 1)
        {
            g.setColour(colour.withAlpha(0.25f));
            g.drawVerticalLine((int) bandRight[b], (float) plot.getY(), (float) plot.getBottom());
        }
    }
}

void ImagerGraphComponent::drawCorrelationMeters(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    // Get crossover x positions for band centers
    float xPositions[PluginProcessor::numCrossovers];
    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
        xPositions[i] = frequencyToX(processor.getCrossoverFrequency(i));

    float bandLeft[PluginProcessor::numBands];
    float bandRight[PluginProcessor::numBands];
    bandLeft[0] = (float) plot.getX();
    bandRight[0] = xPositions[0];
    bandLeft[1] = xPositions[0];
    bandRight[1] = xPositions[1];
    bandLeft[2] = xPositions[1];
    bandRight[2] = xPositions[2];
    bandLeft[3] = xPositions[2];
    bandRight[3] = (float) plot.getRight();

    const float meterWidth = 18.0f;
    const float meterTop = (float) plot.getY() + 30.0f;
    const float meterBottom = (float) plot.getBottom() - 40.0f;
    const float meterHeight = meterBottom - meterTop;

    for (int b = 0; b < PluginProcessor::numBands; ++b)
    {
        const float bandCentreX = (bandLeft[b] + bandRight[b]) * 0.5f;
        const float bandW = bandRight[b] - bandLeft[b];
        if (bandW < meterWidth + 4.0f)
            continue;

        const float corrValue = juce::jlimit(-1.0f, 1.0f, displayCorrelation[b]);

        // Meter background
        auto meterBg = juce::Rectangle<float>(bandCentreX - meterWidth * 0.5f, meterTop, meterWidth, meterHeight);
        g.setColour(juce::Colour(0xff060a10));
        g.fillRoundedRectangle(meterBg, 3.0f);
        g.setColour(ssp::ui::outlineSoft());
        g.drawRoundedRectangle(meterBg, 3.0f, 0.8f);

        // Center line (correlation = 0)
        const float centerY = meterTop + meterHeight * 0.5f;
        g.setColour(ssp::ui::textMuted().withAlpha(0.3f));
        g.drawHorizontalLine((int) centerY, meterBg.getX() + 2.0f, meterBg.getRight() - 2.0f);

        // Correlation bar
        // corrValue: +1 = fully correlated (mono), 0 = uncorrelated, -1 = anti-correlated
        // Map: +1 at top, 0 at center, -1 at bottom
        const float barNorm = (1.0f - corrValue) * 0.5f; // 0=top(+1), 0.5=center(0), 1=bottom(-1)
        const float barY = meterTop + barNorm * meterHeight;

        // Color: green(correlated) -> yellow(uncorrelated) -> red(anti-correlated)
        juce::Colour meterColour;
        if (corrValue > 0.0f)
            meterColour = juce::Colour(0xff2ecc71).interpolatedWith(juce::Colour(0xfff1c40f), 1.0f - corrValue);
        else
            meterColour = juce::Colour(0xfff1c40f).interpolatedWith(juce::Colour(0xffe74c3c), -corrValue);

        // Draw bar from center to current position
        const float barLeft = meterBg.getX() + 2.5f;
        const float barWidth = meterBg.getWidth() - 5.0f;
        if (barY < centerY)
        {
            g.setColour(meterColour.withAlpha(0.7f));
            g.fillRoundedRectangle(barLeft, barY, barWidth, centerY - barY, 2.0f);
        }
        else
        {
            g.setColour(meterColour.withAlpha(0.7f));
            g.fillRoundedRectangle(barLeft, centerY, barWidth, barY - centerY, 2.0f);
        }

        // Position indicator line
        g.setColour(meterColour);
        g.fillRoundedRectangle(barLeft - 1.0f, barY - 1.5f, barWidth + 2.0f, 3.0f, 1.5f);

        // Labels
        g.setColour(ssp::ui::textMuted().withAlpha(0.5f));
        g.setFont(8.5f);
        g.drawText("+", (int) meterBg.getX(), (int) meterTop - 12, (int) meterBg.getWidth(), 12, juce::Justification::centred);
        g.drawText("-", (int) meterBg.getX(), (int) meterBottom, (int) meterBg.getWidth(), 12, juce::Justification::centred);
    }
}

void ImagerGraphComponent::drawWidthIndicators(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    float xPositions[PluginProcessor::numCrossovers];
    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
        xPositions[i] = frequencyToX(processor.getCrossoverFrequency(i));

    float bandLeft[PluginProcessor::numBands];
    float bandRight[PluginProcessor::numBands];
    bandLeft[0] = (float) plot.getX();
    bandRight[0] = xPositions[0];
    bandLeft[1] = xPositions[0];
    bandRight[1] = xPositions[1];
    bandLeft[2] = xPositions[1];
    bandRight[2] = xPositions[2];
    bandLeft[3] = xPositions[2];
    bandRight[3] = (float) plot.getRight();

    const float indicatorY = (float) plot.getY() + 12.0f;

    for (int b = 0; b < PluginProcessor::numBands; ++b)
    {
        const float bandCentreX = (bandLeft[b] + bandRight[b]) * 0.5f;
        const float bandW = bandRight[b] - bandLeft[b];
        if (bandW < 40.0f)
            continue;

        const float widthPct = processor.getBandWidth(b);
        const float maxBarW = juce::jmin(bandW - 8.0f, 60.0f);
        const float barW = (widthPct / 200.0f) * maxBarW;
        auto colour = getBandColour(b);

        // Width bar background
        auto bgBar = juce::Rectangle<float>(bandCentreX - maxBarW * 0.5f, indicatorY, maxBarW, 6.0f);
        g.setColour(juce::Colour(0xff0a0f18));
        g.fillRoundedRectangle(bgBar, 3.0f);

        // Width bar fill (centered, grows outward)
        auto fillBar = juce::Rectangle<float>(bandCentreX - barW * 0.5f, indicatorY, barW, 6.0f);
        g.setColour(colour.withAlpha(0.65f));
        g.fillRoundedRectangle(fillBar, 3.0f);

        // Center tick at 100%
        const float tick100 = (100.0f / 200.0f) * maxBarW;
        g.setColour(ssp::ui::textMuted().withAlpha(0.35f));
        g.drawVerticalLine((int) (bandCentreX - tick100 * 0.5f), indicatorY - 1.0f, indicatorY + 7.0f);
        g.drawVerticalLine((int) (bandCentreX + tick100 * 0.5f), indicatorY - 1.0f, indicatorY + 7.0f);

        // Width percentage text
        g.setColour(colour.withAlpha(0.8f));
        g.setFont(9.5f);
        g.drawText(juce::String((int) widthPct) + "%",
                   (int) (bandCentreX - 25.0f), (int) indicatorY + 7, 50, 12,
                   juce::Justification::centred);
    }
}

void ImagerGraphComponent::drawCrossoverHandles(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const float handleY = (float) plot.getBottom() - 12.0f;

    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
    {
        const float freq = processor.getCrossoverFrequency(i);
        const float x = frequencyToX(freq);
        const bool isHovered = (hoverHandle == i);
        const bool isDragging = (dragHandle == i);

        // Vertical crossover line
        auto lineColour = ssp::ui::textMuted().withAlpha(isDragging ? 0.7f : (isHovered ? 0.5f : 0.3f));
        g.setColour(lineColour);
        g.drawVerticalLine((int) x, (float) plot.getY(), (float) plot.getBottom());

        // Dashed line effect
        const float dashLen = 6.0f;
        const float gapLen = 4.0f;
        g.setColour(ssp::ui::textStrong().withAlpha(isDragging ? 0.5f : (isHovered ? 0.35f : 0.15f)));
        float yy = (float) plot.getY();
        while (yy < (float) plot.getBottom())
        {
            const float end = juce::jmin(yy + dashLen, (float) plot.getBottom());
            g.drawVerticalLine((int) x, yy, end);
            yy = end + gapLen;
        }

        // Handle circle
        const float radius = isDragging ? handleRadius * 1.2f : (isHovered ? handleRadius * 1.1f : handleRadius);
        g.setColour(ssp::ui::brandCyan().withAlpha(isDragging ? 1.0f : (isHovered ? 0.85f : 0.65f)));
        g.fillEllipse(x - radius, handleY - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(ssp::ui::textStrong().withAlpha(0.9f));
        g.drawEllipse(x - radius, handleY - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Frequency label above handle
        g.setColour(ssp::ui::textStrong().withAlpha(isDragging ? 1.0f : 0.75f));
        g.setFont(10.0f);
        g.drawText(formatFrequencyLabel(freq), (int) x - 25, (int) (handleY - radius - 16), 50, 14,
                   juce::Justification::centred);
    }
}

void ImagerGraphComponent::drawBandLabels(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    float xPositions[PluginProcessor::numCrossovers];
    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
        xPositions[i] = frequencyToX(processor.getCrossoverFrequency(i));

    float bandLeft[PluginProcessor::numBands];
    float bandRight[PluginProcessor::numBands];
    bandLeft[0] = (float) plot.getX();
    bandRight[0] = xPositions[0];
    bandLeft[1] = xPositions[0];
    bandRight[1] = xPositions[1];
    bandLeft[2] = xPositions[1];
    bandRight[2] = xPositions[2];
    bandLeft[3] = xPositions[2];
    bandRight[3] = (float) plot.getRight();

    const juce::String bandNames[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    const float labelY = (float) plot.getBottom() - 28.0f;

    for (int b = 0; b < PluginProcessor::numBands; ++b)
    {
        const float bandCentreX = (bandLeft[b] + bandRight[b]) * 0.5f;
        const float bandW = bandRight[b] - bandLeft[b];
        if (bandW < 30.0f)
            continue;

        g.setColour(getBandColour(b).withAlpha(0.6f));
        g.drawText(bandNames[b], (int) (bandCentreX - bandW * 0.5f + 4.0f), (int) labelY,
                   (int) (bandW - 8.0f), 14, juce::Justification::centred);
    }
}

void ImagerGraphComponent::resized()
{
}

void ImagerGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    dragHandle = findHandleAt(event.position);
}

void ImagerGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragHandle < 0)
        return;

    float newFreq = xToFrequency(event.position.x);

    // Enforce ordering with minimum gap
    constexpr float minGapRatio = 1.4f;

    if (dragHandle == 0)
    {
        const float upperLimit = processor.getCrossoverFrequency(1) / minGapRatio;
        newFreq = juce::jlimit(minFrequency, upperLimit, newFreq);
    }
    else if (dragHandle == 1)
    {
        const float lowerLimit = processor.getCrossoverFrequency(0) * minGapRatio;
        const float upperLimit = processor.getCrossoverFrequency(2) / minGapRatio;
        newFreq = juce::jlimit(lowerLimit, upperLimit, newFreq);
    }
    else if (dragHandle == 2)
    {
        const float lowerLimit = processor.getCrossoverFrequency(1) * minGapRatio;
        newFreq = juce::jlimit(lowerLimit, maxFrequency, newFreq);
    }

    const juce::String ids[] = { "crossover1", "crossover2", "crossover3" };
    if (auto* param = processor.apvts.getParameter(ids[dragHandle]))
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(newFreq));
}

void ImagerGraphComponent::mouseUp(const juce::MouseEvent&)
{
    dragHandle = -1;
}

void ImagerGraphComponent::mouseMove(const juce::MouseEvent& event)
{
    const int newHover = findHandleAt(event.position);
    if (newHover != hoverHandle)
    {
        hoverHandle = newHover;
        setMouseCursor(hoverHandle >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                                        : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ImagerGraphComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverHandle >= 0)
    {
        hoverHandle = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}
