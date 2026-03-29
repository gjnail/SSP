#include "HihatGodControlsComponent.h"

namespace
{
using Formatter = std::function<juce::String(double)>;

juce::Colour accentSky() { return ssp::ui::brandCyan(); }
juce::Colour accentMint() { return ssp::ui::brandMint(); }
juce::Colour accentGold() { return ssp::ui::brandGold(); }
juce::Colour accentAmber() { return juce::Colour(0xffffad72); }
juce::Colour accentRose() { return juce::Colour(0xffff7f88); }
juce::Colour plotFill() { return juce::Colour(0xcc081018); }

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatSignedPercent(double value)
{
    const int rounded = juce::roundToInt((float) value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatDecibels(double value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatRate(double value)
{
    return PluginProcessor::getRateLabel(juce::roundToInt((float) value));
}

double getParameterDefaultValue(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
{
    if (auto* parameter = state.getParameter(parameterID))
        return parameter->convertFrom0to1(parameter->getDefaultValue());

    return 0.0;
}

void drawLegendSwatch(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, const juce::String& label)
{
    auto swatch = area.removeFromLeft(20.0f).withTrimmedTop(6.0f).withTrimmedBottom(6.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(swatch, 3.0f);
    g.setColour(ssp::ui::textMuted());
    g.setFont(11.0f);
    g.drawText(label, area.toNearestInt(), juce::Justification::centredLeft, false);
}
} // namespace

class HihatGodControlsComponent::HihatKnobPanel final : public juce::Component
{
public:
    HihatKnobPanel(juce::AudioProcessorValueTreeState& state,
                   const juce::String& parameterID,
                   const juce::String& title,
                   const juce::String& caption,
                   juce::Colour accentColour,
                   Formatter formatterToUse)
        : accent(accentColour),
          attachment(state, parameterID, slider),
          formatter(std::move(formatterToUse))
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(title, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setColour(juce::Label::textColourId, ssp::ui::textStrong());
        titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centredLeft);
        captionLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
        captionLabel.setFont(juce::Font(11.4f));

        slider.setName(title);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.28f));
        slider.setDoubleClickReturnValue(true, getParameterDefaultValue(state, parameterID));
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
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 18.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        titleLabel.setBounds(area.removeFromTop(18));
        auto captionArea = area.removeFromBottom(44);
        slider.setBounds(area.reduced(6, 2));
        captionLabel.setBounds(captionArea);
    }

private:
    juce::Colour accent;
    juce::Label titleLabel;
    juce::Label captionLabel;
    ssp::ui::SSPKnob slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
};

class HihatGodControlsComponent::MotionVisualizer final : public juce::Component
{
public:
    explicit MotionVisualizer(PluginProcessor& p)
        : processor(p)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        ssp::ui::drawPanelBackground(g, bounds, accentSky(), 20.0f);

        auto area = bounds.reduced(18.0f, 18.0f);
        auto header = area.removeFromTop(22.0f);
        g.setColour(ssp::ui::textStrong());
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("BEFORE / AFTER DIFFERENCE", header.toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(ssp::ui::textMuted());
        g.setFont(juce::Font(12.0f));
        g.drawText("Input/output envelope delta plus the live gain and pan modulation driving the effect.",
                   juce::Rectangle<int>((int) header.getRight() - 420, (int) header.getY(), 420, (int) header.getHeight()),
                   juce::Justification::centredRight,
                   false);

        auto legend = area.removeFromTop(18.0f);
        auto legendLeft = legend.removeFromLeft(300.0f);
        drawLegendSwatch(g, legendLeft.removeFromLeft(96.0f), juce::Colour(0xff8ea1b8), "Before");
        drawLegendSwatch(g, legendLeft.removeFromLeft(96.0f), accentSky(), "After");
        drawLegendSwatch(g, legendLeft.removeFromLeft(108.0f), accentAmber(), "Delta");

        auto meta = legend.removeFromRight(250.0f);
        g.setColour(ssp::ui::textMuted());
        g.setFont(11.0f);
        g.drawFittedText("Gain " + formatDecibels(processor.getCurrentGainOffsetDb())
                             + "    Pan " + formatSignedPercent(processor.getCurrentPanOffset() * 100.0f),
                         meta.toNearestInt(), juce::Justification::centredRight, 1);

        area.removeFromTop(8.0f);
        auto mainPlot = area.removeFromTop(area.getHeight() * 0.62f);
        area.removeFromTop(10.0f);
        auto gainPlot = area.removeFromTop(46.0f);
        area.removeFromTop(8.0f);
        auto panPlot = area.removeFromTop(46.0f);

        const auto data = processor.getVisualizerData();
        drawEnvelopePlot(g, mainPlot, data);
        drawMotionStrip(g, gainPlot, data.gainMotion, accentGold(), "GAIN LFO", formatDecibels(processor.getCurrentGainOffsetDb()));
        drawMotionStrip(g, panPlot, data.panMotion, accentMint(), "PAN LFO", formatSignedPercent(processor.getCurrentPanOffset() * 100.0f));
    }

private:
    static void drawPlotFrame(juce::Graphics& g, juce::Rectangle<float> bounds, bool signedData)
    {
        g.setColour(plotFill());
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(juce::Colour(0x28ffffff));
        g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

        for (int i = 1; i < 8; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, 8.0f, bounds.getX(), bounds.getRight());
            g.setColour(juce::Colour(0x14ffffff));
            g.drawVerticalLine((int) std::round(x), bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);
        }

        const int horizontalLines = signedData ? 4 : 3;
        for (int i = 0; i <= horizontalLines; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, (float) horizontalLines, bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);
            g.setColour(juce::Colour(0x14ffffff));
            g.drawHorizontalLine((int) std::round(y), bounds.getX() + 8.0f, bounds.getRight() - 8.0f);
        }

        if (signedData)
        {
            g.setColour(juce::Colour(0x36ffffff));
            g.drawHorizontalLine((int) std::round(bounds.getCentreY()), bounds.getX() + 6.0f, bounds.getRight() - 6.0f);
        }
    }

    static juce::Path buildPositivePath(const std::array<float, PluginProcessor::visualizerPointCount>& series,
                                        juce::Rectangle<float> bounds)
    {
        juce::Path path;
        for (int i = 0; i < PluginProcessor::visualizerPointCount; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (PluginProcessor::visualizerPointCount - 1), bounds.getX(), bounds.getRight());
            const float y = juce::jmap(series[(size_t) i], 0.0f, 1.0f, bounds.getBottom() - 8.0f, bounds.getY() + 8.0f);
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    }

    static juce::Path buildPositiveFill(const std::array<float, PluginProcessor::visualizerPointCount>& series,
                                        juce::Rectangle<float> bounds)
    {
        juce::Path path;
        path.startNewSubPath(bounds.getX(), bounds.getBottom() - 8.0f);
        for (int i = 0; i < PluginProcessor::visualizerPointCount; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (PluginProcessor::visualizerPointCount - 1), bounds.getX(), bounds.getRight());
            const float y = juce::jmap(series[(size_t) i], 0.0f, 1.0f, bounds.getBottom() - 8.0f, bounds.getY() + 8.0f);
            path.lineTo(x, y);
        }
        path.lineTo(bounds.getRight(), bounds.getBottom() - 8.0f);
        path.closeSubPath();
        return path;
    }

    static juce::Path buildSignedPath(const std::array<float, PluginProcessor::visualizerPointCount>& series,
                                      juce::Rectangle<float> bounds)
    {
        juce::Path path;
        for (int i = 0; i < PluginProcessor::visualizerPointCount; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (PluginProcessor::visualizerPointCount - 1), bounds.getX(), bounds.getRight());
            const float y = juce::jmap(series[(size_t) i], -1.0f, 1.0f, bounds.getBottom() - 8.0f, bounds.getY() + 8.0f);
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    }

    static juce::Path buildSignedFill(const std::array<float, PluginProcessor::visualizerPointCount>& series,
                                      juce::Rectangle<float> bounds)
    {
        juce::Path path;
        const float centreY = bounds.getCentreY();
        path.startNewSubPath(bounds.getX(), centreY);
        for (int i = 0; i < PluginProcessor::visualizerPointCount; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (PluginProcessor::visualizerPointCount - 1), bounds.getX(), bounds.getRight());
            const float y = juce::jmap(series[(size_t) i], -1.0f, 1.0f, bounds.getBottom() - 8.0f, bounds.getY() + 8.0f);
            path.lineTo(x, y);
        }
        path.lineTo(bounds.getRight(), centreY);
        path.closeSubPath();
        return path;
    }

    static void drawEnvelopePlot(juce::Graphics& g, juce::Rectangle<float> bounds, const PluginProcessor::VisualizerData& data)
    {
        drawPlotFrame(g, bounds, false);

        auto deltaFill = buildPositiveFill(data.difference, bounds);
        juce::ColourGradient deltaGradient(accentAmber().withAlpha(0.28f), bounds.getX(), bounds.getY(),
                                           accentRose().withAlpha(0.08f), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(deltaGradient);
        g.fillPath(deltaFill);

        auto beforePath = buildPositivePath(data.input, bounds);
        g.setColour(juce::Colour(0xff8ea1b8));
        g.strokePath(beforePath, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto afterPath = buildPositivePath(data.output, bounds);
        juce::ColourGradient afterGradient(accentSky(), bounds.getX(), bounds.getCentreY(),
                                           accentMint(), bounds.getRight(), bounds.getY(), false);
        g.setGradientFill(afterGradient);
        g.strokePath(afterPath, juce::PathStrokeType(2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawMotionStrip(juce::Graphics& g,
                                juce::Rectangle<float> bounds,
                                const std::array<float, PluginProcessor::visualizerPointCount>& series,
                                juce::Colour accent,
                                const juce::String& title,
                                const juce::String& value)
    {
        drawPlotFrame(g, bounds, true);

        auto fill = buildSignedFill(series, bounds);
        g.setColour(accent.withAlpha(0.18f));
        g.fillPath(fill);

        auto path = buildSignedPath(series, bounds);
        g.setColour(accent);
        g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto overlay = bounds.reduced(12.0f, 6.0f);
        auto labelRow = overlay.removeFromTop(14.0f);
        g.setColour(ssp::ui::textMuted());
        g.setFont(11.0f);
        g.drawText(title, labelRow.toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(ssp::ui::textStrong());
        g.drawText(value, labelRow.toNearestInt(), juce::Justification::centredRight, false);
    }

    PluginProcessor& processor;
};

HihatGodControlsComponent::HihatGodControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state),
      presetBrowser(p)
{
    for (auto* label : { &eyebrowLabel, &titleLabel, &subtitleLabel, &presetLabel, &presetMetaLabel, &statusLabel })
        addAndMakeVisible(*label);

    for (auto* button : { static_cast<juce::Component*>(&previousPresetButton),
                          static_cast<juce::Component*>(&presetButton),
                          static_cast<juce::Component*>(&nextPresetButton) })
    {
        addAndMakeVisible(*button);
    }

    eyebrowLabel.setText("HAT MOTION SYSTEM", juce::dontSendNotification);
    eyebrowLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    eyebrowLabel.setColour(juce::Label::textColourId, accentMint());
    eyebrowLabel.setJustificationType(juce::Justification::centredLeft);

    titleLabel.setText("SSP Hihat God", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(34.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, ssp::ui::textStrong());
    titleLabel.setJustificationType(juce::Justification::centredLeft);

    subtitleLabel.setText("Reactor and EQ-inspired vector UI for host-synced gain and stereo motion that keeps programmed hats feeling wider, looser, and less repetitive.",
                          juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);

    presetLabel.setText("PRESET LIBRARY", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(11.5f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, ssp::ui::textStrong());
    presetLabel.setJustificationType(juce::Justification::centredLeft);

    presetMetaLabel.setFont(juce::Font(11.0f));
    presetMetaLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);

    statusLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    statusLabel.setColour(juce::Label::textColourId, ssp::ui::brandGold());
    statusLabel.setJustificationType(juce::Justification::centredRight);

    previousPresetButton.onClick = [this]
    {
        processor.stepFactoryPreset(-1);
        updateDynamicText();
    };

    presetButton.onClick = [this]
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
    };

    nextPresetButton.onClick = [this]
    {
        processor.stepFactoryPreset(1);
        updateDynamicText();
    };

    visualizer = std::make_unique<MotionVisualizer>(processor);
    addAndMakeVisible(*visualizer);

    variationKnob = std::make_unique<HihatKnobPanel>(state,
                                                     "variation",
                                                     "Variation",
                                                     "Master depth for both gain and stereo motion.",
                                                     accentSky(),
                                                     formatPercent);
    rateKnob = std::make_unique<HihatKnobPanel>(state,
                                                "rateIndex",
                                                "Rate",
                                                "Host-synced timing for the movement cycle.",
                                                accentGold(),
                                                formatRate);
    volumeRangeKnob = std::make_unique<HihatKnobPanel>(state,
                                                       "volumeRangeDb",
                                                       "Volume Range",
                                                       "Maximum level swing applied across the cycle.",
                                                       accentAmber(),
                                                       formatDecibels);
    panRangeKnob = std::make_unique<HihatKnobPanel>(state,
                                                    "panRange",
                                                    "Pan Range",
                                                    "Maximum left-right image movement.",
                                                    accentMint(),
                                                    formatPercent);

    for (auto* knob : { variationKnob.get(), rateKnob.get(), volumeRangeKnob.get(), panRangeKnob.get() })
        addAndMakeVisible(*knob);

    addAndMakeVisible(presetBrowser);

    updateDynamicText();
    startTimerHz(24);
}

HihatGodControlsComponent::~HihatGodControlsComponent()
{
    stopTimer();
}

void HihatGodControlsComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void HihatGodControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(24, 20);

    auto header = area.removeFromTop(80);
    auto headerLeft = header.removeFromLeft((int) std::round(header.getWidth() * 0.60f));
    eyebrowLabel.setBounds(headerLeft.removeFromTop(16));
    titleLabel.setBounds(headerLeft.removeFromTop(36));
    subtitleLabel.setBounds(headerLeft);

    auto headerRight = header.reduced(0, 4);
    presetLabel.setBounds(headerRight.removeFromTop(16));
    auto presetRow = headerRight.removeFromTop(32);
    previousPresetButton.setBounds(presetRow.removeFromLeft(34).reduced(0, 2));
    presetRow.removeFromLeft(6);
    nextPresetButton.setBounds(presetRow.removeFromRight(34).reduced(0, 2));
    presetRow.removeFromRight(6);
    presetButton.setBounds(presetRow.reduced(0, 2));
    presetMetaLabel.setBounds(headerRight.removeFromTop(18));
    statusLabel.setBounds(headerRight);

    area.removeFromTop(12);
    visualizer->setBounds(area.removeFromTop((int) std::round(area.getHeight() * 0.56f)));

    area.removeFromTop(14);
    auto knobArea = area;
    const int gap = 14;
    const int knobWidth = (knobArea.getWidth() - gap * 3) / 4;
    variationKnob->setBounds(knobArea.removeFromLeft(knobWidth));
    knobArea.removeFromLeft(gap);
    rateKnob->setBounds(knobArea.removeFromLeft(knobWidth));
    knobArea.removeFromLeft(gap);
    volumeRangeKnob->setBounds(knobArea.removeFromLeft(knobWidth));
    knobArea.removeFromLeft(gap);
    panRangeKnob->setBounds(knobArea);

    presetBrowser.setBounds(getLocalBounds());
    presetBrowser.setAnchorBounds(presetButton.getBounds());
}

bool HihatGodControlsComponent::handleKeyPress(const juce::KeyPress& key)
{
    if (presetBrowser.handleKeyPress(key))
        return true;

    if (key == juce::KeyPress::leftKey)
    {
        processor.stepFactoryPreset(-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        processor.stepFactoryPreset(1);
        return true;
    }

    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    {
        if (presetBrowser.isOpen())
            presetBrowser.close();
        else
            presetBrowser.open();
        return true;
    }

    return false;
}

void HihatGodControlsComponent::timerCallback()
{
    updateDynamicText();
    visualizer->repaint();
}

void HihatGodControlsComponent::updateDynamicText()
{
    auto presetName = processor.getCurrentPresetName();
    if (processor.isCurrentPresetDirty())
        presetName << " *";
    presetButton.setButtonText(presetName);

    presetMetaLabel.setText("CATEGORY  " + processor.getCurrentPresetCategory().toUpperCase()
                                + "    TAGS  " + processor.getCurrentPresetTags().toUpperCase(),
                            juce::dontSendNotification);

    const auto rateIndex = juce::roundToInt(apvts.getRawParameterValue("rateIndex")->load());
    statusLabel.setText("GAIN " + formatDecibels(processor.getCurrentGainOffsetDb())
                            + "    PAN " + formatSignedPercent(processor.getCurrentPanOffset() * 100.0f)
                            + "    RATE " + PluginProcessor::getRateLabel(rateIndex).toUpperCase(),
                        juce::dontSendNotification);
}
