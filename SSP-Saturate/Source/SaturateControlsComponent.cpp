#include "SaturateControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff4b3f38); }
juce::Colour subtleText() { return juce::Colour(0xffdac9bf); }
juce::Colour shellText() { return juce::Colour(0xfff7efe8); }
juce::Colour ember() { return juce::Colour(0xffff9a63); }
juce::Colour gold() { return juce::Colour(0xffffcf72); }
juce::Colour rose() { return juce::Colour(0xffff6f5f); }

juce::String formatDrive(double value)
{
    return "+" + juce::String(value, 1) + " dB";
}

juce::String formatSignedPercent(double value)
{
    const auto rounded = juce::roundToInt((float) value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatDecibels(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

enum class DisplayMode
{
    warm = 0,
    tube,
    tape,
    diode,
    hard
};

juce::StringArray modeNames()
{
    return {"Warm", "Tube", "Tape", "Diode", "Hard"};
}

float shapePreviewSample(float sample, float color, float asymmetry, DisplayMode mode) noexcept
{
    switch (mode)
    {
        case DisplayMode::warm:
        {
            const float warm = std::tanh(sample + asymmetry);
            const float brightInput = juce::jlimit(-3.0f, 3.0f, sample + asymmetry * 0.8f);
            const float cubic = brightInput - 0.16f * brightInput * brightInput * brightInput;
            const float bright = juce::jlimit(-1.2f, 1.2f, cubic);
            return juce::jmap(color, warm, bright);
        }

        case DisplayMode::tube:
        {
            const float tubeInput = sample + asymmetry * 1.1f;
            const float soft = std::tanh(tubeInput * juce::jmap(color, 0.0f, 1.0f, 0.9f, 1.8f));
            const float even = tubeInput / (1.0f + std::abs(tubeInput) * juce::jmap(color, 0.0f, 1.0f, 0.65f, 1.4f));
            return juce::jlimit(-1.25f, 1.25f, juce::jmap(0.42f + color * 0.36f, even, soft));
        }

        case DisplayMode::tape:
        {
            const float tapeInput = sample + asymmetry * 0.45f;
            const float compressed = std::atan(tapeInput * juce::jmap(color, 0.0f, 1.0f, 1.0f, 3.0f)) / 1.25f;
            const float rounded = std::tanh((tapeInput - 0.06f * tapeInput * tapeInput * tapeInput) * 0.92f);
            return juce::jlimit(-1.2f, 1.2f, juce::jmap(color * 0.65f, rounded, compressed));
        }

        case DisplayMode::diode:
        {
            const float diodeInput = sample + asymmetry * 1.4f;
            const float fold = diodeInput / (1.0f + std::abs(diodeInput) * juce::jmap(color, 0.0f, 1.0f, 1.8f, 4.0f));
            const float clipped = juce::jlimit(-1.0f, 1.0f, diodeInput * juce::jmap(color, 0.0f, 1.0f, 1.1f, 2.8f));
            return juce::jlimit(-1.15f, 1.15f, juce::jmap(0.35f + color * 0.45f, fold, clipped));
        }

        case DisplayMode::hard:
        {
            const float hardInput = sample + asymmetry * 0.9f;
            const float cubic = hardInput - 0.12f * hardInput * hardInput * hardInput;
            const float clipped = juce::jlimit(-1.0f, 1.0f, hardInput * juce::jmap(color, 0.0f, 1.0f, 1.2f, 3.4f));
            return juce::jlimit(-1.1f, 1.1f, juce::jmap(0.30f + color * 0.55f, cubic, clipped));
        }
    }

    return std::tanh(sample + asymmetry);
}
} // namespace

class SaturateControlsComponent::WaveshaperDisplay final : public juce::Component
{
public:
    explicit WaveshaperDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff15100d), bounds.getTopLeft(),
                                  juce::Colour(0xff0e0a09), bounds.getBottomRight(),
                                  false);
        fill.addColour(0.52, juce::Colour(0xff1c1512));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 24.0f);

        auto plot = bounds.reduced(18.0f, 16.0f);
        const auto titleArea = plot.removeFromTop(18.0f);

        const auto drawX = [&](float x) { return plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth(); };
        const auto drawY = [&](float y) { return plot.getCentreY() - y * 0.5f * plot.getHeight(); };

        g.setColour(juce::Colour(0x22ffffff));
        for (int i = 0; i < 5; ++i)
        {
            const float t = juce::jmap((float) i, 0.0f, 4.0f, -1.0f, 1.0f);
            g.drawHorizontalLine((int) std::round(drawY(t)), plot.getX(), plot.getRight());
            g.drawVerticalLine((int) std::round(drawX(t)), plot.getY(), plot.getBottom());
        }

        juce::Path diagonal;
        diagonal.startNewSubPath(drawX(-1.0f), drawY(-1.0f));
        diagonal.lineTo(drawX(1.0f), drawY(1.0f));
        g.setColour(juce::Colour(0x55f7efe8));
        g.strokePath(diagonal, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float driveDb = juce::jlimit(0.0f, 24.0f, apvts.getRawParameterValue("drive")->load());
        const float color = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("color")->load() / 100.0f);
        const float bias = juce::jlimit(-1.0f, 1.0f, apvts.getRawParameterValue("bias")->load() / 100.0f);
        const auto mode = (DisplayMode) juce::jlimit(0, modeNames().size() - 1, juce::roundToInt(apvts.getRawParameterValue("mode")->load()));
        const float driveGain = juce::Decibels::decibelsToGain(driveDb);
        const float asymmetry = bias * 0.38f;

        juce::Path curve;
        for (int i = 0; i <= 160; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, 160.0f, -1.0f, 1.0f);
            const float driven = x * driveGain;
            const float saturated = shapePreviewSample(driven, color, asymmetry, mode);
            const float y = juce::jlimit(-1.15f, 1.15f, saturated);

            const float px = drawX(x);
            const float py = drawY(y);
            if (i == 0)
                curve.startNewSubPath(px, py);
            else
                curve.lineTo(px, py);
        }

        juce::Path fillPath(curve);
        fillPath.lineTo(drawX(1.0f), drawY(-1.0f));
        fillPath.lineTo(drawX(-1.0f), drawY(-1.0f));
        fillPath.closeSubPath();

        juce::ColourGradient shapeFill(ember().withAlpha(0.22f), plot.getX(), plot.getY(),
                                       rose().withAlpha(0.10f), plot.getRight(), plot.getBottom(), false);
        g.setGradientFill(shapeFill);
        g.fillPath(fillPath);

        g.setColour(gold());
        g.strokePath(curve, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("WAVESHAPER CURVE", titleArea.toNearestInt(), juce::Justification::topLeft, false);

        g.setFont(juce::Font(11.0f));
        g.drawText(modeNames()[(int) mode],
                   juce::Rectangle<int>((int) bounds.getX() + 150, (int) bounds.getY() + 12, 80, 20),
                   juce::Justification::centredLeft,
                   false);

        auto legendArea = juce::Rectangle<float>(bounds.getRight() - 180.0f, bounds.getY() + 12.0f, 158.0f, 24.0f);
        g.setColour(juce::Colour(0x55f7efe8));
        g.drawLine(legendArea.getX(), legendArea.getCentreY(), legendArea.getX() + 24.0f, legendArea.getCentreY(), 2.0f);
        g.setColour(subtleText());
        g.drawText("Dry",
                   juce::Rectangle<int>((int) legendArea.getX() + 30, (int) legendArea.getY(), 36, (int) legendArea.getHeight()),
                   juce::Justification::centredLeft,
                   false);

        g.setColour(gold());
        g.drawLine(legendArea.getX() + 62.0f, legendArea.getCentreY(), legendArea.getX() + 86.0f, legendArea.getCentreY(), 3.0f);
        g.setColour(subtleText());
        g.drawText("Saturated",
                   juce::Rectangle<int>((int) legendArea.getX() + 92, (int) legendArea.getY(), 62, (int) legendArea.getHeight()),
                   juce::Justification::centredLeft,
                   false);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 24.0f, 1.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class SaturateKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit SaturateKnobLookAndFeel(bool heroStyle)
        : hero(heroStyle)
    {
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(hero ? 8.0f : 10.0f);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float stroke = hero ? juce::jmax(13.0f, radius * 0.13f) : juce::jmax(8.0f, radius * 0.11f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.26f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 9.0f : 6.0f));

        juce::ColourGradient halo(ember().withAlpha(hero ? 0.28f : 0.18f), centre.x, bounds.getBottom(),
                                  rose().withAlpha(hero ? 0.16f : 0.10f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff2a1f1a), centre.x, bounds.getY(),
                                   juce::Colour(0xff15100e), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1e1713));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        juce::Path baseArc;
        const auto arcBounds = bounds.reduced(radius * 0.08f);
        baseArc.addCentredArc(centre.x, centre.y,
                              arcBounds.getWidth() * 0.5f,
                              arcBounds.getHeight() * 0.5f,
                              0.0f,
                              rotaryStartAngle,
                              rotaryEndAngle,
                              true);
        g.setColour(juce::Colour(0xff100c0a));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(gold(), arcBounds.getBottomLeft(), ember(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff1d1613), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff2b211d), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(shellText());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff15100e));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class SaturateControlsComponent::SaturateKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    SaturateKnob(juce::AudioProcessorValueTreeState& state,
                 const juce::String& paramId,
                 const juce::String& heading,
                 const juce::String& caption,
                 Formatter formatterToUse,
                 bool heroStyle)
        : lookAndFeel(heroStyle),
          attachment(state, paramId, slider),
          formatter(std::move(formatterToUse)),
          hero(heroStyle)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(hero ? 30.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 22.0f : 15.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, gold());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.3f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~SaturateKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff211711) : juce::Colour(0xff1b1410),
                                  area.getTopLeft(),
                                  juce::Colour(0xff120e0c),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff2a1d17));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 28.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(gold().withAlpha(0.62f), accent.getX(), accent.getCentreY(),
                                        ember().withAlpha(0.78f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.18f).withTrimmedRight(accent.getWidth() * 0.18f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 28.0f : 20.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 20 : 14, hero ? 16 : 12);

        auto header = area.removeFromTop(hero ? 54 : 40);
        titleLabel.setBounds(header.removeFromTop(hero ? 28 : 20));
        valueLabel.setBounds(header);

        const int captionHeight = hero ? 26 : 22;
        captionLabel.setBounds(area.removeFromBottom(captionHeight));
        area.removeFromBottom(hero ? 10 : 8);

        area = area.reduced(hero ? 6 : 4, 0);
        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    SaturateKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

SaturateControlsComponent::SaturateControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Push the input into warm saturation, tune the tone and asymmetry, and watch the shaping curve change in real time.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("ANALOG HEAT", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, gold());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff6a523a));
    addAndMakeVisible(badgeLabel);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    modeLabel.setColour(juce::Label::textColourId, shellText());
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modeLabel);

    modeBox.addItemList(modeNames(), 1);
    modeBox.setJustificationType(juce::Justification::centred);
    modeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1b1410));
    modeBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    modeBox.setColour(juce::ComboBox::textColourId, shellText());
    modeBox.setColour(juce::ComboBox::arrowColourId, gold());
    modeBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff241914));
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);

    for (auto* label : { &inputMeterLabel, &heatMeterLabel, &outputMeterLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::Font(16.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, shellText());
        addAndMakeVisible(*label);
    }

    waveshaperDisplay = std::make_unique<WaveshaperDisplay>(state);

    driveKnob = std::make_unique<SaturateKnob>(state,
                                               "drive",
                                               "Drive",
                                               "Pushes the signal deeper into saturation.",
                                               formatDrive,
                                               true);
    colorKnob = std::make_unique<SaturateKnob>(state,
                                               "color",
                                               "Color",
                                               "Moves the saturation from darker to brighter.",
                                               formatPercent,
                                               false);
    biasKnob = std::make_unique<SaturateKnob>(state,
                                              "bias",
                                              "Bias",
                                              "Adds asymmetry for more uneven harmonics.",
                                              formatSignedPercent,
                                              false);
    outputKnob = std::make_unique<SaturateKnob>(state,
                                                "output",
                                                "Output",
                                                "Trims the final level after the drive stage.",
                                                formatDecibels,
                                                false);
    mixKnob = std::make_unique<SaturateKnob>(state,
                                             "mix",
                                             "Mix",
                                             "Blends the saturated signal with the dry input.",
                                             formatPercent,
                                             false);

    addAndMakeVisible(*waveshaperDisplay);
    addAndMakeVisible(*driveKnob);
    addAndMakeVisible(*colorKnob);
    addAndMakeVisible(*biasKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(*mixKnob);

    refreshMeters();
    startTimerHz(24);
}

SaturateControlsComponent::~SaturateControlsComponent()
{
    stopTimer();
}

void SaturateControlsComponent::timerCallback()
{
    refreshMeters();
    if (waveshaperDisplay != nullptr)
        waveshaperDisplay->repaint();
}

void SaturateControlsComponent::refreshMeters()
{
    inputMeterLabel.setText("Input: " + juce::String(juce::roundToInt(processor.getInputMeterLevel() * 100.0f)) + "%",
                            juce::dontSendNotification);
    heatMeterLabel.setText("Heat: " + juce::String(juce::roundToInt(processor.getHeatMeterLevel() * 100.0f)) + "%",
                           juce::dontSendNotification);
    outputMeterLabel.setText("Output: " + juce::String(juce::roundToInt(processor.getOutputMeterLevel() * 100.0f)) + "%",
                             juce::dontSendNotification);
}

void SaturateControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff17110e), bounds.getTopLeft(),
                               juce::Colour(0xff0d0a09), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff221813));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 30.0f);

    auto topGlow = bounds.reduced(24.0f, 16.0f).removeFromTop(112.0f);
    juce::ColourGradient glow(ember().withAlpha(0.18f), topGlow.getX(), topGlow.getY(),
                              rose().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 22.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 30.0f, 1.0f);
}

void SaturateControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(78);
    summaryLabel.setBounds(topRow.removeFromTop(34));

    auto lowerTop = topRow;
    badgeLabel.setBounds(lowerTop.removeFromLeft(150).reduced(0, 2));
    lowerTop.removeFromLeft(16);
    modeLabel.setBounds(lowerTop.removeFromLeft(54));
    modeBox.setBounds(lowerTop.removeFromLeft(128).reduced(0, 1));
    lowerTop.removeFromLeft(18);
    inputMeterLabel.setBounds(lowerTop.removeFromLeft(150));
    heatMeterLabel.setBounds(lowerTop.removeFromLeft(150));
    outputMeterLabel.setBounds(lowerTop.removeFromLeft(160));

    area.removeFromTop(14);

    auto previewArea = area.removeFromTop(180);
    waveshaperDisplay->setBounds(previewArea);

    area.removeFromTop(18);

    auto heroArea = area.removeFromLeft((int) std::round(area.getWidth() * 0.42f));
    area.removeFromLeft(18);
    auto rightArea = area;

    driveKnob->setBounds(heroArea);

    auto topRight = rightArea.removeFromTop((rightArea.getHeight() - 18) / 2);
    auto topLeftSmall = topRight.removeFromLeft((topRight.getWidth() - 18) / 2);
    colorKnob->setBounds(topLeftSmall);
    topRight.removeFromLeft(18);
    biasKnob->setBounds(topRight);

    rightArea.removeFromTop(18);
    auto bottomLeftSmall = rightArea.removeFromLeft((rightArea.getWidth() - 18) / 2);
    outputKnob->setBounds(bottomLeftSmall);
    rightArea.removeFromLeft(18);
    mixKnob->setBounds(rightArea);
}
