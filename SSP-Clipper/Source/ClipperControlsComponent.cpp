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

juce::String formatPlainDb(double value)
{
    return juce::String(value, 1) + " dB";
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

class ClipperControlsComponent::ClipVisualizer final : public juce::Component
{
public:
    ClipVisualizer(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
        : processor(processorToUse),
          apvts(state)
    {
        setOpaque(true);
    }

    void resized() override
    {
        cachedBackground = {};
    }

    void paint(juce::Graphics& g) override
    {
        const auto snapshot = processor.getVisualizerSnapshot();
        const float ceilingDb = apvts.getRawParameterValue("ceilingDb")->load();
        const float trimDb = apvts.getRawParameterValue("trimDb")->load();
        refreshBackgroundCacheIfNeeded(ceilingDb, trimDb);
        g.drawImageAt(cachedBackground, 0, 0);

        auto area = getLocalBounds().toFloat().reduced(18.0f, 16.0f);
        auto topRow = area.removeFromTop(24.0f);

        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(11.0f));
        const juce::String headerText = processor.getActiveClipTypeLabel().toUpperCase() + "  |  " + processor.getActiveOversamplingLabel() + "  |  LATENCY "
                                        + juce::String(processor.getLatencySamples()) + " SAMPLES";
        g.drawText(headerText, topRow.toNearestInt(), juce::Justification::centredRight, false);

        area.removeFromTop(8.0f);
        drawWaveformPlot(g, area, snapshot);
    }

private:
    void refreshBackgroundCacheIfNeeded(float ceilingDb, float trimDb)
    {
        const bool boundsChanged = cachedBackground.isNull()
            || cachedBackground.getWidth() != getWidth()
            || cachedBackground.getHeight() != getHeight();
        const bool parameterChanged = std::abs(cachedCeilingDb - ceilingDb) > 0.01f
            || std::abs(cachedTrimDb - trimDb) > 0.01f;

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
        g.drawText("WAVEFORM VIEW", topRow.removeFromLeft(140.0f).toNearestInt(), juce::Justification::centredLeft, false);

        area.removeFromTop(8.0f);
        drawStaticPlotBackground(g, area, ceilingDb, trimDb);

        cachedCeilingDb = ceilingDb;
        cachedTrimDb = trimDb;
    }

    void drawStaticPlotBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float ceilingDb, float trimDb) const
    {
        auto plot = bounds.reduced(6.0f, 4.0f);
        g.setColour(juce::Colour(0xff0a1016));
        g.fillRoundedRectangle(plot, 14.0f);

        const float ceiling = juce::jmax(0.001f, juce::Decibels::decibelsToGain(ceilingDb));
        const float trim = juce::Decibels::decibelsToGain(trimDb) * ceiling;
        const float centreY = plot.getCentreY();
        const float waveformHalfHeight = (plot.getHeight() - 54.0f) * 0.5f;
        const auto mapY = [centreY, waveformHalfHeight](float value)
        {
            const float amplitude = juce::jlimit(-1.2f, 1.2f, value) / 1.2f;
            return centreY - amplitude * waveformHalfHeight;
        };

        g.setColour(reverbui::graphGrid());
        for (int i = 0; i < 5; ++i)
        {
            const float norm = juce::jmap((float) i, 0.0f, 4.0f, -1.0f, 1.0f);
            g.drawHorizontalLine((int) std::round(mapY(norm)), plot.getX() + 10.0f, plot.getRight() - 10.0f);
        }

        g.setColour(reverbui::outlineSoft().withAlpha(0.55f));
        g.drawVerticalLine((int) std::round(plot.getCentreX()), plot.getY() + 12.0f, plot.getBottom() - 20.0f);
        g.drawHorizontalLine((int) std::round(centreY), plot.getX() + 10.0f, plot.getRight() - 10.0f);

        const float ceilingY = mapY(ceiling);
        const float negCeilingY = mapY(-ceiling);
        const float trimY = mapY(trim);
        const float negTrimY = mapY(-trim);
        auto clipTopZone = juce::Rectangle<float>(plot.getX() + 10.0f, plot.getY() + 8.0f, plot.getWidth() - 20.0f, juce::jmax(0.0f, ceilingY - (plot.getY() + 8.0f)));
        auto clipBottomZone = juce::Rectangle<float>(plot.getX() + 10.0f, negCeilingY, plot.getWidth() - 20.0f, juce::jmax(0.0f, (plot.getBottom() - 24.0f) - negCeilingY));
        g.setColour(juce::Colour(0x18ff7d7d));
        g.fillRoundedRectangle(clipTopZone, 8.0f);
        g.fillRoundedRectangle(clipBottomZone, 8.0f);

        g.setColour(reverbui::brandGold().withAlpha(0.42f));
        g.drawHorizontalLine((int) std::round(ceilingY), plot.getX() + 10.0f, plot.getRight() - 10.0f);
        g.drawHorizontalLine((int) std::round(negCeilingY), plot.getX() + 10.0f, plot.getRight() - 10.0f);
        g.setColour(reverbui::brandMint().withAlpha(0.32f));
        g.drawHorizontalLine((int) std::round(trimY), plot.getX() + 10.0f, plot.getRight() - 10.0f);
        g.drawHorizontalLine((int) std::round(negTrimY), plot.getX() + 10.0f, plot.getRight() - 10.0f);
    }

    void drawWaveformPlot(juce::Graphics& g, juce::Rectangle<float> bounds, const PluginProcessor::VisualizerSnapshot& snapshot) const
    {
        auto plot = bounds.reduced(6.0f, 4.0f);

        const float ceiling = juce::jmax(0.001f, juce::Decibels::decibelsToGain(apvts.getRawParameterValue("ceilingDb")->load()));
        const float centreY = plot.getCentreY();
        const float waveformHalfHeight = (plot.getHeight() - 54.0f) * 0.5f;
        const auto mapY = [centreY, waveformHalfHeight](float value)
        {
            const float amplitude = juce::jlimit(-1.2f, 1.2f, value) / 1.2f;
            return centreY - amplitude * waveformHalfHeight;
        };
        const auto mapRadius = [waveformHalfHeight](float value)
        {
            if (value <= 0.0025f)
                return 0.0f;

            const float clamped = juce::jlimit(0.0f, 1.2f, value) / 1.2f;
            return juce::jmax(waveformHalfHeight * 0.025f, clamped * waveformHalfHeight);
        };

        const auto buildMirroredPath = [plot, centreY, mapRadius, &snapshot](const auto& values)
        {
            juce::Path path;
            const auto count = values.size();

            for (size_t i = 0; i < count; ++i)
            {
                const size_t historyIndex = (size_t) ((snapshot.writePosition + (int) i) % (int) count);
                const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
                const float radius = mapRadius(values[historyIndex]);
                const float y = centreY - radius;

                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            for (size_t idx = count; idx-- > 0;)
            {
                const size_t historyIndex = (size_t) ((snapshot.writePosition + (int) idx) % (int) count);
                const float x = juce::jmap((float) idx, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
                path.lineTo(x, centreY + mapRadius(values[historyIndex]));
            }

            path.closeSubPath();
            return path;
        };

        const auto buildEdgePath = [plot, centreY, mapRadius, &snapshot](const auto& values, bool topEdge)
        {
            juce::Path path;
            const auto count = values.size();

            for (size_t i = 0; i < count; ++i)
            {
                const size_t historyIndex = (size_t) ((snapshot.writePosition + (int) i) % (int) count);
                const float x = juce::jmap((float) i, 0.0f, (float) (count - 1), plot.getX(), plot.getRight());
                const float radius = mapRadius(values[historyIndex]);
                const float y = topEdge ? (centreY - radius) : (centreY + radius);

                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            return path;
        };

        auto waveformBody = buildMirroredPath(snapshot.waveformBody);
        auto waveformPeak = buildMirroredPath(snapshot.waveformPeak);
        auto waveformTopEdge = buildEdgePath(snapshot.waveformPeak, true);
        auto waveformBottomEdge = buildEdgePath(snapshot.waveformPeak, false);

        const float ceilingY = mapY(ceiling);
        const float negCeilingY = mapY(-ceiling);
        auto topClipZone = juce::Rectangle<int>((int) std::floor(plot.getX()), (int) std::floor(plot.getY()),
                                                (int) std::ceil(plot.getWidth()), (int) std::ceil(ceilingY - plot.getY()));
        auto middleZone = juce::Rectangle<int>((int) std::floor(plot.getX()), (int) std::floor(ceilingY),
                                               (int) std::ceil(plot.getWidth()), (int) std::ceil(negCeilingY - ceilingY));
        auto bottomClipZone = juce::Rectangle<int>((int) std::floor(plot.getX()), (int) std::floor(negCeilingY),
                                                   (int) std::ceil(plot.getWidth()), (int) std::ceil(plot.getBottom() - negCeilingY));

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(middleZone);
            juce::ColourGradient bodyGradient(juce::Colour(0xffecebea).withAlpha(0.92f), plot.getCentreX(), plot.getY(),
                                              juce::Colour(0xffb9b9b9).withAlpha(0.78f), plot.getCentreX(), plot.getBottom(), false);
            g.setGradientFill(bodyGradient);
            g.fillPath(waveformBody);
            g.setColour(juce::Colour(0xffffffff).withAlpha(0.38f));
            g.fillPath(waveformPeak);
            g.setColour(juce::Colour(0xfff3f3f3).withAlpha(0.92f));
            g.strokePath(waveformTopEdge, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.strokePath(waveformBottomEdge, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const auto drawClippedRegion = [&](juce::Rectangle<int> clipRegion)
        {
            if (clipRegion.getWidth() <= 0 || clipRegion.getHeight() <= 0)
                return;

            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(clipRegion);
            juce::ColourGradient clippedGradient(juce::Colour(0xffffc446).withAlpha(0.96f), plot.getCentreX(), plot.getY(),
                                                 juce::Colour(0xffff9f1a).withAlpha(0.86f), plot.getCentreX(), plot.getBottom(), false);
            g.setGradientFill(clippedGradient);
            g.fillPath(waveformBody);
            g.setColour(juce::Colour(0xffffc446).withAlpha(0.98f));
            g.fillPath(waveformPeak);
            g.setColour(juce::Colour(0xffffc446));
            g.strokePath(waveformTopEdge, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.strokePath(waveformBottomEdge, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        drawClippedRegion(topClipZone);
        drawClippedRegion(bottomClipZone);

        auto footer = juce::Rectangle<float>(plot.getX() + 10.0f, plot.getBottom() - 20.0f, plot.getWidth() - 20.0f, 16.0f);
        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(10.6f));
        g.drawText("Waveform", juce::Rectangle<int>((int) footer.getX(), (int) footer.getY(), 60, (int) footer.getHeight()), juce::Justification::centredLeft, false);
        g.drawText("Clipped", juce::Rectangle<int>((int) footer.getX() + 62, (int) footer.getY(), 48, (int) footer.getHeight()), juce::Justification::centredLeft, false);
        g.drawText("Ceiling", juce::Rectangle<int>((int) footer.getCentreX() - 54, (int) footer.getY(), 54, (int) footer.getHeight()), juce::Justification::centred, false);
        g.drawText("Trim", juce::Rectangle<int>((int) footer.getCentreX() + 4, (int) footer.getY(), 36, (int) footer.getHeight()), juce::Justification::centred, false);
        g.drawText("Clip Zone", juce::Rectangle<int>((int) footer.getRight() - 132, (int) footer.getY(), 60, (int) footer.getHeight()), juce::Justification::centredRight, false);
        g.drawText(juce::String(juce::roundToInt(snapshot.clipAmount * 100.0f)) + "%",
                   juce::Rectangle<int>((int) footer.getRight() - 64, (int) footer.getY(), 64, (int) footer.getHeight()),
                   juce::Justification::centredRight,
                   false);
    }

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Image cachedBackground;
    float cachedCeilingDb = std::numeric_limits<float>::max();
    float cachedTrimDb = std::numeric_limits<float>::max();
};

ClipperControlsComponent::ClipperControlsComponent(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
    : processor(processorToUse),
      apvts(state)
{
    badgeLabel.setText("MODERN CLIPPER SURFACE", juce::dontSendNotification);
    badgeLabel.setJustificationType(juce::Justification::centredLeft);
    badgeLabel.setFont(reverbui::smallCapsFont(11.5f));
    badgeLabel.setColour(juce::Label::textColourId, reverbui::brandCyan());
    addAndMakeVisible(badgeLabel);

    summaryLabel.setText("ClipperX-style layout, large waveform feedback, and SSP Reverb theming with selectable oversampling from 1x to 128x.",
                         juce::dontSendNotification);
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setFont(reverbui::bodyFont(12.0f));
    summaryLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    addAndMakeVisible(summaryLabel);

    addAndMakeVisible(engineStatusLabel);

    engineStatusLabel.setFont(reverbui::bodyFont(11.0f));
    engineStatusLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    engineStatusLabel.setJustificationType(juce::Justification::topLeft);

    clipTypeBox.addItemList(PluginProcessor::getClipTypeChoices(), 1);
    clipTypeBox.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(clipTypeBox);
    clipTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "clipType", clipTypeBox);

    oversamplingBox.addItemList(PluginProcessor::getOversamplingChoices(), 1);
    oversamplingBox.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(oversamplingBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "oversampling", oversamplingBox);

    visualizer = std::make_unique<ClipVisualizer>(processor, apvts);
    driveKnob = std::make_unique<ParameterKnob>(apvts, "driveDb", "Drive", reverbui::brandCyan(), formatSignedDb);
    ceilingKnob = std::make_unique<ParameterKnob>(apvts, "ceilingDb", "Ceiling", reverbui::brandGold(), formatPlainDb);
    trimKnob = std::make_unique<ParameterKnob>(apvts, "trimDb", "Trim", reverbui::brandMint(), formatSignedDb);
    mixKnob = std::make_unique<ParameterKnob>(apvts, "mix", "Mix", reverbui::brandCyan(), formatPercent);

    addAndMakeVisible(*visualizer);
    addAndMakeVisible(*driveKnob);
    addAndMakeVisible(*ceilingKnob);
    addAndMakeVisible(*trimKnob);
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
    auto topControls = area.removeFromTop(34);
    clipTypeBox.setBounds(topControls.removeFromLeft(162));
    topControls.removeFromLeft(12);
    engineStatusLabel.setBounds(topControls.removeFromLeft(310));
    topControls.removeFromLeft(12);
    oversamplingBox.setBounds(topControls.removeFromRight(152));

    area.removeFromTop(10);
    visualizer->setBounds(area.removeFromTop(420));

    area.removeFromTop(14);
    const int gap = 18;
    const int knobWidth = (area.getWidth() - gap * 3) / 4;
    driveKnob->setBounds(area.removeFromLeft(knobWidth));
    area.removeFromLeft(gap);
    ceilingKnob->setBounds(area.removeFromLeft(knobWidth));
    area.removeFromLeft(gap);
    trimKnob->setBounds(area.removeFromLeft(knobWidth));
    area.removeFromLeft(gap);
    mixKnob->setBounds(area);
}

void ClipperControlsComponent::timerCallback()
{
    if (! isShowing())
        return;

    refreshDynamicState();
    visualizer->repaint();
}

void ClipperControlsComponent::refreshDynamicState()
{
    const juce::String newStatus = processor.getActiveClipTypeLabel()
        + juce::String("  |  ")
        + processor.getActiveOversamplingLabel()
        + "  |  Latency "
        + juce::String(processor.getLatencySamples())
        + " samples";

    if (engineStatusLabel.getText() != newStatus)
        engineStatusLabel.setText(newStatus, juce::dontSendNotification);
}
