#include "ReverbControlsComponent.h"

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
                     const juce::String& subtitle,
                     juce::Colour accent)
{
    auto titleArea = bounds.reduced(18, 10).removeFromTop(24);
    g.setColour(reverbui::textStrong());
    g.setFont(reverbui::titleFont(15.0f));
    g.drawText(title, titleArea, juce::Justification::centredLeft, false);

    if (subtitle.isNotEmpty())
    {
        auto subtitleArea = titleArea.withY(titleArea.getBottom() - 1).withHeight(16);
        g.setColour(reverbui::textMuted());
        g.setFont(reverbui::bodyFont(9.3f));
        g.drawText(subtitle, subtitleArea.translated(0, 14), juce::Justification::centredLeft, false);
    }

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

struct ReverbTypePreset
{
    const char* name;
    float loCutHz;
    float hiCutHz;
    float earlyAmount;
    float earlyRate;
    float earlyShape;
    bool earlySpin;
    float diffCrossoverHz;
    float lowDiffAmount;
    float lowDiffScale;
    float lowDiffRate;
    float highDiffAmount;
    float highDiffScale;
    float highDiffRate;
    float size;
    float decaySeconds;
    float predelayMs;
    float stereoWidth;
    float density;
    float dryWet;
    float chorusAmount;
    float chorusRate;
    bool freeze;
    bool flatCut;
    float reflect;
};

const std::array<ReverbTypePreset, 6>& getReverbTypePresets()
{
    static const std::array<ReverbTypePreset, 6> presets {{
        { "Room", 180.0f, 11000.0f, 46.0f, 0.80f, 38.0f, false, 950.0f, 48.0f, 82.0f, 0.45f, 58.0f, 70.0f, 1.10f, 72.0f, 1.40f, 12.0f, 98.0f, 58.0f, 25.0f, 6.0f, 0.55f, false, false, 40.0f },
        { "Hall", 120.0f, 15000.0f, 32.0f, 0.65f, 58.0f, true, 1400.0f, 60.0f, 112.0f, 0.55f, 76.0f, 92.0f, 1.35f, 130.0f, 3.80f, 26.0f, 125.0f, 74.0f, 30.0f, 11.0f, 0.72f, false, false, 48.0f },
        { "Plate", 220.0f, 13500.0f, 22.0f, 0.95f, 42.0f, false, 2100.0f, 38.0f, 74.0f, 0.70f, 82.0f, 68.0f, 1.90f, 92.0f, 2.30f, 18.0f, 118.0f, 82.0f, 28.0f, 8.0f, 0.95f, false, true, 32.0f },
        { "Chamber", 160.0f, 12500.0f, 36.0f, 0.75f, 52.0f, true, 1250.0f, 54.0f, 96.0f, 0.50f, 64.0f, 78.0f, 1.20f, 96.0f, 2.10f, 16.0f, 112.0f, 68.0f, 27.0f, 7.0f, 0.68f, false, false, 44.0f },
        { "Ambience", 240.0f, 16000.0f, 52.0f, 1.40f, 30.0f, false, 1800.0f, 34.0f, 64.0f, 0.85f, 50.0f, 58.0f, 1.80f, 54.0f, 0.80f, 5.0f, 104.0f, 52.0f, 18.0f, 4.0f, 1.10f, false, false, 28.0f },
        { "Bloom", 90.0f, 17000.0f, 24.0f, 0.55f, 64.0f, true, 900.0f, 68.0f, 124.0f, 0.40f, 86.0f, 104.0f, 1.25f, 165.0f, 6.50f, 36.0f, 148.0f, 78.0f, 34.0f, 17.0f, 0.62f, false, false, 56.0f }
    }};

    return presets;
}
} // namespace

class ReverbControlsComponent::ParameterKnob final : public juce::Component
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

    reverbui::SSPKnob& getSlider() noexcept { return slider; }

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

class ReverbControlsComponent::InputFilterGraph final : public juce::Component
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

        g.setFont(reverbui::bodyFont(9.0f));
        g.setColour(reverbui::textMuted());
        g.drawText("20 Hz", plot.withTrimmedTop((int) plot.getHeight() - 14).removeFromLeft(48).toNearestInt(), juce::Justification::centredLeft, false);
        g.drawText("20 kHz", plot.withTrimmedTop((int) plot.getHeight() - 14).removeFromRight(56).toNearestInt(), juce::Justification::centredRight, false);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class ReverbControlsComponent::DiffusionGraph final : public juce::Component
{
public:
    explicit DiffusionGraph(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        drawGraphFrame(g, area);
        auto plot = area.reduced(12.0f, 10.0f);
        drawFrequencyGrid(g, plot);

        const float crossover = getParameterValue(apvts, "diffCrossoverHz");
        const float lowAmount = getParameterValue(apvts, "lowDiffAmount") / 100.0f;
        const float lowScale = getParameterValue(apvts, "lowDiffScale") / 200.0f;
        const float lowRate = getParameterValue(apvts, "lowDiffRate") / 4.0f;
        const float highAmount = getParameterValue(apvts, "highDiffAmount") / 100.0f;
        const float highScale = getParameterValue(apvts, "highDiffScale") / 200.0f;
        const float highRate = getParameterValue(apvts, "highDiffRate") / 8.0f;

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
            const float lowShape = (0.20f + lowAmount * 0.52f + lowScale * 0.16f + lowRate * 0.10f)
                                   * (0.78f + 0.22f * std::cos(norm * juce::MathConstants<float>::pi * 0.85f));
            const float highShape = (0.20f + highAmount * 0.54f + highScale * 0.18f + highRate * 0.10f)
                                    * (0.80f + 0.20f * std::sin((0.2f + norm) * juce::MathConstants<float>::pi * 0.95f));

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
        auto leftLabel = plot.withWidth((int) (plot.getWidth() * 0.24f));
        auto rightLabel = juce::Rectangle<float>(plot.getWidth() * 0.28f, plot.getHeight()).withRightX(plot.getRight()).withY(plot.getY());
        g.drawFittedText("Low Band", leftLabel.toNearestInt(), juce::Justification::topLeft, 1);
        g.drawFittedText("High Band", rightLabel.toNearestInt(), juce::Justification::topRight, 1);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

ReverbControlsComponent::ReverbControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    juce::ignoreUnused(processor);

    inputFilterGraph = std::make_unique<InputFilterGraph>(state);
    diffusionGraph = std::make_unique<DiffusionGraph>(state);
    inputFilterGraphPtr = inputFilterGraph.get();
    diffusionGraphPtr = diffusionGraph.get();

    loCutKnob = std::make_unique<ParameterKnob>(state, "loCutHz", "Lo Cut", reverbui::brandCyan());
    hiCutKnob = std::make_unique<ParameterKnob>(state, "hiCutHz", "Hi Cut", reverbui::brandMint());
    earlyAmountKnob = std::make_unique<ParameterKnob>(state, "earlyAmount", "Amount", reverbui::brandMint());
    earlyRateKnob = std::make_unique<ParameterKnob>(state, "earlyRate", "Rate", reverbui::brandCyan());
    earlyShapeKnob = std::make_unique<ParameterKnob>(state, "earlyShape", "Shape", reverbui::brandGold());
    diffCrossoverKnob = std::make_unique<ParameterKnob>(state, "diffCrossoverHz", "Crossover", reverbui::brandCyan());
    lowDiffAmountKnob = std::make_unique<ParameterKnob>(state, "lowDiffAmount", "Amount", reverbui::brandMint());
    lowDiffScaleKnob = std::make_unique<ParameterKnob>(state, "lowDiffScale", "Scale", reverbui::brandCyan());
    lowDiffRateKnob = std::make_unique<ParameterKnob>(state, "lowDiffRate", "Rate", reverbui::brandGold());
    highDiffAmountKnob = std::make_unique<ParameterKnob>(state, "highDiffAmount", "Amount", reverbui::brandGold());
    highDiffScaleKnob = std::make_unique<ParameterKnob>(state, "highDiffScale", "Scale", reverbui::brandCyan());
    highDiffRateKnob = std::make_unique<ParameterKnob>(state, "highDiffRate", "Rate", reverbui::brandMint());
    sizeKnob = std::make_unique<ParameterKnob>(state, "size", "Size", reverbui::brandMint());
    decayKnob = std::make_unique<ParameterKnob>(state, "decaySeconds", "Decay", reverbui::brandGold());
    predelayKnob = std::make_unique<ParameterKnob>(state, "predelayMs", "Predelay", reverbui::brandCyan());
    widthKnob = std::make_unique<ParameterKnob>(state, "stereoWidth", "Stereo Width", reverbui::brandCyan());
    densityKnob = std::make_unique<ParameterKnob>(state, "density", "Density", reverbui::brandMint());
    dryWetKnob = std::make_unique<ParameterKnob>(state, "dryWet", "Dry / Wet", reverbui::brandGold(), true);
    chorusAmountKnob = std::make_unique<ParameterKnob>(state, "chorusAmount", "Chorus Amt", reverbui::brandCyan());
    chorusRateKnob = std::make_unique<ParameterKnob>(state, "chorusRate", "Chorus Rate", reverbui::brandMint());
    reflectKnob = std::make_unique<ParameterKnob>(state, "reflect", "Reflect", reverbui::brandGold());

    loCutKnobPtr = loCutKnob.get();
    hiCutKnobPtr = hiCutKnob.get();
    earlyAmountKnobPtr = earlyAmountKnob.get();
    earlyRateKnobPtr = earlyRateKnob.get();
    earlyShapeKnobPtr = earlyShapeKnob.get();
    diffCrossoverKnobPtr = diffCrossoverKnob.get();
    lowDiffAmountKnobPtr = lowDiffAmountKnob.get();
    lowDiffScaleKnobPtr = lowDiffScaleKnob.get();
    lowDiffRateKnobPtr = lowDiffRateKnob.get();
    highDiffAmountKnobPtr = highDiffAmountKnob.get();
    highDiffScaleKnobPtr = highDiffScaleKnob.get();
    highDiffRateKnobPtr = highDiffRateKnob.get();
    sizeKnobPtr = sizeKnob.get();
    decayKnobPtr = decayKnob.get();
    predelayKnobPtr = predelayKnob.get();
    widthKnobPtr = widthKnob.get();
    densityKnobPtr = densityKnob.get();
    dryWetKnobPtr = dryWetKnob.get();
    chorusAmountKnobPtr = chorusAmountKnob.get();
    chorusRateKnobPtr = chorusRateKnob.get();
    reflectKnobPtr = reflectKnob.get();

    const std::array<juce::Component*, 23> components {
        inputFilterGraphPtr,
        diffusionGraphPtr,
        loCutKnobPtr,
        hiCutKnobPtr,
        earlyAmountKnobPtr,
        earlyRateKnobPtr,
        earlyShapeKnobPtr,
        diffCrossoverKnobPtr,
        lowDiffAmountKnobPtr,
        lowDiffScaleKnobPtr,
        lowDiffRateKnobPtr,
        highDiffAmountKnobPtr,
        highDiffScaleKnobPtr,
        highDiffRateKnobPtr,
        sizeKnobPtr,
        decayKnobPtr,
        predelayKnobPtr,
        widthKnobPtr,
        densityKnobPtr,
        dryWetKnobPtr,
        chorusAmountKnobPtr,
        chorusRateKnobPtr,
        reflectKnobPtr
    };

    for (auto* component : components)
    {
        addAndMakeVisible(component);
    }

    addAndMakeVisible(earlySpinToggle);
    addAndMakeVisible(freezeToggle);
    addAndMakeVisible(flatCutToggle);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeHintLabel);
    addAndMakeVisible(typeDropdown);

    typeLabel.setText("Reverb Type", juce::dontSendNotification);
    typeLabel.setFont(reverbui::smallCapsFont(11.0f));
    typeLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    typeLabel.setJustificationType(juce::Justification::centredLeft);

    typeHintLabel.setVisible(false);

    typeDropdown.setTextWhenNothingSelected("Select reverb type");
    typeDropdown.setTooltip("Apply a classic reverb-space starting point.");
    for (const auto& preset : getReverbTypePresets())
        typeDropdown.addItem(preset.name, typeDropdown.getNumItems() + 1);

    typeDropdown.onChange = [this]
    {
        if (ignoreTypeSelection)
            return;

        const int index = typeDropdown.getSelectedItemIndex();
        if (index >= 0)
            applyTypePreset(index);
    };

    earlySpinAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "earlySpin", earlySpinToggle);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "freeze", freezeToggle);
    flatCutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "flatCut", flatCutToggle);

    startTimerHz(30);
}

ReverbControlsComponent::~ReverbControlsComponent()
{
    stopTimer();
}

void ReverbControlsComponent::applyTypePreset(int presetIndex)
{
    const auto& preset = getReverbTypePresets()[(size_t) juce::jlimit(0, (int) getReverbTypePresets().size() - 1, presetIndex)];

    setParameterValue(apvts, "loCutHz", preset.loCutHz);
    setParameterValue(apvts, "hiCutHz", preset.hiCutHz);
    setParameterValue(apvts, "earlyAmount", preset.earlyAmount);
    setParameterValue(apvts, "earlyRate", preset.earlyRate);
    setParameterValue(apvts, "earlyShape", preset.earlyShape);
    setParameterValue(apvts, "earlySpin", preset.earlySpin ? 1.0f : 0.0f);
    setParameterValue(apvts, "diffCrossoverHz", preset.diffCrossoverHz);
    setParameterValue(apvts, "lowDiffAmount", preset.lowDiffAmount);
    setParameterValue(apvts, "lowDiffScale", preset.lowDiffScale);
    setParameterValue(apvts, "lowDiffRate", preset.lowDiffRate);
    setParameterValue(apvts, "highDiffAmount", preset.highDiffAmount);
    setParameterValue(apvts, "highDiffScale", preset.highDiffScale);
    setParameterValue(apvts, "highDiffRate", preset.highDiffRate);
    setParameterValue(apvts, "size", preset.size);
    setParameterValue(apvts, "decaySeconds", preset.decaySeconds);
    setParameterValue(apvts, "predelayMs", preset.predelayMs);
    setParameterValue(apvts, "stereoWidth", preset.stereoWidth);
    setParameterValue(apvts, "density", preset.density);
    setParameterValue(apvts, "dryWet", preset.dryWet);
    setParameterValue(apvts, "chorusAmount", preset.chorusAmount);
    setParameterValue(apvts, "chorusRate", preset.chorusRate);
    setParameterValue(apvts, "freeze", preset.freeze ? 1.0f : 0.0f);
    setParameterValue(apvts, "flatCut", preset.flatCut ? 1.0f : 0.0f);
    setParameterValue(apvts, "reflect", preset.reflect);
}

void ReverbControlsComponent::timerCallback()
{
    if (inputFilterGraphPtr != nullptr)
        inputFilterGraphPtr->repaint();

    if (diffusionGraphPtr != nullptr)
        diffusionGraphPtr->repaint();
}

void ReverbControlsComponent::paint(juce::Graphics& g)
{
    reverbui::drawPanelBackground(g, typeSelectorBounds.toFloat(), reverbui::brandMint(), 16.0f);
    reverbui::drawPanelBackground(g, inputSectionBounds.toFloat(), reverbui::brandCyan(), 16.0f);
    reverbui::drawPanelBackground(g, earlySectionBounds.toFloat(), reverbui::brandMint(), 16.0f);
    reverbui::drawPanelBackground(g, modifiersSectionBounds.toFloat(), reverbui::brandGold(), 16.0f);
    reverbui::drawPanelBackground(g, globalSectionBounds.toFloat(), reverbui::brandCyan(), 16.0f);
    reverbui::drawPanelBackground(g, diffusionSectionBounds.toFloat(), reverbui::brandGold(), 16.0f);

    drawSectionText(g, inputSectionBounds, "Input Filter", {}, reverbui::brandCyan());
    drawSectionText(g, earlySectionBounds, "Early Reflections", {}, reverbui::brandMint());
    drawSectionText(g, modifiersSectionBounds, "Modifiers", {}, reverbui::brandGold());
    drawSectionText(g, globalSectionBounds, "Global Controls", {}, reverbui::brandCyan());
    drawSectionText(g, diffusionSectionBounds, "Diffusion Network", {}, reverbui::brandGold());

    drawSubPanel(g, diffusionLowBandBounds, "Low Band", reverbui::brandMint());
    drawSubPanel(g, diffusionHighBandBounds, "High Band", reverbui::brandGold());

}

void ReverbControlsComponent::resized()
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
    earlySectionBounds = topRow.removeFromLeft(286);
    topRow.removeFromLeft(gap);
    modifiersSectionBounds = topRow.removeFromLeft(228);
    topRow.removeFromLeft(gap);
    globalSectionBounds = topRow;

    area.removeFromTop(gap);
    diffusionSectionBounds = area;

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

    auto earlyArea = earlySectionBounds.reduced(16, 14);
    earlyArea.removeFromTop(32);
    auto earlyKnobs = earlyArea.removeFromTop(146);
    const int earlyKnobWidth = earlyKnobs.getWidth() / 3;
    earlyAmountKnobPtr->setBounds(earlyKnobs.removeFromLeft(earlyKnobWidth).reduced(4, 0));
    earlyRateKnobPtr->setBounds(earlyKnobs.removeFromLeft(earlyKnobWidth).reduced(4, 0));
    earlyShapeKnobPtr->setBounds(earlyKnobs.reduced(4, 0));
    earlyArea.removeFromTop(8);
    earlySpinToggle.setBounds(earlyArea.removeFromTop(34));

    auto modifiersArea = modifiersSectionBounds.reduced(16, 14);
    modifiersArea.removeFromTop(32);
    auto modifierTop = modifiersArea.removeFromTop(144);
    auto modifierTopLeft = modifierTop.removeFromLeft(modifierTop.getWidth() / 2);
    chorusAmountKnobPtr->setBounds(modifierTopLeft.reduced(4, 0));
    chorusRateKnobPtr->setBounds(modifierTop.reduced(4, 0));
    modifiersArea.removeFromTop(4);
    auto modifierBottom = modifiersArea.removeFromTop(126);
    auto reflectArea = modifierBottom.removeFromLeft((int) std::round(modifierBottom.getWidth() * 0.52f));
    reflectKnobPtr->setBounds(reflectArea.reduced(4, 0));
    modifierBottom.removeFromLeft(4);
    auto toggleColumn = modifierBottom.reduced(0, 10);
    freezeToggle.setBounds(toggleColumn.removeFromTop(34));
    toggleColumn.removeFromTop(10);
    flatCutToggle.setBounds(toggleColumn.removeFromTop(34));

    auto globalArea = globalSectionBounds.reduced(16, 14);
    globalArea.removeFromTop(32);
    auto globalHeroArea = globalArea.removeFromRight(160);
    globalHeroArea.removeFromLeft(6);
    dryWetKnobPtr->setBounds(globalHeroArea.reduced(4, 0));

    auto globalTop = globalArea.removeFromTop(126);
    const int globalTopWidth = (globalTop.getWidth() - 8) / 3;
    sizeKnobPtr->setBounds(globalTop.removeFromLeft(globalTopWidth).reduced(4, 0));
    globalTop.removeFromLeft(4);
    decayKnobPtr->setBounds(globalTop.removeFromLeft(globalTopWidth).reduced(4, 0));
    globalTop.removeFromLeft(4);
    predelayKnobPtr->setBounds(globalTop.reduced(4, 0));
    globalArea.removeFromTop(8);
    auto globalBottom = globalArea.removeFromTop(126);
    const int globalBottomWidth = (globalBottom.getWidth() - 4) / 2;
    widthKnobPtr->setBounds(globalBottom.removeFromLeft(globalBottomWidth).reduced(4, 0));
    globalBottom.removeFromLeft(4);
    densityKnobPtr->setBounds(globalBottom.reduced(4, 0));

    auto diffusionArea = diffusionSectionBounds.reduced(16, 14);
    diffusionArea.removeFromTop(34);
    diffusionGraphPtr->setBounds(diffusionArea.removeFromTop(118));
    diffusionArea.removeFromTop(8);
    auto crossoverArea = diffusionArea.removeFromTop(114);
    auto centeredKnob = juce::Rectangle<int>(148, crossoverArea.getHeight()).withCentre(crossoverArea.getCentre());
    diffCrossoverKnobPtr->setBounds(centeredKnob);
    diffusionArea.removeFromTop(8);

    auto diffusionBands = diffusionArea.removeFromTop(134);
    diffusionLowBandBounds = diffusionBands.removeFromLeft(diffusionBands.getWidth() / 2);
    diffusionBands.removeFromLeft(10);
    diffusionHighBandBounds = diffusionBands;

    auto lowArea = diffusionLowBandBounds.reduced(12, 8);
    lowArea.removeFromTop(18);
    const int lowWidth = (lowArea.getWidth() - 8) / 3;
    lowDiffAmountKnobPtr->setBounds(lowArea.removeFromLeft(lowWidth).reduced(4, 0));
    lowArea.removeFromLeft(4);
    lowDiffScaleKnobPtr->setBounds(lowArea.removeFromLeft(lowWidth).reduced(4, 0));
    lowArea.removeFromLeft(4);
    lowDiffRateKnobPtr->setBounds(lowArea.reduced(4, 0));

    auto highArea = diffusionHighBandBounds.reduced(12, 8);
    highArea.removeFromTop(18);
    const int highWidth = (highArea.getWidth() - 8) / 3;
    highDiffAmountKnobPtr->setBounds(highArea.removeFromLeft(highWidth).reduced(4, 0));
    highArea.removeFromLeft(4);
    highDiffScaleKnobPtr->setBounds(highArea.removeFromLeft(highWidth).reduced(4, 0));
    highArea.removeFromLeft(4);
    highDiffRateKnobPtr->setBounds(highArea.reduced(4, 0));
}
