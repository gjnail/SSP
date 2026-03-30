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

juce::String formatSigned(float value, int decimals = 2)
{
    const auto prefix = value > 0.0f ? "+" : "";
    return prefix + juce::String(value, decimals);
}

float softScopeScale(float value)
{
    return std::tanh(value * 2.35f);
}

std::array<float, PluginProcessor::numBands + 1> buildBandEdges(const PluginProcessor& processor, const juce::Rectangle<int>& plot)
{
    return {
        (float) plot.getX(),
        (float) juce::jlimit(plot.getX(), plot.getRight(), (int) std::round((float) plot.getX() + frequencyToNormalised(processor.getVisualCrossoverFrequency(0)) * (float) plot.getWidth())),
        (float) juce::jlimit(plot.getX(), plot.getRight(), (int) std::round((float) plot.getX() + frequencyToNormalised(processor.getVisualCrossoverFrequency(1)) * (float) plot.getWidth())),
        (float) juce::jlimit(plot.getX(), plot.getRight(), (int) std::round((float) plot.getX() + frequencyToNormalised(processor.getVisualCrossoverFrequency(2)) * (float) plot.getWidth())),
        (float) plot.getRight()
    };
}

float peakNormalised(float peak)
{
    return juce::jlimit(0.0f, 1.0f, std::sqrt(peak));
}
} // namespace

ImagerGraphComponent::ImagerGraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
    displayCorrelation.fill(1.0f);
    displayWidth.fill(0.0f);
    displayLevelL.fill(0.0f);
    displayLevelR.fill(0.0f);
}

void ImagerGraphComponent::setVisualiserMode(VisualiserMode newMode)
{
    if (visualiserMode == newMode)
        return;

    visualiserMode = newMode;
    repaint();
}

juce::Rectangle<int> ImagerGraphComponent::getScopeBounds() const
{
    auto area = getLocalBounds().reduced(10, 10);
    return area.removeFromLeft((int) std::round((double) area.getWidth() * 0.47)).reduced(4, 2);
}

juce::Rectangle<int> ImagerGraphComponent::getAnalysisBounds() const
{
    auto area = getLocalBounds().reduced(10, 10);
    area.removeFromLeft((int) std::round((double) area.getWidth() * 0.47));
    return area.reduced(4, 2);
}

juce::Rectangle<int> ImagerGraphComponent::getPlotBounds() const
{
    auto analysis = getAnalysisBounds().reduced(18, 16);
    analysis.removeFromTop(42);
    return analysis.removeFromTop(juce::jmax(170, (int) std::round((double) analysis.getHeight() * 0.56)));
}

juce::Rectangle<int> ImagerGraphComponent::getStatsBounds() const
{
    auto analysis = getAnalysisBounds().reduced(18, 16);
    analysis.removeFromTop(42);
    analysis.removeFromTop(juce::jmax(170, (int) std::round((double) analysis.getHeight() * 0.56)));
    analysis.removeFromTop(14);
    return analysis;
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
    if (processor.isAutoLearnActive())
        return -1;

    const auto plot = getPlotBounds();
    const float handleY = (float) plot.getBottom() - 14.0f;

    for (int i = 0; i < PluginProcessor::numCrossovers; ++i)
    {
        const float hx = frequencyToX(processor.getVisualCrossoverFrequency(i));
        if (pos.getDistanceFrom({ hx, handleY }) < hitRadius)
            return i;
    }

    return -1;
}

void ImagerGraphComponent::timerCallback()
{
    float weightedCorrelationSum = 0.0f;
    float correlationWeightSum = 0.0f;

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        const auto& meter = processor.getBandMeter(band);
        displayCorrelation[(size_t) band] = meter.correlation.load();
        displayWidth[(size_t) band] = meter.width.load();
        displayLevelL[(size_t) band] = meter.levelL.load();
        displayLevelR[(size_t) band] = meter.levelR.load();

        const float weight = juce::jmax(0.0001f, displayLevelL[(size_t) band] + displayLevelR[(size_t) band]);
        weightedCorrelationSum += displayCorrelation[(size_t) band] * weight;
        correlationWeightSum += weight;
    }

    overallCorrelation = correlationWeightSum > 0.0f ? weightedCorrelationSum / correlationWeightSum : 1.0f;
    processor.getVectorscopeSnapshot(scopeSnapshot);
    scopeWriteIndex = processor.getVectorscopeWriteIndex();
    repaint();
}

void ImagerGraphComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), ssp::ui::brandCyan());

    const auto scopeBounds = getScopeBounds();
    const auto analysisBounds = getAnalysisBounds();
    const auto plot = getPlotBounds();
    const auto statsBounds = getStatsBounds();

    drawScope(g, scopeBounds);

    g.setColour(juce::Colour(0xff0a1017));
    g.fillRoundedRectangle(analysisBounds.toFloat(), 10.0f);
    g.setColour(ssp::ui::outlineSoft());
    g.drawRoundedRectangle(analysisBounds.toFloat(), 10.0f, 1.0f);
    drawSectionLabel(g, analysisBounds.toFloat().reduced(18.0f, 10.0f).removeFromTop(28.0f),
                     "Spectrum Spread", "Drag split points and watch each band open up.");

    g.setColour(juce::Colour(0xff081018));
    g.fillRoundedRectangle(plot.toFloat(), 8.0f);
    drawBandFills(g, plot);
    drawGrid(g, plot);
    drawBandMeters(g, plot);
    drawCrossoverHandles(g, plot);
    drawBandLabels(g, plot);

    g.setColour(ssp::ui::outlineSoft());
    g.drawRoundedRectangle(plot.toFloat(), 8.0f, 1.0f);

    drawBandStats(g, statsBounds);
}

void ImagerGraphComponent::drawSectionLabel(juce::Graphics& g, juce::Rectangle<float> area,
                                            const juce::String& title, const juce::String& subtitle) const
{
    auto titleArea = area;
    auto subtitleArea = titleArea.removeFromBottom(12.0f);

    g.setColour(ssp::ui::textStrong());
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(title, titleArea.toNearestInt(), juce::Justification::centredLeft, false);

    g.setColour(ssp::ui::textMuted().withAlpha(0.85f));
    g.setFont(juce::Font(10.5f));
    g.drawText(subtitle, subtitleArea.toNearestInt(), juce::Justification::centredLeft, false);
}

void ImagerGraphComponent::drawScope(juce::Graphics& g, const juce::Rectangle<int>& scopeBounds) const
{
    g.setColour(juce::Colour(0xff0a1017));
    g.fillRoundedRectangle(scopeBounds.toFloat(), 10.0f);
    g.setColour(ssp::ui::outlineSoft());
    g.drawRoundedRectangle(scopeBounds.toFloat(), 10.0f, 1.0f);

    auto inner = scopeBounds.reduced(18, 14);
    auto sectionHeader = inner.removeFromTop(28);
    juce::String modeSubtitle = "Goniometer cloud for mono-vs-side balance.";
    if (visualiserMode == VisualiserMode::polarSample)
        modeSubtitle = "Polar spoke view for angular stereo spread and energy.";
    else if (visualiserMode == VisualiserMode::stereoTrail)
        modeSubtitle = "Time trail view for side motion and mono envelope.";
    drawSectionLabel(g, sectionHeader.toFloat(), "Stereo Field", modeSubtitle);

    auto meterBounds = inner.removeFromBottom(30).toFloat();
    inner.removeFromBottom(8);

    auto scopeArea = inner.toFloat().reduced(8.0f);
    const float scopeSize = juce::jmin(scopeArea.getWidth(), scopeArea.getHeight());
    scopeArea = juce::Rectangle<float>(scopeSize, scopeSize).withCentre(scopeArea.getCentre());

    juce::ColourGradient background(juce::Colour(0xff0a121a), scopeArea.getTopLeft(),
                                    juce::Colour(0xff121d28), scopeArea.getBottomLeft(), false);
    background.addColour(0.38, juce::Colour(0xff091119));
    g.setGradientFill(background);
    g.fillRoundedRectangle(scopeArea, 12.0f);

    if (visualiserMode == VisualiserMode::goniometer)
        drawGoniometerScope(g, scopeArea);
    else if (visualiserMode == VisualiserMode::polarSample)
        drawPolarScope(g, scopeArea);
    else
        drawStereoTrailScope(g, scopeArea);

    auto scopeBorder = scopeArea.reduced(6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.drawRoundedRectangle(scopeBorder, 10.0f, 1.0f);

    drawCorrelationMeter(g, meterBounds);
}

void ImagerGraphComponent::drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    auto labelArea = bounds.removeFromLeft(152.0f);
    g.setColour(ssp::ui::textMuted().withAlpha(0.85f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("CORRELATION", labelArea.toNearestInt(), juce::Justification::centredLeft, false);

    juce::String status = "Mono-safe";
    if (overallCorrelation < -0.15f)
        status = "Phase risk";
    else if (overallCorrelation < 0.25f)
        status = "Very wide";

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(status, labelArea.toNearestInt().withTrimmedLeft(92), juce::Justification::centredLeft, false);

    auto meterArea = bounds.reduced(2.0f, 3.0f);
    g.setColour(juce::Colour(0xff09111a));
    g.fillRoundedRectangle(meterArea, 6.0f);

    auto fillArea = meterArea.reduced(2.0f, 8.0f);
    auto dangerArea = fillArea.withWidth(fillArea.getWidth() * 0.42f);
    auto cautionArea = juce::Rectangle<float>(dangerArea.getRight(), fillArea.getY(), fillArea.getWidth() * 0.22f, fillArea.getHeight());
    auto safeArea = juce::Rectangle<float>(cautionArea.getRight(), fillArea.getY(), fillArea.getRight() - cautionArea.getRight(), fillArea.getHeight());

    g.setColour(juce::Colour(0xffd54d56));
    g.fillRoundedRectangle(dangerArea, 4.0f);
    g.setColour(juce::Colour(0xfff0bc4d));
    g.fillRect(cautionArea);
    g.setColour(juce::Colour(0xff36d087));
    g.fillRoundedRectangle(safeArea, 4.0f);

    const float centerX = fillArea.getCentreX();
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.drawVerticalLine((int) centerX, fillArea.getY() - 3.0f, fillArea.getBottom() + 8.0f);

    const float indicatorX = juce::jmap(juce::jlimit(-1.0f, 1.0f, overallCorrelation), -1.0f, 1.0f,
                                        meterArea.getX() + 2.0f, meterArea.getRight() - 2.0f);
    g.setColour(juce::Colour(0xfff8f3dd));
    g.fillRoundedRectangle(indicatorX - 3.0f, meterArea.getY() + 3.0f, 6.0f, meterArea.getHeight() - 6.0f, 3.0f);

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        const float bandX = juce::jmap(juce::jlimit(-1.0f, 1.0f, displayCorrelation[(size_t) band]), -1.0f, 1.0f,
                                       fillArea.getX(), fillArea.getRight());
        g.setColour(getBandColour(band).withAlpha(0.92f));
        g.fillEllipse(bandX - 3.5f, fillArea.getBottom() + 3.0f, 7.0f, 7.0f);
    }

    g.setColour(ssp::ui::textMuted());
    g.setFont(juce::Font(9.5f));
    g.drawText("ANTI", (int) meterArea.getX(), (int) meterArea.getBottom() - 4, 32, 12, juce::Justification::left, false);
    g.drawText("WIDE", (int) centerX - 22, (int) meterArea.getBottom() - 4, 44, 12, juce::Justification::centred, false);
    g.drawText("MONO", (int) meterArea.getRight() - 40, (int) meterArea.getBottom() - 4, 40, 12, juce::Justification::right, false);
    g.drawText(formatSigned(overallCorrelation), (int) meterArea.getRight() - 68, (int) meterArea.getY() - 2, 68, 14, juce::Justification::right, false);
}

void ImagerGraphComponent::drawGoniometerScope(juce::Graphics& g, juce::Rectangle<float> scopeArea) const
{
    const auto centre = scopeArea.getCentre();
    const float radius = scopeArea.getWidth() * 0.5f - 18.0f;

    juce::Path diamond;
    diamond.startNewSubPath(centre.x, centre.y - radius);
    diamond.lineTo(centre.x + radius, centre.y);
    diamond.lineTo(centre.x, centre.y + radius);
    diamond.lineTo(centre.x - radius, centre.y);
    diamond.closeSubPath();

    g.setColour(juce::Colour(0xff172636));
    g.strokePath(diamond, juce::PathStrokeType(1.0f));
    g.setColour(juce::Colour(0xff22364a).withAlpha(0.65f));
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
    g.drawEllipse(centre.x - radius * 0.55f, centre.y - radius * 0.55f, radius * 1.1f, radius * 1.1f, 1.0f);

    g.setColour(juce::Colour(0xff24384d).withAlpha(0.8f));
    g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);
    g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);
    g.drawLine(centre.x - radius * 0.75f, centre.y - radius * 0.75f, centre.x + radius * 0.75f, centre.y + radius * 0.75f, 0.8f);
    g.drawLine(centre.x - radius * 0.75f, centre.y + radius * 0.75f, centre.x + radius * 0.75f, centre.y - radius * 0.75f, 0.8f);

    g.setColour(ssp::ui::textMuted().withAlpha(0.70f));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("MONO", juce::Rectangle<int>((int) centre.x - 28, (int) (scopeArea.getY() + 6.0f), 56, 14), juce::Justification::centred, false);
    g.drawText("L", juce::Rectangle<int>((int) scopeArea.getX() + 4, (int) centre.y - 8, 16, 16), juce::Justification::centred, false);
    g.drawText("R", juce::Rectangle<int>((int) scopeArea.getRight() - 20, (int) centre.y - 8, 16, 16), juce::Justification::centred, false);
    g.drawText("SIDE", juce::Rectangle<int>((int) centre.x - 24, (int) scopeArea.getBottom() - 18, 48, 12), juce::Justification::centred, false);

    for (int i = 0; i < PluginProcessor::vectorscopeSize; ++i)
    {
        const int index = (scopeWriteIndex + i) % PluginProcessor::vectorscopeSize;
        const auto& point = scopeSnapshot[(size_t) index];
        const float x = centre.x + softScopeScale(point.side) * radius * 0.96f;
        const float y = centre.y - softScopeScale(point.mid) * radius * 0.96f;
        const float age = (float) i / (float) PluginProcessor::vectorscopeSize;
        const float alpha = age * age * 0.42f;
        const float dotSize = juce::jmap(age, 0.0f, 1.0f, 1.2f, 2.4f);

        juce::Colour colour = point.band >= 0 ? getBandColour(point.band) : ssp::ui::brandCyan();
        g.setColour(colour.withAlpha(alpha));
        g.fillEllipse(x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
    }
}

void ImagerGraphComponent::drawPolarScope(juce::Graphics& g, juce::Rectangle<float> scopeArea) const
{
    const auto centre = scopeArea.getCentre();
    const float radius = scopeArea.getWidth() * 0.5f - 18.0f;

    g.setColour(juce::Colour(0xff203244));
    for (int ring = 1; ring <= 4; ++ring)
    {
        const float ringRadius = radius * ((float) ring / 4.0f);
        g.drawEllipse(centre.x - ringRadius, centre.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, ring == 4 ? 1.0f : 0.8f);
    }

    for (int spoke = 0; spoke < 8; ++spoke)
    {
        const float angle = juce::MathConstants<float>::twoPi * ((float) spoke / 8.0f) - juce::MathConstants<float>::halfPi;
        g.drawLine(centre.x, centre.y,
                   centre.x + std::cos(angle) * radius,
                   centre.y + std::sin(angle) * radius, 0.8f);
    }

    g.setColour(ssp::ui::textMuted().withAlpha(0.72f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("NARROW", juce::Rectangle<int>((int) centre.x - 32, (int) scopeArea.getY() + 6, 64, 14), juce::Justification::centred, false);
    g.drawText("WIDE", juce::Rectangle<int>((int) centre.x - 24, (int) scopeArea.getBottom() - 18, 48, 12), juce::Justification::centred, false);

    for (int i = 0; i < PluginProcessor::vectorscopeSize; i += 2)
    {
        const int index = (scopeWriteIndex + i) % PluginProcessor::vectorscopeSize;
        const auto& point = scopeSnapshot[(size_t) index];
        const float magnitude = juce::jlimit(0.0f, 1.0f, std::sqrt(point.mid * point.mid + point.side * point.side) * 1.15f);
        const float angle = std::atan2(point.side, point.mid) - juce::MathConstants<float>::halfPi;
        const float length = std::tanh(magnitude * 1.5f) * radius;
        const float age = (float) i / (float) PluginProcessor::vectorscopeSize;

        juce::Colour colour = point.band >= 0 ? getBandColour(point.band) : ssp::ui::brandCyan();
        g.setColour(colour.withAlpha(age * 0.33f));
        g.drawLine(centre.x, centre.y,
                   centre.x + std::cos(angle) * length,
                   centre.y + std::sin(angle) * length,
                   1.0f + age * 0.6f);
    }
}

void ImagerGraphComponent::drawStereoTrailScope(juce::Graphics& g, juce::Rectangle<float> scopeArea) const
{
    const auto centerY = scopeArea.getCentreY();
    const auto maxHeight = scopeArea.getHeight() * 0.40f;

    g.setColour(juce::Colour(0xff213244));
    g.drawHorizontalLine((int) centerY, scopeArea.getX(), scopeArea.getRight());
    g.drawHorizontalLine((int) (centerY - maxHeight), scopeArea.getX(), scopeArea.getRight());
    g.drawHorizontalLine((int) (centerY + maxHeight), scopeArea.getX(), scopeArea.getRight());
    g.drawVerticalLine((int) scopeArea.getCentreX(), scopeArea.getY(), scopeArea.getBottom());

    g.setColour(ssp::ui::textMuted().withAlpha(0.72f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("LEFT", juce::Rectangle<int>((int) scopeArea.getX() + 6, (int) scopeArea.getY() + 6, 36, 12), juce::Justification::left, false);
    g.drawText("RIGHT", juce::Rectangle<int>((int) scopeArea.getRight() - 42, (int) scopeArea.getY() + 6, 42, 12), juce::Justification::right, false);
    g.drawText("TIME", juce::Rectangle<int>((int) scopeArea.getCentreX() - 18, (int) scopeArea.getBottom() - 18, 36, 12), juce::Justification::centred, false);

    juce::Path sideTrail;
    juce::Path monoEnvelope;

    for (int i = 0; i < PluginProcessor::vectorscopeSize; ++i)
    {
        const int index = (scopeWriteIndex + i) % PluginProcessor::vectorscopeSize;
        const auto& point = scopeSnapshot[(size_t) index];
        const float age = (float) i / (float) (PluginProcessor::vectorscopeSize - 1);
        const float x = scopeArea.getX() + age * scopeArea.getWidth();
        const float sideY = centerY - softScopeScale(point.side) * maxHeight;
        const float monoMagnitude = std::abs(softScopeScale(point.mid)) * maxHeight;

        if (i == 0)
        {
            sideTrail.startNewSubPath(x, sideY);
            monoEnvelope.startNewSubPath(x, centerY - monoMagnitude);
        }
        else
        {
            sideTrail.lineTo(x, sideY);
            monoEnvelope.lineTo(x, centerY - monoMagnitude);
        }
    }

    for (int i = PluginProcessor::vectorscopeSize - 1; i >= 0; --i)
    {
        const int index = (scopeWriteIndex + i) % PluginProcessor::vectorscopeSize;
        const auto& point = scopeSnapshot[(size_t) index];
        const float age = (float) i / (float) (PluginProcessor::vectorscopeSize - 1);
        const float x = scopeArea.getX() + age * scopeArea.getWidth();
        const float monoMagnitude = std::abs(softScopeScale(point.mid)) * maxHeight;
        monoEnvelope.lineTo(x, centerY + monoMagnitude);
    }
    monoEnvelope.closeSubPath();

    g.setColour(ssp::ui::brandGold().withAlpha(0.10f));
    g.fillPath(monoEnvelope);
    g.setColour(ssp::ui::brandCyan().withAlpha(0.88f));
    g.strokePath(sideTrail, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ImagerGraphComponent::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const float frequencyMarkers[] = { 30.0f, 60.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 16000.0f };
    const float horizontalMarkers[] = { 0.25f, 0.5f, 0.75f };

    g.setColour(juce::Colour(0xff182432));
    for (auto marker : frequencyMarkers)
    {
        const float x = frequencyToX(marker);
        if (x > plot.getX() && x < plot.getRight())
            g.drawVerticalLine((int) x, (float) plot.getY(), (float) plot.getBottom());
    }

    for (auto marker : horizontalMarkers)
    {
        const float y = juce::jmap(marker, 0.0f, 1.0f, (float) plot.getBottom(), (float) plot.getY());
        g.drawHorizontalLine((int) y, (float) plot.getX(), (float) plot.getRight());
    }

    g.setColour(ssp::ui::textMuted().withAlpha(0.65f));
    g.setFont(juce::Font(9.5f));
    for (auto marker : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = frequencyToX(marker);
        g.drawText(formatFrequencyLabel(marker),
                   (int) x - 24, plot.getBottom() + 4, 48, 12,
                   juce::Justification::centred, false);
    }
}

void ImagerGraphComponent::drawBandFills(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const auto bandEdges = buildBandEdges(processor, plot);

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        auto bandBounds = juce::Rectangle<float>(bandEdges[(size_t) band], (float) plot.getY(),
                                                 bandEdges[(size_t) band + 1] - bandEdges[(size_t) band],
                                                 (float) plot.getHeight());
        if (bandBounds.getWidth() <= 1.0f)
            continue;

        auto colour = getBandColour(band);
        juce::ColourGradient fill(colour.withAlpha(0.16f), bandBounds.getTopLeft(),
                                  colour.withAlpha(0.04f), bandBounds.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRect(bandBounds);

        juce::ColourGradient glow(colour.withAlpha(0.18f), { bandBounds.getCentreX(), bandBounds.getY() + 18.0f },
                                  juce::Colours::transparentBlack, { bandBounds.getCentreX(), bandBounds.getBottom() - 28.0f }, false);
        g.setGradientFill(glow);
        g.fillEllipse(bandBounds.reduced(bandBounds.getWidth() * 0.18f, 18.0f));
    }
}

void ImagerGraphComponent::drawBandMeters(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const auto bandEdges = buildBandEdges(processor, plot);
    juce::Path widthTrace;
    bool startedTrace = false;

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        const float left = bandEdges[(size_t) band];
        const float right = bandEdges[(size_t) band + 1];
        const float centreX = (left + right) * 0.5f;
        const float targetWidthNorm = processor.getBandWidth(band) / 200.0f;
        const float widthY = juce::jmap(targetWidthNorm, 0.0f, 1.0f, (float) plot.getBottom() - 36.0f, (float) plot.getY() + 40.0f);
        const auto colour = getBandColour(band);

        if (! startedTrace)
        {
            widthTrace.startNewSubPath(centreX, widthY);
            startedTrace = true;
        }
        else
        {
            widthTrace.lineTo(centreX, widthY);
        }

        const float levelLNorm = peakNormalised(displayLevelL[(size_t) band]);
        const float levelRNorm = peakNormalised(displayLevelR[(size_t) band]);
        const float barBottom = (float) plot.getBottom() - 18.0f;
        const float maxBarHeight = 50.0f;
        const float barWidth = juce::jmin(12.0f, (right - left) * 0.12f);

        juce::Rectangle<float> leftBar(centreX - barWidth - 4.0f, barBottom - maxBarHeight * levelLNorm, barWidth, maxBarHeight * levelLNorm);
        juce::Rectangle<float> rightBar(centreX + 4.0f, barBottom - maxBarHeight * levelRNorm, barWidth, maxBarHeight * levelRNorm);

        g.setColour(colour.withAlpha(0.22f));
        g.fillRoundedRectangle(centreX - barWidth - 4.0f, barBottom - maxBarHeight, barWidth, maxBarHeight, 3.0f);
        g.fillRoundedRectangle(centreX + 4.0f, barBottom - maxBarHeight, barWidth, maxBarHeight, 3.0f);
        g.setColour(colour.withAlpha(0.82f));
        g.fillRoundedRectangle(leftBar, 3.0f);
        g.fillRoundedRectangle(rightBar, 3.0f);

        g.setColour(colour.withAlpha(0.95f));
        g.fillEllipse(centreX - 5.5f, widthY - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colour(0xfff6f4eb).withAlpha(0.85f));
        g.drawEllipse(centreX - 5.5f, widthY - 5.5f, 11.0f, 11.0f, 1.0f);

        auto corrLabel = juce::Rectangle<int>((int) (centreX - 26.0f), plot.getY() + 10, 52, 14);
        g.setColour(colour.withAlpha(0.9f));
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(formatSigned(displayCorrelation[(size_t) band]), corrLabel, juce::Justification::centred, false);
    }

    g.setColour(ssp::ui::brandCyan().withAlpha(0.22f));
    g.strokePath(widthTrace, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ssp::ui::brandCyan().withAlpha(0.92f));
    g.strokePath(widthTrace, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ImagerGraphComponent::drawCrossoverHandles(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const float handleY = (float) plot.getBottom() - 14.0f;

    for (int index = 0; index < PluginProcessor::numCrossovers; ++index)
    {
        const float frequency = processor.getVisualCrossoverFrequency(index);
        const float x = frequencyToX(frequency);
        const bool isHovered = hoverHandle == index;
        const bool isDragging = dragHandle == index;

        g.setColour(ssp::ui::textMuted().withAlpha(isDragging ? 0.85f : (isHovered ? 0.60f : 0.32f)));
        g.drawVerticalLine((int) x, (float) plot.getY(), (float) plot.getBottom());

        const float radius = isDragging ? handleRadius * 1.25f : (isHovered ? handleRadius * 1.1f : handleRadius);
        g.setColour(ssp::ui::brandCyan().withAlpha(isDragging ? 1.0f : 0.84f));
        g.fillEllipse(x - radius, handleY - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xfff6f4eb).withAlpha(0.92f));
        g.drawEllipse(x - radius, handleY - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        g.setColour(ssp::ui::textStrong());
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(formatFrequencyLabel(frequency), (int) x - 30, (int) (handleY - radius - 18.0f), 60, 14, juce::Justification::centred, false);
    }
}

void ImagerGraphComponent::drawBandLabels(juce::Graphics& g, const juce::Rectangle<int>& plot) const
{
    const auto bandEdges = buildBandEdges(processor, plot);
    const juce::String bandNames[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        const float left = bandEdges[(size_t) band];
        const float right = bandEdges[(size_t) band + 1];
        if (right - left < 32.0f)
            continue;

        g.setColour(getBandColour(band).withAlpha(0.74f));
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(bandNames[band], (int) left + 6, plot.getBottom() - 18, (int) (right - left) - 12, 14,
                   juce::Justification::centred, false);
    }
}

void ImagerGraphComponent::drawBandStats(juce::Graphics& g, const juce::Rectangle<int>& statsBounds) const
{
    if (statsBounds.isEmpty())
        return;

    const juce::String bandNames[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    const int cardGap = 10;
    const int cardWidth = (statsBounds.getWidth() - cardGap * (PluginProcessor::numBands - 1)) / PluginProcessor::numBands;

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        auto card = juce::Rectangle<float>((float) (statsBounds.getX() + band * (cardWidth + cardGap)),
                                           (float) statsBounds.getY(), (float) cardWidth, (float) statsBounds.getHeight());
        auto colour = getBandColour(band);

        juce::ColourGradient fill(juce::Colour(0xff111a24), card.getTopLeft(),
                                  juce::Colour(0xff0b1219), card.getBottomLeft(), false);
        fill.addColour(0.0, colour.withAlpha(0.12f));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(ssp::ui::outlineSoft());
        g.drawRoundedRectangle(card, 8.0f, 1.0f);

        auto area = card.reduced(12.0f, 10.0f).toNearestInt();
        auto titleArea = area.removeFromTop(18);
        auto statusArea = area.removeFromTop(14);
        auto statsArea = area.removeFromTop(42);
        area.removeFromTop(6);
        auto meterArea = area.removeFromTop(38);

        g.setColour(colour);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(bandNames[band], titleArea, juce::Justification::centredLeft, false);

        juce::String status = processor.isBandSoloed(band) ? "SOLO" : (processor.isBandMuted(band) ? "MUTED" : "ACTIVE");
        g.setColour(processor.isBandMuted(band) ? juce::Colour(0xffe07171)
                                                : (processor.isBandSoloed(band) ? juce::Colour(0xffffd66d) : ssp::ui::textMuted()));
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(status, statusArea, juce::Justification::centredLeft, false);

        g.setColour(ssp::ui::textStrong());
        g.setFont(juce::Font(12.5f, juce::Font::bold));
        g.drawText("W " + juce::String((int) std::round(processor.getBandWidth(band))) + "%", statsArea.removeFromTop(14), juce::Justification::centredLeft, false);
        g.drawText("C " + formatSigned(displayCorrelation[(size_t) band]), statsArea.removeFromTop(14), juce::Justification::centredLeft, false);
        g.drawText("P " + formatSigned(processor.getBandPan(band), 0), statsArea.removeFromTop(14), juce::Justification::centredLeft, false);

        const float leftNorm = peakNormalised(displayLevelL[(size_t) band]);
        const float rightNorm = peakNormalised(displayLevelR[(size_t) band]);
        auto leftTrack = meterArea.removeFromLeft(meterArea.getWidth() / 2).reduced(2, 2).toFloat();
        auto rightTrack = meterArea.reduced(2, 2).toFloat();

        g.setColour(juce::Colour(0xff081018));
        g.fillRoundedRectangle(leftTrack, 4.0f);
        g.fillRoundedRectangle(rightTrack, 4.0f);

        auto leftFill = leftTrack;
        leftFill.removeFromTop(leftTrack.getHeight() * (1.0f - leftNorm));
        auto rightFill = rightTrack;
        rightFill.removeFromTop(rightTrack.getHeight() * (1.0f - rightNorm));

        g.setColour(colour.withAlpha(0.85f));
        g.fillRoundedRectangle(leftFill, 4.0f);
        g.fillRoundedRectangle(rightFill, 4.0f);

        g.setColour(ssp::ui::textMuted().withAlpha(0.85f));
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("L", leftTrack.toNearestInt().withTrimmedTop((int) std::round(leftTrack.getHeight() - 12.0f)), juce::Justification::centred, false);
        g.drawText("R", rightTrack.toNearestInt().withTrimmedTop((int) std::round(rightTrack.getHeight() - 12.0f)), juce::Justification::centred, false);
    }
}

void ImagerGraphComponent::resized()
{
}

void ImagerGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    if (processor.isAutoLearnActive())
        return;

    dragHandle = findHandleAt(event.position);
}

void ImagerGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (processor.isAutoLearnActive() || dragHandle < 0)
        return;

    float newFreq = xToFrequency(event.position.x);
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
    if (auto* parameter = processor.apvts.getParameter(ids[dragHandle]))
        parameter->setValueNotifyingHost(parameter->getNormalisableRange().convertTo0to1(newFreq));
}

void ImagerGraphComponent::mouseUp(const juce::MouseEvent&)
{
    dragHandle = -1;
}

void ImagerGraphComponent::mouseMove(const juce::MouseEvent& event)
{
    if (processor.isAutoLearnActive())
    {
        if (hoverHandle >= 0)
        {
            hoverHandle = -1;
            repaint();
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

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
