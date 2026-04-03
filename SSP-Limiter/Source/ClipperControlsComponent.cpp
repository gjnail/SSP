#include "ClipperControlsComponent.h"
#include <limits>

namespace
{
using Formatter = std::function<juce::String(double)>;

juce::String formatSignedDb(double value)
{
    if (std::abs(value) < 0.05)
        return "0.0 dB";

    return (value > 0.0 ? "+" : "-") + juce::String(std::abs(value), 1) + " dB";
}

juce::String formatDb(double value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatMilliseconds(double value)
{
    return juce::String(value, 1) + " ms";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}
} // namespace

class ClipperControlsComponent::ParameterKnob final : public juce::Component
{
public:
    ParameterKnob(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& labelText,
                  juce::Colour accentColour,
                  Formatter formatterToUse)
        : attachment(state, parameterID, slider),
          accent(accentColour)
    {
        setOpaque(true);
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(labelText, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
        titleLabel.setFont(reverbui::smallCapsFont(12.0f));

        slider.setName(labelText);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.15f));
        slider.textFromValueFunction = [formatterToUse](double value)
        {
            return formatterToUse != nullptr ? formatterToUse(value) : juce::String(value, 2);
        };
        slider.valueFromTextFunction = [this](const juce::String& text)
        {
            const auto sanitized = text.retainCharacters("0123456789+-.");
            const auto parsed = sanitized.isNotEmpty() ? sanitized.getDoubleValue() : slider.getValue();
            return juce::jlimit(slider.getMinimum(), slider.getMaximum(), parsed);
        };

        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(parameterID)))
            slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }

    void paint(juce::Graphics& g) override
    {
        reverbui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 18.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        slider.setBounds(area);
    }

private:
    juce::Label titleLabel;
    reverbui::SSPKnob slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::Colour accent;
};

class ClipperControlsComponent::LimiterVisualizer final : public juce::Component
{
public:
    LimiterVisualizer(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
        : processor(processorToUse),
          apvts(state)
    {
    }

    void resized() override
    {
        cachedBackground = {};
    }

    void paint(juce::Graphics& g) override
    {
        const float ceilingDb = apvts.getRawParameterValue("ceilingDb")->load();
        refreshBackgroundCacheIfNeeded(ceilingDb);
        g.drawImageAt(cachedBackground, 0, 0);

        const auto snapshot = processor.getVisualizerSnapshot();
        auto area = getLocalBounds().toFloat().reduced(18.0f, 16.0f);
        area.removeFromTop(24.0f + 8.0f);
        drawWaveformPlot(g, area, snapshot, ceilingDb);
    }

private:
    static float mapRadius(float halfHeight, float value)
    {
        const float clamped = juce::jlimit(0.0f, 1.15f, value) / 1.15f;
        return juce::jmax(0.0f, clamped * halfHeight);
    }

    static juce::Path buildMirroredPath(juce::Rectangle<float> plot,
                                        float centreY,
                                        float halfHeight,
                                        const std::array<float, PluginProcessor::visualizerPointCount>& values,
                                        int writePosition)
    {
        juce::Path path;
        const auto count = values.size();

        for (size_t i = 0; i < count; ++i)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float y = centreY - mapRadius(halfHeight, values[index]);

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        for (size_t i = count; i-- > 0;)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float y = centreY + mapRadius(halfHeight, values[index]);
            path.lineTo(x, y);
        }

        path.closeSubPath();
        return path;
    }

    static juce::Path buildEdgePath(juce::Rectangle<float> plot,
                                    float centreY,
                                    float halfHeight,
                                    const std::array<float, PluginProcessor::visualizerPointCount>& values,
                                    int writePosition,
                                    bool topEdge)
    {
        juce::Path path;
        const auto count = values.size();

        for (size_t i = 0; i < count; ++i)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float radius = mapRadius(halfHeight, values[index]);
            const float y = topEdge ? (centreY - radius) : (centreY + radius);

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        return path;
    }

    static juce::Path buildDifferencePath(juce::Rectangle<float> plot,
                                          float centreY,
                                          float halfHeight,
                                          const std::array<float, PluginProcessor::visualizerPointCount>& outerValues,
                                          const std::array<float, PluginProcessor::visualizerPointCount>& innerValues,
                                          int writePosition,
                                          bool topHalf)
    {
        juce::Path path;
        const auto count = outerValues.size();

        for (size_t i = 0; i < count; ++i)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float outerRadius = mapRadius(halfHeight, outerValues[index]);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float y = topHalf ? (centreY - outerRadius) : (centreY + outerRadius);

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        for (size_t i = count; i-- > 0;)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float outerRadius = mapRadius(halfHeight, outerValues[index]);
            const float innerRadius = juce::jmin(outerRadius, mapRadius(halfHeight, innerValues[index]));
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float y = topHalf ? (centreY - innerRadius) : (centreY + innerRadius);
            path.lineTo(x, y);
        }

        path.closeSubPath();
        return path;
    }

    static juce::Path buildGainReductionPath(juce::Rectangle<float> plot,
                                             const std::array<float, PluginProcessor::visualizerPointCount>& values,
                                             int writePosition,
                                             float maxDb)
    {
        juce::Path path;
        const auto count = values.size();
        const float top = plot.getY() + 18.0f;
        const float bottom = plot.getY() + juce::jmin(110.0f, plot.getHeight() * 0.34f);

        for (size_t i = 0; i < count; ++i)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
            const float y = juce::jmap(juce::jlimit(0.0f, maxDb, values[index]), 0.0f, maxDb, top, bottom);

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        return path;
    }

    void refreshBackgroundCacheIfNeeded(float ceilingDb)
    {
        const bool boundsChanged = cachedBackground.isNull()
            || cachedBackground.getWidth() != getWidth()
            || cachedBackground.getHeight() != getHeight();
        const bool parameterChanged = std::abs(cachedCeilingDb - ceilingDb) > 0.01f;

        if (! boundsChanged && ! parameterChanged)
            return;

        if (getWidth() <= 0 || getHeight() <= 0)
        {
            cachedBackground = {};
            return;
        }

        cachedBackground = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics g(cachedBackground);
        reverbui::drawPanelBackground(g, getLocalBounds().toFloat(), reverbui::brandGold(), 18.0f);

        auto area = getLocalBounds().toFloat().reduced(18.0f, 16.0f);
        auto topRow = area.removeFromTop(24.0f);
        g.setColour(reverbui::textStrong());
        g.setFont(reverbui::smallCapsFont(12.0f));
        g.drawText("LIMITER VIEW", topRow.removeFromLeft(160.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(11.0f));
        g.drawText("Input ghost, limited output, limiter-hit peaks, and gain reduction.",
                   topRow.toNearestInt(),
                   juce::Justification::centredRight,
                   false);

        area.removeFromTop(8.0f);
        drawStaticPlotBackground(g, area, ceilingDb);
        cachedCeilingDb = ceilingDb;
    }

    void drawStaticPlotBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float ceilingDb) const
    {
        auto plot = bounds.reduced(6.0f, 4.0f);
        g.setColour(juce::Colour(0xff091018));
        g.fillRoundedRectangle(plot, 14.0f);

        const float centreY = plot.getCentreY();
        const float waveformHalfHeight = (plot.getHeight() - 58.0f) * 0.5f;
        const auto mapY = [centreY, waveformHalfHeight](float value)
        {
            const float amplitude = juce::jlimit(-1.15f, 1.15f, value) / 1.15f;
            return centreY - amplitude * waveformHalfHeight;
        };

        g.setColour(reverbui::graphGrid());
        for (int i = 0; i < 7; ++i)
        {
            const float norm = juce::jmap((float) i, 0.0f, 6.0f, -1.0f, 1.0f);
            g.drawHorizontalLine((int) std::round(mapY(norm)), plot.getX() + 12.0f, plot.getRight() - 12.0f);
        }

        for (int i = 1; i < 12; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, 12.0f, plot.getX() + 12.0f, plot.getRight() - 12.0f);
            g.setColour(reverbui::outlineSoft().withAlpha(0.24f));
            g.drawVerticalLine((int) std::round(x), plot.getY() + 14.0f, plot.getBottom() - 24.0f);
        }

        g.setColour(reverbui::outlineSoft().withAlpha(0.55f));
        g.drawHorizontalLine((int) std::round(centreY), plot.getX() + 12.0f, plot.getRight() - 12.0f);

        const float ceilingGain = juce::jlimit(0.0f, 1.1f, juce::Decibels::decibelsToGain(ceilingDb));
        const float ceilingY = mapY(ceilingGain);
        const float negCeilingY = mapY(-ceilingGain);

        g.setColour(juce::Colour(0x12ffcf4d));
        g.fillRoundedRectangle({ plot.getX() + 12.0f, plot.getY() + 10.0f, plot.getWidth() - 24.0f, ceilingY - plot.getY() - 10.0f }, 10.0f);
        g.fillRoundedRectangle({ plot.getX() + 12.0f, negCeilingY, plot.getWidth() - 24.0f, plot.getBottom() - 24.0f - negCeilingY }, 10.0f);

        g.setColour(reverbui::brandCyan().withAlpha(0.80f));
        g.drawHorizontalLine((int) std::round(ceilingY), plot.getX() + 12.0f, plot.getRight() - 12.0f);
        g.drawHorizontalLine((int) std::round(negCeilingY), plot.getX() + 12.0f, plot.getRight() - 12.0f);
    }

    void drawWaveformPlot(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          const PluginProcessor::VisualizerSnapshot& snapshot,
                          float ceilingDb) const
    {
        auto plot = bounds.reduced(6.0f, 4.0f);
        const float centreY = plot.getCentreY();
        const float waveformHalfHeight = (plot.getHeight() - 58.0f) * 0.5f;
        const auto mapY = [centreY, waveformHalfHeight](float value)
        {
            const float amplitude = juce::jlimit(-1.15f, 1.15f, value) / 1.15f;
            return centreY - amplitude * waveformHalfHeight;
        };

        auto inputBodyPath = buildMirroredPath(plot, centreY, waveformHalfHeight, snapshot.inputBody, snapshot.writePosition);
        auto inputPeakTop = buildEdgePath(plot, centreY, waveformHalfHeight, snapshot.inputPeak, snapshot.writePosition, true);
        auto inputPeakBottom = buildEdgePath(plot, centreY, waveformHalfHeight, snapshot.inputPeak, snapshot.writePosition, false);
        auto outputBodyPath = buildMirroredPath(plot, centreY, waveformHalfHeight, snapshot.outputBody, snapshot.writePosition);
        auto outputPeakTop = buildEdgePath(plot, centreY, waveformHalfHeight, snapshot.outputPeak, snapshot.writePosition, true);
        auto outputPeakBottom = buildEdgePath(plot, centreY, waveformHalfHeight, snapshot.outputPeak, snapshot.writePosition, false);
        auto gainReductionPath = buildGainReductionPath(plot, snapshot.gainReductionHistory, snapshot.writePosition, 18.0f);
        auto gainReductionFill = gainReductionPath;
        gainReductionFill.lineTo(plot.getRight(), plot.getY() + juce::jmin(110.0f, plot.getHeight() * 0.34f));
        gainReductionFill.lineTo(plot.getX(), plot.getY() + juce::jmin(110.0f, plot.getHeight() * 0.34f));
        gainReductionFill.closeSubPath();

        const float ceilingGain = juce::jlimit(0.0f, 1.1f, juce::Decibels::decibelsToGain(ceilingDb));
        std::array<float, PluginProcessor::visualizerPointCount> clampedInputPeaks{};
        bool hasLimiterHit = false;
        for (size_t i = 0; i < clampedInputPeaks.size(); ++i)
        {
            clampedInputPeaks[i] = juce::jmin(snapshot.inputPeak[i], ceilingGain);
            if (snapshot.inputPeak[i] > ceilingGain + 0.0005f)
                hasLimiterHit = true;
        }

        auto clippedTopPath = buildDifferencePath(plot, centreY, waveformHalfHeight, snapshot.inputPeak, clampedInputPeaks, snapshot.writePosition, true);
        auto clippedBottomPath = buildDifferencePath(plot, centreY, waveformHalfHeight, snapshot.inputPeak, clampedInputPeaks, snapshot.writePosition, false);

        g.setColour(juce::Colour(0xffbfc5cc).withAlpha(0.10f));
        g.fillPath(inputBodyPath);
        g.setColour(juce::Colour(0xffd9dde2).withAlpha(0.24f));
        g.strokePath(inputPeakTop, juce::PathStrokeType(0.95f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath(inputPeakBottom, juce::PathStrokeType(0.95f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::ColourGradient outputGradient(juce::Colour(0xfffaf5ea).withAlpha(0.95f), plot.getCentreX(), plot.getY(),
                                            juce::Colour(0xffd8d0bf).withAlpha(0.82f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill(outputGradient);
        g.fillPath(outputBodyPath);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.86f));
        g.strokePath(outputPeakTop, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath(outputPeakBottom, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (hasLimiterHit)
        {
            juce::ColourGradient clipGradient(juce::Colour(0xffffdb6e).withAlpha(0.96f), plot.getCentreX(), plot.getY(),
                                              juce::Colour(0xffffb300).withAlpha(0.84f), plot.getCentreX(), plot.getBottom(), false);
            g.setGradientFill(clipGradient);
            g.fillPath(clippedTopPath);
            g.fillPath(clippedBottomPath);
        }

        juce::ColourGradient grFillGradient(juce::Colour(0x30ff5a5a), plot.getCentreX(), plot.getY() + 18.0f,
                                            juce::Colour(0x06ff5a5a), plot.getCentreX(), plot.getY() + juce::jmin(110.0f, plot.getHeight() * 0.34f), false);
        g.setGradientFill(grFillGradient);
        g.fillPath(gainReductionFill);
        g.setColour(juce::Colour(0xffff6b6b).withAlpha(0.94f));
        g.strokePath(gainReductionPath, juce::PathStrokeType(1.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ceilingY = mapY(ceilingGain);
        g.setColour(reverbui::brandCyan());
        g.setFont(reverbui::bodyFont(10.8f));
        g.drawText("Ceiling", juce::Rectangle<int>((int) plot.getX() + 14, (int) ceilingY - 14, 64, 16), juce::Justification::centredLeft, false);
        g.drawText(formatDb(ceilingDb), juce::Rectangle<int>((int) plot.getRight() - 94, (int) ceilingY - 14, 80, 16), juce::Justification::centredRight, false);

        auto footer = juce::Rectangle<float>(plot.getX() + 12.0f, plot.getBottom() - 20.0f, plot.getWidth() - 24.0f, 16.0f);
        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(10.5f));
        g.drawText("Input Ghost", juce::Rectangle<int>((int) footer.getX(), (int) footer.getY(), 84, (int) footer.getHeight()), juce::Justification::centredLeft, false);
        g.drawText("Limited Output", juce::Rectangle<int>((int) footer.getX() + 96, (int) footer.getY(), 102, (int) footer.getHeight()), juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xffffcf4d));
        g.drawText("Ceiling Hits", juce::Rectangle<int>((int) footer.getX() + 212, (int) footer.getY(), 86, (int) footer.getHeight()), juce::Justification::centredLeft, false);
    }

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Image cachedBackground;
    float cachedCeilingDb = std::numeric_limits<float>::max();
};

class ClipperControlsComponent::GainReductionMeter final : public juce::Component
{
public:
    explicit GainReductionMeter(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        reverbui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffff5555), 18.0f);

        auto area = getLocalBounds().toFloat().reduced(16.0f, 14.0f);
        auto header = area.removeFromTop(22.0f);
        g.setColour(reverbui::textStrong());
        g.setFont(reverbui::smallCapsFont(11.8f));
        g.drawText("GAIN REDUCTION", header.toNearestInt(), juce::Justification::centredLeft, false);

        const auto snapshot = processor.getVisualizerSnapshot();
        const float currentGr = processor.getGainReductionDb();
        const float heldGr = findHeldReduction(snapshot);

        auto meterArea = area.removeFromRight(54.0f);
        auto historyArea = area;
        historyArea.removeFromRight(8.0f);

        drawHistory(g, historyArea, snapshot);
        drawMeter(g, meterArea, currentGr, heldGr);
    }

private:
    static float findHeldReduction(const PluginProcessor::VisualizerSnapshot& snapshot)
    {
        float held = 0.0f;
        for (float value : snapshot.gainReductionHistory)
            held = juce::jmax(held, value);
        return held;
    }

    static juce::Path buildHistoryPath(juce::Rectangle<float> area,
                                       const std::array<float, PluginProcessor::visualizerPointCount>& values,
                                       int writePosition)
    {
        juce::Path path;
        const auto count = values.size();
        for (size_t i = 0; i < count; ++i)
        {
            const size_t index = (size_t) ((writePosition + (int) i) % (int) count);
            const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), area.getX(), area.getRight());
            const float y = juce::jmap(juce::jlimit(0.0f, 18.0f, values[index]), 0.0f, 18.0f, area.getY(), area.getBottom());
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    }

    void drawHistory(juce::Graphics& g, juce::Rectangle<float> area, const PluginProcessor::VisualizerSnapshot& snapshot) const
    {
        g.setColour(juce::Colour(0xff0d141b));
        g.fillRoundedRectangle(area, 10.0f);
        g.setColour(reverbui::outlineSoft());
        g.drawRoundedRectangle(area, 10.0f, 1.0f);

        for (int i = 0; i <= 6; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, 6.0f, area.getY(), area.getBottom());
            g.setColour(reverbui::outlineSoft().withAlpha(i == 0 ? 0.30f : 0.18f));
            g.drawHorizontalLine((int) std::round(y), area.getX() + 8.0f, area.getRight() - 8.0f);
        }

        auto path = buildHistoryPath(area.reduced(8.0f, 8.0f), snapshot.gainReductionHistory, snapshot.writePosition);
        auto fill = path;
        fill.lineTo(area.getRight() - 8.0f, area.getBottom() - 8.0f);
        fill.lineTo(area.getX() + 8.0f, area.getBottom() - 8.0f);
        fill.closeSubPath();

        juce::ColourGradient fillGradient(juce::Colour(0x44ff5a5a), area.getCentreX(), area.getY(),
                                          juce::Colour(0x08ff5a5a), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill(fillGradient);
        g.fillPath(fill);
        g.setColour(juce::Colour(0xffff6b6b));
        g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(9.8f));
        g.drawText("0", juce::Rectangle<int>((int) area.getX() + 8, (int) area.getY() - 2, 24, 12), juce::Justification::centredLeft, false);
        g.drawText("6", juce::Rectangle<int>((int) area.getX() + 8, (int) juce::jmap(6.0f, 0.0f, 18.0f, area.getY(), area.getBottom()) - 6, 24, 12), juce::Justification::centredLeft, false);
        g.drawText("12", juce::Rectangle<int>((int) area.getX() + 8, (int) juce::jmap(12.0f, 0.0f, 18.0f, area.getY(), area.getBottom()) - 6, 24, 12), juce::Justification::centredLeft, false);
        g.drawText("18 dB", juce::Rectangle<int>((int) area.getX() + 8, (int) area.getBottom() - 14, 36, 12), juce::Justification::centredLeft, false);
    }

    void drawMeter(juce::Graphics& g, juce::Rectangle<float> area, float currentGr, float heldGr) const
    {
        auto meterTrack = area.removeFromBottom(area.getHeight() - 34.0f);
        g.setColour(juce::Colour(0xff0c1218));
        g.fillRoundedRectangle(meterTrack, 10.0f);
        g.setColour(reverbui::outlineSoft());
        g.drawRoundedRectangle(meterTrack, 10.0f, 1.0f);

        const float fillHeight = meterTrack.getHeight() * juce::jlimit(0.0f, 1.0f, currentGr / 18.0f);
        auto fill = meterTrack.withY(meterTrack.getBottom() - fillHeight).withHeight(fillHeight);
        juce::ColourGradient gradient(juce::Colour(0xffff8b8b), fill.getCentreX(), fill.getY(),
                                      juce::Colour(0xffff3232), fill.getCentreX(), fill.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(fill, 10.0f);

        const float heldY = juce::jmap(juce::jlimit(0.0f, 18.0f, heldGr), 0.0f, 18.0f, meterTrack.getY(), meterTrack.getBottom());
        g.setColour(juce::Colour(0xffffd4d4));
        g.drawHorizontalLine((int) std::round(heldY), meterTrack.getX() + 4.0f, meterTrack.getRight() - 4.0f);

        auto readout = area.reduced(0.0f, 4.0f);
        g.setColour(reverbui::textStrong());
        g.setFont(reverbui::titleFont(18.0f));
        g.drawText(juce::String(currentGr, 1), readout.removeFromTop(22.0f).toNearestInt(), juce::Justification::centred, false);
        g.setColour(juce::Colour(0xffff8b8b));
        g.setFont(reverbui::bodyFont(10.2f));
        g.drawText("dB GR", readout.removeFromTop(14.0f).toNearestInt(), juce::Justification::centred, false);
    }

    PluginProcessor& processor;
};

ClipperControlsComponent::ClipperControlsComponent(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
    : processor(processorToUse),
      apvts(state)
{
    badgeLabel.setText("MODERN LIMITER SURFACE", juce::dontSendNotification);
    badgeLabel.setJustificationType(juce::Justification::centredLeft);
    badgeLabel.setFont(reverbui::smallCapsFont(11.5f));
    badgeLabel.setColour(juce::Label::textColourId, reverbui::brandCyan());
    addAndMakeVisible(badgeLabel);

    summaryLabel.setText("Lookahead limiting, cleaner peak-hit feedback, and a dedicated gain reduction view.",
                         juce::dontSendNotification);
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setFont(reverbui::bodyFont(12.0f));
    summaryLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    addAndMakeVisible(summaryLabel);

    limitTypeBox.addItemList(PluginProcessor::getLimitTypeChoices(), 1);
    limitTypeBox.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(limitTypeBox);
    limitTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "limitType", limitTypeBox);

    engineStatusLabel.setFont(reverbui::bodyFont(11.2f));
    engineStatusLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    engineStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(engineStatusLabel);

    visualizer = std::make_unique<LimiterVisualizer>(processor, apvts);
    gainReductionMeter = std::make_unique<GainReductionMeter>(processor);
    inputKnob = std::make_unique<ParameterKnob>(apvts, "inputDb", "Input", reverbui::brandCyan(), formatSignedDb);
    ceilingKnob = std::make_unique<ParameterKnob>(apvts, "ceilingDb", "Ceiling", reverbui::brandMint(), formatDb);
    releaseKnob = std::make_unique<ParameterKnob>(apvts, "releaseMs", "Release", reverbui::brandGold(), formatMilliseconds);
    lookaheadKnob = std::make_unique<ParameterKnob>(apvts, "lookaheadMs", "Lookahead", reverbui::brandCyan(), formatMilliseconds);
    stereoLinkKnob = std::make_unique<ParameterKnob>(apvts, "stereoLink", "Stereo Link", reverbui::brandMint(), formatPercent);
    outputKnob = std::make_unique<ParameterKnob>(apvts, "outputDb", "Output", reverbui::brandGold(), formatSignedDb);
    mixKnob = std::make_unique<ParameterKnob>(apvts, "mix", "Mix", reverbui::brandCyan(), formatPercent);

    addAndMakeVisible(*visualizer);
    addAndMakeVisible(*gainReductionMeter);
    addAndMakeVisible(*inputKnob);
    addAndMakeVisible(*ceilingKnob);
    addAndMakeVisible(*releaseKnob);
    addAndMakeVisible(*lookaheadKnob);
    addAndMakeVisible(*stereoLinkKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(*mixKnob);

    refreshDynamicState();
    startTimerHz(24);
}

ClipperControlsComponent::~ClipperControlsComponent()
{
    stopTimer();
}

void ClipperControlsComponent::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void ClipperControlsComponent::resized()
{
    auto area = getLocalBounds();

    auto textRow = area.removeFromTop(48);
    badgeLabel.setBounds(textRow.removeFromTop(18));
    summaryLabel.setBounds(textRow);

    area.removeFromTop(10);
    auto topControls = area.removeFromTop(28);
    limitTypeBox.setBounds(topControls.removeFromLeft(180));
    topControls.removeFromLeft(12);
    engineStatusLabel.setBounds(topControls);

    area.removeFromTop(10);
    auto visualRow = area.removeFromTop(430);
    gainReductionMeter->setBounds(visualRow.removeFromRight(170));
    visualRow.removeFromRight(14);
    visualizer->setBounds(visualRow);

    area.removeFromTop(14);
    const int rowGap = 16;
    const int knobGap = 18;
    const int topKnobWidth = (area.getWidth() - knobGap * 3) / 4;
    const int rowHeight = (area.getHeight() - rowGap) / 2;

    auto topRow = area.removeFromTop(rowHeight);
    inputKnob->setBounds(topRow.removeFromLeft(topKnobWidth));
    topRow.removeFromLeft(knobGap);
    ceilingKnob->setBounds(topRow.removeFromLeft(topKnobWidth));
    topRow.removeFromLeft(knobGap);
    releaseKnob->setBounds(topRow.removeFromLeft(topKnobWidth));
    topRow.removeFromLeft(knobGap);
    outputKnob->setBounds(topRow);

    area.removeFromTop(rowGap);
    auto bottomRow = area.removeFromTop(rowHeight);
    const int bottomKnobWidth = (bottomRow.getWidth() - knobGap * 2) / 3;
    lookaheadKnob->setBounds(bottomRow.removeFromLeft(bottomKnobWidth));
    bottomRow.removeFromLeft(knobGap);
    stereoLinkKnob->setBounds(bottomRow.removeFromLeft(bottomKnobWidth));
    bottomRow.removeFromLeft(knobGap);
    mixKnob->setBounds(bottomRow);
}

void ClipperControlsComponent::timerCallback()
{
    if (! isShowing())
        return;

    refreshDynamicState();
    visualizer->repaint();
    gainReductionMeter->repaint();
}

void ClipperControlsComponent::refreshDynamicState()
{
    const float lookaheadMs = apvts.getRawParameterValue("lookaheadMs")->load();
    const float stereoLink = apvts.getRawParameterValue("stereoLink")->load();
    const float gainReductionDb = processor.getGainReductionDb();
    const juce::String newStatus = processor.getActiveLimitTypeLabel()
        + "  |  Ceiling "
        + juce::String(apvts.getRawParameterValue("ceilingDb")->load(), 1)
        + " dB  |  Lookahead "
        + juce::String(lookaheadMs, 1)
        + " ms  |  Link "
        + juce::String(juce::roundToInt(stereoLink * 100.0f))
        + "%  |  GR "
        + juce::String(gainReductionDb, 1)
        + " dB  |  Latency "
        + juce::String(processor.getLatencySamples())
        + " samples";

    if (engineStatusLabel.getText() != newStatus)
        engineStatusLabel.setText(newStatus, juce::dontSendNotification);
}
