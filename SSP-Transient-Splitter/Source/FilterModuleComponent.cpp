#include "FilterModuleComponent.h"

#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotFloatParamID(int slotIndex, int parameterIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
}

juce::String getFXSlotVariantParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Variant";
}

void setChoiceParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1((float) value));
}

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(size >= 14.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(just);
}

void styleToggle(juce::ToggleButton& button, juce::Colour accent)
{
    button.setButtonText("On");
    button.setClickingTogglesState(true);
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, accent);
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff59606a));
}

void styleSlider(juce::Slider& slider, juce::Colour accent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 22);
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.5f));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, reactorui::outline());
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f151c));
    slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
}

void styleSelector(juce::ComboBox& combo, juce::Colour accent)
{
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11161c));
    combo.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    combo.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    combo.setColour(juce::ComboBox::arrowColourId, accent);
}

void drawDeck(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    juce::ColourGradient fill(juce::Colour(0xff12161c), bounds.getTopLeft(),
                              juce::Colour(0xff090d12), bounds.getBottomLeft(), false);
    fill.addColour(0.12, accent.withAlpha(0.10f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(juce::Colour(0xff050608));
    g.drawRoundedRectangle(bounds, 7.0f, 1.2f);
    g.setColour(reactorui::outlineSoft().withAlpha(0.95f));
    g.drawRoundedRectangle(bounds.reduced(1.2f), 6.0f, 1.0f);
}

float normalToFrequency(float value)
{
    constexpr float minFrequency = 24.0f;
    constexpr float maxFrequency = 18000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

struct FilterPresentation
{
    std::array<juce::String, 4> labels;
    juce::String description;
};

FilterPresentation getFilterPresentation(int typeIndex)
{
    switch (typeIndex)
    {
        case 7:
            return { { "Freq", "Q", "Gain", "Drive" },
                     "Focused bell filter for carving or pushing a precise band forward." };
        case 8:
        case 9:
            return { { "Freq", "Slope", "Gain", "Drive" },
                     typeIndex == 8 ? "Broad tonal shelf for warming lows or hollowing the bottom edge."
                                    : "Top-end shelf for lifting air or dulling the sheen." };
        case 10:
            return { { "Freq", "Q", "Spread", "Drive" },
                     "Phase-rotation filter that stays level while shifting motion and smear." };
        case 11:
        case 12:
            return { { "Tune", "Feedback", "Damp", "Drive" },
                     typeIndex == 11 ? "Positive comb resonator for string-like metallic overtones."
                                     : "Negative comb notch pattern for hollow animated cancellation." };
        case 13:
            return { { "Tone", "Focus", "Shift", "Drive" },
                     "Twin-formant filter for vocal-like bite without locking into a single vowel." };
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
            return { { "Tone", "Focus", "Shift", "Drive" },
                     "Vowel-shaped formant response that adds human colour and moving articulation." };
        case 19:
            return { { "Cutoff", "Res", "Drive", "Bias" },
                     "Driven ladder-style low pass with rounded saturation and thicker resonance." };
        default:
            return { { "Cutoff", "Res", "Drive", "Shape" },
                     "Tone-shaping filter with a visual response preview and broad sweet spots." };
    }
}

}

FilterModuleComponent::FilterModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xff7fe17b);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeSelector);

    titleLabel.setText("FILTER", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleLabel(typeLabel, 10.5f, reactorui::textMuted());
    typeLabel.setText("Type", juce::dontSendNotification);
    styleToggle(onButton, accent);
    styleSelector(typeSelector, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        addAndMakeVisible(controlLabels[i]);
        addAndMakeVisible(controls[i]);
        styleLabel(controlLabels[i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, (int) i);
        controls[i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        controlAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                        parameterID,
                                                                                                        controls[i]);
    }

    controls[0].textFromValueFunction = [] (double value)
    {
        const float frequency = normalToFrequency((float) value);
        return frequency >= 1000.0f ? juce::String(frequency / 1000.0f, 2) + " kHz"
                                    : juce::String(juce::roundToInt(frequency)) + " Hz";
    };
    controls[0].valueFromTextFunction = [] (const juce::String& text)
    {
        const bool isKhz = text.containsIgnoreCase("k");
        const double entered = text.upToFirstOccurrenceOf(" ", false, false).getDoubleValue();
        const float frequency = (float) (isKhz ? entered * 1000.0 : entered);
        constexpr float minFrequency = 24.0f;
        constexpr float maxFrequency = 18000.0f;
        const float clamped = juce::jlimit(minFrequency, maxFrequency, frequency);
        return std::log(clamped / minFrequency) / std::log(maxFrequency / minFrequency);
    };
    controlLabels[4].setText("Mix", juce::dontSendNotification);

    typeSelector.addItemList(PluginProcessor::getFXFilterTypeNames(), 1);
    typeSelector.onChange = [this]
    {
        setSelectedType(juce::jmax(0, typeSelector.getSelectedItemIndex()));
    };

    syncTypeSelector();
    updatePresentation();
    startTimerHz(24);
}

int FilterModuleComponent::getPreferredHeight() const
{
    return 486;
}

int FilterModuleComponent::getSelectedType() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jlimit(0, PluginProcessor::getFXFilterTypeNames().size() - 1, (int) std::round(raw->load()));

    return 0;
}

void FilterModuleComponent::setSelectedType(int typeIndex)
{
    setChoiceParameterValue(apvts,
                            getFXSlotVariantParamID(slotIndex),
                            juce::jlimit(0, PluginProcessor::getFXFilterTypeNames().size() - 1, typeIndex));
    syncTypeSelector();
    updatePresentation();
    repaint();
}

void FilterModuleComponent::syncTypeSelector()
{
    const int selectedType = getSelectedType();
    if (typeSelector.getSelectedItemIndex() != selectedType)
        typeSelector.setSelectedItemIndex(selectedType, juce::dontSendNotification);
}

void FilterModuleComponent::updatePresentation()
{
    const auto presentation = getFilterPresentation(getSelectedType());
    subtitleLabel.setText(presentation.description, juce::dontSendNotification);

    for (int i = 0; i < 4; ++i)
        controlLabels[(size_t) i].setText(presentation.labels[(size_t) i], juce::dontSendNotification);

    controlLabels[4].setText("Mix", juce::dontSendNotification);
}

void FilterModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xff7fe17b);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);

    reactorui::drawDisplayPanel(g, previewBounds, accent);
    const auto inner = previewBounds.reduced(10.0f, 8.0f);
    reactorui::drawSubtleGrid(g, inner, accent.withAlpha(0.12f), 5, 4);

    drawDeck(g, typeDeckBounds.toFloat(), accent);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.72f));
    }

    const bool filterEnabled = onButton.getToggleState();
    const float cutoffValue = juce::jlimit(20.0f, 20000.0f, normalToFrequency((float) controls[0].getValue()));
    const float cutoffLog = (float) juce::mapFromLog10(cutoffValue, 20.0f, 20000.0f);
    const float resonanceNorm = juce::jlimit(0.0f, 1.0f, (float) controls[1].getValue());
    const float amount = (float) controls[2].getValue();
    const float shape = (float) controls[3].getValue();
    const float mix = (float) controls[4].getValue();
    const int type = getSelectedType();

    juce::Path responsePath;
    juce::Path mixPath;
    for (int x = 0; x < (int) inner.getWidth(); ++x)
    {
        const float normX = (float) x / juce::jmax(1.0f, inner.getWidth() - 1.0f);
        const float frequency = (float) juce::mapToLog10(normX, 20.0f, 20000.0f);
        float magnitude = 0.55f;

        if (filterEnabled)
        {
            switch (type)
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    double response = 1.0;

                    switch (type)
                    {
                        case 0:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(visualSampleRate,
                                                                                                          juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                          juce::jlimit(0.55f, 6.5f, 0.55f + resonanceNorm * 5.95f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            break;
                        }

                        case 1:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(visualSampleRate,
                                                                                                          juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                          juce::jlimit(0.65f, 4.0f, 0.65f + resonanceNorm * 3.35f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            response *= response;
                            break;
                        }

                        case 2:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(visualSampleRate,
                                                                                                           juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                           juce::jlimit(0.55f, 6.5f, 0.55f + resonanceNorm * 5.95f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            break;
                        }

                        case 3:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(visualSampleRate,
                                                                                                           juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                           juce::jlimit(0.65f, 4.0f, 0.65f + resonanceNorm * 3.35f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            response *= response;
                            break;
                        }

                        case 4:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(visualSampleRate,
                                                                                                           juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                           juce::jlimit(0.85f, 12.0f, 0.85f + resonanceNorm * 11.15f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            break;
                        }

                        case 5:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(visualSampleRate,
                                                                                                           juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                           juce::jlimit(0.85f, 10.0f, 0.85f + resonanceNorm * 9.15f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            response *= response;
                            break;
                        }

                        case 6:
                        {
                            const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeNotch(visualSampleRate,
                                                                                                        juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                        juce::jlimit(0.75f, 10.0f, 0.75f + resonanceNorm * 9.25f));
                            response = coefficients->getMagnitudeForFrequency(frequency, visualSampleRate);
                            break;
                        }

                        default:
                            break;
                    }

                    const float responseDb = juce::Decibels::gainToDecibels((float) response, -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 1.02f);
                    break;
                }
                case 7:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const auto coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(visualSampleRate,
                                                                                                   juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                   juce::jlimit(0.35f, 8.0f, 0.7f + resonanceNorm * 6.0f),
                                                                                                   juce::Decibels::decibelsToGain(juce::jmap(amount, -18.0f, 18.0f)));
                    const float responseDb = juce::Decibels::gainToDecibels((float) coefficients->getMagnitudeForFrequency(frequency, visualSampleRate), -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 0.95f);
                    break;
                }
                case 8:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(visualSampleRate,
                                                                                                  juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                  juce::jlimit(0.5f, 1.5f, 0.65f + resonanceNorm * 0.8f),
                                                                                                  juce::Decibels::decibelsToGain(juce::jmap(amount, -18.0f, 18.0f)));
                    const float responseDb = juce::Decibels::gainToDecibels((float) coefficients->getMagnitudeForFrequency(frequency, visualSampleRate), -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 0.95f);
                    break;
                }
                case 9:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(visualSampleRate,
                                                                                                   juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                   juce::jlimit(0.5f, 1.5f, 0.65f + resonanceNorm * 0.8f),
                                                                                                   juce::Decibels::decibelsToGain(juce::jmap(amount, -18.0f, 18.0f)));
                    const float responseDb = juce::Decibels::gainToDecibels((float) coefficients->getMagnitudeForFrequency(frequency, visualSampleRate), -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 0.95f);
                    break;
                }
                case 10:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(visualSampleRate,
                                                                                                juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                                juce::jlimit(0.45f, 1.5f, 0.55f + resonanceNorm * 0.9f));
                    const float responseDb = juce::Decibels::gainToDecibels((float) coefficients->getMagnitudeForFrequency(frequency, visualSampleRate), -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.22f, 0.92f);
                    break;
                }
                case 11:
                case 12:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const float feedForward = 0.85f;
                    const float feedback = juce::jlimit(0.1f, 0.95f, 0.18f + resonanceNorm * 0.72f);
                    const float delaySamples = juce::jlimit(18.0f, 3000.0f, visualSampleRate / juce::jlimit(40.0f, 12000.0f, cutoffValue * 0.42f));
                    const float omega = juce::MathConstants<float>::twoPi * frequency / visualSampleRate;
                    const float phase = omega * delaySamples;
                    const float cosine = std::cos(phase);
                    const bool invert = type == 12;
                    const float numerator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedForward * feedForward + (invert ? -2.0f : 2.0f) * feedForward * cosine));
                    const float denominator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedback * feedback + (invert ? 2.0f : -2.0f) * feedback * cosine));
                    const float responseDb = juce::Decibels::gainToDecibels(numerator / denominator, -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 0.95f);
                    break;
                }
                case 13:
                {
                    const float c1 = juce::jlimit(0.12f, 0.55f, cutoffLog * 0.75f);
                    const float c2 = juce::jlimit(0.28f, 0.82f, c1 + 0.18f + shape * 0.06f);
                    const float d1 = (normX - c1) / 0.05f;
                    const float d2 = (normX - c2) / 0.06f;
                    magnitude = 0.16f + std::exp(-d1 * d1 * 1.8f) * 0.48f + std::exp(-d2 * d2 * 1.8f) * 0.42f;
                    break;
                }
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                {
                    float c1 = 0.42f;
                    float c2 = 0.58f;
                    switch (type)
                    {
                        case 14: c1 = 0.42f; c2 = 0.58f; break;
                        case 15: c1 = 0.24f; c2 = 0.70f; break;
                        case 16: c1 = 0.18f; c2 = 0.78f; break;
                        case 17: c1 = 0.30f; c2 = 0.48f; break;
                        case 18: c1 = 0.22f; c2 = 0.40f; break;
                        default: break;
                    }

                    const float sweep = (cutoffLog - 0.5f) * 0.22f + (amount - 0.5f) * 0.08f;
                    c1 = juce::jlimit(0.05f, 0.92f, c1 + sweep);
                    c2 = juce::jlimit(0.08f, 0.96f, c2 + sweep + shape * 0.04f);
                    const float d1 = (normX - c1) / 0.045f;
                    const float d2 = (normX - c2) / 0.060f;
                    magnitude = 0.16f + std::exp(-d1 * d1 * 1.9f) * 0.50f + std::exp(-d2 * d2 * 1.9f) * 0.44f;
                    break;
                }
                case 19:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const auto stage1 = juce::dsp::IIR::Coefficients<float>::makeLowPass(visualSampleRate,
                                                                                          juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue),
                                                                                          juce::jlimit(0.75f, 3.0f, 0.75f + resonanceNorm * 2.25f));
                    const auto stage2 = juce::dsp::IIR::Coefficients<float>::makeLowPass(visualSampleRate,
                                                                                          juce::jlimit(20.0f, visualSampleRate * 0.45f, cutoffValue * (0.78f + shape * 0.34f)),
                                                                                          juce::jlimit(0.70f, 1.8f, 0.72f + resonanceNorm * 1.08f));
                    const float response = (float) (stage1->getMagnitudeForFrequency(frequency, visualSampleRate)
                                                  * stage2->getMagnitudeForFrequency(frequency, visualSampleRate));
                    const float responseDb = juce::Decibels::gainToDecibels(juce::jmax(0.001f, response * (1.0f + amount * 0.45f)), -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 1.02f);
                    break;
                }
                default:
                    break;
            }

            // Approximate the post-filter saturation stage so the Drive control visibly changes the preview.
            const float driveVisual = amount;
            if (driveVisual > 0.0001f)
            {
                const float cutoffFocus = std::exp(-std::pow((normX - cutoffLog) / 0.12f, 2.0f));
                const float lowBias = 1.0f - normX;
                const float highBias = normX;

                switch (type)
                {
                    case 0:
                    case 1:
                    case 19:
                        magnitude += driveVisual * (0.08f + cutoffFocus * 0.20f + lowBias * 0.06f);
                        break;

                    case 2:
                    case 3:
                        magnitude += driveVisual * (0.05f + cutoffFocus * 0.14f + highBias * 0.08f);
                        break;

                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                    case 17:
                    case 18:
                        magnitude += driveVisual * (0.06f + cutoffFocus * 0.24f);
                        break;

                    case 8:
                        magnitude += driveVisual * (0.04f + lowBias * 0.12f + cutoffFocus * 0.08f);
                        break;

                    case 9:
                        magnitude += driveVisual * (0.04f + highBias * 0.12f + cutoffFocus * 0.08f);
                        break;

                    case 10:
                        magnitude += driveVisual * (0.03f + cutoffFocus * 0.10f);
                        break;

                    default:
                        magnitude += driveVisual * 0.06f;
                        break;
                }
            }

            magnitude = juce::jlimit(0.05f, 1.18f, magnitude + shape * 0.08f);
        }

        const float mixedMagnitude = juce::jmap(mix, 0.55f, magnitude);
        const float usableHeight = inner.getHeight() - 18.0f;
        const float y = inner.getBottom() - magnitude * usableHeight - 8.0f;
        const float mixedY = inner.getBottom() - mixedMagnitude * usableHeight - 8.0f;
        const float drawX = inner.getX() + (float) x;

        if (x == 0)
        {
            responsePath.startNewSubPath(drawX, y);
            mixPath.startNewSubPath(drawX, mixedY);
        }
        else
        {
            responsePath.lineTo(drawX, y);
            mixPath.lineTo(drawX, mixedY);
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.strokePath(mixPath, juce::PathStrokeType(4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(0.22f));
    g.strokePath(responsePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.strokePath(mixPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(responsePath, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void FilterModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(160));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    previewBounds = area.removeFromTop(114).toFloat();
    area.removeFromTop(10);

    typeDeckBounds = area.removeFromTop(62);
    auto selectorArea = typeDeckBounds.reduced(12, 10);
    typeLabel.setBounds(selectorArea.removeFromTop(14));
    selectorArea.removeFromTop(8);
    typeSelector.setBounds(selectorArea.removeFromLeft(300).withHeight(28));

    area.removeFromTop(10);
    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(6, 4);
    const int rowGap = 12;
    const int columnGap = 14;
    auto topRow = controlArea.removeFromTop(juce::jmax(96, (int) std::round(controlArea.getHeight() * 0.36f)));
    controlArea.removeFromTop(rowGap);
    auto bottomRow = controlArea;

    const int topWidth = (topRow.getWidth() - columnGap * 2) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = topRow.removeFromLeft(topWidth);
        if (i < 2)
            topRow.removeFromLeft(columnGap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(0, 2);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }

    const int bottomWidth = (bottomRow.getWidth() - columnGap) / 2;
    for (int i = 3; i < 5; ++i)
    {
        auto cell = bottomRow.removeFromLeft(bottomWidth);
        if (i == 3)
            bottomRow.removeFromLeft(columnGap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(0, 0);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }
}

void FilterModuleComponent::timerCallback()
{
    syncTypeSelector();
    updatePresentation();

    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    typeSelector.setEnabled(enabled);

    repaint();
}
