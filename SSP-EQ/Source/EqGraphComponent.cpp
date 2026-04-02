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

float getSignedDynamicRangeDb(const PluginProcessor::EqPoint& point)
{
    const float magnitude = juce::jmin(std::abs(point.gainDb), juce::jlimit(0.0f, 24.0f, point.dynamicRangeDb));
    return point.gainDb < 0.0f ? -magnitude : magnitude;
}

PluginProcessor::EqPoint makeFullDynamicSetPoint(const PluginProcessor::EqPoint& point)
{
    auto result = point;
    result.gainDb = point.gainDb;
    return result;
}

PluginProcessor::EqPoint makeRangeLimitedDynamicPoint(const PluginProcessor::EqPoint& point)
{
    auto result = point;
    result.gainDb = getSignedDynamicRangeDb(point);
    return result;
}

juce::Colour getDynamicBandColour(const PluginProcessor::EqPoint& point)
{
    auto base = ssp::ui::getStereoModeColour(point.stereoMode);
    if (! point.dynamicEnabled)
        return base;

    return point.dynamicDirection == PluginProcessor::dynamicBelow
        ? base.interpolatedWith(juce::Colour(0xff52df88), 0.38f)
        : base.interpolatedWith(juce::Colour(0xfff0844f), 0.38f);
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

int slopeIndexToDbPerOct(int slopeIndex)
{
    static constexpr int slopes[] = {6, 12, 18, 24, 36, 48, 72, 96};
    return slopes[(size_t) juce::jlimit(0, 7, slopeIndex)];
}

int stageCountForSlope(int slopeIndex)
{
    return juce::jlimit(1, PluginProcessor::maxStagesPerPoint, (int) std::ceil((double) slopeIndexToDbPerOct(slopeIndex) / 12.0));
}

juce::IIRCoefficients makeTiltLowShelf(double sampleRate, float frequency, float q, float gainDb)
{
    return juce::IIRCoefficients::makeLowShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(-gainDb * 0.5f));
}

juce::IIRCoefficients makeTiltHighShelf(double sampleRate, float frequency, float q, float gainDb)
{
    return juce::IIRCoefficients::makeHighShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb * 0.5f));
}

PluginProcessor::PointFilterSetup buildPointSetup(const PluginProcessor::EqPoint& point, double sampleRate)
{
    PluginProcessor::PointFilterSetup setup{};
    if (! point.enabled || sampleRate <= 0.0)
        return setup;

    const float frequency = juce::jlimit(minFrequency, maxFrequency, point.frequency);
    const float q = juce::jlimit(0.2f, 18.0f, point.q);
    const float gainDb = pointTypeUsesGain(point.type) ? juce::jlimit(minGainDb, maxGainDb, point.gainDb) : 0.0f;
    const int stageCount = stageCountForSlope(point.slopeIndex);
    auto pushStage = [&](const juce::IIRCoefficients& coeffs)
    {
        if (setup.numStages < PluginProcessor::maxStagesPerPoint)
            setup.coeffs[(size_t) setup.numStages++] = coeffs;
    };

    switch (point.type)
    {
        case PluginProcessor::lowShelf:
            for (int stage = 0; stage < stageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeLowShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb / (float) stageCount)));
            break;
        case PluginProcessor::highShelf:
            for (int stage = 0; stage < stageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeHighShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb / (float) stageCount)));
            break;
        case PluginProcessor::lowCut:
            for (int stage = 0; stage < stageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeHighPass(sampleRate, frequency, 0.70710678f));
            break;
        case PluginProcessor::highCut:
            for (int stage = 0; stage < stageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeLowPass(sampleRate, frequency, 0.70710678f));
            break;
        case PluginProcessor::notch:
            pushStage(juce::IIRCoefficients::makeNotchFilter(sampleRate, frequency, q));
            break;
        case PluginProcessor::bandPass:
            pushStage(juce::IIRCoefficients::makeBandPass(sampleRate, frequency, q));
            break;
        case PluginProcessor::tiltShelf:
            pushStage(makeTiltLowShelf(sampleRate, frequency, q, gainDb));
            pushStage(makeTiltHighShelf(sampleRate, frequency, q, gainDb));
            break;
        case PluginProcessor::bell:
        default:
            pushStage(juce::IIRCoefficients::makePeakFilter(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb)));
            break;
    }

    return setup;
}

double getMagnitudeForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto* c = coefficients.coefficients;
    const double cosW = std::cos(w);
    const double sinW = std::sin(w);
    const double cos2W = std::cos(2.0 * w);
    const double sin2W = std::sin(2.0 * w);
    const double numReal = (double) c[0] + (double) c[1] * cosW + (double) c[2] * cos2W;
    const double numImag = -((double) c[1] * sinW + (double) c[2] * sin2W);
    const double denReal = 1.0 + (double) c[3] * cosW + (double) c[4] * cos2W;
    const double denImag = -((double) c[3] * sinW + (double) c[4] * sin2W);
    const double numeratorMagnitude = std::sqrt(numReal * numReal + numImag * numImag);
    const double denominatorMagnitude = std::sqrt(denReal * denReal + denImag * denImag);
    return denominatorMagnitude > 0.0 ? numeratorMagnitude / denominatorMagnitude : 1.0;
}

float getResponseForSnapshot(const PluginProcessor::PointArray& points, double sampleRate, float frequency, int stereoMode = -1)
{
    double magnitude = 1.0;
    bool anyMatched = stereoMode < 0;

    for (const auto& point : points)
    {
        if (! point.enabled)
            continue;
        if (stereoMode >= 0 && point.stereoMode != stereoMode)
            continue;

        anyMatched = true;
        const auto setup = buildPointSetup(point, sampleRate);
        for (int stage = 0; stage < setup.numStages; ++stage)
            magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, sampleRate);
    }

    return anyMatched ? juce::Decibels::gainToDecibels((float) magnitude, -48.0f) : 0.0f;
}

float getResponseForPointSnapshot(const PluginProcessor::EqPoint& point, double sampleRate, float frequency)
{
    const auto setup = buildPointSetup(point, sampleRate);
    double magnitude = 1.0;
    for (int stage = 0; stage < setup.numStages; ++stage)
        magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, sampleRate);

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

juce::Path buildResponsePath(const PluginProcessor::PointArray& points,
                             const juce::Rectangle<int>& plot,
                             double sampleRate,
                             int stereoMode = -1)
{
    juce::Path path;
    bool started = false;
    for (int x = 0; x < plot.getWidth(); ++x)
    {
        const float frequency = normalisedToFrequency((float) x / (float) juce::jmax(1, plot.getWidth() - 1));
        const float gainDb = juce::jlimit(minGainDb, maxGainDb, getResponseForSnapshot(points, sampleRate, frequency, stereoMode));
        const float y = juce::jmap(gainDb, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
        const float px = (float) plot.getX() + (float) x;
        if (! started)
        {
            path.startNewSubPath(px, y);
            started = true;
        }
        else
        {
            path.lineTo(px, y);
        }
    }

    return path;
}

bool pointsDiffer(const PluginProcessor::PointArray& a, const PluginProcessor::PointArray& b)
{
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        const auto& lhs = a[(size_t) i];
        const auto& rhs = b[(size_t) i];
        if (lhs.enabled != rhs.enabled
            || std::abs(lhs.frequency - rhs.frequency) > 0.001f
            || std::abs(lhs.gainDb - rhs.gainDb) > 0.001f
            || std::abs(lhs.q - rhs.q) > 0.001f
            || lhs.type != rhs.type
            || lhs.slopeIndex != rhs.slopeIndex
            || lhs.stereoMode != rhs.stereoMode)
            return true;
    }

    return false;
}
} // namespace

EqGraphComponent::EqGraphComponent(PluginProcessor& p)
    : processor(p)
{
    transitionFromPoints = processor.getPointsSnapshot();
    transitionToPoints = transitionFromPoints;
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
    const auto previewReferencePoints = processor.getPreviewReferencePointsSnapshot();
    const bool previewActive = processor.isPresetPreviewActive();
    const int soloPoint = processor.getSoloPointIndex();

    const auto now = juce::Time::getMillisecondCounter();
    const float rawTransition = transitionActive ? juce::jlimit(0.0f, 1.0f, (float) (now - transitionStartMs) / (float) juce::jmax(1, transitionDurationMs)) : 1.0f;
    const float easedTransition = 1.0f - std::pow(1.0f - rawTransition, 2.0f);
    if (transitionActive && rawTransition >= 1.0f)
        transitionActive = false;

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

    if (transitionActive)
    {
        g.setColour(juce::Colour(0xff7fd7ff).withAlpha((1.0f - easedTransition) * 0.24f));
        g.strokePath(buildResponsePath(transitionFromPoints, plot, sampleRate), juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (previewActive)
    {
        auto previewPath = buildResponsePath(previewReferencePoints, plot, sampleRate);
        juce::Path dashed;
        const float dashes[] = { 7.0f, 5.0f };
        juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dashed, previewPath, dashes, 2);
        g.setColour(juce::Colour(0xff8fe5ff).withAlpha(0.45f));
        g.fillPath(dashed);
    }

    for (int pointIndex = 0; pointIndex < PluginProcessor::maxPoints; ++pointIndex)
    {
        const auto& point = points[(size_t) pointIndex];
        if (! point.enabled || ! point.dynamicEnabled)
            continue;

        juce::Path setCurve;
        juce::Path rangeCurve;
        juce::Path activeCurve;
        bool startedSet = false;
        const bool showRangeLimit = pointIndex == selectedPoint && std::abs(std::abs(point.gainDb) - std::abs(getSignedDynamicRangeDb(point))) > 0.01f;
        const auto fullSetPoint = makeFullDynamicSetPoint(point);
        const auto rangeLimitedPoint = makeRangeLimitedDynamicPoint(point);

        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const float frequency = normalisedToFrequency((float) x / (float) juce::jmax(1, plot.getWidth() - 1));
            const float setDb = juce::jlimit(minGainDb, maxGainDb, getResponseForPointSnapshot(fullSetPoint, sampleRate, frequency));
            const float rangeDb = juce::jlimit(minGainDb, maxGainDb, getResponseForPointSnapshot(rangeLimitedPoint, sampleRate, frequency));
            const float activeDb = juce::jlimit(minGainDb, maxGainDb, processor.getBandResponseForFrequency(pointIndex, frequency));
            const float px = (float) plot.getX() + (float) x;
            const float setY = juce::jmap(setDb, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
            const float rangeY = juce::jmap(rangeDb, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());
            const float activeY = juce::jmap(activeDb, maxGainDb, minGainDb, (float) plot.getY(), (float) plot.getBottom());

            if (! startedSet)
            {
                setCurve.startNewSubPath(px, setY);
                rangeCurve.startNewSubPath(px, rangeY);
                activeCurve.startNewSubPath(px, activeY);
                startedSet = true;
            }
            else
            {
                setCurve.lineTo(px, setY);
                rangeCurve.lineTo(px, rangeY);
                activeCurve.lineTo(px, activeY);
            }
        }

        const auto dynamicColour = getDynamicBandColour(point);
        juce::Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dashed, setCurve, dashes, 2);
        g.setColour(dynamicColour.withAlpha(0.28f));
        g.fillPath(dashed);
        if (showRangeLimit)
        {
            juce::Path dotted;
            const float dots[] = { 2.0f, 4.0f };
            juce::PathStrokeType(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createDashedStroke(dotted, rangeCurve, dots, 2);
            g.setColour(dynamicColour.withAlpha(0.52f));
            g.fillPath(dotted);
        }
        g.setColour(dynamicColour.withAlpha(0.9f));
        g.strokePath(activeCurve, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

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

    if (selectedPoint >= 0 && juce::isPositiveAndBelow(selectedPoint, PluginProcessor::maxPoints))
    {
        const auto point = points[(size_t) selectedPoint];
        if (point.enabled && point.dynamicEnabled)
        {
            const float thresholdY = juce::jmap(juce::jlimit(-96.0f, 0.0f, point.dynamicThresholdDb),
                                                -96.0f, 0.0f, (float) plot.getBottom(), (float) plot.getY());
            g.setColour(juce::Colour(0xfff0a35a).withAlpha(0.55f));
            g.drawHorizontalLine(juce::roundToInt(thresholdY), (float) plot.getX(), (float) plot.getRight());
            g.setFont(10.5f);
            g.drawText("Thr " + juce::String(point.dynamicThresholdDb, 1) + " dB",
                       plot.getRight() - 94, juce::roundToInt(thresholdY) - 10, 90, 16,
                       juce::Justification::centredRight, false);
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

        const auto modeColour = ssp::ui::getStereoModeColour(mode).withAlpha(previewActive ? 0.86f : 1.0f);
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

        auto currentPosition = point.dynamicEnabled ? pointToDisplayScreen(i, point)
                                                    : pointToScreenForSnapshot(i, point, points);
        const auto previousPoint = transitionFromPoints[(size_t) i];
        const auto previousPosition = previousPoint.enabled ? pointToScreenForSnapshot(i, previousPoint, transitionFromPoints) : currentPosition;
        const auto position = previousPosition + (currentPosition - previousPosition) * easedTransition;
        const bool isSelected = (i == selectedPoint);
        const bool isMultiSelected = isPointSelected(i);
        const bool isHovered = (i == hoverPoint);
        const bool isSolo = (i == soloPoint);
        const auto dynamicState = processor.getDynamicVisualState(i);
        const float radius = isSelected ? selectedPointRadius : ((isMultiSelected ? pointRadius + 1.8f : pointRadius) + (isHovered ? 1.5f : 0.0f));
        const auto modeColour = point.dynamicEnabled ? getDynamicBandColour(point) : ssp::ui::getStereoModeColour(point.stereoMode);
        float nodeAlpha = previousPoint.enabled && point.enabled ? 1.0f : (point.enabled ? easedTransition : (1.0f - easedTransition));
        if (soloPoint >= 0 && ! isSolo)
            nodeAlpha *= 0.34f;

        const float dynamicPulse = point.dynamicEnabled ? dynamicState.activity : 0.0f;
        const float pulse = isSolo ? (0.55f + 0.25f * std::sin((float) now * 0.015f)) : dynamicPulse;
        g.setColour(modeColour.withAlpha((isSelected ? 0.45f : 0.22f) * nodeAlpha + pulse * 0.18f));
        g.fillEllipse(position.x - radius - 4.0f, position.y - radius - 4.0f, (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
        if (isSolo)
        {
            g.setColour(juce::Colour(0xff48d8cb).withAlpha(0.18f + pulse * 0.24f));
            g.fillEllipse(position.x - radius - 10.0f, position.y - radius - 10.0f, (radius + 10.0f) * 2.0f, (radius + 10.0f) * 2.0f);
        }
        else if (point.dynamicEnabled)
        {
            const float ringRadius = radius + 7.0f + dynamicState.activity * 10.0f;
            g.setColour(modeColour.withAlpha(0.15f + dynamicState.activity * 0.35f));
            g.drawEllipse(position.x - ringRadius, position.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 1.2f + dynamicState.activity * 1.4f);
        }

        g.setColour(juce::Colour(0xff111822).withAlpha(nodeAlpha));
        g.fillEllipse(position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour((isSelected ? modeColour.brighter(0.25f) : modeColour).withAlpha(nodeAlpha));
        g.drawEllipse(position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f, isSelected ? 2.8f : (isMultiSelected ? 2.3f : 1.8f));

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

        if (point.dynamicEnabled && dynamicState.activity > 0.01f)
        {
            g.setColour(modeColour.brighter(0.3f));
            g.setFont(10.5f);
            const auto grBounds = juce::Rectangle<int>((int) position.x + 10, (int) position.y + 8, 56, 14);
            g.drawText(juce::String(dynamicState.effectiveGainDb, 1) + " dB", grBounds, juce::Justification::centredLeft, false);
        }

        if (isHovered || isSolo)
        {
            const auto soloBounds = getSoloButtonBounds(i);
            g.setColour(isSolo ? juce::Colour(0xfff1a84d) : juce::Colour(0xff3d2a17));
            g.fillEllipse(soloBounds);
            g.setColour(isSolo ? juce::Colour(0xffffe0b8) : juce::Colour(0xfff1a84d));
            g.drawEllipse(soloBounds, 1.2f);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("S", soloBounds.toNearestInt(), juce::Justification::centred, false);
        }
    }

    if (marqueeSelecting && ! marqueeBounds.isEmpty())
    {
        g.setColour(juce::Colour(0xff63d0ff).withAlpha(0.12f));
        g.fillRect(marqueeBounds);
        g.setColour(juce::Colour(0xff9de7ff).withAlpha(0.72f));
        g.drawRect(marqueeBounds, 1.2f);
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

    if (const int soloHit = hitTestSoloButton(event.position); soloHit >= 0)
    {
        processor.toggleSoloPoint(soloHit);
        hoverPoint = soloHit;
        repaint();
        return;
    }

    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
    {
        if (const int hit = hitTestPoint(event.position); hit >= 0)
        {
            processor.removePoint(hit);
            selectedPoints.removeFirstMatchingValue(hit);
            dragPoint = -1;
            if (hit == selectedPoint)
                selectPoint(-1);
            repaint();
        }
        return;
    }

    dragPoint = hitTestPoint(event.position);
    dragStartPoints = processor.getPointsSnapshot();
    dragStartPosition = event.position;
    marqueeSelecting = false;
    marqueeBounds = {};

    if (dragPoint >= 0)
    {
        const bool additive = event.mods.isShiftDown() || event.mods.isCommandDown() || event.mods.isCtrlDown();
        if (additive)
        {
            if (isPointSelected(dragPoint))
                selectedPoints.removeFirstMatchingValue(dragPoint);
            else
                addPointToSelection(dragPoint, true);
        }
        else if (! isPointSelected(dragPoint))
        {
            clearSelection();
            addPointToSelection(dragPoint, true);
        }

        selectPoint(dragPoint);
        return;
    }

    marqueeSelecting = getPlotBounds().contains(event.getPosition());
    marqueeAdditive = event.mods.isShiftDown() || event.mods.isCommandDown() || event.mods.isCtrlDown();
    if (marqueeSelecting && ! marqueeAdditive)
        clearSelection();
}

void EqGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (marqueeSelecting)
    {
        marqueeBounds = juce::Rectangle<float>(dragStartPosition, event.position).getIntersection(getPlotBounds().toFloat());
        repaint();
        return;
    }

    if (dragPoint < 0 || selectedPoints.isEmpty())
        return;

    const auto plot = getPlotBounds().toFloat();
    const float deltaNormalisedX = (event.position.x - dragStartPosition.x) / juce::jmax(1.0f, plot.getWidth());
    const float deltaGain = juce::jmap((event.position.y - dragStartPosition.y) / juce::jmax(1.0f, plot.getHeight()),
                                       0.0f, 1.0f, 0.0f, minGainDb - maxGainDb);

    for (int selectionIndex = 0; selectionIndex < selectedPoints.size(); ++selectionIndex)
    {
        const int pointIndex = selectedPoints.getReference(selectionIndex);
        auto point = dragStartPoints[(size_t) pointIndex];
        if (! point.enabled)
            continue;

        point.frequency = normalisedToFrequency(frequencyToNormalised(point.frequency) + deltaNormalisedX);
        if (pointTypeUsesGain(point.type))
            point.gainDb = juce::jlimit(minGainDb, maxGainDb, point.gainDb + deltaGain);
        processor.setPoint(pointIndex, point);
    }
}

void EqGraphComponent::mouseUp(const juce::MouseEvent&)
{
    if (marqueeSelecting)
        updateMarqueeSelection(marqueeAdditive);

    dragPoint = -1;
    marqueeSelecting = false;
    marqueeBounds = {};
}

void EqGraphComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!getPlotBounds().contains(event.getPosition()))
        return;

    if (const int hit = hitTestPoint(event.position); hit >= 0)
    {
        auto point = processor.getPoint(hit);
        if (point.enabled)
        {
            point.dynamicEnabled = ! point.dynamicEnabled;
            point.dynamicRangeDb = juce::jmin(std::abs(point.gainDb), juce::jmax(0.0f, point.dynamicRangeDb));
            processor.setPoint(hit, point);
            selectPoint(hit);
        }
        return;
    }

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
    clearSelection();
    if (index >= 0)
        addPointToSelection(index, false);

    if (selectedPoint == index)
    {
        repaint();
        return;
    }

    selectedPoint = index;
    repaint();
}

void EqGraphComponent::timerCallback()
{
    const auto points = processor.getPointsSnapshot();
    if (pointsDiffer(points, transitionToPoints))
    {
        transitionFromPoints = transitionToPoints;
        transitionToPoints = points;
        transitionStartMs = juce::Time::getMillisecondCounter();
        transitionDurationMs = processor.consumePendingVisualTransitionMs(90);
        transitionActive = true;
    }

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
    return pointToScreenForSnapshot(pointIndex, point, processor.getPointsSnapshot());
}

juce::Point<float> EqGraphComponent::pointToDisplayScreen(int pointIndex, const PluginProcessor::EqPoint& point) const
{
    const auto plot = getPlotBounds().toFloat();
    const float x = plot.getX() + frequencyToNormalised(point.frequency) * plot.getWidth();
    const float responseAtFrequency = point.dynamicEnabled
        ? juce::jlimit(minGainDb, maxGainDb, processor.getBandResponseForFrequency(pointIndex, point.frequency))
        : pointToScreen(pointIndex, point).y;

    if (! point.dynamicEnabled)
        return pointToScreen(pointIndex, point);

    const float y = juce::jmap(responseAtFrequency, maxGainDb, minGainDb, plot.getY(), plot.getBottom());
    return { x, y };
}

juce::Point<float> EqGraphComponent::pointToScreenForSnapshot(int pointIndex,
                                                              const PluginProcessor::EqPoint& point,
                                                              const PluginProcessor::PointArray& sourcePoints) const
{
    juce::ignoreUnused(pointIndex, sourcePoints);
    const auto plot = getPlotBounds().toFloat();
    const float x = plot.getX() + frequencyToNormalised(point.frequency) * plot.getWidth();
    const auto setup = buildPointSetup(point, juce::jmax(1.0, processor.getCurrentSampleRate()));
    double magnitude = 1.0;
    for (int stage = 0; stage < setup.numStages; ++stage)
        magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], point.frequency, juce::jmax(1.0, processor.getCurrentSampleRate()));
    const float responseAtFrequency = juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
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

        const auto screenPoint = point.dynamicEnabled ? pointToDisplayScreen(i, point) : pointToScreen(i, point);
        const float hitRadius = point.dynamicEnabled ? selectedPointRadius + 10.0f : selectedPointRadius + 4.0f;
        if (screenPoint.getDistanceFrom(position) <= hitRadius)
            return i;
    }

    return -1;
}

bool EqGraphComponent::isPointSelected(int index) const
{
    return selectedPoints.contains(index);
}

void EqGraphComponent::clearSelection()
{
    selectedPoints.clearQuick();
}

void EqGraphComponent::addPointToSelection(int index, bool makePrimary)
{
    if (juce::isPositiveAndBelow(index, PluginProcessor::maxPoints))
        selectedPoints.addIfNotAlreadyThere(index);

    if (makePrimary)
        selectedPoint = index;
}

void EqGraphComponent::updateMarqueeSelection(bool additive)
{
    juce::ignoreUnused(additive);

    const auto points = processor.getPointsSnapshot();
    juce::Array<int> hits;
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        if (! points[(size_t) i].enabled)
            continue;
        if (marqueeBounds.contains(pointToScreen(i, points[(size_t) i])))
            hits.add(i);
    }

    if (! marqueeAdditive)
        clearSelection();

    for (auto index : hits)
        selectedPoints.addIfNotAlreadyThere(index);

    if (! hits.isEmpty())
        selectPoint(hits.getFirst());
    else if (! marqueeAdditive)
        selectPoint(-1);
}

int EqGraphComponent::hitTestSoloButton(juce::Point<float> position) const
{
    const auto points = processor.getPointsSnapshot();
    for (int i = PluginProcessor::maxPoints - 1; i >= 0; --i)
    {
        if (! points[(size_t) i].enabled)
            continue;
        if (getSoloButtonBounds(i).contains(position))
            return i;
    }

    return -1;
}

juce::Rectangle<float> EqGraphComponent::getSoloButtonBounds(int pointIndex) const
{
    const auto points = processor.getPointsSnapshot();
    if (! juce::isPositiveAndBelow(pointIndex, PluginProcessor::maxPoints) || ! points[(size_t) pointIndex].enabled)
        return {};

    const auto& point = points[(size_t) pointIndex];
    const auto node = point.dynamicEnabled ? pointToDisplayScreen(pointIndex, point) : pointToScreen(pointIndex, point);
    return juce::Rectangle<float>(18.0f, 18.0f).withCentre({ node.x + 17.0f, node.y - 17.0f });
}

void EqGraphComponent::selectPoint(int index)
{
    if (index >= 0)
        selectedPoints.addIfNotAlreadyThere(index);
    else
        clearSelection();

    selectedPoint = index;
    if (onSelectionChanged)
        onSelectionChanged(index);
    repaint();
}
