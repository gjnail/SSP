#include "GateModuleComponent.h"

#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
constexpr int gateStepCount = 16;

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

bool isRhythmicGate(int variant)
{
    return (variant & 0x1) != 0;
}

int withRhythmicGate(int variant, bool rhythmic)
{
    if (rhythmic)
        return variant | 0x1;

    return variant & ~0x1;
}

bool getStepEnabledFromVariant(int variant, int stepIndex)
{
    return ((variant >> (stepIndex + 1)) & 0x1) != 0;
}

int withStepEnabled(int variant, int stepIndex, bool enabled)
{
    const int mask = 0x1 << (stepIndex + 1);
    if (enabled)
        return variant | mask;

    return variant & ~mask;
}

float normalToThresholdDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -60.0f, -6.0f);
}

float normalToAttackMs(float value)
{
    return 0.5f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.2f) * 70.0f;
}

float normalToHoldMs(float value)
{
    return std::pow(juce::jlimit(0.0f, 1.0f, value), 1.08f) * 180.0f;
}

float normalToReleaseMs(float value)
{
    return 4.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.15f) * 420.0f;
}

struct GateRateDivision
{
    const char* name;
    float beats = 0.25f;
};

const std::array<GateRateDivision, 10>& getGateRateDivisions()
{
    static const std::array<GateRateDivision, 10> divisions{{
        { "1/32", 0.125f },
        { "1/16T", 1.0f / 6.0f },
        { "1/16", 0.25f },
        { "1/8T", 1.0f / 3.0f },
        { "1/8", 0.5f },
        { "1/4T", 2.0f / 3.0f },
        { "1/4", 1.0f },
        { "1/2", 2.0f },
        { "1 Bar", 4.0f },
        { "2 Bars", 8.0f }
    }};
    return divisions;
}

int normalToGateRateIndex(float value)
{
    const auto maxIndex = (int) getGateRateDivisions().size() - 1;
    return juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
}

float gateRateIndexToNormal(int index)
{
    const auto maxIndex = (int) getGateRateDivisions().size() - 1;
    if (maxIndex <= 0)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, (float) juce::jlimit(0, maxIndex, index) / (float) maxIndex);
}

std::array<juce::String, 6> getControlNames(int typeIndex)
{
    if (typeIndex == 1)
        return { "Attack", "Decay", "Sustain", "Release", "Rate", "Mix" };

    return { "Threshold", "Attack", "Hold", "Release", "", "Mix" };
}

juce::String getTypeDescription(int typeIndex)
{
    if (typeIndex == 1)
        return "Kilohearts-style trance gate lane with a 16-step sequencer. Program the pattern above and sculpt each hit with ADSR-style controls.";

    return "Classic detector gate for cleaning tails, pumping noise, and tightening sustain with threshold, attack, hold, and release.";
}
}

GateModuleComponent::GateModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xfff26a7d);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeSelector);

    titleLabel.setText("GATE", juce::dontSendNotification);
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
        if (getSelectedType() == 0)
            return juce::String(normalToThresholdDb((float) value), 1) + " dB";

        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[1].textFromValueFunction = [this] (double value)
    {
        if (getSelectedType() == 0)
            return juce::String(normalToAttackMs((float) value), 1) + " ms";

        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[2].textFromValueFunction = [this] (double value)
    {
        if (getSelectedType() == 0)
            return juce::String(normalToHoldMs((float) value), 1) + " ms";

        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[3].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToReleaseMs((float) value), 1) + " ms";
    };
    controls[4].textFromValueFunction = [] (double value)
    {
        return juce::String(getGateRateDivisions()[(size_t) normalToGateRateIndex((float) value)].name);
    };
    controls[5].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };

    typeSelector.addItemList(PluginProcessor::getFXGateTypeNames(), 1);
    typeSelector.onChange = [this]
    {
        setSelectedType(juce::jmax(0, typeSelector.getSelectedItemIndex()));
    };

    syncTypeSelector();
    updateDescription();
    updateControlPresentation();
    updateRateFormatting();
    startTimerHz(60);
}

int GateModuleComponent::getPreferredHeight() const
{
    return 492;
}

int GateModuleComponent::getPackedVariant() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return (int) std::round(raw->load());

    return 0;
}

void GateModuleComponent::setPackedVariant(int variant)
{
    setChoiceParameterValue(apvts, getFXSlotVariantParamID(slotIndex), juce::jlimit(0, (1 << 20) - 1, variant));
}

int GateModuleComponent::getSelectedType() const
{
    return isRhythmicGate(getPackedVariant()) ? 1 : 0;
}

void GateModuleComponent::setSelectedType(int typeIndex)
{
    int variant = getPackedVariant();
    variant = withRhythmicGate(variant, typeIndex == 1);
    setPackedVariant(variant);
    syncTypeSelector();
    updateDescription();
    updateControlPresentation();
    controls[4].setVisible(typeIndex == 1);
    controlLabels[4].setVisible(typeIndex == 1);
    resized();
    repaint();
}

bool GateModuleComponent::isStepEnabled(int stepIndex) const
{
    return getStepEnabledFromVariant(getPackedVariant(), stepIndex);
}

void GateModuleComponent::setStepEnabled(int stepIndex, bool enabled)
{
    setPackedVariant(withStepEnabled(getPackedVariant(), stepIndex, enabled));
    repaint();
}

int GateModuleComponent::findStepAt(juce::Point<float> position) const
{
    if (getSelectedType() != 1)
        return -1;

    for (int step = 0; step < gateStepCount; ++step)
    {
        if (stepCellBounds[(size_t) step].toFloat().contains(position))
            return step;
    }

    return -1;
}

void GateModuleComponent::syncTypeSelector()
{
    const int selectedType = getSelectedType();
    if (typeSelector.getSelectedItemIndex() != selectedType)
        typeSelector.setSelectedItemIndex(selectedType, juce::dontSendNotification);
}

void GateModuleComponent::updateDescription()
{
    subtitleLabel.setText(getTypeDescription(getSelectedType()), juce::dontSendNotification);
}

void GateModuleComponent::updateControlPresentation()
{
    presentedType = getSelectedType();
    const auto names = getControlNames(presentedType);
    for (size_t i = 0; i < controlLabels.size(); ++i)
        controlLabels[i].setText(names[i], juce::dontSendNotification);

    for (auto& control : controls)
        control.setValue(control.getValue(), juce::dontSendNotification);
}

void GateModuleComponent::updateRateFormatting()
{
    controls[4].textFromValueFunction = [] (double value)
    {
        return juce::String(getGateRateDivisions()[(size_t) normalToGateRateIndex((float) value)].name);
    };
    controls[4].valueFromTextFunction = [] (const juce::String& text)
    {
        for (size_t i = 0; i < getGateRateDivisions().size(); ++i)
            if (text.trim().equalsIgnoreCase(getGateRateDivisions()[i].name))
                return (double) gateRateIndexToNormal((int) i);

        return 0.0;
    };

    controls[4].setValue(controls[4].getValue(), juce::dontSendNotification);
}

void GateModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xfff26a7d);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    drawDeck(g, typeDeckBounds.toFloat(), accent);
    if (! stepDeckBounds.isEmpty())
        drawDeck(g, stepDeckBounds.toFloat(), accent);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.74f));
    }

    for (int step = 0; step < gateStepCount; ++step)
    {
        const auto cell = stepCellBounds[(size_t) step].toFloat();
        if (cell.isEmpty())
            continue;

        const bool rhythmic = getSelectedType() == 1;
        const bool enabled = isStepEnabled(step);
        g.setColour(juce::Colour(0xff10151c));
        g.fillRoundedRectangle(cell, 4.0f);
        g.setColour(reactorui::outlineSoft().withAlpha(0.95f));
        g.drawRoundedRectangle(cell, 4.0f, 1.0f);

        if (rhythmic && enabled)
        {
            auto fill = cell.reduced(3.0f, 3.0f);
            g.setColour(accent.withAlpha(0.26f));
            g.fillRoundedRectangle(fill, 3.0f);
            g.setColour(accent.brighter(0.18f));
            g.fillRoundedRectangle(fill.removeFromBottom(fill.getHeight() * 0.82f), 3.0f);
        }

        g.setColour(rhythmic ? reactorui::textStrong().withAlpha(0.86f)
                             : reactorui::textMuted().withAlpha(0.42f));
        g.setFont(reactorui::bodyFont(9.5f));
        g.drawFittedText(juce::String(step + 1), cell.toNearestInt().removeFromBottom(14), juce::Justification::centred, 1);
    }

}

void GateModuleComponent::resized()
{
    for (auto& cell : stepCellBounds)
        cell = {};
    for (auto& cell : knobCellBounds)
        cell = {};

    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(160));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    typeDeckBounds = area.removeFromTop(62);
    auto selectorArea = typeDeckBounds.reduced(12, 10);
    typeLabel.setBounds(selectorArea.removeFromTop(14));
    selectorArea.removeFromTop(8);
    typeSelector.setBounds(selectorArea.removeFromLeft(220).withHeight(28));

    const bool rhythmic = getSelectedType() == 1;
    if (rhythmic)
    {
        area.removeFromTop(10);
        stepDeckBounds = area.removeFromTop(118);
        auto stepArea = stepDeckBounds.reduced(10, 12);
        const int gap = 6;
        const int cellWidth = (stepArea.getWidth() - gap * (gateStepCount - 1)) / gateStepCount;
        for (int step = 0; step < gateStepCount; ++step)
        {
            stepCellBounds[(size_t) step] = stepArea.removeFromLeft(cellWidth);
            if (step < gateStepCount - 1)
                stepArea.removeFromLeft(gap);
        }
    }
    else
    {
        stepDeckBounds = {};
    }

    area.removeFromTop(10);
    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int rowGap = 12;
    const int columnGap = 12;
    auto topRow = controlArea.removeFromTop(juce::jmax(102, (int) std::round(controlArea.getHeight() * (rhythmic ? 0.48f : 0.40f))));
    controlArea.removeFromTop(rowGap);
    auto bottomRow = controlArea;

    const int topCount = rhythmic ? 4 : 4;
    const int topWidth = (topRow.getWidth() - columnGap * (topCount - 1)) / topCount;
    for (int i = 0; i < topCount; ++i)
    {
        auto cell = topRow.removeFromLeft(topWidth);
        if (i < topCount - 1)
            topRow.removeFromLeft(columnGap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 3);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }

    if (rhythmic)
    {
        const int bottomWidth = (bottomRow.getWidth() - columnGap) / 2;
        for (int i = 4; i < 6; ++i)
        {
            auto cell = bottomRow.removeFromLeft(bottomWidth);
            if (i == 4)
                bottomRow.removeFromLeft(columnGap);

            knobCellBounds[(size_t) i] = cell;
            auto content = cell.reduced(2, 2);
            controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
            controls[(size_t) i].setBounds(content);
        }
    }
    else
    {
        auto mixCell = bottomRow.removeFromLeft(juce::jmax(220, bottomRow.getWidth() / 2));
        knobCellBounds[5] = mixCell;
        auto content = mixCell.reduced(2, 2);
        controlLabels[5].setBounds(content.removeFromTop(18));
        controls[5].setBounds(content);
        controls[4].setBounds({});
        controlLabels[4].setBounds({});
    }
}

void GateModuleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (const int step = findStepAt(event.position); step >= 0)
        setStepEnabled(step, ! isStepEnabled(step));
}

void GateModuleComponent::mouseDrag(const juce::MouseEvent& event)
{
}

void GateModuleComponent::mouseUp(const juce::MouseEvent& event)
{
}

void GateModuleComponent::timerCallback()
{
    syncTypeSelector();
    updateDescription();
    if (getSelectedType() != presentedType)
    {
        updateControlPresentation();
        controls[4].setVisible(getSelectedType() == 1);
        controlLabels[4].setVisible(getSelectedType() == 1);
        resized();
    }
    updateRateFormatting();

    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    typeSelector.setEnabled(enabled);
    controls[4].setVisible(getSelectedType() == 1);
    controlLabels[4].setVisible(getSelectedType() == 1);

    repaint();
}
