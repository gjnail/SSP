#include "SpectrumAnalyzerComponent.h"
#include "SpectrumVectorUI.h"

namespace
{
constexpr float topDb = 12.0f;

juce::Colour pickRowColour(const spectrumui::Palette& palette, float intensity)
{
    const auto t = juce::jlimit(0.0f, 1.0f, intensity);
    return palette.accentPrimary.interpolatedWith(palette.accentWarm, t * 0.75f)
        .interpolatedWith(palette.positive, juce::jlimit(0.0f, 1.0f, (t - 0.5f) * 1.6f));
}

float frequencyAtNormalisedPosition(float normalised, double sampleRate)
{
    const float minFrequency = 20.0f;
    const float maxFrequency = (float) juce::jmin(20000.0, sampleRate * 0.48);
    const float minLog = std::log10(minFrequency);
    const float maxLog = std::log10(maxFrequency);
    return std::pow(10.0f, juce::jmap(normalised, 0.0f, 1.0f, minLog, maxLog));
}

void drawTrace(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::String formatNoteWithCents(float frequency)
{
    if (frequency <= 0.0f)
        return "--";

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    const int nearest = juce::roundToInt(midiNote);
    const int octave = nearest / 12 - 1;
    const int cents = juce::roundToInt((midiNote - (float) nearest) * 100.0f);
    const juce::String centsText = cents > 0 ? "+" + juce::String(cents) : juce::String(cents);
    return juce::String(noteNames[(nearest + 1200) % 12]) + juce::String(octave) + " " + centsText + "c";
}
} // namespace

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent(PluginProcessor& p)
    : processor(p)
{
    snapshot = processor.getAnalyzerSnapshotCopy();
    startTimerHz(30);
}

SpectrumAnalyzerComponent::~SpectrumAnalyzerComponent()
{
    stopTimer();
}

void SpectrumAnalyzerComponent::timerCallback()
{
    snapshot = processor.getAnalyzerSnapshotCopy();
    waterfallFramePending = ! snapshot.freezeActive;
    repaint();
}

void SpectrumAnalyzerComponent::resized()
{
}

void SpectrumAnalyzerComponent::mouseMove(const juce::MouseEvent& event)
{
    if (lastPlotArea.contains(event.position))
    {
        const float x = juce::jlimit(0.0f, 1.0f, (event.position.x - lastPlotArea.getX()) / juce::jmax(1.0f, lastPlotArea.getWidth()));
        hoverIndex = juce::jlimit(0, PluginProcessor::displayPointCount - 1,
                                  juce::roundToInt(x * (float) (PluginProcessor::displayPointCount - 1)));
    }
    else
    {
        hoverIndex = -1;
    }

    repaint();
}

void SpectrumAnalyzerComponent::mouseExit(const juce::MouseEvent&)
{
    hoverIndex = -1;
    repaint();
}

void SpectrumAnalyzerComponent::recallOrCaptureReference(ReferenceTrace& target, ReferenceTrace& other, const juce::ModifierKeys& mods)
{
    const bool recapture = mods.isShiftDown() || mods.isAltDown();
    const bool keepOtherVisible = mods.isCommandDown() || mods.isCtrlDown();

    if (! target.captured || recapture)
    {
        target.values = snapshot.primary;
        target.captured = true;
    }

    target.visible = true;
    if (! keepOtherVisible)
        other.visible = false;

    repaint();
}

void SpectrumAnalyzerComponent::handleReferenceA(const juce::ModifierKeys& mods)
{
    recallOrCaptureReference(referenceA, referenceB, mods);
}

void SpectrumAnalyzerComponent::handleReferenceB(const juce::ModifierKeys& mods)
{
    recallOrCaptureReference(referenceB, referenceA, mods);
}

void SpectrumAnalyzerComponent::clearReferences()
{
    referenceA.captured = false;
    referenceA.visible = false;
    referenceB.captured = false;
    referenceB.visible = false;
    repaint();
}

float SpectrumAnalyzerComponent::mapDbToY(float db, juce::Rectangle<float> plot, float rangeDb)
{
    const float bottomDb = topDb - juce::jmax(24.0f, rangeDb);
    const float proportion = juce::jmap(juce::jlimit(bottomDb, topDb, db), topDb, bottomDb, 0.0f, 1.0f);
    return plot.getY() + plot.getHeight() * proportion;
}

juce::Path SpectrumAnalyzerComponent::makeTracePath(const std::array<float, PluginProcessor::displayPointCount>& values,
                                                    juce::Rectangle<float> plot,
                                                    float rangeDb)
{
    juce::Path path;
    bool started = false;

    for (int i = 0; i < PluginProcessor::displayPointCount; ++i)
    {
        const float x = juce::jmap((float) i,
                                   0.0f,
                                   (float) (PluginProcessor::displayPointCount - 1),
                                   plot.getX(),
                                   plot.getRight());
        const float y = mapDbToY(values[(size_t) i], plot, rangeDb);

        if (! started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    return path;
}

juce::String SpectrumAnalyzerComponent::formatFrequency(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 1 : 2) + " kHz";

    return juce::String(frequency, frequency >= 100.0f ? 0 : 1) + " Hz";
}

juce::String SpectrumAnalyzerComponent::formatNote(float frequency)
{
    if (frequency <= 0.0f)
        return "--";

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    const int rounded = juce::roundToInt(midiNote);
    const int octave = rounded / 12 - 1;
    return juce::String(noteNames[(rounded + 1200) % 12]) + juce::String(octave);
}

void SpectrumAnalyzerComponent::ensureWaterfallImage(int width, int height)
{
    width = juce::jmax(1, width);
    height = juce::jmax(1, height);

    if (waterfallImage.isNull() || waterfallImage.getWidth() != width || waterfallImage.getHeight() != height)
        waterfallImage = juce::Image(juce::Image::ARGB, width, height, true);
}

void SpectrumAnalyzerComponent::pushWaterfallRow()
{
    if (waterfallImage.isNull())
        return;

    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    juce::Image next(juce::Image::ARGB, waterfallImage.getWidth(), waterfallImage.getHeight(), true);
    juce::Graphics g(next);
    g.drawImageAt(waterfallImage, 0, 1);

    const float rangeDb = juce::jmax(24.0f, snapshot.rangeDb);
    const float bottomDb = topDb - rangeDb;
    for (int x = 0; x < next.getWidth(); ++x)
    {
        const int index = juce::jlimit(0,
                                       PluginProcessor::displayPointCount - 1,
                                       juce::roundToInt((float) x / (float) juce::jmax(1, next.getWidth() - 1)
                                                        * (float) (PluginProcessor::displayPointCount - 1)));
        const float db = snapshot.primary[(size_t) index];
        const float intensity = juce::jlimit(0.0f, 1.0f, (db - bottomDb) / juce::jmax(1.0f, topDb - bottomDb));
        g.setColour(pickRowColour(palette, intensity).withAlpha(0.18f + intensity * 0.82f));
        g.fillRect(x, 0, 1, 1);
    }

    waterfallImage = next;
}

void SpectrumAnalyzerComponent::drawSidebar(juce::Graphics& g, juce::Rectangle<float> area, const juce::Colour& accent)
{
    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.5f));
    g.drawText("READOUT", area.removeFromTop(18.0f).toNearestInt(), juce::Justification::left);

    auto dominant = area.removeFromTop(84.0f);
    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.0f));
    g.drawText("Dominant", dominant.removeFromTop(16.0f).toNearestInt(), juce::Justification::left);
    g.setColour(palette.textStrong);
    g.setFont(spectrumui::titleFont(18.0f));
    g.drawFittedText(formatFrequency(snapshot.dominantFrequencyHz), dominant.removeFromTop(26.0f).toNearestInt(), juce::Justification::left, 1);
    g.setColour(accent);
    g.setFont(spectrumui::bodyFont(11.0f));
    g.drawText(formatNoteWithCents(snapshot.dominantFrequencyHz) + "   " + juce::String(snapshot.dominantDb, 1) + " dB",
               dominant.removeFromTop(18.0f).toNearestInt(),
               juce::Justification::left);

    area.removeFromTop(8.0f);

    auto levels = area.removeFromTop(118.0f);
    auto leftCard = levels.removeFromTop(34.0f);
    auto rightCard = levels.removeFromTop(34.0f);
    auto rmsCard = levels.removeFromTop(42.0f);

    const auto drawMeterText = [&](juce::Rectangle<float> row, juce::String label, float a, float b, juce::Colour colourA, juce::Colour colourB)
    {
        g.setColour(palette.textMuted);
        g.setFont(spectrumui::smallCapsFont(10.0f));
        g.drawText(label, row.removeFromLeft(40.0f).toNearestInt(), juce::Justification::left);
        g.setColour(colourA);
        g.setFont(spectrumui::bodyFont(11.0f));
        g.drawText("L " + juce::String(a, 1), row.removeFromLeft((int) row.getWidth() / 2).toNearestInt(), juce::Justification::left);
        g.setColour(colourB);
        g.drawText("R " + juce::String(b, 1), row.toNearestInt(), juce::Justification::left);
    };

    drawMeterText(leftCard, "Peak", snapshot.peakLeftDb, snapshot.peakRightDb, palette.accentPrimary, palette.accentSecondary);
    drawMeterText(rightCard, "RMS", snapshot.rmsLeftDb, snapshot.rmsRightDb, palette.accentPrimary.withAlpha(0.8f), palette.accentSecondary.withAlpha(0.8f));

    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.0f));
    g.drawText("Correlation", rmsCard.removeFromTop(16.0f).toNearestInt(), juce::Justification::left);

    auto correlationBar = rmsCard.removeFromTop(16.0f).reduced(0.0f, 1.0f);
    g.setColour(palette.backgroundSoft.darker(0.18f));
    g.fillRoundedRectangle(correlationBar, 7.0f);
    g.setColour(palette.outline);
    g.drawRoundedRectangle(correlationBar.reduced(0.5f), 7.0f, 1.0f);

    const float correlationNorm = juce::jmap(snapshot.correlation, -1.0f, 1.0f, 0.0f, 1.0f);
    auto active = correlationBar.reduced(2.0f);
    active.setWidth(active.getWidth() * correlationNorm);
    juce::ColourGradient correlationFill(palette.danger, active.getTopLeft(), palette.positive, { active.getRight(), active.getY() }, false);
    g.setGradientFill(correlationFill);
    g.fillRoundedRectangle(active, 5.0f);

    g.setColour(palette.textStrong);
    g.setFont(spectrumui::bodyFont(10.5f));
    g.drawText(juce::String(snapshot.correlation, 2), rmsCard.removeFromTop(18.0f).toNearestInt(), juce::Justification::left);

    area.removeFromTop(8.0f);

    auto badges = area.removeFromTop(56.0f);
    auto clipBadge = badges.removeFromTop(24.0f);
    spectrumui::drawBadge(g,
                          clipBadge,
                          palette,
                          snapshot.clipping ? "CLIP DETECTED" : "HEADROOM OK",
                          snapshot.clipping ? palette.danger.withAlpha(0.28f) : palette.positive.withAlpha(0.22f),
                          snapshot.clipping ? palette.danger.brighter(0.08f) : palette.positive.brighter(0.08f));

    auto scBadge = badges.removeFromTop(24.0f);
    spectrumui::drawBadge(g,
                          scBadge,
                          palette,
                          snapshot.sidechainConnected ? "SIDECHAIN READY" : "SIDECHAIN IDLE",
                          snapshot.sidechainConnected ? palette.accentSecondary.withAlpha(0.25f) : palette.backgroundSoft.withAlpha(0.6f),
                          snapshot.sidechainConnected ? palette.textStrong : palette.textMuted);
}

void SpectrumAnalyzerComponent::drawWaveformMeter(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    g.setColour(palette.backgroundSoft.darker(0.25f));
    g.fillRoundedRectangle(area, 14.0f);
    g.setColour(palette.outlineSoft.withAlpha(0.75f));
    g.drawRoundedRectangle(area.reduced(0.5f), 14.0f, 1.0f);

    auto inner = area.reduced(14.0f, 12.0f);
    auto header = inner.removeFromTop(18.0f);
    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.0f));
    g.drawText("Waveform", header.removeFromLeft(92.0f).toNearestInt(), juce::Justification::left);

    g.setColour(palette.textStrong.withAlpha(0.84f));
    g.setFont(spectrumui::bodyFont(10.0f));
    g.drawText("Span " + juce::String(snapshot.waveformSpanSeconds, 1) + " s", header.removeFromLeft(78.0f).toNearestInt(), juce::Justification::left);
    g.setColour(palette.accentPrimary);
    g.drawText("Short " + juce::String(snapshot.shortLoudnessDb, 1), header.removeFromLeft(88.0f).toNearestInt(), juce::Justification::left);
    g.setColour(palette.accentWarm);
    g.drawText("Crest " + juce::String(snapshot.crestDb, 1), header.toNearestInt(), juce::Justification::left);

    auto plot = inner;
    const float centreY = plot.getCentreY();
    g.setColour(palette.outlineSoft.withAlpha(0.32f));
    g.drawHorizontalLine((int) std::round(centreY), plot.getX(), plot.getRight());

    for (int i = 1; i < 8; ++i)
    {
        const float x = juce::jmap((float) i, 0.0f, 8.0f, plot.getX(), plot.getRight());
        g.setColour(palette.outlineSoft.withAlpha(i % 2 == 0 ? 0.15f : 0.09f));
        g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
    }

    juce::Path waveform;
    const float amplitude = plot.getHeight() * 0.42f;

    for (int i = 0; i < PluginProcessor::waveformPointCount; ++i)
    {
        const float x = juce::jmap((float) i,
                                   0.0f,
                                   (float) (PluginProcessor::waveformPointCount - 1),
                                   plot.getX(),
                                   plot.getRight());
        const float y = centreY - snapshot.waveformMax[(size_t) i] * amplitude;
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    for (int i = PluginProcessor::waveformPointCount - 1; i >= 0; --i)
    {
        const float x = juce::jmap((float) i,
                                   0.0f,
                                   (float) (PluginProcessor::waveformPointCount - 1),
                                   plot.getX(),
                                   plot.getRight());
        const float y = centreY - snapshot.waveformMin[(size_t) i] * amplitude;
        waveform.lineTo(x, y);
    }

    waveform.closeSubPath();

    juce::ColourGradient fill(palette.accentSecondary.withAlpha(0.24f), plot.getTopLeft(),
                              palette.accentPrimary.withAlpha(0.10f), plot.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillPath(waveform);

    juce::Path topEdge;
    for (int i = 0; i < PluginProcessor::waveformPointCount; ++i)
    {
        const float x = juce::jmap((float) i,
                                   0.0f,
                                   (float) (PluginProcessor::waveformPointCount - 1),
                                   plot.getX(),
                                   plot.getRight());
        const float y = centreY - snapshot.waveformMax[(size_t) i] * amplitude;
        if (i == 0)
            topEdge.startNewSubPath(x, y);
        else
            topEdge.lineTo(x, y);
    }
    drawTrace(g, topEdge, palette.accentPrimary.withAlpha(0.95f), 1.6f);

    juce::Path bottomEdge;
    for (int i = 0; i < PluginProcessor::waveformPointCount; ++i)
    {
        const float x = juce::jmap((float) i,
                                   0.0f,
                                   (float) (PluginProcessor::waveformPointCount - 1),
                                   plot.getX(),
                                   plot.getRight());
        const float y = centreY - snapshot.waveformMin[(size_t) i] * amplitude;
        if (i == 0)
            bottomEdge.startNewSubPath(x, y);
        else
            bottomEdge.lineTo(x, y);
    }
    drawTrace(g, bottomEdge, palette.accentSecondary.withAlpha(0.75f), 1.1f);
}

void SpectrumAnalyzerComponent::drawVectorscopeMeter(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    g.setColour(palette.backgroundSoft.darker(0.25f));
    g.fillRoundedRectangle(area, 14.0f);
    g.setColour(palette.outlineSoft.withAlpha(0.75f));
    g.drawRoundedRectangle(area.reduced(0.5f), 14.0f, 1.0f);

    auto inner = area.reduced(14.0f, 12.0f);
    auto header = inner.removeFromTop(18.0f);
    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.0f));
    g.drawText("Vectorscope", header.removeFromLeft(92.0f).toNearestInt(), juce::Justification::left);
    g.setColour(palette.textStrong.withAlpha(0.86f));
    g.setFont(spectrumui::bodyFont(10.0f));
    g.drawText("Corr " + juce::String(snapshot.correlation, 2), header.toNearestInt(), juce::Justification::right);

    auto footer = inner.removeFromBottom(28.0f);
    auto scope = inner;
    const float size = juce::jmin(scope.getWidth(), scope.getHeight());
    scope = juce::Rectangle<float>(size, size).withCentre(scope.getCentre());

    const auto centre = scope.getCentre();
    const float radius = scope.getWidth() * 0.46f;

    g.setColour(palette.outlineSoft.withAlpha(0.22f));
    g.drawLine(scope.getX(), centre.y, scope.getRight(), centre.y, 1.0f);
    g.drawLine(centre.x, scope.getY(), centre.x, scope.getBottom(), 1.0f);

    juce::Path diamond;
    diamond.startNewSubPath(centre.x, scope.getY() + 6.0f);
    diamond.lineTo(scope.getRight() - 6.0f, centre.y);
    diamond.lineTo(centre.x, scope.getBottom() - 6.0f);
    diamond.lineTo(scope.getX() + 6.0f, centre.y);
    diamond.closeSubPath();
    g.setColour(palette.textStrong.withAlpha(0.18f));
    g.strokePath(diamond, juce::PathStrokeType(1.0f));

    for (int i = 0; i < PluginProcessor::vectorscopePointCount; ++i)
    {
        const float age = (float) i / (float) juce::jmax(1, PluginProcessor::vectorscopePointCount - 1);
        const auto& point = snapshot.vectorscope[(size_t) i];
        const float x = centre.x + point.side * radius * 0.95f;
        const float y = centre.y - point.mid * radius * 0.95f;
        const float dot = 1.6f + (1.0f - age) * 1.8f;
        const auto colour = palette.accentPrimary.interpolatedWith(palette.positive, 1.0f - age).withAlpha(0.10f + (1.0f - age) * 0.72f);
        g.setColour(colour);
        g.fillEllipse(x - dot * 0.5f, y - dot * 0.5f, dot, dot);
    }

    g.setColour(palette.accentWarm);
    g.setFont(spectrumui::bodyFont(10.0f));
    g.drawText("Short " + juce::String(snapshot.shortLoudnessDb, 1), footer.removeFromLeft((int) footer.getWidth() / 2).toNearestInt(), juce::Justification::left);
    g.setColour(palette.accentSecondary);
    g.drawText("Peak " + juce::String(juce::jmax(snapshot.peakLeftDb, snapshot.peakRightDb), 1), footer.toNearestInt(), juce::Justification::right);
}

void SpectrumAnalyzerComponent::drawHoverReadout(juce::Graphics& g, juce::Rectangle<float> plot)
{
    if (hoverIndex < 0 || hoverIndex >= PluginProcessor::displayPointCount)
        return;

    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    const float x = juce::jmap((float) hoverIndex,
                               0.0f,
                               (float) (PluginProcessor::displayPointCount - 1),
                               plot.getX(),
                               plot.getRight());
    const float frequency = frequencyAtNormalisedPosition((float) hoverIndex / (float) (PluginProcessor::displayPointCount - 1),
                                                          snapshot.sampleRate);
    const float db = snapshot.primary[(size_t) hoverIndex];

    g.setColour(palette.textStrong.withAlpha(0.32f));
    g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());

    auto bubble = juce::Rectangle<float>(132.0f, 56.0f).withCentre({ juce::jlimit(plot.getX() + 70.0f, plot.getRight() - 70.0f, x), plot.getY() + 30.0f });
    g.setColour(palette.backgroundSoft.withAlpha(0.92f));
    g.fillRoundedRectangle(bubble, 10.0f);
    g.setColour(palette.outline);
    g.drawRoundedRectangle(bubble.reduced(0.5f), 10.0f, 1.0f);

    g.setColour(palette.textStrong);
    g.setFont(spectrumui::smallCapsFont(10.5f));
    g.drawFittedText(formatFrequency(frequency), bubble.removeFromTop(18.0f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour(palette.textMuted);
    g.setFont(spectrumui::bodyFont(10.0f));
    g.drawFittedText(formatNoteWithCents(frequency), bubble.removeFromTop(16.0f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour(palette.accentWarm);
    g.setFont(spectrumui::bodyFont(10.5f));
    g.drawFittedText(juce::String(db, 1) + " dB", bubble.toNearestInt(), juce::Justification::centred, 1);
}

void SpectrumAnalyzerComponent::paint(juce::Graphics& g)
{
    const auto palette = spectrumui::getPalette(snapshot.themeIndex);
    auto bounds = getLocalBounds().toFloat();
    spectrumui::drawPanelBackground(g, bounds, palette, palette.accentPrimary, 18.0f);

    auto inner = bounds.reduced(18.0f, 16.0f);
    auto header = inner.removeFromTop(26.0f);

    g.setColour(palette.textStrong);
    g.setFont(spectrumui::titleFont(16.0f));
    g.drawText("SSP Spectrum", header.removeFromLeft(180.0f).toNearestInt(), juce::Justification::left);

    g.setColour(palette.textMuted);
    g.setFont(spectrumui::smallCapsFont(10.0f));
    g.drawText("Real-time FFT analyzer with waveform, vectorscope, hold traces, note readout, and waterfall history.",
               header.removeFromLeft(420.0f).toNearestInt(),
               juce::Justification::left);

    auto modeBadge = header.removeFromRight(108.0f).reduced(2.0f, 0.0f);
    spectrumui::drawBadge(g,
                          modeBadge,
                          palette,
                          PluginProcessor::getDisplayModeNames()[snapshot.displayMode],
                          palette.accentSecondary.withAlpha(0.22f),
                          palette.textStrong);

    inner.removeFromTop(8.0f);

    auto sidebar = inner.removeFromRight(232.0f);
    auto waterfallArea = snapshot.waterfallEnabled ? inner.removeFromBottom(116.0f) : juce::Rectangle<float>();
    inner.removeFromBottom(snapshot.waterfallEnabled ? 10.0f : 0.0f);
    auto miniMetersArea = inner.removeFromBottom(126.0f);
    inner.removeFromBottom(10.0f);
    auto plot = inner;
    lastPlotArea = plot;

    g.setColour(palette.backgroundSoft.darker(0.25f));
    g.fillRoundedRectangle(plot, 16.0f);
    g.setColour(palette.outlineSoft.withAlpha(0.75f));
    g.drawRoundedRectangle(plot.reduced(0.5f), 16.0f, 1.0f);

    const std::array<float, 10> dbLines{{ 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -72.0f, -96.0f }};
    for (auto db : dbLines)
    {
        if (db < topDb - snapshot.rangeDb)
            continue;

        const float y = mapDbToY(db, plot, snapshot.rangeDb);
        g.setColour(db >= 0.0f ? palette.accentWarm.withAlpha(0.08f) : palette.outlineSoft.withAlpha(0.25f));
        g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        g.setColour(palette.textMuted.withAlpha(0.8f));
        g.setFont(spectrumui::bodyFont(9.5f));
        g.drawText(juce::String(db, 0), juce::Rectangle<int>((int) plot.getX() + 6, (int) y - 8, 40, 16), juce::Justification::left);
    }

    const std::array<int, 10> freqLines{{ 20, 40, 80, 100, 200, 500, 1000, 2000, 5000, 10000 }};
    for (auto freq : freqLines)
    {
        const float x = juce::jmap(std::log10((float) freq),
                                   std::log10(20.0f),
                                   std::log10((float) juce::jmin(20000.0, snapshot.sampleRate * 0.48)),
                                   plot.getX(),
                                   plot.getRight());
        g.setColour(palette.outlineSoft.withAlpha(0.18f));
        g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
        g.setColour(palette.textMuted.withAlpha(0.85f));
        g.setFont(spectrumui::bodyFont(9.5f));
        const auto freqText = freq >= 1000 ? juce::String(freq / 1000) + "k" : juce::String(freq);
        g.drawText(freqText,
                   juce::Rectangle<int>((int) x - 18, (int) plot.getBottom() - 32, 36, 16),
                   juce::Justification::centred);
        g.setFont(spectrumui::bodyFont(8.6f));
        g.drawText(formatNote((float) freq),
                   juce::Rectangle<int>((int) x - 20, (int) plot.getBottom() - 18, 40, 14),
                   juce::Justification::centred);
        g.setFont(spectrumui::bodyFont(9.5f));
    }

    const auto primaryPath = makeTracePath(snapshot.primary, plot, snapshot.rangeDb);
    const auto peakHoldPath = makeTracePath(snapshot.peakHold, plot, snapshot.rangeDb);
    const auto maxHoldPath = makeTracePath(snapshot.maxHold, plot, snapshot.rangeDb);

    juce::Path fill(primaryPath);
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.lineTo(plot.getX(), plot.getBottom());
    fill.closeSubPath();
    juce::ColourGradient fillGradient(palette.accentPrimary.withAlpha(0.28f), plot.getCentreX(), plot.getY(),
                                      juce::Colours::transparentBlack, plot.getCentreX(), plot.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillPath(fill);

    if (referenceA.captured && referenceA.visible)
        drawTrace(g, makeTracePath(referenceA.values, plot, snapshot.rangeDb), palette.accentWarm.withAlpha(0.72f), 1.2f);
    if (referenceB.captured && referenceB.visible)
        drawTrace(g, makeTracePath(referenceB.values, plot, snapshot.rangeDb), palette.positive.withAlpha(0.72f), 1.2f);

    if (snapshot.secondaryVisible)
        drawTrace(g, makeTracePath(snapshot.secondary, plot, snapshot.rangeDb), palette.accentSecondary.withAlpha(0.85f), 1.6f);
    if (snapshot.sidechainVisible)
        drawTrace(g, makeTracePath(snapshot.sidechain, plot, snapshot.rangeDb), palette.accentWarm.withAlpha(0.9f), 1.4f);

    drawTrace(g, maxHoldPath, palette.textStrong.withAlpha(0.22f), 1.0f);
    drawTrace(g, peakHoldPath, palette.textStrong.withAlpha(0.42f), 1.1f);
    drawTrace(g, primaryPath, palette.accentPrimary, 2.35f);

    drawHoverReadout(g, plot);
    auto scopeArea = miniMetersArea.removeFromRight(212.0f);
    miniMetersArea.removeFromRight(10.0f);
    drawWaveformMeter(g, miniMetersArea);
    drawVectorscopeMeter(g, scopeArea);
    drawSidebar(g, sidebar, palette.accentPrimary);

    if (snapshot.waterfallEnabled)
    {
        ensureWaterfallImage((int) std::round(waterfallArea.getWidth()), (int) std::round(waterfallArea.getHeight() - 20.0f));
        if (waterfallFramePending && ! snapshot.freezeActive)
        {
            pushWaterfallRow();
            waterfallFramePending = false;
        }

        auto waterfallHeader = waterfallArea.removeFromTop(16.0f);
        g.setColour(palette.textMuted);
        g.setFont(spectrumui::smallCapsFont(10.0f));
        g.drawText(snapshot.freezeActive ? "Waterfall Paused" : "Waterfall", waterfallHeader.toNearestInt(), juce::Justification::left);

        auto imageArea = waterfallArea.reduced(0.0f, 2.0f);
        g.setColour(palette.backgroundSoft.darker(0.25f));
        g.fillRoundedRectangle(imageArea, 12.0f);
        g.setColour(palette.outlineSoft.withAlpha(0.75f));
        g.drawRoundedRectangle(imageArea.reduced(0.5f), 12.0f, 1.0f);
        g.drawImageWithin(waterfallImage,
                          juce::roundToInt(imageArea.getX()),
                          juce::roundToInt(imageArea.getY()),
                          juce::roundToInt(imageArea.getWidth()),
                          juce::roundToInt(imageArea.getHeight()),
                          juce::RectanglePlacement::stretchToFit);
    }
}
