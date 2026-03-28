#include "EchoControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3a3227); }
juce::Colour subtleText() { return juce::Colour(0xffc2b7a6); }
juce::Colour warmGold() { return juce::Colour(0xffffc76f); }
juce::Colour warmOrange() { return juce::Colour(0xffff8c42); }
juce::Colour mossGreen() { return juce::Colour(0xff9fba72); }

juce::String formatMilliseconds(double value)
{
    return juce::String(juce::roundToInt((float) value)) + " ms";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatDrive(double value)
{
    return "+" + juce::String(value, 1) + " dB";
}
} // namespace

class EchoKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit EchoKnobLookAndFeel(bool heroStyle)
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

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.28f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 9.0f : 6.0f));

        juce::ColourGradient halo(warmOrange().withAlpha(hero ? 0.26f : 0.18f), centre.x, bounds.getBottom(),
                                  mossGreen().withAlpha(hero ? 0.14f : 0.10f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff322b22), centre.x, bounds.getY(),
                                   juce::Colour(0xff141518), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff232018));
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
        g.setColour(juce::Colour(0xff0f1114));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(warmOrange(), arcBounds.getBottomLeft(), warmGold(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff171a1f), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff1f262f), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 7.5f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(warmGold());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff101318));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class EchoControlsComponent::EchoKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    EchoKnob(juce::AudioProcessorValueTreeState& state,
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
        valueLabel.setColour(juce::Label::textColourId, warmGold());

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

    ~EchoKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff1d1a16) : juce::Colour(0xff181613),
                                  area.getTopLeft(),
                                  juce::Colour(0xff101114),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff1d1a17));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 24.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(warmOrange().withAlpha(0.62f), accent.getX(), accent.getCentreY(),
                                        mossGreen().withAlpha(0.72f), accent.getRight(), accent.getCentreY(), false);
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

    EchoKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

class EchoControlsComponent::MeterColumn final : public juce::Component
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
        juce::ColourGradient background(juce::Colour(0xff181715), area.getX(), area.getY(),
                                        juce::Colour(0xff111214), area.getRight(), area.getBottom(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(area, 18.0f);

        auto meterBody = area.reduced(18.0f, 46.0f);
        g.setColour(juce::Colour(0xff0d0e12));
        g.fillRoundedRectangle(meterBody, 12.0f);

        const auto clamped = juce::jlimit(0.0f, 1.0f, level);
        auto fillBounds = meterBody.withTrimmedTop(meterBody.getHeight() * (1.0f - clamped));
        juce::ColourGradient fill(fillColour.withAlpha(0.88f), fillBounds.getCentreX(), fillBounds.getBottom(),
                                  warmGold().withAlpha(0.95f), fillBounds.getCentreX(), fillBounds.getY(), false);
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

EchoControlsComponent::EchoControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Vintage-leaning repeats with filtered feedback, a little repeat drive, and modulated pitch drift to keep echoes moving.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("COLORED REPEATS", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, warmGold());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff433421));
    addAndMakeVisible(badgeLabel);

    timeKnob = std::make_unique<EchoKnob>(state, "timeMs", "Time", "Delay time before the repeat blooms.", formatMilliseconds, true);
    feedbackKnob = std::make_unique<EchoKnob>(state, "feedback", "Feedback", "How long the echoes keep circling.", formatPercent, true);
    mixKnob = std::make_unique<EchoKnob>(state, "mix", "Mix", "Blend echoes under the dry source.", formatPercent, false);
    colorKnob = std::make_unique<EchoKnob>(state, "color", "Color", "Dark, worn repeats to brighter reflections.", formatPercent, false);
    driveKnob = std::make_unique<EchoKnob>(state, "driveDb", "Drive", "Push repeat saturation harder.", formatDrive, false);
    flutterKnob = std::make_unique<EchoKnob>(state, "flutter", "Flutter", "Pitch drift and movement on the repeats.", formatPercent, false);

    inputMeter = std::make_unique<MeterColumn>("Input", mossGreen());
    echoMeter = std::make_unique<MeterColumn>("Echo", warmOrange());
    outputMeter = std::make_unique<MeterColumn>("Output", warmGold());

    addAndMakeVisible(*timeKnob);
    addAndMakeVisible(*feedbackKnob);
    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*colorKnob);
    addAndMakeVisible(*driveKnob);
    addAndMakeVisible(*flutterKnob);
    addAndMakeVisible(*inputMeter);
    addAndMakeVisible(*echoMeter);
    addAndMakeVisible(*outputMeter);

    refreshMeters();
    startTimerHz(24);
}

EchoControlsComponent::~EchoControlsComponent()
{
    stopTimer();
}

void EchoControlsComponent::timerCallback()
{
    refreshMeters();
}

void EchoControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    echoMeter->setLevel(processor.getEchoMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void EchoControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff171512), bounds.getTopLeft(),
                               juce::Colour(0xff111214), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff1b1712));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(26.0f, 16.0f).removeFromTop(100.0f);
    juce::ColourGradient glow(warmOrange().withAlpha(0.18f), topGlow.getX(), topGlow.getY(),
                              mossGreen().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 20.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void EchoControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(38);
    auto badgeArea = topRow.removeFromRight(170);
    summaryLabel.setBounds(topRow);
    badgeLabel.setBounds(badgeArea.reduced(0, 4));

    area.removeFromTop(14);

    auto meterArea = area.removeFromRight(190);
    auto mainArea = area;

    auto heroRow = mainArea.removeFromTop((int) std::round(mainArea.getHeight() * 0.60f));
    auto timeArea = heroRow.removeFromLeft(heroRow.getWidth() / 2);
    heroRow.removeFromLeft(12);
    auto feedbackArea = heroRow;
    timeKnob->setBounds(timeArea);
    feedbackKnob->setBounds(feedbackArea);

    mainArea.removeFromTop(12);
    const int smallWidth = (mainArea.getWidth() - 36) / 4;
    auto mixArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto colorArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto driveArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto flutterArea = mainArea;
    mixKnob->setBounds(mixArea);
    colorKnob->setBounds(colorArea);
    driveKnob->setBounds(driveArea);
    flutterKnob->setBounds(flutterArea);

    const int meterWidth = (meterArea.getWidth() - 24) / 3;
    auto inputArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto echoArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto outputArea = meterArea;
    inputMeter->setBounds(inputArea);
    echoMeter->setBounds(echoArea);
    outputMeter->setBounds(outputArea);
}
