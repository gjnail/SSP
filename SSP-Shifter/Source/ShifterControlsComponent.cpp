#include "ShifterControlsComponent.h"

namespace
{
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

juce::String formatSignedSemitones(double value)
{
    return juce::String(value >= 0.0 ? "+" : "") + juce::String(value, 1) + " st";
}

juce::String formatSignedCents(double value)
{
    return juce::String(value >= 0.0 ? "+" : "") + juce::String(value, 0) + " ct";
}

juce::String formatMilliseconds(double value)
{
    return juce::String(value, 1) + " ms";
}

juce::String formatFrequencyShift(double value)
{
    return juce::String(value >= 0.0 ? "+" : "") + juce::String(value, 1) + " Hz";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatDecibels(double value)
{
    return juce::String(value >= 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

float getParameterValue(const juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    if (auto* raw = state.getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

void drawSectionHeader(juce::Graphics& g,
                       juce::Rectangle<int> bounds,
                       const juce::String& title,
                       const juce::String& subtitle,
                       juce::Colour accent)
{
    auto area = bounds.reduced(18, 12);
    auto titleArea = area.removeFromTop(20);
    g.setColour(shifterui::textStrong());
    g.setFont(shifterui::titleFont(14.0f));
    g.drawText(title, titleArea, juce::Justification::centredLeft, false);

    if (subtitle.isNotEmpty())
    {
        g.setColour(shifterui::textMuted());
        g.setFont(shifterui::bodyFont(9.0f));
        g.drawText(subtitle, area.removeFromTop(16), juce::Justification::centredLeft, false);
    }

    auto marker = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ (float) titleArea.getX() - 8.0f, (float) titleArea.getCentreY() });
    g.setColour(accent);
    g.fillEllipse(marker);
}

void drawInactiveOverlay(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text)
{
    auto area = bounds.toFloat();
    g.setColour(juce::Colour(0xcc091018));
    g.fillRoundedRectangle(area.reduced(4.0f), 8.0f);
    g.setColour(shifterui::textMuted());
    g.setFont(shifterui::smallCapsFont(12.0f));
    g.drawFittedText(text, bounds.reduced(22, 20), juce::Justification::centred, 2);
}

void drawPitchSummary(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::AudioProcessorValueTreeState& state)
{
    auto display = bounds.toFloat().reduced(12.0f, 10.0f);

    g.setColour(juce::Colour(0xff0d141d));
    g.fillRoundedRectangle(display, 10.0f);

    const float semitones = getParameterValue(state, "pitchSemitones");
    const float cents = getParameterValue(state, "fineTuneCents");
    const float totalShift = semitones + cents / 100.0f;
    const float ratio = std::pow(2.0f, totalShift / 12.0f);
    const float normalized = juce::jlimit(-1.0f, 1.0f, totalShift / 24.0f);
    const float centerX = display.getCentreX();
    const float centerY = display.getCentreY();

    g.setColour(shifterui::outlineSoft());
    g.drawHorizontalLine((int) centerY, display.getX() + 18.0f, display.getRight() - 18.0f);
    g.drawVerticalLine((int) centerX, display.getY() + 18.0f, display.getBottom() - 18.0f);

    juce::Path sweep;
    sweep.startNewSubPath(display.getX() + 28.0f, centerY);
    sweep.cubicTo(centerX - 42.0f, centerY - normalized * (display.getHeight() * 0.24f),
                  centerX + 36.0f, centerY + normalized * (display.getHeight() * 0.24f),
                  display.getRight() - 28.0f, centerY - normalized * (display.getHeight() * 0.12f));
    g.setColour(shifterui::brandCyan().withAlpha(0.95f));
    g.strokePath(sweep, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(shifterui::textStrong());
    g.setFont(shifterui::titleFont(16.0f));
    g.drawText(juce::String(totalShift >= 0.0f ? "+" : "") + juce::String(totalShift, 2) + " st",
               display.toNearestInt().removeFromTop(26),
               juce::Justification::centred, false);
    g.setColour(shifterui::textMuted());
    g.setFont(shifterui::bodyFont(9.5f));
    g.drawText("Playback ratio " + juce::String(ratio, 3) + "x",
               display.toNearestInt().removeFromBottom(22),
               juce::Justification::centred, false);
}

void drawFrequencySummary(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::AudioProcessorValueTreeState& state)
{
    auto display = bounds.toFloat().reduced(12.0f, 10.0f);

    g.setColour(juce::Colour(0xff0d141d));
    g.fillRoundedRectangle(display, 10.0f);

    const float shiftHz = getParameterValue(state, "freqShiftHz");
    const float normalized = juce::jlimit(-1.0f, 1.0f, shiftHz / 1200.0f);
    const float left = display.getX() + 26.0f;
    const float right = display.getRight() - 26.0f;
    const float top = display.getY() + 18.0f;
    const float bottom = display.getBottom() - 26.0f;

    for (int i = 0; i < 5; ++i)
    {
        const float sourceX = juce::jmap((float) i / 4.0f, left, right);
        const float sourceHeight = juce::jmap((float) i / 4.0f, bottom - 10.0f, top + 14.0f);
        g.setColour(shifterui::outlineSoft());
        g.drawLine(sourceX, bottom, sourceX, sourceHeight, 1.0f);

        const float shiftedX = juce::jlimit(left, right, sourceX + normalized * 42.0f);
        g.setColour(shifterui::brandGold().withAlpha(0.88f));
        g.drawLine(shiftedX, bottom, shiftedX, sourceHeight - 10.0f, 2.0f);
    }

    g.setColour(shifterui::textStrong());
    g.setFont(shifterui::titleFont(16.0f));
    g.drawText(juce::String(shiftHz >= 0.0f ? "+" : "") + juce::String(shiftHz, 1) + " Hz",
               display.toNearestInt().removeFromTop(26),
               juce::Justification::centred, false);
    g.setColour(shifterui::textMuted());
    g.setFont(shifterui::bodyFont(9.5f));
    g.drawText("Every partial moves by the same fixed frequency offset.",
               display.toNearestInt().removeFromBottom(22),
               juce::Justification::centred, false);
}

} // namespace

class ShifterControlsComponent::ParameterKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    ParameterKnob(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& labelText,
                  juce::Colour accentColour,
                  Formatter formatterToUse)
        : attachment(state, parameterID, slider),
          formatter(std::move(formatterToUse))
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);

        slider.setName(labelText);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.onValueChange = [this] { refreshValue(); };

        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(parameterID)))
            slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));

        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, shifterui::textStrong());
        label.setFont(shifterui::smallCapsFont(11.5f));
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);

        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.10f));
        valueLabel.setFont(shifterui::bodyFont(10.2f));
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setInterceptsMouseClicks(false, false);

        refreshValue();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(20));
        valueLabel.setBounds(area.removeFromBottom(18));
        slider.setBounds(area.reduced(2, 0));
    }

private:
    void refreshValue()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    shifterui::SSPKnob slider;
    juce::Label label;
    juce::Label valueLabel;
    SliderAttachment attachment;
    Formatter formatter;
};

class ShifterControlsComponent::EngineDisplay final : public juce::Component
{
public:
    EngineDisplay(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
        : processor(processorToUse),
          apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        const bool pitchMode = processor.getCurrentModeIndex() == 0;
        if (pitchMode)
            drawPitchSummary(g, getLocalBounds(), apvts);
        else
            drawFrequencySummary(g, getLocalBounds(), apvts);
    }

private:
    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
};

ShifterControlsComponent::ShifterControlsComponent(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
    : processor(processorToUse),
      apvts(state)
{
    for (const auto& parameterID : { "mode", "pitchSemitones", "fineTuneCents", "grainMs", "freqShiftHz" })
        apvts.addParameterListener(parameterID, this);

    modeLabel.setText("Shifter Mode", juce::dontSendNotification);
    modeLabel.setFont(shifterui::smallCapsFont(11.0f));
    modeLabel.setColour(juce::Label::textColourId, shifterui::textMuted());
    addAndMakeVisible(modeLabel);

    modeHintLabel.setText("Pitch keeps interval relationships. Frequency shifts every partial by the same Hz offset.",
                          juce::dontSendNotification);
    modeHintLabel.setFont(shifterui::bodyFont(9.8f));
    modeHintLabel.setColour(juce::Label::textColourId, shifterui::textMuted());
    addAndMakeVisible(modeHintLabel);

    for (int i = 0; i < PluginProcessor::getModeNames().size(); ++i)
        modeBox.addItem(PluginProcessor::getModeNames()[i], i + 1);
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);

    pitchKnob = std::make_unique<ParameterKnob>(apvts, "pitchSemitones", "Pitch", shifterui::brandCyan(), formatSignedSemitones);
    fineKnob = std::make_unique<ParameterKnob>(apvts, "fineTuneCents", "Fine", shifterui::brandMint(), formatSignedCents);
    grainKnob = std::make_unique<ParameterKnob>(apvts, "grainMs", "Texture", shifterui::brandGold(), formatMilliseconds);
    frequencyKnob = std::make_unique<ParameterKnob>(apvts, "freqShiftHz", "Shift", shifterui::brandGold(), formatFrequencyShift);
    toneKnob = std::make_unique<ParameterKnob>(apvts, "tone", "Tone", shifterui::brandCyan(), formatPercent);
    mixKnob = std::make_unique<ParameterKnob>(apvts, "mix", "Mix", shifterui::brandMint(), formatPercent);
    outputKnob = std::make_unique<ParameterKnob>(apvts, "outputGain", "Output", shifterui::brandGold(), formatDecibels);
    engineDisplay = std::make_unique<EngineDisplay>(processor, apvts);

    addAndMakeVisible(*engineDisplay);
    for (auto* component : { static_cast<juce::Component*>(pitchKnob.get()),
                             static_cast<juce::Component*>(fineKnob.get()),
                             static_cast<juce::Component*>(grainKnob.get()),
                             static_cast<juce::Component*>(frequencyKnob.get()),
                             static_cast<juce::Component*>(toneKnob.get()),
                             static_cast<juce::Component*>(mixKnob.get()),
                             static_cast<juce::Component*>(outputKnob.get()) })
    {
        addAndMakeVisible(*component);
    }

    updateModeState();
}

ShifterControlsComponent::~ShifterControlsComponent()
{
    for (const auto& parameterID : { "mode", "pitchSemitones", "fineTuneCents", "grainMs", "freqShiftHz" })
        apvts.removeParameterListener(parameterID, this);
}

void ShifterControlsComponent::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == "mode")
        pendingModeRefresh.store(true);
    else
        pendingEngineRefresh.store(true);

    triggerAsyncUpdate();
}

void ShifterControlsComponent::handleAsyncUpdate()
{
    const bool shouldRefreshMode = pendingModeRefresh.exchange(false);
    const bool shouldRefreshEngine = pendingEngineRefresh.exchange(false);

    if (shouldRefreshMode)
        updateModeState();

    if (shouldRefreshEngine || shouldRefreshMode)
    {
        if (engineDisplay != nullptr)
            engineDisplay->repaint();
    }
}

void ShifterControlsComponent::updateModeState()
{
    const bool pitchMode = processor.getCurrentModeIndex() == 0;

    for (auto* component : { static_cast<juce::Component*>(pitchKnob.get()),
                             static_cast<juce::Component*>(fineKnob.get()),
                             static_cast<juce::Component*>(grainKnob.get()) })
    {
        component->setVisible(pitchMode);
        component->setEnabled(pitchMode);
    }

    if (frequencyKnob != nullptr)
    {
        frequencyKnob->setVisible(! pitchMode);
        frequencyKnob->setEnabled(! pitchMode);
    }

    if (showingPitchMode != pitchMode)
    {
        showingPitchMode = pitchMode;
        resized();
        repaint();
    }
}

void ShifterControlsComponent::paint(juce::Graphics& g)
{
    const bool pitchMode = processor.getCurrentModeIndex() == 0;

    shifterui::drawPanelBackground(g, modeSectionBounds.toFloat(), shifterui::brandMint(), 16.0f);
    shifterui::drawPanelBackground(g, sharedSectionBounds.toFloat(), shifterui::brandMint(), 16.0f);

    if (pitchMode)
    {
        shifterui::drawPanelBackground(g, pitchSectionBounds.toFloat(), shifterui::brandCyan(), 16.0f);
        drawSectionHeader(g, pitchSectionBounds, "Pitch Engine", "Interval shifts, fine detune, and grain texture.", shifterui::brandCyan());
    }
    else
    {
        shifterui::drawPanelBackground(g, frequencySectionBounds.toFloat(), shifterui::brandGold(), 16.0f);
        drawSectionHeader(g, frequencySectionBounds, "Frequency Engine", "Fixed-Hz sideband movement for inharmonic tones.", shifterui::brandGold());
    }

    drawSectionHeader(g, sharedSectionBounds, "Shared Finish", "Tone, wet blend, and output trim.", shifterui::brandMint());
}

void ShifterControlsComponent::resized()
{
    auto area = getLocalBounds();
    constexpr int gap = 14;
    const bool pitchMode = processor.getCurrentModeIndex() == 0;

    modeSectionBounds = area.removeFromTop(60);
    area.removeFromTop(gap);
    auto engineSectionBounds = area.removeFromTop((int) std::round(area.getHeight() * 0.64f));
    pitchSectionBounds = {};
    frequencySectionBounds = {};

    if (pitchMode)
        pitchSectionBounds = engineSectionBounds;
    else
        frequencySectionBounds = engineSectionBounds;

    area.removeFromTop(gap);
    sharedSectionBounds = area;

    auto modeArea = modeSectionBounds.reduced(16, 10);
    modeLabel.setBounds(modeArea.removeFromLeft(100).removeFromTop(18));
    modeBox.setBounds(modeArea.removeFromLeft(180).removeFromTop(28));
    modeArea.removeFromLeft(14);
    modeHintLabel.setBounds(modeArea);

    if (pitchMode)
    {
        auto pitchArea = pitchSectionBounds.reduced(14, 14);
        auto displayArea = pitchArea.removeFromTop((int) std::round(pitchArea.getHeight() * 0.50f));
        engineDisplay->setBounds(juce::Rectangle<int>((int) std::round(displayArea.getWidth() * 0.70f),
                                                      (int) std::round(displayArea.getHeight() * 0.80f))
                                     .withCentre(displayArea.getCentre()));
        pitchArea.removeFromTop(12);
        const int pitchWidth = (pitchArea.getWidth() - 8) / 3;
        pitchKnob->setBounds(pitchArea.removeFromLeft(pitchWidth).reduced(4, 0));
        pitchArea.removeFromLeft(4);
        fineKnob->setBounds(pitchArea.removeFromLeft(pitchWidth).reduced(4, 0));
        pitchArea.removeFromLeft(4);
        grainKnob->setBounds(pitchArea.reduced(4, 0));
    }

    if (! pitchMode)
    {
        auto frequencyArea = frequencySectionBounds.reduced(18, 14);
        auto displayArea = frequencyArea.removeFromTop((int) std::round(frequencyArea.getHeight() * 0.50f));
        engineDisplay->setBounds(juce::Rectangle<int>((int) std::round(displayArea.getWidth() * 0.70f),
                                                      (int) std::round(displayArea.getHeight() * 0.80f))
                                     .withCentre(displayArea.getCentre()));
        frequencyArea.removeFromTop(12);
        frequencyKnob->setBounds(juce::Rectangle<int>(168, 170).withCentre({ frequencyArea.getCentreX(), frequencyArea.getY() + 92 }));
    }

    auto sharedArea = sharedSectionBounds.reduced(14, 14);
    sharedArea.removeFromTop(36);
    const int knobRowHeight = juce::jmin(150, sharedArea.getHeight());
    auto knobRow = juce::Rectangle<int>(sharedArea.getWidth(), knobRowHeight).withCentre(sharedArea.getCentre());
    const int sharedWidth = (knobRow.getWidth() - 8) / 3;
    toneKnob->setBounds(knobRow.removeFromLeft(sharedWidth).reduced(4, 0));
    knobRow.removeFromLeft(4);
    mixKnob->setBounds(knobRow.removeFromLeft(sharedWidth).reduced(4, 0));
    knobRow.removeFromLeft(4);
    outputKnob->setBounds(knobRow.reduced(4, 0));
}
