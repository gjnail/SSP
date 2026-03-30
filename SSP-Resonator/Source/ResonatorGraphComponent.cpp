#include "ResonatorGraphComponent.h"
#include "MusicNoteUtils.h"
#include "SSPVectorUI.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;

float frequencyToNormalised(float frequency)
{
    const float clamped = juce::jlimit(minFrequency, maxFrequency, frequency);
    return (std::log10(clamped) - std::log10(minFrequency)) / (std::log10(maxFrequency) - std::log10(minFrequency));
}

juce::String formatFrequencyLabel(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";
    return juce::String((int) frequency);
}

juce::Colour getVoiceColour(int index)
{
    static const juce::Colour palette[] =
    {
        juce::Colour(0xff45d7ff),
        juce::Colour(0xff50f3cf),
        juce::Colour(0xffffd75e),
        juce::Colour(0xffff7d8c),
        juce::Colour(0xff9e84ff),
        juce::Colour(0xffffa85a),
        juce::Colour(0xff6aff8f),
        juce::Colour(0xff7ab6ff)
    };

    return palette[(size_t) juce::jlimit(0, (int) std::size(palette) - 1, index % (int) std::size(palette))];
}
} // namespace

ResonatorGraphComponent::ResonatorGraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
}

void ResonatorGraphComponent::resized() {}

void ResonatorGraphComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<int> ResonatorGraphComponent::getPlotBounds() const
{
    auto bounds = getLocalBounds().reduced(18);
    bounds.removeFromTop(54);
    bounds.removeFromBottom(34);
    bounds.removeFromLeft(16);
    bounds.removeFromRight(16);
    return bounds;
}

float ResonatorGraphComponent::frequencyToX(float frequency, const juce::Rectangle<int>& plot) const
{
    return (float) plot.getX() + frequencyToNormalised(frequency) * (float) plot.getWidth();
}

float ResonatorGraphComponent::buildResonanceProfile(float frequency,
                                                     const PluginProcessor::VoiceArray& voices,
                                                     float resonance,
                                                     float brightness) const
{
    float sum = 0.0f;
    const float widthOctaves = juce::jmap(brightness, 0.14f, 0.38f);
    const float intensity = juce::jmap(resonance, 0.7f, 1.55f);

    for (const auto& voice : voices)
    {
        if (! voice.active)
            continue;

        const float logDistance = std::abs(std::log2(juce::jmax(frequency, minFrequency) / juce::jmax(voice.frequency, minFrequency)));
        const float gaussian = std::exp(-(logDistance * logDistance) / juce::jmax(0.02f, widthOctaves * widthOctaves));
        sum += gaussian * voice.level * intensity;
    }

    return juce::jlimit(0.0f, 1.4f, sum);
}

void ResonatorGraphComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    ssp::ui::drawPanelBackground(g, bounds, juce::Colour(0xff45d7ff), 18.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Chord Resonance Visualiser", 18, 12, 320, 22, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff8aaad0));
    g.setFont(12.0f);
    g.drawText(processor.getCurrentChordLabel(), 18, 34, 320, 16, juce::Justification::centredLeft, false);

    const auto plot = getPlotBounds();
    g.setColour(juce::Colour(0xff111924));
    g.fillRoundedRectangle(plot.toFloat(), 14.0f);

    juce::ColourGradient wash(juce::Colour(0x1428e1ff), (float) plot.getX(), (float) plot.getY(),
                              juce::Colour(0x1014ff9d), (float) plot.getRight(), (float) plot.getBottom(), false);
    wash.addColour(0.4, juce::Colour(0x141ef6d2));
    g.setGradientFill(wash);
    g.fillRoundedRectangle(plot.toFloat(), 14.0f);

    g.saveState();
    g.reduceClipRegion(plot);

    static constexpr float gridFrequencies[] = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
    static constexpr float energyLines[] = {0.18f, 0.36f, 0.54f, 0.72f, 0.9f};

    for (float frequency : gridFrequencies)
    {
        const auto x = juce::roundToInt(frequencyToX(frequency, plot));
        g.setColour(frequency == 1000.0f ? juce::Colour(0xff39679e) : juce::Colour(0xff213142));
        g.drawVerticalLine(x, (float) plot.getY(), (float) plot.getBottom());
    }

    for (float line : energyLines)
    {
        const float y = juce::jmap(line, 0.0f, 1.0f, (float) plot.getBottom(), (float) plot.getY());
        g.setColour(line > 0.6f ? juce::Colour(0x1affffff) : juce::Colour(0x10294d66));
        g.drawHorizontalLine(juce::roundToInt(y), (float) plot.getX(), (float) plot.getRight());
    }

    const auto analyzerFrame = processor.getAnalyzerFrameCopy();
    const auto voices = processor.getVoicesSnapshot();
    const double sampleRate = juce::jmax(1.0, processor.getCurrentSampleRate());
    const float resonance = processor.getResonanceAmount();
    const float brightness = processor.getBrightnessAmount();
    const auto now = (float) juce::Time::getMillisecondCounter();

    auto drawAnalyzer = [&](const PluginProcessor::SpectrumArray& spectrum, juce::Colour topColour, juce::Colour strokeColour)
    {
        juce::Path spectrumPath;
        bool started = false;

        for (int i = 1; i < (int) spectrum.size(); ++i)
        {
            const float frequency = (float) i * (float) sampleRate / (float) PluginProcessor::fftSize;
            if (frequency < minFrequency || frequency > maxFrequency)
                continue;

            const float x = frequencyToX(frequency, plot);
            const float y = juce::jmap(juce::jlimit(-96.0f, 0.0f, spectrum[(size_t) i]),
                                       -96.0f, 0.0f,
                                       (float) plot.getBottom(),
                                       (float) plot.getY());

            if (! started)
            {
                spectrumPath.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }
        }

        if (! started)
            return;

        juce::Path fill(spectrumPath);
        fill.lineTo((float) plot.getRight(), (float) plot.getBottom());
        fill.lineTo((float) plot.getX(), (float) plot.getBottom());
        fill.closeSubPath();

        juce::ColourGradient gradient(topColour, (float) plot.getCentreX(), (float) plot.getY(),
                                      topColour.withAlpha(0.02f), (float) plot.getCentreX(), (float) plot.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillPath(fill);

        g.setColour(strokeColour);
        g.strokePath(spectrumPath, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawAnalyzer(analyzerFrame.pre, juce::Colour(0x223bd4ff), juce::Colour(0x8859c8ff));
    drawAnalyzer(analyzerFrame.post, juce::Colour(0x244bfec7), juce::Colour(0xaa47f3d9));

    juce::Path resonancePath;
    bool started = false;
    for (int x = 0; x < plot.getWidth(); ++x)
    {
        const float frequency = std::pow(10.0f,
                                         std::log10(minFrequency)
                                             + ((float) x / (float) juce::jmax(1, plot.getWidth() - 1))
                                                   * (std::log10(maxFrequency) - std::log10(minFrequency)));
        const float profile = buildResonanceProfile(frequency, voices, resonance, brightness);
        const float y = juce::jmap(profile, 0.0f, 1.35f, (float) plot.getBottom() - 10.0f, (float) plot.getY() + 50.0f);
        const float px = (float) plot.getX() + (float) x;

        if (! started)
        {
            resonancePath.startNewSubPath(px, y);
            started = true;
        }
        else
        {
            resonancePath.lineTo(px, y);
        }
    }

    juce::Path resonanceFill(resonancePath);
    resonanceFill.lineTo((float) plot.getRight(), (float) plot.getBottom());
    resonanceFill.lineTo((float) plot.getX(), (float) plot.getBottom());
    resonanceFill.closeSubPath();

    juce::ColourGradient resonanceGradient(juce::Colour(0x404defff), (float) plot.getX(), (float) plot.getY(),
                                           juce::Colour(0x1888ff88), (float) plot.getRight(), (float) plot.getBottom(), false);
    resonanceGradient.addColour(0.55, juce::Colour(0x30ffd56d));
    g.setGradientFill(resonanceGradient);
    g.fillPath(resonanceFill);

    g.setColour(juce::Colour(0xff89f1ff).withAlpha(0.92f));
    g.strokePath(resonancePath, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < PluginProcessor::maxResonatorVoices; ++i)
    {
        const auto& voice = voices[(size_t) i];
        if (! voice.active)
            continue;

        const auto colour = getVoiceColour(i);
        const float x = frequencyToX(voice.frequency, plot);
        const float profile = buildResonanceProfile(voice.frequency, voices, resonance, brightness);
        const float y = juce::jmap(profile, 0.0f, 1.35f, (float) plot.getBottom() - 10.0f, (float) plot.getY() + 50.0f);
        const float pulse = 0.72f + 0.2f * std::sin(now * 0.0045f + (float) i * 0.7f);

        juce::ColourGradient beam(colour.withAlpha(0.42f * pulse), x, (float) plot.getY(),
                                  colour.withAlpha(0.0f), x, (float) plot.getBottom(), false);
        g.setGradientFill(beam);
        g.fillRect(juce::Rectangle<float>(x - 1.5f, (float) plot.getY(), 3.0f, (float) plot.getHeight()));

        g.setColour(colour.withAlpha(0.18f * pulse));
        g.fillEllipse(x - 24.0f, y - 24.0f, 48.0f, 48.0f);
        g.setColour(colour.withAlpha(0.95f));
        g.fillEllipse(x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f);

        const auto noteInfo = ssp::notes::frequencyToNote(voice.frequency, true);
        const auto labelText = noteInfo.name + juce::String(noteInfo.octave);
        auto labelBounds = juce::Rectangle<float>(x - 22.0f, (float) plot.getY() + 10.0f + 18.0f * (float) (i % 2), 44.0f, 18.0f);

        g.setColour(juce::Colour(0xd4111822));
        g.fillRoundedRectangle(labelBounds, 7.0f);
        g.setColour(colour.withAlpha(0.78f));
        g.drawRoundedRectangle(labelBounds, 7.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText(labelText, labelBounds.toNearestInt(), juce::Justification::centred, false);
    }

    g.restoreState();

    g.setColour(juce::Colour(0xff91a9c4));
    g.setFont(11.0f);
    for (float frequency : gridFrequencies)
    {
        const float x = frequencyToX(frequency, plot);
        g.drawText(formatFrequencyLabel(frequency),
                   juce::roundToInt(x) - 20,
                   plot.getBottom() + 4,
                   40,
                   14,
                   juce::Justification::centred,
                   false);
    }

    g.setColour(juce::Colour(0xffd8e9ff));
    g.setFont(12.0f);
    g.drawText(processor.getCurrentChordNoteSummary(),
               plot.getX(),
               plot.getY() - 4,
               plot.getWidth(),
               16,
               juce::Justification::centredRight,
               false);
}
