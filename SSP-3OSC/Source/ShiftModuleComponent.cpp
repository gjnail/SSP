#include "ShiftModuleComponent.h"

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
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.45f));
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

float normalToDetuneCents(float value)
{
    return 2.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.2f) * 36.0f;
}

float normalToShiftDelayMs(float value)
{
    return 2.5f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.15f) * 28.0f;
}

float normalToShiftPreDelayMs(float value)
{
    return std::pow(juce::jlimit(0.0f, 1.0f, value), 1.18f) * 120.0f;
}

float normalToShiftRateHz(float value)
{
    constexpr float minRate = 5.0f;
    constexpr float maxRate = 2200.0f;
    return minRate * std::pow(maxRate / minRate, juce::jlimit(0.0f, 1.0f, value));
}

juce::String formatRate(float value)
{
    const float hz = normalToShiftRateHz(value);
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, 2) + " kHz"
                         : juce::String(hz, hz >= 100.0f ? 0 : 1) + " Hz";
}

juce::String getTypeDescription(int typeIndex)
{
    switch (typeIndex)
    {
        case 1: return "Bigger dual-detune widening with stronger motion and more obvious modern stereo bloom.";
        case 2: return "Classic ring modulation for metallic, inharmonic, animated sidebands and strange tremolo textures.";
        case 3: return "Approximate single-sideband frequency shifting for alien movement, barber-pole-ish drift, and unstable width.";
        default: return "Short-delay micro-pitch widening that keeps the centre strong while opening the stereo image around it.";
    }
}

std::array<juce::String, 5> getControlNames(int typeIndex)
{
    if (typeIndex == 2)
        return { "Rate", "Spread", "Delay", "Feedback", "Mix" };

    if (typeIndex == 3)
        return { "Shift", "Spread", "Delay", "Feedback", "Mix" };

    return { "Detune", "Spread", "Delay", "Feedback", "Mix" };
}
}

ShiftModuleComponent::ShiftModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xff95ffb3);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeSelector);

    titleLabel.setText("PITCH / SHIFT", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleLabel(typeLabel, 10.5f, reactorui::textMuted());
    typeLabel.setText("Mode", juce::dontSendNotification);
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

    controls[0].textFromValueFunction = [this] (double value)
    {
        const int type = getSelectedType();
        if (type <= 1)
            return juce::String(normalToDetuneCents((float) value), 1) + " ct";

        return formatRate((float) value);
    };
    controls[1].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[2].textFromValueFunction = [this] (double value)
    {
        const int type = getSelectedType();
        const float ms = type <= 1 ? normalToShiftDelayMs((float) value)
                                   : normalToShiftPreDelayMs((float) value);
        return juce::String(ms, ms >= 10.0f ? 1 : 2) + " ms";
    };
    controls[3].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[4].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };

    typeSelector.addItemList(PluginProcessor::getFXShiftTypeNames(), 1);
    typeSelector.onChange = [this]
    {
        setSelectedType(juce::jmax(0, typeSelector.getSelectedItemIndex()));
    };

    syncTypeSelector();
    updateDescription();
    updateControlPresentation();
    startTimerHz(24);
}

int ShiftModuleComponent::getPreferredHeight() const
{
    return 390;
}

int ShiftModuleComponent::getSelectedType() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jlimit(0, PluginProcessor::getFXShiftTypeNames().size() - 1, (int) std::round(raw->load()));

    return 0;
}

void ShiftModuleComponent::setSelectedType(int typeIndex)
{
    setChoiceParameterValue(apvts,
                            getFXSlotVariantParamID(slotIndex),
                            juce::jlimit(0, PluginProcessor::getFXShiftTypeNames().size() - 1, typeIndex));
    syncTypeSelector();
    updateDescription();
    updateControlPresentation();
    repaint();
}

void ShiftModuleComponent::syncTypeSelector()
{
    const int selectedType = getSelectedType();
    if (typeSelector.getSelectedItemIndex() != selectedType)
        typeSelector.setSelectedItemIndex(selectedType, juce::dontSendNotification);
}

void ShiftModuleComponent::updateDescription()
{
    subtitleLabel.setText(getTypeDescription(getSelectedType()), juce::dontSendNotification);
}

void ShiftModuleComponent::updateControlPresentation()
{
    presentedType = getSelectedType();
    const auto names = getControlNames(presentedType);
    for (size_t i = 0; i < controlLabels.size(); ++i)
        controlLabels[i].setText(names[i], juce::dontSendNotification);

    for (auto& control : controls)
        control.setValue(control.getValue(), juce::dontSendNotification);
}

void ShiftModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xff95ffb3);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    drawDeck(g, typeDeckBounds.toFloat(), accent);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.74f));
    }
}

void ShiftModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(220));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    typeDeckBounds = area.removeFromTop(62);
    auto selectorArea = typeDeckBounds.reduced(12, 10);
    typeLabel.setBounds(selectorArea.removeFromTop(14));
    selectorArea.removeFromTop(8);
    typeSelector.setBounds(selectorArea.removeFromLeft(300).withHeight(28));

    area.removeFromTop(10);
    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int rowGap = 12;
    const int columnGap = 14;
    auto topRow = controlArea.removeFromTop(juce::jmax(102, (int) std::round(controlArea.getHeight() * 0.42f)));
    controlArea.removeFromTop(rowGap);
    auto bottomRow = controlArea;

    const int topWidth = (topRow.getWidth() - columnGap * 2) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = topRow.removeFromLeft(topWidth);
        if (i < 2)
            topRow.removeFromLeft(columnGap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
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
        auto content = cell.reduced(2, 4);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }
}

void ShiftModuleComponent::timerCallback()
{
    syncTypeSelector();
    updateDescription();
    if (getSelectedType() != presentedType)
        updateControlPresentation();

    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    typeSelector.setEnabled(enabled);

    repaint();
}
