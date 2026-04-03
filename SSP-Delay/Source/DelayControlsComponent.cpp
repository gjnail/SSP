#include "DelayControlsComponent.h"

namespace
{
juce::Colour strongText() { return juce::Colour(0xfff1e8d6); }
juce::Colour mutedText() { return juce::Colour(0xff99a7b8); }
juce::Colour gold() { return juce::Colour(0xffffcf4d); }
juce::Colour cyan() { return juce::Colour(0xff3ecfff); }
juce::Colour mint() { return juce::Colour(0xff79f0cf); }

juce::String formatMilliseconds(double value) { return juce::String(juce::roundToInt((float) value)) + " ms"; }
juce::String formatPercent(double value) { return juce::String(juce::roundToInt((float) value * 100.0f)) + "%"; }
juce::RangedAudioParameter* getRangedParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(id));
}

void setControlLinkedState(juce::Component& component, bool enabled)
{
    component.setEnabled(enabled);
    component.setAlpha(enabled ? 1.0f : 0.42f);
}
} // namespace

class DelayControlsComponent::ParameterKnob final : public juce::Component
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
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), prominent ? cyan() : gold(), 16.0f);
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

class DelayControlsComponent::TimeControl final : public juce::Component
{
public:
    TimeControl(juce::AudioProcessorValueTreeState& state,
                const juce::String& modeID,
                const juce::String& timeID,
                const juce::String& labelText,
                juce::Colour accentColour)
        : apvts(state),
          modeParameterID(modeID),
          timeParameterID(timeID),
          accent(accentColour),
          syncToggle("SYNC")
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(caption);
        addAndMakeVisible(syncToggle);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setName(labelText);
        slider.onValueChange = [this]
        {
            if (suppressCallbacks)
                return;

            if (isSyncEnabled())
                setParameterValue(modeParameterID, (float) juce::roundToInt(slider.getValue()));
            else
                setParameterValue(timeParameterID, (float) slider.getValue());

            refreshFromState();
        };

        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, strongText());
        label.setFont(juce::Font(18.0f, juce::Font::bold));
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);

        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.12f));
        valueLabel.setFont(juce::Font(17.0f, juce::Font::bold));
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setInterceptsMouseClicks(false, false);

        caption.setColour(juce::Label::textColourId, mutedText());
        caption.setFont(juce::Font(11.5f));
        caption.setJustificationType(juce::Justification::centred);
        caption.setInterceptsMouseClicks(false, false);

        syncToggle.onClick = [this]
        {
            if (suppressCallbacks)
                return;

            const bool enableSync = syncToggle.getToggleState();
            if (enableSync)
            {
                const int currentMode = juce::jmax(1, juce::roundToInt(apvts.getRawParameterValue(modeParameterID)->load()));
                setParameterValue(modeParameterID, (float) currentMode);
            }
            else
            {
                setParameterValue(modeParameterID, 0.0f);
            }

            refreshFromState();
        };

        refreshFromState();
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 16.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16, 14);
        auto topRow = area.removeFromTop(24);
        label.setBounds(topRow.removeFromLeft(topRow.getWidth() - 74));
        syncToggle.setBounds(topRow.removeFromRight(70));
        valueLabel.setBounds(area.removeFromTop(24));
        caption.setBounds(area.removeFromBottom(30));
        area.removeFromBottom(6);
        slider.setBounds(area);
    }

    void refreshFromState()
    {
        const bool syncEnabled = isSyncEnabled();
        const float timeMs = apvts.getRawParameterValue(timeParameterID)->load();
        const int mode = juce::roundToInt(apvts.getRawParameterValue(modeParameterID)->load());

        juce::ScopedValueSetter<bool> setter(suppressCallbacks, true);
        syncToggle.setToggleState(syncEnabled, juce::dontSendNotification);

        if (syncEnabled)
        {
            slider.setRange(1.0, (double) PluginProcessor::getTimeModeNames().size() - 1.0, 1.0);
            slider.setValue((double) juce::jlimit(1, PluginProcessor::getTimeModeNames().size() - 1, mode), juce::dontSendNotification);
            valueLabel.setText(PluginProcessor::getTimeModeNames()[juce::jlimit(1, PluginProcessor::getTimeModeNames().size() - 1, mode)],
                               juce::dontSendNotification);
            caption.setText("Sync on. Turn for note divisions.", juce::dontSendNotification);
        }
        else
        {
            slider.setRange(20.0, 2000.0, 1.0);
            slider.setValue(timeMs, juce::dontSendNotification);
            valueLabel.setText(formatMilliseconds(timeMs), juce::dontSendNotification);
            caption.setText("Sync off. Turn for milliseconds.", juce::dontSendNotification);
        }
    }

private:
    bool isSyncEnabled() const
    {
        return juce::roundToInt(apvts.getRawParameterValue(modeParameterID)->load()) > 0;
    }

    void setParameterValue(const juce::String& parameterID, float rawValue)
    {
        if (auto* parameter = getRangedParameter(apvts, parameterID))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(rawValue));
            parameter->endChangeGesture();
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String modeParameterID;
    juce::String timeParameterID;
    juce::Colour accent;
    ssp::ui::SSPKnob slider;
    juce::Label label;
    juce::Label valueLabel;
    juce::Label caption;
    ssp::ui::SSPToggle syncToggle;
    bool suppressCallbacks = false;
};

class DelayControlsComponent::MeterColumn final : public juce::Component
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

class DelayControlsComponent::StereoDisplay final : public juce::Component
{
public:
    explicit StereoDisplay(juce::AudioProcessorValueTreeState& state)
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
        g.drawText("Timing + Spread", title.toNearestInt(), juce::Justification::centredLeft, false);

        auto subtitle = area.removeFromTop(16.0f);
        g.setColour(mutedText());
        g.setFont(juce::Font(11.0f));
        g.drawText("Stereo width offsets taps and crossfeeds the returns.", subtitle.toNearestInt(), juce::Justification::centredLeft, false);

        auto plot = area.reduced(0.0f, 8.0f);
        g.setColour(juce::Colour(0xff0d141c));
        g.fillRoundedRectangle(plot, 12.0f);

        const float width = apvts.getRawParameterValue("width")->load();
        const float tone = apvts.getRawParameterValue("tone")->load();
        const float mix = apvts.getRawParameterValue("mix")->load();

        const float leftX = plot.getCentreX() - plot.getWidth() * (0.08f + width * 0.18f);
        const float rightX = plot.getCentreX() + plot.getWidth() * (0.08f + width * 0.18f);
        const float topY = plot.getY() + plot.getHeight() * (0.24f - mix * 0.06f);
        const float bottomY = plot.getBottom() - plot.getHeight() * (0.24f - mix * 0.06f);

        g.setColour(cyan().withAlpha(0.80f));
        g.drawLine(leftX, topY, leftX, bottomY, 2.0f);
        g.drawLine(rightX, topY, rightX, bottomY, 2.0f);

        juce::Path cross;
        cross.startNewSubPath(leftX, plot.getCentreY());
        cross.cubicTo(plot.getCentreX() - 20.0f, plot.getCentreY() - 28.0f * width,
                      plot.getCentreX() + 20.0f, plot.getCentreY() + 28.0f * width,
                      rightX, plot.getCentreY());
        g.setColour(gold().withAlpha(0.72f));
        g.strokePath(cross, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto toneBand = plot.removeFromBottom(18.0f);
        juce::ColourGradient toneGradient(juce::Colour(0xff2d3945), toneBand.getX(), toneBand.getY(),
                                          cyan(), toneBand.getRight(), toneBand.getY(), false);
        toneGradient.addColour(juce::jlimit(0.0f, 1.0f, tone), gold().withAlpha(0.9f));
        g.setGradientFill(toneGradient);
        g.fillRoundedRectangle(toneBand, 6.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

DelayControlsComponent::DelayControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state),
      unlinkToggle("UNLINK")
{
    summaryLabel.setText("Clean stereo delay with milliseconds or note-sync timing, quick tone shaping, and width control.",
                         juce::dontSendNotification);
    summaryLabel.setColour(juce::Label::textColourId, mutedText());
    summaryLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("DUAL TIMING", juce::dontSendNotification);
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

    leftTimeControl = std::make_unique<TimeControl>(state, "timeMode", "timeMs", "Time L", cyan());
    rightTimeControl = std::make_unique<TimeControl>(state, "rightTimeMode", "rightTimeMs", "Time R", mint());
    leftFeedbackKnob = std::make_unique<ParameterKnob>(state, "feedback", "Feedback L", "Left repeat decay.", gold(), formatPercent, true);
    rightFeedbackKnob = std::make_unique<ParameterKnob>(state, "rightFeedback", "Feedback R", "Right repeat decay.", cyan(), formatPercent, true);
    mixKnob = std::make_unique<ParameterKnob>(state, "mix", "Mix", "Blend wet against dry.", mint(), formatPercent, false);
    toneKnob = std::make_unique<ParameterKnob>(state, "tone", "Tone", "Dark repeats to brighter taps.", gold(), formatPercent, false);
    widthKnob = std::make_unique<ParameterKnob>(state, "width", "Width", "Offset and crossfeed the stereo taps.", cyan(), formatPercent, false);
    inputMeter = std::make_unique<MeterColumn>("Input", mint());
    delayMeter = std::make_unique<MeterColumn>("Delay", cyan());
    outputMeter = std::make_unique<MeterColumn>("Output", gold());
    stereoDisplay = std::make_unique<StereoDisplay>(state);

    for (auto* component : { static_cast<juce::Component*>(leftTimeControl.get()),
                             static_cast<juce::Component*>(rightTimeControl.get()),
                             static_cast<juce::Component*>(leftFeedbackKnob.get()),
                             static_cast<juce::Component*>(rightFeedbackKnob.get()),
                             static_cast<juce::Component*>(mixKnob.get()),
                             static_cast<juce::Component*>(toneKnob.get()),
                             static_cast<juce::Component*>(widthKnob.get()),
                             static_cast<juce::Component*>(inputMeter.get()),
                             static_cast<juce::Component*>(delayMeter.get()),
                             static_cast<juce::Component*>(outputMeter.get()),
                             static_cast<juce::Component*>(stereoDisplay.get()) })
    {
        addAndMakeVisible(*component);
    }

    refreshMeters();
    updateLinkState();
    startTimerHz(24);
}

DelayControlsComponent::~DelayControlsComponent()
{
    stopTimer();
}

void DelayControlsComponent::timerCallback()
{
    refreshMeters();
    leftTimeControl->refreshFromState();
    rightTimeControl->refreshFromState();
    updateLinkState();
    stereoDisplay->repaint();
}

void DelayControlsComponent::refreshMeters()
{
    inputMeter->setLevel(processor.getInputMeterLevel());
    delayMeter->setLevel(processor.getDelayMeterLevel());
    outputMeter->setLevel(processor.getOutputMeterLevel());
}

void DelayControlsComponent::updateLinkState()
{
    const bool unlinked = apvts.getRawParameterValue("unlinkChannels")->load() >= 0.5f;
    setControlLinkedState(*rightTimeControl, unlinked);
    setControlLinkedState(*rightFeedbackKnob, unlinked);
}

void DelayControlsComponent::paint(juce::Graphics& g)
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

void DelayControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 18);
    auto top = area.removeFromTop(28);
    summaryLabel.setBounds(top.removeFromLeft(area.getWidth() - 140));
    badgeLabel.setBounds(top.removeFromRight(128));

    area.removeFromTop(12);
    auto left = area.removeFromLeft((int) std::round(area.getWidth() * 0.73f));
    meterSectionBounds = area;

    heroSectionBounds = left.removeFromTop((int) std::round(left.getHeight() * 0.54f));
    left.removeFromTop(12);
    spreadSectionBounds = left;

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

    leftTimeControl->setBounds(leftTimeArea);
    rightTimeControl->setBounds(rightTimeArea);
    leftFeedbackKnob->setBounds(leftFeedbackArea);
    rightFeedbackKnob->setBounds(rightFeedbackArea);

    auto linkArea = centerHero.reduced(0, 24);
    linkLabel.setBounds(linkArea.removeFromTop(20));
    linkArea.removeFromTop(8);
    unlinkToggle.setBounds(linkArea.removeFromTop(28));

    auto displayArea = spreadSectionBounds.removeFromLeft((int) std::round(spreadSectionBounds.getWidth() * 0.46f));
    spreadSectionBounds.removeFromLeft(12);
    stereoDisplay->setBounds(displayArea);

    const int smallWidth = (spreadSectionBounds.getWidth() - 24) / 3;
    auto mixArea = spreadSectionBounds.removeFromLeft(smallWidth);
    spreadSectionBounds.removeFromLeft(12);
    auto toneArea = spreadSectionBounds.removeFromLeft(smallWidth);
    spreadSectionBounds.removeFromLeft(12);
    auto widthArea = spreadSectionBounds;

    mixKnob->setBounds(mixArea);
    toneKnob->setBounds(toneArea);
    widthKnob->setBounds(widthArea);

    auto meters = meterSectionBounds.reduced(0, 8);
    const int meterWidth = (meters.getWidth() - 24) / 3;
    auto inputArea = meters.removeFromLeft(meterWidth);
    meters.removeFromLeft(12);
    auto delayArea = meters.removeFromLeft(meterWidth);
    meters.removeFromLeft(12);
    auto outputArea = meters;

    inputMeter->setBounds(inputArea);
    delayMeter->setBounds(delayArea);
    outputMeter->setBounds(outputArea);
}
