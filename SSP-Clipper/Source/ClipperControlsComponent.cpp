#include "ClipperControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3a3137); }
juce::Colour subtleText() { return juce::Colour(0xffb0bac4); }
juce::Colour emberBright() { return juce::Colour(0xffffc057); }
juce::Colour emberRed() { return juce::Colour(0xffff5b3a); }

juce::String formatSignedDb(double value)
{
    if (std::abs(value) < 0.05)
        return "0.0 dB";

    const juce::String magnitude(std::abs(value), 1);
    return (value > 0.0 ? "+" : "-") + magnitude + " dB";
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

class ClipperKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit ClipperKnobLookAndFeel(bool heroStyle)
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
        const float stroke = hero ? juce::jmax(13.0f, radius * 0.14f) : juce::jmax(8.0f, radius * 0.11f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.30f : 0.20f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 10.0f : 7.0f));

        juce::ColourGradient glow(emberRed().withAlpha(hero ? 0.26f : 0.18f), centre.x, bounds.getBottom(),
                                  emberBright().withAlpha(hero ? 0.14f : 0.10f), centre.x, bounds.getY(), false);
        g.setGradientFill(glow);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff2c262c), centre.x, bounds.getY(), juce::Colour(0xff121318), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1d1a21));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        juce::Path baseArc;
        const auto arcBounds = bounds.reduced(radius * 0.08f);
        baseArc.addCentredArc(centre.x, centre.y, arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff0d0f13));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f, 0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient accent(emberRed(), arcBounds.getBottomLeft(), emberBright(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff171920), cap.getCentreX(), cap.getY(), juce::Colour(0xff212733), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 7.5f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(emberBright());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff101318));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class ClipperControlsComponent::ClipperKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    ClipperKnob(juce::AudioProcessorValueTreeState& state,
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
        titleLabel.setFont(juce::Font(hero ? 28.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 22.0f : 15.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, emberBright());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.5f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~ClipperKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff1a171d) : juce::Colour(0xff17161c),
                                  area.getTopLeft(), juce::Colour(0xff101117), area.getBottomRight(), false);
        fill.addColour(0.46, juce::Colour(0xff1c1b23));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 24.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(emberRed().withAlpha(0.60f), accent.getX(), accent.getCentreY(), emberBright().withAlpha(0.90f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.20f).withTrimmedRight(accent.getWidth() * 0.20f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 24.0f : 20.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 20 : 16, hero ? 18 : 14);
        titleLabel.setBounds(area.removeFromTop(hero ? 34 : 24));
        valueLabel.setBounds(area.removeFromTop(hero ? 28 : 22));
        captionLabel.setBounds(area.removeFromBottom(24));
        area.removeFromBottom(hero ? 8 : 6);

        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2), juce::dontSendNotification);
    }

    ClipperKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

class ClipperControlsComponent::MeterColumn final : public juce::Component
{
public:
    MeterColumn(juce::String titleToUse, juce::Colour fillToUse, bool percentMode)
        : title(std::move(titleToUse)),
          fillColour(fillToUse),
          showPercent(percentMode)
    {
    }

    void setLevel(float newLevel)
    {
        level = juce::jlimit(0.0f, 1.2f, newLevel);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient background(juce::Colour(0xff18171d), area.getX(), area.getY(), juce::Colour(0xff111218), area.getRight(), area.getBottom(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(area, 18.0f);

        auto meterBody = area.reduced(18.0f, 46.0f);
        g.setColour(juce::Colour(0xff0d0e13));
        g.fillRoundedRectangle(meterBody, 12.0f);

        const auto clamped = juce::jlimit(0.0f, 1.0f, level);
        auto fillBounds = meterBody.withTrimmedTop(meterBody.getHeight() * (1.0f - clamped));
        juce::ColourGradient fill(fillColour.withAlpha(0.88f), fillBounds.getCentreX(), fillBounds.getBottom(), emberBright().withAlpha(0.95f), fillBounds.getCentreX(), fillBounds.getY(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(fillBounds, 12.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), 18.0f, 1.0f);

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawFittedText(title, getLocalBounds().removeFromTop(24), juce::Justification::centred, 1);

        juce::String valueText;
        if (showPercent)
            valueText = juce::String(juce::roundToInt(clamped * 100.0f)) + "%";
        else
            valueText = juce::String(juce::Decibels::gainToDecibels(juce::jmax(clamped, 0.00001f), -60.0f), 1) + " dB";

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawFittedText(valueText, getLocalBounds().removeFromBottom(28), juce::Justification::centred, 1);
    }

private:
    juce::String title;
    juce::Colour fillColour;
    float level = 0.0f;
    bool showPercent = false;
};

ClipperControlsComponent::ClipperControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Fast utility clipper for shaving peaks, leaning into crunch, and staying clean enough for mix-bus prep.", juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    oversamplingLabel.setText("FIXED 4x OVERSAMPLING", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    oversamplingLabel.setJustificationType(juce::Justification::centred);
    oversamplingLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    oversamplingLabel.setColour(juce::Label::backgroundColourId, emberBright());
    oversamplingLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff3c3327));
    addAndMakeVisible(oversamplingLabel);

    driveKnob = std::make_unique<ClipperKnob>(state, "driveDb", "Drive", "Push harder into the clipper.", formatSignedDb, true);
    ceilingKnob = std::make_unique<ClipperKnob>(state, "ceilingDb", "Ceiling", "Set the clip target for the wet path.", formatPlainDb, true);
    softnessKnob = std::make_unique<ClipperKnob>(state, "softness", "Softness", "Morph from hard edges to a rounder shave.", formatPercent, false);
    trimKnob = std::make_unique<ClipperKnob>(state, "trimDb", "Trim", "Level-match after clipping.", formatSignedDb, false);
    mixKnob = std::make_unique<ClipperKnob>(state, "mix", "Mix", "Blend the clipped signal against dry.", formatPercent, false);

    inputMeter = std::make_unique<MeterColumn>("Input", juce::Colour(0xffff9d4d), false);
    clipMeter = std::make_unique<MeterColumn>("Clip", juce::Colour(0xffff5b3a), true);
    outputMeter = std::make_unique<MeterColumn>("Output", juce::Colour(0xffffc057), false);

    addAndMakeVisible(*driveKnob);
    addAndMakeVisible(*ceilingKnob);
    addAndMakeVisible(*softnessKnob);
    addAndMakeVisible(*trimKnob);
    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*inputMeter);
    addAndMakeVisible(*clipMeter);
    addAndMakeVisible(*outputMeter);

    refreshMeters();
    startTimerHz(24);
}

ClipperControlsComponent::~ClipperControlsComponent()
{
    stopTimer();
}

void ClipperControlsComponent::timerCallback()
{
    refreshMeters();
}

void ClipperControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    clipMeter->setLevel(processor.getClipMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void ClipperControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff16141b), bounds.getTopLeft(), juce::Colour(0xff101116), bounds.getBottomRight(), false);
    shell.addColour(0.48, juce::Colour(0xff1b1820));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(26.0f, 16.0f).removeFromTop(100.0f);
    juce::ColourGradient glow(juce::Colour(0x24ff8a37), topGlow.getX(), topGlow.getY(), juce::Colour(0x00ff8a37), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 20.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void ClipperControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(38);
    auto badgeArea = topRow.removeFromRight(190);
    summaryLabel.setBounds(topRow);
    oversamplingLabel.setBounds(badgeArea.reduced(0, 4));

    area.removeFromTop(14);

    auto meterArea = area.removeFromRight(190);
    auto mainArea = area;

    auto heroRow = mainArea.removeFromTop((int) std::round(mainArea.getHeight() * 0.60f));
    auto driveArea = heroRow.removeFromLeft(heroRow.getWidth() / 2);
    heroRow.removeFromLeft(12);
    auto ceilingArea = heroRow;
    driveKnob->setBounds(driveArea);
    ceilingKnob->setBounds(ceilingArea);

    mainArea.removeFromTop(12);
    const int smallWidth = (mainArea.getWidth() - 24) / 3;
    auto softnessArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto trimArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto mixArea = mainArea;
    softnessKnob->setBounds(softnessArea);
    trimKnob->setBounds(trimArea);
    mixKnob->setBounds(mixArea);

    const int meterWidth = (meterArea.getWidth() - 24) / 3;
    auto inputArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto clipArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto outputArea = meterArea;
    inputMeter->setBounds(inputArea);
    clipMeter->setBounds(clipArea);
    outputMeter->setBounds(outputArea);
}
