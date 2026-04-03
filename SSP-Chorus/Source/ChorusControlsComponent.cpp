#include "ChorusControlsComponent.h"

namespace
{
constexpr float minGraphFrequency = 20.0f;
constexpr float maxGraphFrequency = 20000.0f;

float mapFrequencyToX(float frequency, juce::Rectangle<float> bounds)
{
    const float logMin = std::log(minGraphFrequency);
    const float logMax = std::log(maxGraphFrequency);
    const float clamped = juce::jlimit(minGraphFrequency, maxGraphFrequency, frequency);
    const float norm = (std::log(clamped) - logMin) / (logMax - logMin);
    return bounds.getX() + norm * bounds.getWidth();
}

void drawGraphFrame(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient fill(juce::Colour(0xff0f161f), bounds.getTopLeft(),
                              juce::Colour(0xff091018), bounds.getBottomLeft(), false);
    fill.addColour(0.55, juce::Colour(0xff121a25));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 10.0f);

    g.setColour(reverbui::outlineSoft().withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);
}

void drawFrequencyGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    static constexpr std::array<float, 10> frequencies {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };

    g.setColour(reverbui::graphGrid());

    for (float frequency : frequencies)
    {
        const float x = mapFrequencyToX(frequency, bounds);
        g.drawVerticalLine((int) std::round(x), bounds.getY(), bounds.getBottom());
    }

    for (int row = 1; row < 4; ++row)
    {
        const float y = bounds.getY() + bounds.getHeight() * ((float) row / 4.0f);
        g.drawHorizontalLine((int) std::round(y), bounds.getX(), bounds.getRight());
    }
}

void drawSectionText(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     const juce::String& title,
                     juce::Colour accent)
{
    auto titleArea = bounds.reduced(18, 10).removeFromTop(24);
    g.setColour(reverbui::textStrong());
    g.setFont(reverbui::titleFont(15.0f));
    g.drawText(title, titleArea, juce::Justification::centredLeft, false);

    auto marker = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ (float) titleArea.getX() - 8.0f, (float) titleArea.getCentreY() });
    g.setColour(accent);
    g.fillEllipse(marker);
}

void drawSubPanel(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent)
{
    auto area = bounds.toFloat();
    juce::ColourGradient fill(juce::Colour(0xff111922), area.getTopLeft(),
                              juce::Colour(0xff0d141c), area.getBottomLeft(), false);
    fill.addColour(0.42, accent.withAlpha(0.05f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(area, 12.0f);

    g.setColour(reverbui::outlineSoft().withAlpha(0.95f));
    g.drawRoundedRectangle(area.reduced(0.5f), 12.0f, 1.0f);
    g.setColour(reverbui::textMuted());
    g.setFont(reverbui::smallCapsFont(11.0f));
    g.drawText(title, bounds.reduced(12, 10).removeFromTop(16), juce::Justification::centredLeft, false);
}

float getParameterValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

void setParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

struct ChorusTypePreset
{
    const char* name;
    float loCutHz;
    float hiCutHz;
    float motionAmount;
    float motionRate;
    float motionShape;
    bool motionSpin;
    float voiceCrossoverHz;
    float lowVoiceAmount;
    float lowVoiceScale;
    float lowVoiceRate;
    float highVoiceAmount;
    float highVoiceScale;
    float highVoiceRate;
    float delayMs;
    float feedback;
    float tone;
    float stereoWidth;
    float drive;
    float dryWet;
    float shineAmount;
    float shineRate;
    float spreadMs;
    bool vibrato;
    bool focusCut;
};

const std::array<ChorusTypePreset, 6>& getChorusTypePresets()
{
    static const std::array<ChorusTypePreset, 6> presets {{
        { "Studio", 120.0f, 15000.0f, 34.0f, 0.45f, 42.0f, true, 1400.0f, 56.0f, 108.0f, 0.28f, 64.0f, 84.0f, 0.72f, 9.0f, 18.0f, 54.0f, 118.0f, 8.0f, 32.0f, 12.0f, 1.00f, 4.0f, false, false },
        { "Wide", 90.0f, 17000.0f, 48.0f, 0.62f, 56.0f, true, 1250.0f, 62.0f, 128.0f, 0.34f, 78.0f, 96.0f, 0.95f, 10.8f, 22.0f, 60.0f, 156.0f, 10.0f, 40.0f, 18.0f, 1.28f, 6.8f, false, false },
        { "Ensemble", 70.0f, 15500.0f, 58.0f, 0.38f, 64.0f, true, 980.0f, 74.0f, 148.0f, 0.24f, 86.0f, 118.0f, 0.62f, 13.5f, 28.0f, 50.0f, 148.0f, 14.0f, 46.0f, 24.0f, 0.84f, 8.5f, false, true },
        { "Liquid", 110.0f, 18000.0f, 66.0f, 0.86f, 72.0f, true, 1800.0f, 48.0f, 102.0f, 0.48f, 84.0f, 88.0f, 1.65f, 7.5f, 20.0f, 68.0f, 126.0f, 12.0f, 38.0f, 32.0f, 1.85f, 5.0f, false, false },
        { "Vibrato", 140.0f, 14500.0f, 72.0f, 1.35f, 58.0f, false, 2200.0f, 36.0f, 82.0f, 0.68f, 58.0f, 72.0f, 2.20f, 6.0f, 12.0f, 62.0f, 110.0f, 6.0f, 100.0f, 44.0f, 2.30f, 3.2f, true, false },
        { "Glow", 85.0f, 13500.0f, 44.0f, 0.28f, 46.0f, true, 1120.0f, 64.0f, 136.0f, 0.20f, 74.0f, 102.0f, 0.55f, 15.5f, 34.0f, 46.0f, 138.0f, 24.0f, 42.0f, 36.0f, 0.72f, 9.5f, false, true }
    }};

    return presets;
}
} // namespace

class ChorusControlsComponent::ParameterKnob final : public juce::Component
{
public:
    ParameterKnob(juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& labelText,
                  juce::Colour accentColour,
                  bool prominentStyle = false)
        : attachment(state, parameterID, slider),
          prominent(prominentStyle)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);

        slider.setName(labelText);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);

        if (auto* parameter = state.getParameter(parameterID))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));

        label.setText(labelText, juce::dontSendNotification);
        label.setFont(reverbui::smallCapsFont(prominent ? 13.0f : 12.0f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, reverbui::textStrong().withAlpha(prominent ? 0.98f : 0.90f));
        label.setInterceptsMouseClicks(false, false);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int labelHeight = prominent ? 24 : 22;
        auto labelArea = area.removeFromTop(labelHeight);
        area.removeFromTop(4);
        slider.setBounds(area.reduced(prominent ? 1 : 2));
        label.setBounds(labelArea);
    }

private:
    reverbui::SSPKnob slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool prominent = false;
};

class ChorusControlsComponent::InputFilterGraph final : public juce::Component
{
public:
    explicit InputFilterGraph(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        drawGraphFrame(g, area);
        auto plot = area.reduced(12.0f, 10.0f);
        drawFrequencyGrid(g, plot);

        const float loCut = getParameterValue(apvts, "loCutHz");
        const float hiCut = getParameterValue(apvts, "hiCutHz");

        juce::Path fillPath;
        juce::Path strokePath;

        auto responseToY = [plot](float magnitude)
        {
            const float db = juce::Decibels::gainToDecibels(juce::jmax(0.03f, magnitude), -36.0f);
            return juce::jmap(db, -36.0f, 0.0f, plot.getBottom(), plot.getY() + 4.0f);
        };

        for (int point = 0; point <= (int) plot.getWidth(); ++point)
        {
            const float x = plot.getX() + (float) point;
            const float norm = (x - plot.getX()) / plot.getWidth();
            const float frequency = std::exp(std::log(minGraphFrequency) + norm * (std::log(maxGraphFrequency) - std::log(minGraphFrequency)));
            const float highPass = 1.0f / std::sqrt(1.0f + std::pow(loCut / juce::jmax(1.0f, frequency), 4.0f));
            const float lowPass = 1.0f / std::sqrt(1.0f + std::pow(juce::jmax(1.0f, frequency) / juce::jmax(loCut + 10.0f, hiCut), 4.0f));
            const float y = responseToY(highPass * lowPass);

            if (point == 0)
            {
                strokePath.startNewSubPath(x, y);
                fillPath.startNewSubPath(x, plot.getBottom());
                fillPath.lineTo(x, y);
            }
            else
            {
                strokePath.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }

        fillPath.lineTo(plot.getRight(), plot.getBottom());
        fillPath.closeSubPath();

        juce::ColourGradient curveFill(reverbui::brandCyan().withAlpha(0.24f), plot.getTopLeft(),
                                       reverbui::brandMint().withAlpha(0.10f), plot.getBottomRight(), false);
        g.setGradientFill(curveFill);
        g.fillPath(fillPath);

        g.setColour(reverbui::brandCyan().withAlpha(0.95f));
        g.strokePath(strokePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float loX = mapFrequencyToX(loCut, plot);
        const float hiX = mapFrequencyToX(hiCut, plot);
        g.setColour(reverbui::brandCyan().withAlpha(0.55f));
        g.drawVerticalLine((int) std::round(loX), plot.getY(), plot.getBottom());
        g.setColour(reverbui::brandMint().withAlpha(0.55f));
        g.drawVerticalLine((int) std::round(hiX), plot.getY(), plot.getBottom());
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class ChorusControlsComponent::VoiceGraph final : public juce::Component
{
public:
    explicit VoiceGraph(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        drawGraphFrame(g, area);
        auto plot = area.reduced(12.0f, 10.0f);
        drawFrequencyGrid(g, plot);

        const float crossover = getParameterValue(apvts, "voiceCrossoverHz");
        const float lowAmount = getParameterValue(apvts, "lowVoiceAmount") / 100.0f;
        const float lowScale = getParameterValue(apvts, "lowVoiceScale") / 200.0f;
        const float lowRate = getParameterValue(apvts, "lowVoiceRate") / 4.0f;
        const float highAmount = getParameterValue(apvts, "highVoiceAmount") / 100.0f;
        const float highScale = getParameterValue(apvts, "highVoiceScale") / 200.0f;
        const float highRate = getParameterValue(apvts, "highVoiceRate") / 8.0f;

        juce::Path lowFill;
        juce::Path highFill;
        juce::Path lowStroke;
        juce::Path highStroke;
        juce::Path sumStroke;

        auto toY = [plot](float level)
        {
            return plot.getBottom() - juce::jlimit(0.0f, 1.0f, level) * (plot.getHeight() * 0.88f);
        };

        for (int point = 0; point <= (int) plot.getWidth(); ++point)
        {
            const float x = plot.getX() + (float) point;
            const float norm = (x - plot.getX()) / plot.getWidth();
            const float frequency = std::exp(std::log(minGraphFrequency) + norm * (std::log(maxGraphFrequency) - std::log(minGraphFrequency)));
            const float logDelta = (std::log(frequency) - std::log(crossover)) / std::log(maxGraphFrequency / minGraphFrequency);
            float transition = juce::jlimit(0.0f, 1.0f, 0.5f + logDelta * 5.0f);
            transition = transition * transition * (3.0f - 2.0f * transition);
            const float lowInfluence = 1.0f - transition;
            const float highInfluence = transition;
            const float lowShape = (0.18f + lowAmount * 0.54f + lowScale * 0.18f + lowRate * 0.10f)
                                   * (0.84f + 0.16f * std::cos(norm * juce::MathConstants<float>::pi * 0.82f));
            const float highShape = (0.18f + highAmount * 0.56f + highScale * 0.18f + highRate * 0.11f)
                                    * (0.78f + 0.22f * std::sin((0.22f + norm) * juce::MathConstants<float>::pi * 0.96f));

            const float lowY = toY(lowShape * lowInfluence);
            const float highY = toY(highShape * highInfluence);
            const float sumY = toY(juce::jlimit(0.0f, 1.0f, lowShape * lowInfluence + highShape * highInfluence));

            if (point == 0)
            {
                lowFill.startNewSubPath(x, plot.getBottom());
                lowFill.lineTo(x, lowY);
                lowStroke.startNewSubPath(x, lowY);
                highFill.startNewSubPath(x, plot.getBottom());
                highFill.lineTo(x, highY);
                highStroke.startNewSubPath(x, highY);
                sumStroke.startNewSubPath(x, sumY);
            }
            else
            {
                lowFill.lineTo(x, lowY);
                lowStroke.lineTo(x, lowY);
                highFill.lineTo(x, highY);
                highStroke.lineTo(x, highY);
                sumStroke.lineTo(x, sumY);
            }
        }

        lowFill.lineTo(plot.getRight(), plot.getBottom());
        lowFill.closeSubPath();
        highFill.lineTo(plot.getRight(), plot.getBottom());
        highFill.closeSubPath();

        g.setColour(reverbui::brandMint().withAlpha(0.18f));
        g.fillPath(lowFill);
        g.setColour(reverbui::brandGold().withAlpha(0.18f));
        g.fillPath(highFill);

        g.setColour(reverbui::brandMint().withAlpha(0.85f));
        g.strokePath(lowStroke, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(reverbui::brandGold().withAlpha(0.85f));
        g.strokePath(highStroke, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(reverbui::brandCyan().withAlpha(0.94f));
        g.strokePath(sumStroke, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float crossoverX = mapFrequencyToX(crossover, plot);
        g.setColour(reverbui::brandCyan().withAlpha(0.65f));
        g.drawVerticalLine((int) std::round(crossoverX), plot.getY(), plot.getBottom());

        g.setFont(reverbui::bodyFont(10.0f));
        g.setColour(reverbui::textMuted());
        auto leftLabel = plot.withWidth(plot.getWidth() * 0.24f);
        auto rightLabel = juce::Rectangle<float>(plot.getWidth() * 0.28f, plot.getHeight()).withRightX(plot.getRight()).withY(plot.getY());
        g.drawFittedText("Body Voice", leftLabel.toNearestInt(), juce::Justification::topLeft, 1);
        g.drawFittedText("Air Voice", rightLabel.toNearestInt(), juce::Justification::topRight, 1);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

ChorusControlsComponent::ChorusControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    juce::ignoreUnused(processor);

    inputFilterGraph = std::make_unique<InputFilterGraph>(state);
    voiceGraph = std::make_unique<VoiceGraph>(state);
    inputFilterGraphPtr = inputFilterGraph.get();
    voiceGraphPtr = voiceGraph.get();

    loCutKnob = std::make_unique<ParameterKnob>(state, "loCutHz", "Lo Cut", reverbui::brandCyan());
    hiCutKnob = std::make_unique<ParameterKnob>(state, "hiCutHz", "Hi Cut", reverbui::brandMint());
    motionAmountKnob = std::make_unique<ParameterKnob>(state, "motionAmount", "Amount", reverbui::brandMint());
    motionRateKnob = std::make_unique<ParameterKnob>(state, "motionRate", "Rate", reverbui::brandCyan());
    motionShapeKnob = std::make_unique<ParameterKnob>(state, "motionShape", "Shape", reverbui::brandGold());
    voiceCrossoverKnob = std::make_unique<ParameterKnob>(state, "voiceCrossoverHz", "Crossover", reverbui::brandCyan());
    lowVoiceAmountKnob = std::make_unique<ParameterKnob>(state, "lowVoiceAmount", "Amount", reverbui::brandMint());
    lowVoiceScaleKnob = std::make_unique<ParameterKnob>(state, "lowVoiceScale", "Depth", reverbui::brandCyan());
    lowVoiceRateKnob = std::make_unique<ParameterKnob>(state, "lowVoiceRate", "Rate", reverbui::brandGold());
    highVoiceAmountKnob = std::make_unique<ParameterKnob>(state, "highVoiceAmount", "Amount", reverbui::brandGold());
    highVoiceScaleKnob = std::make_unique<ParameterKnob>(state, "highVoiceScale", "Depth", reverbui::brandCyan());
    highVoiceRateKnob = std::make_unique<ParameterKnob>(state, "highVoiceRate", "Rate", reverbui::brandMint());
    delayKnob = std::make_unique<ParameterKnob>(state, "delayMs", "Delay", reverbui::brandMint());
    feedbackKnob = std::make_unique<ParameterKnob>(state, "feedback", "Feedback", reverbui::brandGold());
    toneKnob = std::make_unique<ParameterKnob>(state, "tone", "Tone", reverbui::brandCyan());
    widthKnob = std::make_unique<ParameterKnob>(state, "stereoWidth", "Stereo Width", reverbui::brandCyan());
    driveKnob = std::make_unique<ParameterKnob>(state, "drive", "Drive", reverbui::brandMint());
    dryWetKnob = std::make_unique<ParameterKnob>(state, "dryWet", "Dry / Wet", reverbui::brandGold(), true);
    shineAmountKnob = std::make_unique<ParameterKnob>(state, "shineAmount", "Shine Amt", reverbui::brandCyan());
    shineRateKnob = std::make_unique<ParameterKnob>(state, "shineRate", "Shine Rate", reverbui::brandMint());
    spreadKnob = std::make_unique<ParameterKnob>(state, "spreadMs", "Spread", reverbui::brandGold());

    loCutKnobPtr = loCutKnob.get();
    hiCutKnobPtr = hiCutKnob.get();
    motionAmountKnobPtr = motionAmountKnob.get();
    motionRateKnobPtr = motionRateKnob.get();
    motionShapeKnobPtr = motionShapeKnob.get();
    voiceCrossoverKnobPtr = voiceCrossoverKnob.get();
    lowVoiceAmountKnobPtr = lowVoiceAmountKnob.get();
    lowVoiceScaleKnobPtr = lowVoiceScaleKnob.get();
    lowVoiceRateKnobPtr = lowVoiceRateKnob.get();
    highVoiceAmountKnobPtr = highVoiceAmountKnob.get();
    highVoiceScaleKnobPtr = highVoiceScaleKnob.get();
    highVoiceRateKnobPtr = highVoiceRateKnob.get();
    delayKnobPtr = delayKnob.get();
    feedbackKnobPtr = feedbackKnob.get();
    toneKnobPtr = toneKnob.get();
    widthKnobPtr = widthKnob.get();
    driveKnobPtr = driveKnob.get();
    dryWetKnobPtr = dryWetKnob.get();
    shineAmountKnobPtr = shineAmountKnob.get();
    shineRateKnobPtr = shineRateKnob.get();
    spreadKnobPtr = spreadKnob.get();

    const std::array<juce::Component*, 23> components {
        inputFilterGraphPtr,
        voiceGraphPtr,
        loCutKnobPtr,
        hiCutKnobPtr,
        motionAmountKnobPtr,
        motionRateKnobPtr,
        motionShapeKnobPtr,
        voiceCrossoverKnobPtr,
        lowVoiceAmountKnobPtr,
        lowVoiceScaleKnobPtr,
        lowVoiceRateKnobPtr,
        highVoiceAmountKnobPtr,
        highVoiceScaleKnobPtr,
        highVoiceRateKnobPtr,
        delayKnobPtr,
        feedbackKnobPtr,
        toneKnobPtr,
        widthKnobPtr,
        driveKnobPtr,
        dryWetKnobPtr,
        shineAmountKnobPtr,
        shineRateKnobPtr,
        spreadKnobPtr
    };

    for (auto* component : components)
        addAndMakeVisible(component);

    addAndMakeVisible(motionSpinToggle);
    addAndMakeVisible(vibratoToggle);
    addAndMakeVisible(focusCutToggle);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeDropdown);

    typeLabel.setText("Chorus Type", juce::dontSendNotification);
    typeLabel.setFont(reverbui::smallCapsFont(11.0f));
    typeLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    typeLabel.setJustificationType(juce::Justification::centredLeft);

    typeDropdown.setTextWhenNothingSelected("Select chorus type");
    typeDropdown.setTooltip("Apply a classic chorus starting point.");
    for (const auto& preset : getChorusTypePresets())
        typeDropdown.addItem(preset.name, typeDropdown.getNumItems() + 1);

    typeDropdown.onChange = [this]
    {
        if (ignoreTypeSelection)
            return;

        const int index = typeDropdown.getSelectedItemIndex();
        if (index >= 0)
            applyTypePreset(index);
    };

    motionSpinAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "motionSpin", motionSpinToggle);
    vibratoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "vibrato", vibratoToggle);
    focusCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "focusCut", focusCutToggle);

    startTimerHz(30);
}

ChorusControlsComponent::~ChorusControlsComponent()
{
    stopTimer();
}

void ChorusControlsComponent::applyTypePreset(int presetIndex)
{
    const auto& preset = getChorusTypePresets()[(size_t) juce::jlimit(0, (int) getChorusTypePresets().size() - 1, presetIndex)];

    setParameterValue(apvts, "loCutHz", preset.loCutHz);
    setParameterValue(apvts, "hiCutHz", preset.hiCutHz);
    setParameterValue(apvts, "motionAmount", preset.motionAmount);
    setParameterValue(apvts, "motionRate", preset.motionRate);
    setParameterValue(apvts, "motionShape", preset.motionShape);
    setParameterValue(apvts, "motionSpin", preset.motionSpin ? 1.0f : 0.0f);
    setParameterValue(apvts, "voiceCrossoverHz", preset.voiceCrossoverHz);
    setParameterValue(apvts, "lowVoiceAmount", preset.lowVoiceAmount);
    setParameterValue(apvts, "lowVoiceScale", preset.lowVoiceScale);
    setParameterValue(apvts, "lowVoiceRate", preset.lowVoiceRate);
    setParameterValue(apvts, "highVoiceAmount", preset.highVoiceAmount);
    setParameterValue(apvts, "highVoiceScale", preset.highVoiceScale);
    setParameterValue(apvts, "highVoiceRate", preset.highVoiceRate);
    setParameterValue(apvts, "delayMs", preset.delayMs);
    setParameterValue(apvts, "feedback", preset.feedback);
    setParameterValue(apvts, "tone", preset.tone);
    setParameterValue(apvts, "stereoWidth", preset.stereoWidth);
    setParameterValue(apvts, "drive", preset.drive);
    setParameterValue(apvts, "dryWet", preset.dryWet);
    setParameterValue(apvts, "shineAmount", preset.shineAmount);
    setParameterValue(apvts, "shineRate", preset.shineRate);
    setParameterValue(apvts, "spreadMs", preset.spreadMs);
    setParameterValue(apvts, "vibrato", preset.vibrato ? 1.0f : 0.0f);
    setParameterValue(apvts, "focusCut", preset.focusCut ? 1.0f : 0.0f);
}

void ChorusControlsComponent::timerCallback()
{
    if (inputFilterGraphPtr != nullptr)
        inputFilterGraphPtr->repaint();

    if (voiceGraphPtr != nullptr)
        voiceGraphPtr->repaint();
}

void ChorusControlsComponent::paint(juce::Graphics& g)
{
    reverbui::drawPanelBackground(g, typeSelectorBounds.toFloat(), reverbui::brandMint(), 16.0f);
    reverbui::drawPanelBackground(g, inputSectionBounds.toFloat(), reverbui::brandCyan(), 16.0f);
    reverbui::drawPanelBackground(g, motionSectionBounds.toFloat(), reverbui::brandMint(), 16.0f);
    reverbui::drawPanelBackground(g, modifiersSectionBounds.toFloat(), reverbui::brandGold(), 16.0f);
    reverbui::drawPanelBackground(g, globalSectionBounds.toFloat(), reverbui::brandCyan(), 16.0f);
    reverbui::drawPanelBackground(g, voiceSectionBounds.toFloat(), reverbui::brandGold(), 16.0f);

    drawSectionText(g, inputSectionBounds, "Input Filter", reverbui::brandCyan());
    drawSectionText(g, motionSectionBounds, "Motion Field", reverbui::brandMint());
    drawSectionText(g, modifiersSectionBounds, "Modifiers", reverbui::brandGold());
    drawSectionText(g, globalSectionBounds, "Global Controls", reverbui::brandCyan());
    drawSectionText(g, voiceSectionBounds, "Voice Network", reverbui::brandGold());

    drawSubPanel(g, bodyVoiceBounds, "Body Voice", reverbui::brandMint());
    drawSubPanel(g, airVoiceBounds, "Air Voice", reverbui::brandGold());
}

void ChorusControlsComponent::resized()
{
    auto area = getLocalBounds();
    constexpr int gap = 14;
    constexpr int typeSelectorHeight = 46;
    constexpr int rowOneHeight = 268;

    typeSelectorBounds = area.removeFromTop(typeSelectorHeight);
    area.removeFromTop(gap);

    auto topRow = area.removeFromTop(rowOneHeight);
    inputSectionBounds = topRow.removeFromLeft(244);
    topRow.removeFromLeft(gap);
    motionSectionBounds = topRow.removeFromLeft(286);
    topRow.removeFromLeft(gap);
    modifiersSectionBounds = topRow.removeFromLeft(228);
    topRow.removeFromLeft(gap);
    globalSectionBounds = topRow;

    area.removeFromTop(gap);
    voiceSectionBounds = area;

    auto typeArea = typeSelectorBounds.reduced(16, 8);
    typeLabel.setBounds(typeArea.removeFromLeft(90).removeFromTop(18));
    typeDropdown.setBounds(juce::Rectangle<int>(230, 28).withCentre(typeArea.removeFromLeft(250).getCentre()));

    auto inputArea = inputSectionBounds.reduced(16, 14);
    inputArea.removeFromTop(32);
    inputFilterGraphPtr->setBounds(inputArea.removeFromTop(112));
    inputArea.removeFromTop(10);
    auto inputKnobs = inputArea.removeFromTop(128);
    auto leftInput = inputKnobs.removeFromLeft(inputKnobs.getWidth() / 2);
    loCutKnobPtr->setBounds(leftInput.reduced(4, 0));
    hiCutKnobPtr->setBounds(inputKnobs.reduced(4, 0));

    auto motionArea = motionSectionBounds.reduced(16, 14);
    motionArea.removeFromTop(32);
    auto motionKnobs = motionArea.removeFromTop(146);
    const int motionKnobWidth = motionKnobs.getWidth() / 3;
    motionAmountKnobPtr->setBounds(motionKnobs.removeFromLeft(motionKnobWidth).reduced(4, 0));
    motionRateKnobPtr->setBounds(motionKnobs.removeFromLeft(motionKnobWidth).reduced(4, 0));
    motionShapeKnobPtr->setBounds(motionKnobs.reduced(4, 0));
    motionArea.removeFromTop(8);
    motionSpinToggle.setBounds(motionArea.removeFromTop(34));

    auto modifiersArea = modifiersSectionBounds.reduced(16, 14);
    modifiersArea.removeFromTop(32);
    auto modifierTop = modifiersArea.removeFromTop(144);
    auto modifierTopLeft = modifierTop.removeFromLeft(modifierTop.getWidth() / 2);
    shineAmountKnobPtr->setBounds(modifierTopLeft.reduced(4, 0));
    shineRateKnobPtr->setBounds(modifierTop.reduced(4, 0));
    modifiersArea.removeFromTop(4);
    auto modifierBottom = modifiersArea.removeFromTop(126);
    auto spreadArea = modifierBottom.removeFromLeft((int) std::round(modifierBottom.getWidth() * 0.52f));
    spreadKnobPtr->setBounds(spreadArea.reduced(4, 0));
    modifierBottom.removeFromLeft(4);
    auto toggleColumn = modifierBottom.reduced(0, 10);
    vibratoToggle.setBounds(toggleColumn.removeFromTop(34));
    toggleColumn.removeFromTop(10);
    focusCutToggle.setBounds(toggleColumn.removeFromTop(34));

    auto globalArea = globalSectionBounds.reduced(16, 14);
    globalArea.removeFromTop(32);
    auto globalHeroArea = globalArea.removeFromRight(160);
    globalHeroArea.removeFromLeft(6);
    dryWetKnobPtr->setBounds(globalHeroArea.reduced(4, 0));

    auto globalTop = globalArea.removeFromTop(126);
    const int globalTopWidth = (globalTop.getWidth() - 8) / 3;
    delayKnobPtr->setBounds(globalTop.removeFromLeft(globalTopWidth).reduced(4, 0));
    globalTop.removeFromLeft(4);
    feedbackKnobPtr->setBounds(globalTop.removeFromLeft(globalTopWidth).reduced(4, 0));
    globalTop.removeFromLeft(4);
    toneKnobPtr->setBounds(globalTop.reduced(4, 0));
    globalArea.removeFromTop(8);
    auto globalBottom = globalArea.removeFromTop(126);
    const int globalBottomWidth = (globalBottom.getWidth() - 4) / 2;
    widthKnobPtr->setBounds(globalBottom.removeFromLeft(globalBottomWidth).reduced(4, 0));
    globalBottom.removeFromLeft(4);
    driveKnobPtr->setBounds(globalBottom.reduced(4, 0));

    auto voiceArea = voiceSectionBounds.reduced(16, 14);
    voiceArea.removeFromTop(34);
    voiceGraphPtr->setBounds(voiceArea.removeFromTop(118));
    voiceArea.removeFromTop(8);
    auto crossoverArea = voiceArea.removeFromTop(114);
    auto centeredKnob = juce::Rectangle<int>(148, crossoverArea.getHeight()).withCentre(crossoverArea.getCentre());
    voiceCrossoverKnobPtr->setBounds(centeredKnob);
    voiceArea.removeFromTop(8);

    auto voiceBands = voiceArea.removeFromTop(134);
    bodyVoiceBounds = voiceBands.removeFromLeft(voiceBands.getWidth() / 2);
    voiceBands.removeFromLeft(10);
    airVoiceBounds = voiceBands;

    auto lowArea = bodyVoiceBounds.reduced(12, 8);
    lowArea.removeFromTop(18);
    const int lowWidth = (lowArea.getWidth() - 8) / 3;
    lowVoiceAmountKnobPtr->setBounds(lowArea.removeFromLeft(lowWidth).reduced(4, 0));
    lowArea.removeFromLeft(4);
    lowVoiceScaleKnobPtr->setBounds(lowArea.removeFromLeft(lowWidth).reduced(4, 0));
    lowArea.removeFromLeft(4);
    lowVoiceRateKnobPtr->setBounds(lowArea.reduced(4, 0));

    auto highArea = airVoiceBounds.reduced(12, 8);
    highArea.removeFromTop(18);
    const int highWidth = (highArea.getWidth() - 8) / 3;
    highVoiceAmountKnobPtr->setBounds(highArea.removeFromLeft(highWidth).reduced(4, 0));
    highArea.removeFromLeft(4);
    highVoiceScaleKnobPtr->setBounds(highArea.removeFromLeft(highWidth).reduced(4, 0));
    highArea.removeFromLeft(4);
    highVoiceRateKnobPtr->setBounds(highArea.reduced(4, 0));
}
