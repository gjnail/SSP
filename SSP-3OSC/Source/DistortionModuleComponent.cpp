#include "DistortionModuleComponent.h"

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
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.5f));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, reactorui::outline());
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f151c));
    slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
}

void styleModeSelector(juce::ComboBox& combo, juce::Colour accent)
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

float shapeDistortionSample(float input, float drive, int variant)
{
    const float shapedInput = input * (1.0f + drive * 10.0f);

    switch (variant)
    {
        case 1:
            return juce::jlimit(-1.0f, 1.0f, shapedInput * 0.72f);

        case 2:
        {
            const float asymmetric = shapedInput + 0.32f * shapedInput * shapedInput;
            return std::tanh(asymmetric) * 0.92f;
        }

        case 3:
        {
            const float threshold = juce::jmax(0.10f, 0.62f - drive * 0.24f);
            float folded = shapedInput * 0.65f;
            if (std::abs(folded) > threshold)
            {
                const float range = threshold * 4.0f;
                folded = std::abs(std::fmod(folded - threshold, range));
                folded = std::abs(folded - threshold * 2.0f) - threshold;
            }

            return juce::jlimit(-1.0f, 1.0f, folded / juce::jmax(0.05f, threshold));
        }

        case 4:
            return std::tanh(shapedInput * 0.70f + std::sin(shapedInput * (1.8f + drive * 2.4f)) * 0.45f);

        case 5:
        {
            const float diode = std::tanh(juce::jmax(0.0f, shapedInput) * (1.4f + drive * 5.0f))
                              - std::tanh(juce::jmax(0.0f, -shapedInput) * (0.5f + drive * 1.8f));
            return juce::jlimit(-1.0f, 1.0f, diode);
        }

        case 6:
            return std::sin(juce::jlimit(-juce::MathConstants<float>::pi,
                                         juce::MathConstants<float>::pi,
                                         shapedInput * (0.9f + drive * 1.8f)));

        case 7:
        {
            const float clamped = juce::jlimit(-1.0f, 1.0f, shapedInput * (0.42f + drive * 0.55f));
            const float crushed = std::round(clamped * (10.0f + drive * 30.0f)) / (10.0f + drive * 30.0f);
            return std::tanh(crushed * (1.8f + drive * 2.4f));
        }

        default:
            return std::tanh(shapedInput);
    }
}

juce::String getModeDescription(int variant)
{
    switch (variant)
    {
        case 1: return "Sharper clamp for immediate bite and harder transient edges.";
        case 2: return "Asymmetric tube-style push with warmer upper harmonics.";
        case 3: return "Folded shape for metallic edges and obvious synthetic grit.";
        case 4: return "Aggressive fuzz with animated buzz on the top end.";
        case 5: return "Asymmetric diode bite with a tighter positive-edge snap.";
        case 6: return "Rounded sine shaper that stays liquid while pushing harmonics forward.";
        case 7: return "Crushed speaker-style breakup with stepped edges and grit.";
        default: return "Rounded clip that stays easy to dial in for broad sweet spots.";
    }
}
}

DistortionModuleComponent::DistortionModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffff9e45);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeSelector);

    titleLabel.setText("DISTORTION", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleLabel(modeLabel, 10.5f, reactorui::textMuted());
    modeLabel.setText("Mode", juce::dontSendNotification);
    styleToggle(onButton, accent);
    styleModeSelector(modeSelector, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    const std::array<juce::String, 3> controlNames{ "Drive", "Tone", "Mix" };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        addAndMakeVisible(controlLabels[i]);
        addAndMakeVisible(controls[i]);
        controlLabels[i].setText(controlNames[i], juce::dontSendNotification);
        styleLabel(controlLabels[i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, (int) i);
        controls[i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        controlAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                        parameterID,
                                                                                                        controls[i]);
    }

    modeSelector.addItemList(PluginProcessor::getFXDriveTypeNames(), 1);
    modeSelector.onChange = [this]
    {
        setSelectedMode(juce::jmax(0, modeSelector.getSelectedItemIndex()));
    };

    syncModeSelector();
    updateModeDescription();
    startTimerHz(24);
}

int DistortionModuleComponent::getPreferredHeight() const
{
    return 370;
}

int DistortionModuleComponent::getSelectedMode() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jlimit(0, PluginProcessor::getFXDriveTypeNames().size() - 1, (int) std::round(raw->load()));

    return 0;
}

void DistortionModuleComponent::setSelectedMode(int modeIndex)
{
    setChoiceParameterValue(apvts,
                            getFXSlotVariantParamID(slotIndex),
                            juce::jlimit(0, PluginProcessor::getFXDriveTypeNames().size() - 1, modeIndex));
    syncModeSelector();
    updateModeDescription();
    repaint();
}

void DistortionModuleComponent::syncModeSelector()
{
    const int selectedMode = getSelectedMode();
    if (modeSelector.getSelectedItemIndex() != selectedMode)
        modeSelector.setSelectedItemIndex(selectedMode, juce::dontSendNotification);
}

void DistortionModuleComponent::updateModeDescription()
{
    subtitleLabel.setText(getModeDescription(getSelectedMode()), juce::dontSendNotification);
}

void DistortionModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffff9e45);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);

    reactorui::drawDisplayPanel(g, previewBounds, accent);
    const auto inner = previewBounds.reduced(10.0f, 8.0f);
    reactorui::drawSubtleGrid(g, inner, accent.withAlpha(0.10f), 4, 2);

    drawDeck(g, modeDeckBounds.toFloat(), accent);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (int i = 0; i < (int) knobCellBounds.size() - 1; ++i)
    {
        const auto& cell = knobCellBounds[(size_t) i];
        if (cell.isEmpty())
            continue;

        const int separatorX = cell.getRight() + 6;
        g.setColour(reactorui::outlineSoft().withAlpha(0.9f));
        g.drawVerticalLine(separatorX,
                           (float) controlDeckBounds.getY() + 12.0f,
                           (float) controlDeckBounds.getBottom() - 12.0f);
    }

    const float drive = (float) controls[0].getValue();
    const float tone = (float) controls[1].getValue();
    const float mix = (float) controls[2].getValue();
    const int mode = getSelectedMode();

    juce::Path dryPath;
    juce::Path wetPath;
    juce::Path mixPath;
    for (int x = 0; x < (int) inner.getWidth(); ++x)
    {
        const float nx = juce::jmap((float) x, 0.0f, inner.getWidth() - 1.0f, -1.0f, 1.0f);
        const float drySample = nx;
        const float wetSample = shapeDistortionSample(nx, drive, mode);
        const float brightenedWet = juce::jmap(tone,
                                               wetSample * 0.68f,
                                               juce::jlimit(-1.0f, 1.0f, wetSample + (wetSample - std::tanh(wetSample * 0.35f)) * 0.28f));
        const float mixedSample = juce::jmap(mix, drySample, brightenedWet);

        const float dryY = inner.getCentreY() - drySample * inner.getHeight() * 0.34f;
        const float wetY = inner.getCentreY() - brightenedWet * inner.getHeight() * 0.34f;
        const float mixedY = inner.getCentreY() - mixedSample * inner.getHeight() * 0.34f;
        const float drawX = inner.getX() + (float) x;

        if (x == 0)
        {
            dryPath.startNewSubPath(drawX, dryY);
            wetPath.startNewSubPath(drawX, wetY);
            mixPath.startNewSubPath(drawX, mixedY);
        }
        else
        {
            dryPath.lineTo(drawX, dryY);
            wetPath.lineTo(drawX, wetY);
            mixPath.lineTo(drawX, mixedY);
        }
    }

    g.setColour(reactorui::textMuted().withAlpha(0.45f));
    g.strokePath(dryPath, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.strokePath(mixPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(0.18f));
    g.strokePath(wetPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.strokePath(mixPath, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(0.98f));
    g.strokePath(wetPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DistortionModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(180));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);

    previewBounds = area.removeFromTop(84).toFloat();
    area.removeFromTop(10);

    modeDeckBounds = area.removeFromTop(62);
    auto modeArea = modeDeckBounds.reduced(12, 10);
    modeLabel.setBounds(modeArea.removeFromTop(14));
    modeArea.removeFromTop(8);
    modeSelector.setBounds(modeArea.removeFromLeft(300).withHeight(28));

    area.removeFromTop(10);
    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(14, 10);
    const int knobGap = 12;
    const int knobWidth = (controlArea.getWidth() - knobGap * 2) / 3;

    for (size_t i = 0; i < controls.size(); ++i)
    {
        auto cell = controlArea.removeFromLeft(knobWidth);
        if (i < controls.size() - 1)
            controlArea.removeFromLeft(knobGap);

        knobCellBounds[i] = cell;
        auto content = cell.reduced(2, 6);
        controlLabels[i].setBounds(content.removeFromTop(16));
        controls[i].setBounds(content);
    }
}

void DistortionModuleComponent::timerCallback()
{
    syncModeSelector();
    updateModeDescription();

    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    modeSelector.setEnabled(enabled);

    repaint();
}
