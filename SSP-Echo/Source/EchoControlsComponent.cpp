#include "EchoControlsComponent.h"
#include "EchoVectorUI.h"

namespace
{
juce::Colour strongText() { return juce::Colour(0xfff1e8d6); }
juce::Colour mutedText() { return juce::Colour(0xff99a7b8); }
juce::Colour gold() { return juce::Colour(0xffffcf4d); }
juce::Colour cyan() { return juce::Colour(0xff3ecfff); }
juce::Colour mint() { return juce::Colour(0xff79f0cf); }

juce::String formatMilliseconds(double value) { return juce::String(juce::roundToInt((float) value)) + " ms"; }
juce::String formatPercent(double value) { return juce::String(juce::roundToInt((float) value * 100.0f)) + "%"; }
juce::String formatDrive(double value) { return "+" + juce::String(value, 1) + " dB"; }
void setControlLinkedState(juce::Component& component, bool enabled)
{
    component.setEnabled(enabled);
    component.setAlpha(enabled ? 1.0f : 0.42f);
}
} // namespace

class EchoControlsComponent::ParameterKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    ParameterKnob(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& labelText,
                  const juce::String& captionText,
                  juce::Colour accentColour,
                  Formatter formatterToUse,
                  bool prominentStyle = false)
        : attachment(state, parameterID, slider),
          formatter(std::move(formatterToUse)),
          prominent(prominentStyle)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(caption);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setName(labelText);
        slider.onValueChange = [this] { refreshValue(); };

        if (auto* parameter = state.getParameter(parameterID))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));

        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, strongText());
        label.setFont(juce::Font(prominent ? 18.0f : 15.0f, juce::Font::bold));
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);

        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.12f));
        valueLabel.setFont(juce::Font(prominent ? 17.0f : 13.0f, juce::Font::bold));
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setInterceptsMouseClicks(false, false);

        caption.setText(captionText, juce::dontSendNotification);
        caption.setColour(juce::Label::textColourId, mutedText());
        caption.setFont(juce::Font(11.5f));
        caption.setJustificationType(juce::Justification::centred);
        caption.setInterceptsMouseClicks(false, false);

        refreshValue();
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), prominent ? gold() : cyan(), 16.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(prominent ? 16 : 14, 14);
        label.setBounds(area.removeFromTop(prominent ? 24 : 20));
        valueLabel.setBounds(area.removeFromTop(prominent ? 24 : 18));
        caption.setBounds(area.removeFromBottom(30));
        area.removeFromBottom(6);
        slider.setBounds(area);
    }

private:
    void refreshValue()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    ssp::ui::SSPKnob slider;
    juce::Label label;
    juce::Label valueLabel;
    juce::Label caption;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool prominent = false;
};

class EchoControlsComponent::MeterColumn final : public juce::Component
{
public:
    MeterColumn(juce::String labelText, juce::Colour accentColour)
        : title(std::move(labelText)),
          accent(accentColour)
    {
    }

    void setLevel(float newLevel)
    {
        level = juce::jlimit(0.0f, 1.0f, newLevel);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 14.0f);

        auto body = getLocalBounds().toFloat().reduced(16.0f, 18.0f);
        auto meter = body.withTrimmedTop(18.0f).withTrimmedBottom(24.0f);
        g.setColour(juce::Colour(0xff0d141c));
        g.fillRoundedRectangle(meter, 10.0f);

        auto fill = meter.withTrimmedTop(meter.getHeight() * (1.0f - level));
        juce::ColourGradient gradient(accent.withAlpha(0.95f), fill.getCentreX(), fill.getBottom(),
                                      gold().withAlpha(0.90f), fill.getCentreX(), fill.getY(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(fill, 10.0f);

        g.setColour(mutedText());
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawFittedText(title, body.removeFromTop(16).toNearestInt(), juce::Justification::centred, 1);

        const auto value = juce::String(juce::Decibels::gainToDecibels(juce::jmax(level, 0.00001f), -60.0f), 1) + " dB";
        g.setColour(strongText());
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawFittedText(value, body.removeFromBottom(18).toNearestInt(), juce::Justification::centred, 1);
    }

private:
    juce::String title;
    juce::Colour accent;
    float level = 0.0f;
};

class EchoControlsComponent::CharacterDisplay final : public juce::Component
{
public:
    explicit CharacterDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), mint(), 16.0f);

        auto area = getLocalBounds().toFloat().reduced(18.0f, 18.0f);
        auto title = area.removeFromTop(18.0f);
        g.setColour(strongText());
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("Repeat Character", title.toNearestInt(), juce::Justification::centredLeft, false);

        auto subtitle = area.removeFromTop(16.0f);
        g.setColour(mutedText());
        g.setFont(juce::Font(11.0f));
        g.drawText("Tone, saturation, and flutter sculpt the echo trail.", subtitle.toNearestInt(), juce::Justification::centredLeft, false);

        auto plot = area.reduced(0.0f, 8.0f);
        g.setColour(juce::Colour(0xff0d141c));
        g.fillRoundedRectangle(plot, 12.0f);

        const float color = apvts.getRawParameterValue("color")->load();
        const float drive = apvts.getRawParameterValue("driveDb")->load() / 18.0f;
        const float flutter = apvts.getRawParameterValue("flutter")->load();

        juce::Path trail;
        for (int i = 0; i <= 160; ++i)
        {
            const float t = (float) i / 160.0f;
            const float x = plot.getX() + t * plot.getWidth();
            const float damping = std::exp(-t * juce::jmap(color, 1.5f, 4.5f));
            const float motion = std::sin(t * juce::MathConstants<float>::twoPi * (2.0f + flutter * 8.0f));
            const float saturation = std::tanh((motion * (0.55f + flutter * 0.35f) + std::sin(t * 13.0f) * 0.12f) * (1.0f + drive * 2.0f));
            const float y = plot.getCentreY() - saturation * damping * plot.getHeight() * 0.30f;

            if (i == 0)
                trail.startNewSubPath(x, y);
            else
                trail.lineTo(x, y);
        }

        juce::Path glow = trail;
        g.setColour(gold().withAlpha(0.16f));
        g.strokePath(glow, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(cyan().withAlpha(0.90f));
        g.strokePath(trail, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (int i = 0; i < 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) i / 3.0f);
            g.setColour(juce::Colour(0x24f4efe4));
            g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
        }
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

EchoControlsComponent::EchoControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state),
      unlinkToggle("UNLINK")
{
    summaryLabel.setText("Vintage-leaning repeats with filtered feedback, repeat drive, and flutter drift in a single front panel.",
                         juce::dontSendNotification);
    summaryLabel.setColour(juce::Label::textColourId, mutedText());
    summaryLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("COLORED REPEATS", juce::dontSendNotification);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff101318));
    badgeLabel.setColour(juce::Label::backgroundColourId, gold());
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(badgeLabel);

    linkLabel.setText("Channel Link", juce::dontSendNotification);
    linkLabel.setColour(juce::Label::textColourId, mutedText());
    linkLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(linkLabel);

    addAndMakeVisible(unlinkToggle);
    unlinkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "unlinkChannels", unlinkToggle);

    leftTimeKnob = std::make_unique<ParameterKnob>(state, "timeMs", "Time L", "Left channel repeat timing.", gold(), formatMilliseconds, true);
    rightTimeKnob = std::make_unique<ParameterKnob>(state, "rightTimeMs", "Time R", "Right channel repeat timing.", cyan(), formatMilliseconds, true);
    leftFeedbackKnob = std::make_unique<ParameterKnob>(state, "feedback", "Feedback L", "Left channel repeat length.", cyan(), formatPercent, true);
    rightFeedbackKnob = std::make_unique<ParameterKnob>(state, "rightFeedback", "Feedback R", "Right channel repeat length.", mint(), formatPercent, true);
    mixKnob = std::make_unique<ParameterKnob>(state, "mix", "Mix", "Blend the repeats under dry.", mint(), formatPercent, false);
    colorKnob = std::make_unique<ParameterKnob>(state, "color", "Color", "Dark tape to brighter reflections.", gold(), formatPercent, false);
    driveKnob = std::make_unique<ParameterKnob>(state, "driveDb", "Drive", "Saturate the repeat path.", cyan(), formatDrive, false);
    flutterKnob = std::make_unique<ParameterKnob>(state, "flutter", "Flutter", "Pitch drift and movement.", mint(), formatPercent, false);
    inputMeter = std::make_unique<MeterColumn>("Input", mint());
    echoMeter = std::make_unique<MeterColumn>("Echo", gold());
    outputMeter = std::make_unique<MeterColumn>("Output", cyan());
    characterDisplay = std::make_unique<CharacterDisplay>(state);

    for (auto* component : { static_cast<juce::Component*>(leftTimeKnob.get()),
                             static_cast<juce::Component*>(rightTimeKnob.get()),
                             static_cast<juce::Component*>(leftFeedbackKnob.get()),
                             static_cast<juce::Component*>(rightFeedbackKnob.get()),
                             static_cast<juce::Component*>(mixKnob.get()),
                             static_cast<juce::Component*>(colorKnob.get()),
                             static_cast<juce::Component*>(driveKnob.get()),
                             static_cast<juce::Component*>(flutterKnob.get()),
                             static_cast<juce::Component*>(inputMeter.get()),
                             static_cast<juce::Component*>(echoMeter.get()),
                             static_cast<juce::Component*>(outputMeter.get()),
                             static_cast<juce::Component*>(characterDisplay.get()) })
    {
        addAndMakeVisible(*component);
    }

    refreshMeters();
    updateLinkState();
    startTimerHz(24);
}

EchoControlsComponent::~EchoControlsComponent()
{
    stopTimer();
}

void EchoControlsComponent::timerCallback()
{
    refreshMeters();
    updateLinkState();
    characterDisplay->repaint();
}

void EchoControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    echoMeter->setLevel(processor.getEchoMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void EchoControlsComponent::updateLinkState()
{
    const bool unlinked = apvts.getRawParameterValue("unlinkChannels")->load() >= 0.5f;
    setControlLinkedState(*rightTimeKnob, unlinked);
    setControlLinkedState(*rightFeedbackKnob, unlinked);
}

void EchoControlsComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient fill(juce::Colour(0xff111822), area.getTopLeft(),
                              juce::Colour(0xff0b1118), area.getBottomLeft(), false);
    fill.addColour(0.52, juce::Colour(0xff0f1620));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(area, 18.0f);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(area.reduced(0.5f), 18.0f, 1.2f);
}

void EchoControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 18);
    auto top = area.removeFromTop(28);
    summaryLabel.setBounds(top.removeFromLeft(area.getWidth() - 170));
    badgeLabel.setBounds(top.removeFromRight(156));

    area.removeFromTop(12);
    auto left = area.removeFromLeft((int) std::round(area.getWidth() * 0.73f));
    meterSectionBounds = area;

    heroSectionBounds = left.removeFromTop((int) std::round(left.getHeight() * 0.54f));
    left.removeFromTop(12);
    contourSectionBounds = left;

    const int centerWidth = 102;
    auto leftHero = heroSectionBounds.removeFromLeft((heroSectionBounds.getWidth() - centerWidth - 24) / 2);
    heroSectionBounds.removeFromLeft(12);
    auto centerHero = heroSectionBounds.removeFromLeft(centerWidth);
    heroSectionBounds.removeFromLeft(12);
    auto rightHero = heroSectionBounds;

    auto leftTimeArea = leftHero.removeFromTop((leftHero.getHeight() - 12) / 2);
    leftHero.removeFromTop(12);
    auto leftFeedbackArea = leftHero;
    auto rightTimeArea = rightHero.removeFromTop((rightHero.getHeight() - 12) / 2);
    rightHero.removeFromTop(12);
    auto rightFeedbackArea = rightHero;

    leftTimeKnob->setBounds(leftTimeArea);
    rightTimeKnob->setBounds(rightTimeArea);
    leftFeedbackKnob->setBounds(leftFeedbackArea);
    rightFeedbackKnob->setBounds(rightFeedbackArea);

    auto linkArea = centerHero.reduced(0, 24);
    linkLabel.setBounds(linkArea.removeFromTop(20));
    linkArea.removeFromTop(8);
    unlinkToggle.setBounds(linkArea.removeFromTop(28));

    auto displayArea = contourSectionBounds.removeFromLeft((int) std::round(contourSectionBounds.getWidth() * 0.38f));
    contourSectionBounds.removeFromLeft(12);
    characterDisplay->setBounds(displayArea);

    const int smallWidth = (contourSectionBounds.getWidth() - 24) / 3;
    auto mixArea = contourSectionBounds.removeFromLeft(smallWidth);
    contourSectionBounds.removeFromLeft(12);
    auto colorArea = contourSectionBounds.removeFromLeft(smallWidth);
    contourSectionBounds.removeFromLeft(12);
    auto rightColumn = contourSectionBounds;
    auto driveArea = rightColumn.removeFromTop((rightColumn.getHeight() - 12) / 2);
    rightColumn.removeFromTop(12);
    auto flutterArea = rightColumn;

    mixKnob->setBounds(mixArea);
    colorKnob->setBounds(colorArea);
    driveKnob->setBounds(driveArea);
    flutterKnob->setBounds(flutterArea);

    auto meters = meterSectionBounds.reduced(0, 8);
    const int meterWidth = (meters.getWidth() - 24) / 3;
    auto inputArea = meters.removeFromLeft(meterWidth);
    meters.removeFromLeft(12);
    auto echoArea = meters.removeFromLeft(meterWidth);
    meters.removeFromLeft(12);
    auto outputArea = meters;

    inputMeter->setBounds(inputArea);
    echoMeter->setBounds(echoArea);
    outputMeter->setBounds(outputArea);
}
