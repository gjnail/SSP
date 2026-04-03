#include "ReducerGraphComponent.h"

namespace
{
constexpr int fftOrder = 11;
constexpr int fftSize = 1 << fftOrder;
constexpr double minFrequency = 20.0;
constexpr float graphDisplayMinDb = -72.0f;
constexpr float graphDisplayMaxDb = 6.0f;

void buildSpectrum(const float* source, double sampleRate, std::array<float, fftSize / 2>& magnitudes)
{
    static juce::dsp::FFT fft(fftOrder);
    static juce::dsp::WindowingFunction<float> window((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);

    std::array<float, fftSize * 2> fftData{};
    std::copy(source, source + fftSize, fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (int bin = 0; bin < fftSize / 2; ++bin)
    {
        const auto normalised = fftData[(size_t) bin] / (float) fftSize;
        const auto magnitudeDb = juce::Decibels::gainToDecibels(normalised, graphDisplayMinDb);
        const auto frequency = sampleRate * (double) bin / (double) fftSize;
        magnitudes[(size_t) bin] = frequency >= minFrequency ? magnitudeDb : graphDisplayMinDb;
    }
}

juce::Path createSpectrumPath(const std::array<float, fftSize / 2>& magnitudes,
                              double sampleRate,
                              juce::Rectangle<float>,
                              std::function<float(float)> mapMagnitudeToY,
                              std::function<float(double)> mapFrequencyToX)
{
    juce::Path path;
    bool started = false;

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const auto frequency = sampleRate * (double) bin / (double) fftSize;

        if (frequency < minFrequency || frequency > 20000.0)
            continue;

        const auto x = mapFrequencyToX(frequency);
        const auto y = mapMagnitudeToY(magnitudes[(size_t) bin]);

        if (!started)
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
}

ReducerGraphComponent::ReducerGraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(24);
}

ReducerGraphComponent::~ReducerGraphComponent()
{
    stopTimer();
}

void ReducerGraphComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<float> ReducerGraphComponent::getPlotBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    auto content = bounds.reduced(14.0f, 12.0f);
    content.removeFromTop(18.0f);
    content.removeFromBottom(24.0f);
    content.removeFromLeft(8.0f);
    content.removeFromRight(8.0f);
    return content;
}

float ReducerGraphComponent::frequencyToX(double frequency, juce::Rectangle<float> plot) const
{
    const auto logMin = std::log(minFrequency);
    const auto logMax = std::log(20000.0);
    const auto proportion = (std::log(juce::jlimit(minFrequency, 20000.0, frequency)) - logMin) / (logMax - logMin);
    return plot.getX() + plot.getWidth() * (float) proportion;
}

float ReducerGraphComponent::magnitudeToY(float magnitudeDb, juce::Rectangle<float> plot) const
{
    const auto clamped = juce::jlimit(graphDisplayMinDb, graphDisplayMaxDb, magnitudeDb);
    const auto proportion = (clamped - graphDisplayMaxDb) / (graphDisplayMinDb - graphDisplayMaxDb);
    return plot.getY() + plot.getHeight() * proportion;
}

void ReducerGraphComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    reducerui::drawPanelBackground(g, bounds, reducerui::accent(), 14.0f);

    auto plot = getPlotBounds();
    const std::array<double, 10> freqLines{ 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    const std::array<float, 6> magnitudeLines{ -72.0f, -60.0f, -48.0f, -36.0f, -24.0f, -12.0f };

    g.setColour(juce::Colours::white.withAlpha(0.035f));
    for (auto magnitude : magnitudeLines)
    {
        const auto y = magnitudeToY(magnitude, plot);
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }

    g.setColour(reducerui::outlineSoft().withAlpha(0.55f));
    for (auto frequency : freqLines)
    {
        const auto x = frequencyToX(frequency, plot);
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }

    auto parameters = processor.getCurrentParameters();
    const auto previewSampleRate = juce::jmax(44100.0, parameters.sampleRate);
    parameters.sampleRate = previewSampleRate;

    juce::AudioBuffer<float> dryBuffer(2, fftSize);
    juce::AudioBuffer<float> wetBuffer(2, fftSize);

    auto* leftDry = dryBuffer.getWritePointer(0);
    auto* rightDry = dryBuffer.getWritePointer(1);
    const auto fundamental = 110.0;

    for (int sample = 0; sample < fftSize; ++sample)
    {
        const auto time = (double) sample / previewSampleRate;
        double value = 0.0;

        for (int harmonic = 1; harmonic <= 24; ++harmonic)
            value += std::sin(juce::MathConstants<double>::twoPi * fundamental * (double) harmonic * time) / (double) harmonic;

        value += 0.18 * std::sin(juce::MathConstants<double>::twoPi * 3200.0 * time);
        const auto sampleValue = (float) juce::jlimit(-1.0, 1.0, value * 0.65);
        leftDry[sample] = sampleValue;
        rightDry[sample] = sampleValue;
    }

    wetBuffer.makeCopyOf(dryBuffer, true);
    reducerdsp::State previewState;
    reducerdsp::prepareState(previewState, previewSampleRate, fftSize);
    reducerdsp::processWetBuffer(wetBuffer, parameters, previewState);

    std::array<float, fftSize / 2> drySpectrum{};
    std::array<float, fftSize / 2> wetSpectrum{};
    buildSpectrum(dryBuffer.getReadPointer(0), previewSampleRate, drySpectrum);
    buildSpectrum(wetBuffer.getReadPointer(0), previewSampleRate, wetSpectrum);

    const auto mapY = [this, plot](float magnitude) { return magnitudeToY(magnitude, plot); };
    const auto mapX = [this, plot](double frequency) { return frequencyToX(frequency, plot); };
    const auto dryPath = createSpectrumPath(drySpectrum, previewSampleRate, plot, mapY, mapX);
    const auto wetPath = createSpectrumPath(wetSpectrum, previewSampleRate, plot, mapY, mapX);

    g.setColour(reducerui::textMuted().withAlpha(0.30f));
    g.strokePath(dryPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ColourGradient wetGlow(reducerui::accent().withAlpha(0.20f), plot.getCentreX(), plot.getY(),
                                 reducerui::accent().withAlpha(0.02f), plot.getCentreX(), plot.getBottom(), false);
    g.setGradientFill(wetGlow);
    auto wetFill = wetPath;
    wetFill.lineTo(plot.getRight(), plot.getBottom());
    wetFill.lineTo(plot.getX(), plot.getBottom());
    wetFill.closeSubPath();
    g.fillPath(wetFill);

    g.setColour(reducerui::accentBright());
    g.strokePath(wetPath, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(reducerui::textMuted());
    g.setFont(reducerui::bodyFont(9.5f));

    auto footer = bounds.reduced(16.0f, 0.0f).removeFromBottom(18.0f);
    g.drawText("20 Hz", footer.removeFromLeft(48.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("20 kHz", footer.removeFromRight(56.0f).toNearestInt(), juce::Justification::centredRight, false);
    g.drawText("Preview Spectrum", footer.toNearestInt(), juce::Justification::centred, false);

    auto header = bounds.reduced(18.0f, 10.0f).removeFromTop(14.0f);
    g.setColour(reducerui::textStrong());
    g.setFont(reducerui::smallCapsFont(10.0f));
    g.drawText("DRY", header.removeFromLeft(32.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.setColour(reducerui::textMuted());
    g.setFont(reducerui::bodyFont(10.0f));
    g.drawText("reference", header.removeFromLeft(48.0f).toNearestInt(), juce::Justification::centredLeft, false);
    header.removeFromLeft(10.0f);
    g.setColour(reducerui::accentBright());
    g.setFont(reducerui::smallCapsFont(10.0f));
    g.drawText("CRUSHED", header.removeFromLeft(54.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.setColour(reducerui::textMuted());
    g.setFont(reducerui::bodyFont(10.0f));
    g.drawText("current settings", header.toNearestInt(), juce::Justification::centredLeft, false);

    auto infoStrip = plot.removeFromBottom(18.0f);
    const auto reducedRateHz = reducerdsp::reducedSampleRateFromNormalised(parameters.rate, previewSampleRate);
    const auto infoText = "Rate " + juce::String(reducedRateHz >= 1000.0 ? reducedRateHz / 1000.0 : reducedRateHz,
                                                 reducedRateHz >= 1000.0 ? 2 : 0)
                        + (reducedRateHz >= 1000.0 ? " kHz" : " Hz")
                        + "   Bits " + juce::String(reducerdsp::bitDepthFromNormalised(parameters.bits))
                        + "   ADC " + juce::String(juce::roundToInt((float) parameters.adcQuality * 100.0f)) + "%"
                        + "   DAC " + juce::String(juce::roundToInt((float) parameters.dacQuality * 100.0f)) + "%"
                        + "   Dither " + juce::String(juce::roundToInt((float) parameters.dither * 100.0f)) + "%";
    g.setColour(reducerui::textMuted().withAlpha(0.78f));
    g.setFont(reducerui::bodyFont(9.0f));
    g.drawText(infoText, infoStrip.toNearestInt(), juce::Justification::centredRight, false);
}
