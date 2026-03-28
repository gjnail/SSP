#include "CombControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3a4031); }
juce::Colour subtleText() { return juce::Colour(0xffc9cfbb); }
juce::Colour acidGreen() { return juce::Colour(0xffc6f36a); }
juce::Colour mint() { return juce::Colour(0xff66e0b5); }
juce::Colour amber() { return juce::Colour(0xffffc86e); }

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
} // namespace

class CombKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit CombKnobLookAndFeel(bool heroStyle)
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

        juce::ColourGradient halo(acidGreen().withAlpha(hero ? 0.25f : 0.18f), centre.x, bounds.getBottom(),
                                  mint().withAlpha(hero ? 0.16f : 0.11f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff252b21), centre.x, bounds.getY(),
                                   juce::Colour(0xff121512), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1d2218));
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
        g.setColour(juce::Colour(0xff0f120e));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(mint(), arcBounds.getBottomLeft(), acidGreen(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff191d17), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff23281f), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 7.5f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(acidGreen());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff11140f));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class CombControlsComponent::CombKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    CombKnob(juce::AudioProcessorValueTreeState& state,
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
        valueLabel.setColour(juce::Label::textColourId, acidGreen());

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

    ~CombKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff1a1e16) : juce::Colour(0xff181c15),
                                  area.getTopLeft(),
                                  juce::Colour(0xff121411),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff1f241a));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 24.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(mint().withAlpha(0.60f), accent.getX(), accent.getCentreY(),
                                        acidGreen().withAlpha(0.76f), accent.getRight(), accent.getCentreY(), false);
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

    CombKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

class CombControlsComponent::MeterColumn final : public juce::Component
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
        juce::ColourGradient background(juce::Colour(0xff141712), area.getX(), area.getY(),
                                        juce::Colour(0xff10120f), area.getRight(), area.getBottom(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(area, 18.0f);

        auto meterBody = area.reduced(18.0f, 46.0f);
        g.setColour(juce::Colour(0xff0d100c));
        g.fillRoundedRectangle(meterBody, 12.0f);

        const auto clamped = juce::jlimit(0.0f, 1.0f, level);
        auto fillBounds = meterBody.withTrimmedTop(meterBody.getHeight() * (1.0f - clamped));
        juce::ColourGradient fill(fillColour.withAlpha(0.88f), fillBounds.getCentreX(), fillBounds.getBottom(),
                                  amber().withAlpha(0.92f), fillBounds.getCentreX(), fillBounds.getY(), false);
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

CombControlsComponent::CombControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Stereo comb filtering for hollow notches, metallic resonance, and quick resonator color without extra routing.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("HOLLOW RESONANCE", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, amber());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff58482f));
    addAndMakeVisible(badgeLabel);

    polarityLabel.setText("Polarity", juce::dontSendNotification);
    polarityLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    polarityLabel.setColour(juce::Label::textColourId, subtleText());
    addAndMakeVisible(polarityLabel);

    polarityBox.addItemList(PluginProcessor::getPolarityModeNames(), 1);
    polarityBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff131710));
    polarityBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    polarityBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    polarityBox.setColour(juce::ComboBox::arrowColourId, acidGreen());
    polarityBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1a1f16));
    addAndMakeVisible(polarityBox);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "polarityMode", polarityBox);

    frequencyKnob = std::make_unique<CombKnob>(state, "frequencyHz", "Frequency", "Sets the comb spacing in Hertz.", formatFrequency, true);
    feedbackKnob = std::make_unique<CombKnob>(state, "feedback", "Feedback", "Controls how strongly the comb resonates.", formatPercent, true);
    dampKnob = std::make_unique<CombKnob>(state, "damp", "Damp", "Darken the feedback path to soften the ring.", formatPercent, false);
    spreadKnob = std::make_unique<CombKnob>(state, "spread", "Spread", "Offsets left and right delay lengths.", formatPercent, false);
    mixKnob = std::make_unique<CombKnob>(state, "mix", "Mix", "Blend dry signal with the comb texture.", formatPercent, false);

    inputMeter = std::make_unique<MeterColumn>("Input", mint());
    combMeter = std::make_unique<MeterColumn>("Comb", acidGreen());
    outputMeter = std::make_unique<MeterColumn>("Output", amber());

    addAndMakeVisible(*frequencyKnob);
    addAndMakeVisible(*feedbackKnob);
    addAndMakeVisible(*dampKnob);
    addAndMakeVisible(*spreadKnob);
    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*inputMeter);
    addAndMakeVisible(*combMeter);
    addAndMakeVisible(*outputMeter);

    refreshMeters();
    startTimerHz(24);
}

CombControlsComponent::~CombControlsComponent()
{
    stopTimer();
}

void CombControlsComponent::timerCallback()
{
    refreshMeters();
}

void CombControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    combMeter->setLevel(processor.getCombMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void CombControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff171b15), bounds.getTopLeft(),
                               juce::Colour(0xff111310), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff1a1f17));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(26.0f, 16.0f).removeFromTop(100.0f);
    juce::ColourGradient glow(mint().withAlpha(0.16f), topGlow.getX(), topGlow.getY(),
                              acidGreen().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 20.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void CombControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(60);
    auto rightTop = topRow.removeFromRight(250);
    summaryLabel.setBounds(topRow.removeFromTop(32));

    auto badgeArea = rightTop.removeFromTop(22);
    badgeLabel.setBounds(badgeArea.removeFromRight(148));
    auto modeArea = rightTop;
    polarityLabel.setBounds(modeArea.removeFromLeft(60));
    polarityBox.setBounds(modeArea.removeFromLeft(170));

    area.removeFromTop(14);

    auto meterArea = area.removeFromRight(190);
    auto mainArea = area;

    auto heroRow = mainArea.removeFromTop((int) std::round(mainArea.getHeight() * 0.60f));
    auto freqArea = heroRow.removeFromLeft(heroRow.getWidth() / 2);
    heroRow.removeFromLeft(12);
    auto feedbackArea = heroRow;
    frequencyKnob->setBounds(freqArea);
    feedbackKnob->setBounds(feedbackArea);

    mainArea.removeFromTop(12);
    const int smallWidth = (mainArea.getWidth() - 24) / 3;
    auto dampArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto spreadArea = mainArea.removeFromLeft(smallWidth);
    mainArea.removeFromLeft(12);
    auto mixArea = mainArea;
    dampKnob->setBounds(dampArea);
    spreadKnob->setBounds(spreadArea);
    mixKnob->setBounds(mixArea);

    const int meterWidth = (meterArea.getWidth() - 24) / 3;
    auto inputArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto combArea = meterArea.removeFromLeft(meterWidth);
    meterArea.removeFromLeft(12);
    auto outputArea = meterArea;
    inputMeter->setBounds(inputArea);
    combMeter->setBounds(combArea);
    outputMeter->setBounds(outputArea);
}
