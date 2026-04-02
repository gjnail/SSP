#include "PrismControlsComponent.h"

namespace
{
constexpr float minFilterHz = 20.0f;
constexpr float maxFilterHz = 20000.0f;

const juce::Colour coralAccent(0xffff8f88);
const juce::Colour lilacAccent(0xffb38fff);
const juce::Colour iceAccent(0xff9de7ff);
const juce::Colour emberAccent(0xffffb08a);

struct ChordDefinition
{
    const char* name;
    std::array<int, 5> intervals;
    int count = 3;
};

const std::array<ChordDefinition, 8>& getChordDefinitions()
{
    static const std::array<ChordDefinition, 8> chords {{
        { "Major Triad", { 0, 4, 7, 0, 0 }, 3 },
        { "Minor Triad", { 0, 3, 7, 0, 0 }, 3 },
        { "Sus2",        { 0, 2, 7, 0, 0 }, 3 },
        { "Sus4",        { 0, 5, 7, 0, 0 }, 3 },
        { "Major 7",     { 0, 4, 7, 11, 0 }, 4 },
        { "Minor 7",     { 0, 3, 7, 10, 0 }, 4 },
        { "Power",       { 0, 7, 12, 0, 0 }, 3 },
        { "Pentatonic",  { 0, 2, 4, 7, 9 }, 5 }
    }};

    return chords;
}

int getChoiceIndex(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return juce::roundToInt(raw->load());

    return 0;
}

juce::String formatMilliseconds(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, 2) + " s";

    return juce::String(juce::roundToInt((float) value)) + " ms";
}

juce::String formatShift(double value)
{
    if (std::abs(value) < 0.5)
        return "0 Hz";

    return juce::String(juce::roundToInt((float) value)) + " Hz";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatSignedPercent(double value)
{
    const auto percent = juce::roundToInt((float) value * 100.0f);
    return juce::String(percent > 0 ? "+" : "") + juce::String(percent) + "%";
}

juce::String formatRate(bool synced, double value)
{
    static const std::array<const char*, 8> divisions { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

    if (synced)
    {
        const auto index = juce::jlimit(0, (int) divisions.size() - 1,
                                        juce::roundToInt((float) value * (float) (divisions.size() - 1)));
        return divisions[(size_t) index];
    }

    const float hz = juce::jmap((float) value, 0.0f, 1.0f, 0.12f, 12.0f);
    return juce::String(hz, hz < 1.0f ? 2 : 1) + " Hz";
}

juce::String formatShape(double value)
{
    if (value < 0.33)
        return "Sine";

    if (value < 0.66)
        return "Tri";

    return "Step";
}

juce::String formatFrequency(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String(juce::roundToInt((float) value)) + " Hz";
}

float frequencyToNormalised(float frequency)
{
    const float clamped = juce::jlimit(minFilterHz, maxFilterHz, frequency);
    const float minLog = std::log(minFilterHz);
    const float maxLog = std::log(maxFilterHz);
    return (std::log(clamped) - minLog) / (maxLog - minLog);
}

float normalisedToFrequency(float normalised)
{
    const float minLog = std::log(minFilterHz);
    const float maxLog = std::log(maxFilterHz);
    return std::exp(minLog + juce::jlimit(0.0f, 1.0f, normalised) * (maxLog - minLog));
}

void setParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::Colour withSectionTint(juce::Colour accent, float alpha)
{
    return accent.withAlpha(alpha);
}
} // namespace

class PrismControlsComponent::ParameterKnob final : public juce::Component
{
public:
    ParameterKnob(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& labelText,
                  juce::Colour accentColour,
                  std::function<juce::String(double)> formatterToUse = {},
                  bool prominentStyle = false)
        : attachment(state, parameterID, slider),
          formatter(std::move(formatterToUse)),
          prominent(prominentStyle),
          accent(accentColour)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);

        slider.setName(labelText);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setColour(juce::Slider::thumbColourId, accentColour.brighter(0.18f));

        if (auto* parameter = state.getParameter(parameterID))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));

        slider.onValueChange = [this] { refreshValue(); };

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(reverbui::smallCapsFont(prominent ? 12.2f : 11.0f));
        label.setColour(juce::Label::textColourId, reverbui::textStrong().withAlpha(0.94f));
        label.setInterceptsMouseClicks(false, false);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(reverbui::bodyFont(prominent ? 9.9f : 9.2f));
        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.10f));
        valueLabel.setInterceptsMouseClicks(false, false);

        refreshValue();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(prominent ? 38 : 34);
        label.setBounds(footer.removeFromTop(18));
        valueLabel.setBounds(footer);
        slider.setBounds(area.reduced(prominent ? 0 : 2, 0));
    }

private:
    void refreshValue()
    {
        if (formatter)
            valueLabel.setText(formatter(slider.getValue()), juce::dontSendNotification);
        else
            valueLabel.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification);
    }

    reverbui::SSPKnob slider;
    juce::Label label;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    std::function<juce::String(double)> formatter;
    bool prominent = false;
    juce::Colour accent;
};

class PrismControlsComponent::ChoiceField final : public juce::Component
{
public:
    ChoiceField(juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                const juce::String& labelText,
                const juce::StringArray& items)
        : attachment(state, parameterID, combo)
    {
        addAndMakeVisible(label);
        addAndMakeVisible(combo);

        combo.addItemList(items, 1);
        combo.setSelectedItemIndex(getChoiceIndex(state, parameterID), juce::dontSendNotification);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(reverbui::smallCapsFont(10.2f));
        label.setColour(juce::Label::textColourId, reverbui::textMuted());
        label.setInterceptsMouseClicks(false, false);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(14));
        area.removeFromTop(4);
        combo.setBounds(area);
    }

private:
    juce::Label label;
    reverbui::SSPDropdown combo;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
};

class PrismControlsComponent::NoteDisplay final : public juce::Component,
                                                 private juce::Timer
{
public:
    explicit NoteDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        startTimerHz(18);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(4.0f, 6.0f);
        auto pillArea = area.removeFromTop(76.0f);

        juce::ColourGradient fill(juce::Colour(0xff141b24), pillArea.getTopLeft(),
                                  juce::Colour(0xff0d131a), pillArea.getBottomLeft(), false);
        fill.addColour(0.5, juce::Colour(0xff181f29));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(pillArea, 12.0f);
        g.setColour(reverbui::outlineSoft().withAlpha(0.9f));
        g.drawRoundedRectangle(pillArea.reduced(0.5f), 12.0f, 1.0f);

        auto nodesArea = area.reduced(10.0f, 0.0f);
        const auto chordIndex = juce::jlimit(0, (int) getChordDefinitions().size() - 1, getChoiceIndex(apvts, "chord"));
        const auto rootIndex = juce::jlimit(0, 11, getChoiceIndex(apvts, "rootNote"));
        const auto spreadIndex = juce::jlimit(0, 3, getChoiceIndex(apvts, "spread"));
        const auto& chord = getChordDefinitions()[(size_t) chordIndex];

        std::array<bool, 12> active{};
        active[(size_t) rootIndex] = true;
        for (int i = 0; i < chord.count; ++i)
            active[(size_t) ((rootIndex + chord.intervals[(size_t) i]) % 12)] = true;

        const float step = nodesArea.getWidth() / 11.0f;
        const float yTop = nodesArea.getY() + 12.0f;
        const float yBottom = nodesArea.getBottom() - 14.0f;

        for (int i = 0; i < 12; ++i)
        {
            const float x = nodesArea.getX() + step * (float) i;
            const bool isRoot = i == rootIndex;
            const bool isActive = active[(size_t) i];
            const float y = (i % 2 == 0) ? yBottom : yTop;
            auto dot = juce::Rectangle<float>(24.0f, 24.0f).withCentre({ x, y });

            g.setColour(juce::Colour(0xff262f3c));
            g.fillEllipse(dot);
            g.setColour(reverbui::outlineSoft());
            g.drawEllipse(dot, 1.0f);

            if (isActive)
            {
                const auto accent = isRoot ? coralAccent : lilacAccent.withMultipliedBrightness(1.08f);
                g.setColour(accent.withAlpha(isRoot ? 0.98f : 0.88f));
                g.fillEllipse(dot.reduced(1.8f));
            }
        }

        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(9.0f));
        g.drawFittedText(spreadIndex == 0 ? "TIGHT VOICING" : "SPREAD UP TO " + juce::String(spreadIndex) + " OCT",
                         getLocalBounds().removeFromBottom(16),
                         juce::Justification::centredLeft, 1);
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
};

class PrismControlsComponent::FrequencyRangeSlider final : public juce::Component,
                                                          private juce::Timer
{
public:
    explicit FrequencyRangeSlider(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
        startTimerHz(18);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(8.0f, 12.0f);
        auto track = bounds.removeFromBottom(18.0f).withTrimmedTop(4.0f);
        const float lowCut = apvts.getRawParameterValue("lowCutHz")->load();
        const float highCut = apvts.getRawParameterValue("highCutHz")->load();
        const float lowX = track.getX() + frequencyToNormalised(lowCut) * track.getWidth();
        const float highX = track.getX() + frequencyToNormalised(highCut) * track.getWidth();

        g.setColour(juce::Colour(0xff2a3340));
        g.fillRoundedRectangle(track.withHeight(4.0f).withCentre({ track.getCentreX(), track.getCentreY() }), 2.0f);

        auto active = juce::Rectangle<float>(highX - lowX, 4.0f)
                          .withPosition(lowX, track.getCentreY() - 2.0f);
        juce::ColourGradient activeFill(coralAccent.withAlpha(0.95f), active.getTopLeft(),
                                        lilacAccent.withAlpha(0.90f), active.getTopRight(), false);
        g.setGradientFill(activeFill);
        g.fillRoundedRectangle(active, 2.0f);

        auto drawHandle = [&](float x, juce::Colour colour)
        {
            auto handle = juce::Rectangle<float>(15.0f, 15.0f).withCentre({ x, track.getCentreY() });
            g.setColour(juce::Colour(0xff151c24));
            g.fillEllipse(handle);
            g.setColour(colour);
            g.fillEllipse(handle.reduced(2.4f));
            g.setColour(juce::Colour(0xff0b1017));
            g.drawEllipse(handle.reduced(2.4f), 1.0f);
        };

        drawHandle(lowX, coralAccent);
        drawHandle(highX, lilacAccent);

        g.setFont(reverbui::bodyFont(9.0f));
        g.setColour(reverbui::textMuted());
        g.drawText(formatFrequency(lowCut), bounds.removeFromLeft(bounds.getWidth() * 0.45f).toNearestInt(),
                   juce::Justification::centredLeft, false);
        g.drawText(formatFrequency(highCut), bounds.toNearestInt(), juce::Justification::centredRight, false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto lowCut = apvts.getRawParameterValue("lowCutHz")->load();
        const auto highCut = apvts.getRawParameterValue("highCutHz")->load();
        const auto track = getTrackBounds();
        const float lowX = track.getX() + frequencyToNormalised(lowCut) * track.getWidth();
        const float highX = track.getX() + frequencyToNormalised(highCut) * track.getWidth();

        activeHandle = std::abs(event.position.x - lowX) < std::abs(event.position.x - highX) ? 0 : 1;

        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(activeHandle == 0 ? "lowCutHz" : "highCutHz")))
            parameter->beginChangeGesture();

        mouseDrag(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (activeHandle < 0)
            return;

        const auto track = getTrackBounds();
        const float norm = juce::jlimit(0.0f, 1.0f, (event.position.x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
        const float value = normalisedToFrequency(norm);

        const float currentLow = apvts.getRawParameterValue("lowCutHz")->load();
        const float currentHigh = apvts.getRawParameterValue("highCutHz")->load();

        if (activeHandle == 0)
            setParameterValue(apvts, "lowCutHz", juce::jmin(value, currentHigh - 60.0f));
        else
            setParameterValue(apvts, "highCutHz", juce::jmax(value, currentLow + 60.0f));
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (activeHandle >= 0)
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(activeHandle == 0 ? "lowCutHz" : "highCutHz")))
                parameter->endChangeGesture();

        activeHandle = -1;
    }

private:
    juce::Rectangle<float> getTrackBounds() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(16.0f, 10.0f);
        return bounds.removeFromBottom(24.0f).withTrimmedTop(4.0f);
    }

    void timerCallback() override
    {
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    int activeHandle = -1;
};

PrismControlsComponent::PrismControlsComponent(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    chordChoice = std::make_unique<ChoiceField>(apvts, "chord", "Chord", juce::StringArray{
        "Major Triad", "Minor Triad", "Sus2", "Sus4", "Major 7", "Minor 7", "Power", "Pentatonic"
    });
    rootChoice = std::make_unique<ChoiceField>(apvts, "rootNote", "Root", juce::StringArray{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    });
    octaveChoice = std::make_unique<ChoiceField>(apvts, "octave", "Octave", juce::StringArray{
        "0", "1", "2", "3", "4"
    });
    spreadChoice = std::make_unique<ChoiceField>(apvts, "spread", "Spread", juce::StringArray{
        "Tight", "1 Oct", "2 Oct", "3 Oct"
    });
    noteDisplay = std::make_unique<NoteDisplay>(apvts);
    attackKnob = std::make_unique<ParameterKnob>(apvts, "attackMs", "Attack", coralAccent, formatMilliseconds);
    decayKnob = std::make_unique<ParameterKnob>(apvts, "decayMs", "Decay", lilacAccent, formatMilliseconds);

    arpPatternChoice = std::make_unique<ChoiceField>(apvts, "arpPattern", "Pattern", juce::StringArray{
        "Up", "Down", "Ping Pong", "Random", "Chord"
    });
    arpRateKnob = std::make_unique<ParameterKnob>(apvts, "arpRate", "Rate", coralAccent,
                                                  [this](double value)
                                                  {
                                                      return formatRate(apvts.getRawParameterValue("arpSync")->load() >= 0.5f, value);
                                                  });
    arpEnabledToggle = std::make_unique<reverbui::SSPToggle>("On");
    arpEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpEnabled", *arpEnabledToggle);
    arpSyncToggle = std::make_unique<reverbui::SSPToggle>("Sync");
    arpSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpSync", *arpSyncToggle);

    warpKnob = std::make_unique<ParameterKnob>(apvts, "warp", "Warp", lilacAccent, formatSignedPercent);
    driftKnob = std::make_unique<ParameterKnob>(apvts, "drift", "Drift", coralAccent, formatPercent);
    shiftKnob = std::make_unique<ParameterKnob>(apvts, "shiftHz", "Shift", iceAccent, formatShift);

    motionStrengthKnob = std::make_unique<ParameterKnob>(apvts, "motionStrength", "Strength", coralAccent, formatPercent);
    motionRateKnob = std::make_unique<ParameterKnob>(apvts, "motionRate", "Rate", lilacAccent,
                                                     [this](double value)
                                                     {
                                                         return formatRate(apvts.getRawParameterValue("motionSync")->load() >= 0.5f, value);
                                                     });
    motionShapeKnob = std::make_unique<ParameterKnob>(apvts, "motionShape", "Shape", emberAccent, formatShape);
    motionUnisonKnob = std::make_unique<ParameterKnob>(apvts, "motionUnison", "Unison", iceAccent, formatPercent);
    motionSyncToggle = std::make_unique<reverbui::SSPToggle>("Sync");
    motionSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "motionSync", *motionSyncToggle);

    filterTiltKnob = std::make_unique<ParameterKnob>(apvts, "filterTilt", "Tilt", coralAccent, formatSignedPercent);
    filterQKnob = std::make_unique<ParameterKnob>(apvts, "filterQ", "Q", lilacAccent,
                                                  [](double value) { return juce::String(value, 2); });
    depthKnob = std::make_unique<ParameterKnob>(apvts, "depth", "Depth", lilacAccent, formatPercent, true);
    rangeSlider = std::make_unique<FrequencyRangeSlider>(apvts);

    addAndMakeVisible(*chordChoice);
    addAndMakeVisible(*rootChoice);
    addAndMakeVisible(*octaveChoice);
    addAndMakeVisible(*spreadChoice);
    addAndMakeVisible(*noteDisplay);
    addAndMakeVisible(*attackKnob);
    addAndMakeVisible(*decayKnob);

    addAndMakeVisible(*arpEnabledToggle);
    addAndMakeVisible(*arpPatternChoice);
    addAndMakeVisible(*arpRateKnob);
    addAndMakeVisible(*arpSyncToggle);

    addAndMakeVisible(*warpKnob);
    addAndMakeVisible(*driftKnob);
    addAndMakeVisible(*shiftKnob);

    addAndMakeVisible(*motionStrengthKnob);
    addAndMakeVisible(*motionRateKnob);
    addAndMakeVisible(*motionShapeKnob);
    addAndMakeVisible(*motionUnisonKnob);
    addAndMakeVisible(*motionSyncToggle);

    addAndMakeVisible(*filterTiltKnob);
    addAndMakeVisible(*filterQKnob);
    addAndMakeVisible(*depthKnob);
    addAndMakeVisible(*rangeSlider);
}

PrismControlsComponent::~PrismControlsComponent() = default;

void PrismControlsComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    reverbui::drawPanelBackground(g, bounds, coralAccent, 15.0f);

    auto content = bounds.reduced(18.0f, 14.0f);
    g.setColour(reverbui::outlineSoft().withAlpha(0.95f));

    for (auto dividerX : { chordSection.getRight(), arpSection.getRight(), spacingSection.getRight(), motionSection.getRight() })
        g.drawVerticalLine(dividerX, content.getY() + 10.0f, content.getBottom() - 10.0f);

    drawSectionChrome(g, chordSection, "Bismuth Core", coralAccent);
    drawSectionChrome(g, arpSection, "Arp", coralAccent);
    drawSectionChrome(g, spacingSection, "Spacing", lilacAccent);
    drawSectionChrome(g, motionSection, "Motion", coralAccent);
    drawSectionChrome(g, filterSection, "Filter", lilacAccent);
}

void PrismControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 14);
    const int gap = 0;
    const int headerHeight = 22;

    chordSection = area.removeFromLeft(juce::roundToInt((float) getWidth() * 0.34f));
    area.removeFromLeft(gap);
    arpSection = area.removeFromLeft(juce::roundToInt((float) getWidth() * 0.13f));
    area.removeFromLeft(gap);
    spacingSection = area.removeFromLeft(juce::roundToInt((float) getWidth() * 0.16f));
    area.removeFromLeft(gap);
    motionSection = area.removeFromLeft(juce::roundToInt((float) getWidth() * 0.18f));
    area.removeFromLeft(gap);
    filterSection = area;

    auto chordContent = chordSection.reduced(16, 12);
    chordContent.removeFromTop(headerHeight);
    auto chordRight = chordContent.removeFromRight(102);
    auto chordFooter = chordContent.removeFromBottom(58);
    chordChoice->setBounds(chordContent.removeFromTop(40));
    chordContent.removeFromTop(8);
    noteDisplay->setBounds(chordContent);
    rootChoice->setBounds(chordFooter.removeFromLeft(chordFooter.getWidth() / 3).reduced(0, 2));
    octaveChoice->setBounds(chordFooter.removeFromLeft(chordFooter.getWidth() / 2).reduced(6, 2));
    spreadChoice->setBounds(chordFooter.reduced(6, 2));
    attackKnob->setBounds(chordRight.removeFromTop(114));
    chordRight.removeFromTop(8);
    decayKnob->setBounds(chordRight.removeFromTop(114));

    auto arpContent = arpSection.reduced(14, 12);
    arpContent.removeFromTop(headerHeight);
    arpEnabledToggle->setBounds(arpContent.removeFromTop(28));
    arpContent.removeFromTop(4);
    arpPatternChoice->setBounds(arpContent.removeFromTop(40));
    arpContent.removeFromTop(8);
    arpRateKnob->setBounds(arpContent.removeFromTop(116));
    arpSyncToggle->setBounds(arpContent.removeFromTop(30));

    auto spacingContent = spacingSection.reduced(14, 12);
    spacingContent.removeFromTop(headerHeight);
    auto spacingTop = spacingContent.removeFromTop(110);
    warpKnob->setBounds(spacingTop.removeFromLeft(spacingTop.getWidth() / 2).withTrimmedRight(6));
    shiftKnob->setBounds(spacingTop.withTrimmedLeft(6));
    spacingContent.removeFromTop(4);
    driftKnob->setBounds(spacingContent.removeFromTop(118).withTrimmedRight(32));

    auto motionContent = motionSection.reduced(14, 12);
    motionContent.removeFromTop(headerHeight);
    auto motionTop = motionContent.removeFromTop(112);
    auto motionBottom = motionContent.removeFromTop(116);
    motionStrengthKnob->setBounds(motionTop.removeFromLeft(motionTop.getWidth() / 2).withTrimmedRight(6));
    motionRateKnob->setBounds(motionTop.withTrimmedLeft(6));
    motionUnisonKnob->setBounds(motionBottom.removeFromLeft(motionBottom.getWidth() / 2).withTrimmedRight(6));
    motionShapeKnob->setBounds(motionBottom.withTrimmedLeft(6));
    motionSyncToggle->setBounds(motionContent.removeFromTop(30));

    auto filterContent = filterSection.reduced(14, 12);
    filterContent.removeFromTop(headerHeight);
    auto filterBottom = filterContent.removeFromBottom(54);
    auto filterMain = filterContent;
    auto filterLeft = filterMain.removeFromLeft(118);
    filterMain.removeFromLeft(8);
    filterTiltKnob->setBounds(filterLeft.removeFromTop(114));
    filterLeft.removeFromTop(8);
    filterQKnob->setBounds(filterLeft.removeFromTop(114));
    depthKnob->setBounds(filterMain.removeFromTop(188));
    rangeSlider->setBounds(filterBottom);
}

void PrismControlsComponent::drawSectionChrome(juce::Graphics& g,
                                               juce::Rectangle<int> bounds,
                                               const juce::String& title,
                                               juce::Colour accent)
{
    auto titleArea = bounds.reduced(14, 10).removeFromTop(18);
    g.setColour(accent.withAlpha(0.92f));
    g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre({ (float) titleArea.getX() + 6.0f,
                                                                   (float) titleArea.getCentreY() }));

    g.setColour(reverbui::textStrong());
    g.setFont(reverbui::titleFont(13.4f));
    g.drawText(title, titleArea.withTrimmedLeft(18), juce::Justification::centredLeft, false);

    auto wash = bounds.toFloat().reduced(1.0f);
    juce::ColourGradient sectionGlow(withSectionTint(accent, 0.07f), wash.getTopLeft(),
                                     juce::Colours::transparentBlack, wash.getBottomRight(), false);
    g.setGradientFill(sectionGlow);
    g.fillRect(wash);
}
