#include "HyperResonanceControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff39434b); }
juce::Colour subtleText() { return juce::Colour(0xffd2d2c4); }
juce::Colour spectralBlue() { return juce::Colour(0xff48d7ff); }
juce::Colour solarOrange() { return juce::Colour(0xffffa45e); }
juce::Colour ember() { return juce::Colour(0xffff7e52); }
juce::Colour paleGlow() { return juce::Colour(0xffffd690); }

juce::String formatFrequency(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, 2) + " kHz";

    return juce::String(juce::roundToInt((float) value)) + " Hz";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatDecibels(double value)
{
    if (std::abs(value) < 0.05)
        return "0.0 dB";

    return juce::String(value, 1) + " dB";
}
} // namespace

class HyperKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit HyperKnobLookAndFeel(bool heroStyle)
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
        const float stroke = hero ? juce::jmax(12.0f, radius * 0.13f) : juce::jmax(8.0f, radius * 0.11f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.26f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 9.0f : 6.0f));

        juce::ColourGradient halo(solarOrange().withAlpha(hero ? 0.23f : 0.16f), centre.x, bounds.getBottom(),
                                  spectralBlue().withAlpha(hero ? 0.16f : 0.10f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff26292d), centre.x, bounds.getY(),
                                   juce::Colour(0xff121316), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1a1d23));
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
        g.setColour(juce::Colour(0xff0f1012));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(spectralBlue(), arcBounds.getBottomLeft(), solarOrange(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff191a1e), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff242832), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 7.5f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(paleGlow());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff111319));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class HyperResonanceControlsComponent::HyperKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    HyperKnob(juce::AudioProcessorValueTreeState& state,
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
        titleLabel.setFont(juce::Font(hero ? 27.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 21.0f : 15.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, paleGlow());

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

    ~HyperKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff1a1f24) : juce::Colour(0xff171b21),
                                  area.getTopLeft(),
                                  juce::Colour(0xff111217),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff22262e));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 24.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(spectralBlue().withAlpha(0.68f), accent.getX(), accent.getCentreY(),
                                        solarOrange().withAlpha(0.82f), accent.getRight(), accent.getCentreY(), false);
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
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    HyperKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

class HyperResonanceControlsComponent::DispersionDisplay final : public juce::Component
{
public:
    explicit DispersionDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient background(juce::Colour(0xff171b22), area.getTopLeft(),
                                        juce::Colour(0xff0f1116), area.getBottomRight(), false);
        background.addColour(0.5, juce::Colour(0xff1a1f28));
        g.setGradientFill(background);
        g.fillRoundedRectangle(area, 24.0f);

        auto plot = area.reduced(24.0f, 20.0f);
        auto titleArea = plot.removeFromTop(28.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("Dispersion Focus", titleArea.toNearestInt(), juce::Justification::centredLeft);

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f));
        g.drawText("Synthetic group-delay view for the active phase cluster.", titleArea.toNearestInt(), juce::Justification::centredRight);

        plot.removeFromTop(8.0f);

        const float frequency = apvts.getRawParameterValue("frequencyHz")->load();
        const float amount = apvts.getRawParameterValue("amount")->load();
        const float pinch = apvts.getRawParameterValue("pinch")->load();
        const float spread = apvts.getRawParameterValue("spread")->load();

        g.setColour(juce::Colour(0xff0d1014));
        g.fillRoundedRectangle(plot, 18.0f);

        const std::array<float, 7> gridHz{40.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2500.0f, 6000.0f};
        const auto toX = [plot](float hz)
        {
            constexpr float minHz = 30.0f;
            constexpr float maxHz = 8000.0f;
            const float proportion = std::log(hz / minHz) / std::log(maxHz / minHz);
            return plot.getX() + proportion * plot.getWidth();
        };

        g.setColour(juce::Colour(0x22ffffff));
        for (float hz : gridHz)
        {
            const float x = toX(hz);
            g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
        }

        g.setColour(juce::Colour(0x18ffffff));
        g.drawHorizontalLine((int) std::round(plot.getCentreY()), plot.getX(), plot.getRight());

        const float centreX = toX(frequency);
        const float widthNormalised = juce::jmap(pinch, 0.0f, 1.0f, 0.34f, 0.08f);
        const float heightNormalised = 0.16f + amount * 0.78f;
        const float stereoBias = spread * plot.getWidth() * 0.08f;

        juce::Path fillPath;
        juce::Path linePath;

        const float baseLine = plot.getBottom() - 14.0f;
        fillPath.startNewSubPath(plot.getX(), baseLine);
        linePath.startNewSubPath(plot.getX(), baseLine);

        for (int i = 0; i <= 128; ++i)
        {
            const float t = (float) i / 128.0f;
            const float x = plot.getX() + t * plot.getWidth();
            const float distance = (x - centreX) / juce::jmax(18.0f, plot.getWidth() * widthNormalised);
            const float leftLobe = std::exp(-0.5f * std::pow(distance + spread * 0.85f, 2.0f));
            const float rightLobe = std::exp(-0.5f * std::pow(distance - spread * 0.85f, 2.0f));
            const float phaseRipple = 0.58f + 0.42f * std::cos(distance * juce::MathConstants<float>::pi * (1.35f + pinch * 2.8f));
            const float profile = juce::jmax(leftLobe, rightLobe) * phaseRipple * heightNormalised;
            const float y = baseLine - profile * (plot.getHeight() - 36.0f);

            fillPath.lineTo(x, y);
            linePath.lineTo(x, y);
        }

        fillPath.lineTo(plot.getRight(), baseLine);
        fillPath.closeSubPath();

        juce::ColourGradient fill(spectralBlue().withAlpha(0.42f), plot.getX(), plot.getBottom(),
                                  solarOrange().withAlpha(0.58f), plot.getRight(), plot.getY(), false);
        g.setGradientFill(fill);
        g.fillPath(fillPath);

        juce::ColourGradient stroke(paleGlow(), plot.getX(), plot.getCentreY(),
                                    spectralBlue(), plot.getRight(), plot.getCentreY(), false);
        g.setGradientFill(stroke);
        g.strokePath(linePath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(solarOrange().withAlpha(0.75f));
        g.fillEllipse(juce::Rectangle<float>(14.0f, 14.0f).withCentre(juce::Point<float>(centreX - stereoBias, baseLine - 8.0f)));
        g.setColour(spectralBlue().withAlpha(0.82f));
        g.fillEllipse(juce::Rectangle<float>(14.0f, 14.0f).withCentre(juce::Point<float>(centreX + stereoBias, baseLine - 8.0f)));

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), 24.0f, 1.0f);
        g.drawRoundedRectangle(plot.reduced(0.5f), 18.0f, 1.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class HyperResonanceControlsComponent::MeterColumn final : public juce::Component
{
public:
    MeterColumn(juce::String titleToUse, juce::Colour fillToUse)
        : title(std::move(titleToUse)),
          fillColour(fillToUse)
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
        juce::ColourGradient background(juce::Colour(0xff15191f), area.getX(), area.getY(),
                                        juce::Colour(0xff101216), area.getRight(), area.getBottom(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(area, 18.0f);

        auto meterBody = area.reduced(18.0f, 46.0f);
        g.setColour(juce::Colour(0xff0d1014));
        g.fillRoundedRectangle(meterBody, 12.0f);

        const auto clamped = juce::jlimit(0.0f, 1.0f, level);
        auto fillBounds = meterBody.withTrimmedTop(meterBody.getHeight() * (1.0f - clamped));
        juce::ColourGradient fill(fillColour.withAlpha(0.88f), fillBounds.getCentreX(), fillBounds.getBottom(),
                                  paleGlow().withAlpha(0.92f), fillBounds.getCentreX(), fillBounds.getY(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(fillBounds, 12.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), 18.0f, 1.0f);

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawFittedText(title, getLocalBounds().removeFromTop(24), juce::Justification::centred, 1);

        const juce::String valueText = juce::String(juce::Decibels::gainToDecibels(juce::jmax(clamped, 0.00001f), -60.0f), 1) + " dB";
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawFittedText(valueText, getLocalBounds().removeFromBottom(28), juce::Justification::centred, 1);
    }

private:
    juce::String title;
    juce::Colour fillColour;
    float level = 0.0f;
};

HyperResonanceControlsComponent::HyperResonanceControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p)
{
    summaryLabel.setText("Cluster all-pass filters around a chosen frequency to pull transients forward, smear them backward, or make drums bloom without touching the EQ curve directly.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("PHASE BLOOM", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, paleGlow());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff615849));
    addAndMakeVisible(badgeLabel);

    display = std::make_unique<DispersionDisplay>(state);
    frequencyKnob = std::make_unique<HyperKnob>(state, "frequencyHz", "Frequency", "Tune the phase focus in Hertz.", formatFrequency, true);
    amountKnob = std::make_unique<HyperKnob>(state, "amount", "Amount", "Adds more stacked all-pass motion and depth.", formatPercent, true);
    pinchKnob = std::make_unique<HyperKnob>(state, "pinch", "Pinch", "Tighten the cluster around the target band.", formatPercent, false);
    spreadKnob = std::make_unique<HyperKnob>(state, "spread", "Spread", "Offsets the left and right focus points.", formatPercent, false);
    mixKnob = std::make_unique<HyperKnob>(state, "mix", "Mix", "Blend dry punch with dispersed motion.", formatPercent, false);
    outputKnob = std::make_unique<HyperKnob>(state, "outputDb", "Output", "Trim the result after the phase stack.", formatDecibels, false);

    inputMeter = std::make_unique<MeterColumn>("Input", spectralBlue());
    resonanceMeter = std::make_unique<MeterColumn>("Phase", solarOrange());
    outputMeter = std::make_unique<MeterColumn>("Output", ember());

    addAndMakeVisible(*display);
    addAndMakeVisible(*frequencyKnob);
    addAndMakeVisible(*amountKnob);
    addAndMakeVisible(*pinchKnob);
    addAndMakeVisible(*spreadKnob);
    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(*inputMeter);
    addAndMakeVisible(*resonanceMeter);
    addAndMakeVisible(*outputMeter);

    refreshMeters();
    startTimerHz(24);
}

HyperResonanceControlsComponent::~HyperResonanceControlsComponent()
{
    stopTimer();
}

void HyperResonanceControlsComponent::timerCallback()
{
    refreshMeters();
    if (display != nullptr)
        display->repaint();
}

void HyperResonanceControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    resonanceMeter->setLevel(processor.getResonanceMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void HyperResonanceControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff171b20), bounds.getTopLeft(),
                               juce::Colour(0xff101216), bounds.getBottomRight(), false);
    shell.addColour(0.48, juce::Colour(0xff1b2027));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(26.0f, 16.0f).removeFromTop(100.0f);
    juce::ColourGradient glow(spectralBlue().withAlpha(0.16f), topGlow.getX(), topGlow.getY(),
                              solarOrange().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 20.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void HyperResonanceControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(60);
    summaryLabel.setBounds(topRow.removeFromTop(32));
    badgeLabel.setBounds(topRow.removeFromRight(136).removeFromTop(22));

    area.removeFromTop(14);

    auto meterArea = area.removeFromRight(190);
    auto mainArea = area;

    auto displayArea = mainArea.removeFromTop((int) std::round(mainArea.getHeight() * 0.39f));
    display->setBounds(displayArea);

    mainArea.removeFromTop(12);
    auto heroRow = mainArea.removeFromTop((int) std::round(mainArea.getHeight() * 0.58f));
    auto frequencyArea = heroRow.removeFromLeft(heroRow.getWidth() / 2);
    heroRow.removeFromLeft(12);
    auto amountArea = heroRow;
    frequencyKnob->setBounds(frequencyArea);
    amountKnob->setBounds(amountArea);

    mainArea.removeFromTop(12);
    const int smallWidth = (mainArea.getWidth() - 36) / 4;
    auto pinchArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto spreadArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto mixArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto outputArea = mainArea;
    pinchKnob->setBounds(pinchArea);
    spreadKnob->setBounds(spreadArea);
    mixKnob->setBounds(mixArea);
    outputKnob->setBounds(outputArea);

    const int meterWidth = (meterArea.getWidth() - 24) / 3;
    auto inputArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto phaseArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto meterOutputArea = meterArea;
    inputMeter->setBounds(inputArea);
    resonanceMeter->setBounds(phaseArea);
    outputMeter->setBounds(meterOutputArea);
}
