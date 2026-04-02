#include "CompressorModuleComponent.h"

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

void styleModeButton(juce::TextButton& button, juce::Colour accent)
{
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141a20));
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
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

float normalToInputDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.0f, 24.0f);
}

float normalToAttackMs(float value)
{
    return 0.02f * std::pow(300.0f, 1.0f - juce::jlimit(0.0f, 1.0f, value));
}

float normalToReleaseMs(float value)
{
    return 40.0f * std::pow(27.5f, juce::jlimit(0.0f, 1.0f, value));
}

float normalToOutputDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -6.0f, 18.0f);
}

void setChoiceParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1((float) value));
}
}

CompressorModuleComponent::CompressorModuleComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state, int slot)
    : processor(p),
      apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffff6f5f);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(ratioLabel);

    titleLabel.setText("VINTAGE COMP", juce::dontSendNotification);
    subtitleLabel.setText("FET-style squeeze with fast attack, program release, ratio buttons, and a classic gain-reduction meter.", juce::dontSendNotification);
    ratioLabel.setText("Ratio", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleLabel(ratioLabel, 10.5f, reactorui::textMuted());
    styleToggle(onButton, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    const auto ratioNames = PluginProcessor::getFXCompressorTypeNames();
    for (size_t i = 0; i < ratioButtons.size(); ++i)
    {
        addAndMakeVisible(ratioButtons[i]);
        ratioButtons[i].setButtonText(ratioNames[(int) i].upToFirstOccurrenceOf(":", false, false));
        styleModeButton(ratioButtons[i], accent);
        ratioButtons[i].onClick = [this, i]
        {
            setSelectedRatio((int) i);
        };
    }

    const std::array<juce::String, 5> controlNames{ "Input", "Attack", "Release", "Output", "Mix" };
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

    controls[0].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToInputDb((float) value), 1) + " dB";
    };
    controls[1].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToAttackMs((float) value), 2) + " ms";
    };
    controls[2].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToReleaseMs((float) value), 0) + " ms";
    };
    controls[3].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToOutputDb((float) value), 1) + " dB";
    };
    controls[4].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };

    syncRatioButtons();
    startTimerHz(24);
}

int CompressorModuleComponent::getPreferredHeight() const
{
    return 410;
}

void CompressorModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffff6f5f);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    drawDeck(g, meterBounds.toFloat(), accent);
    drawDeck(g, ratioDeckBounds.toFloat(), accent);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.74f));
    }

    if (! meterBounds.isEmpty())
    {
        auto meterBox = meterBounds;
        auto meterWindow = meterBox.reduced(8, 8).toFloat();
        juce::ColourGradient meterFill(juce::Colour(0xfff7ecd2), meterWindow.getTopLeft(),
                                       juce::Colour(0xffdcc8a2), meterWindow.getBottomLeft(), false);
        meterFill.addColour(0.42, juce::Colour(0xffefe1bf));
        g.setGradientFill(meterFill);
        g.fillRoundedRectangle(meterWindow, 5.0f);
        g.setColour(juce::Colour(0xff4b3628).withAlpha(0.75f));
        g.drawRoundedRectangle(meterWindow, 5.0f, 1.1f);

        auto meter = meterWindow.reduced(10.0f, 8.0f);
        const auto centre = juce::Point<float>(meter.getCentreX(), meter.getBottom() - 10.0f);
        const float radius = juce::jmin(meter.getWidth() * 0.44f, meter.getHeight() * 0.88f);
        const float startAngle = juce::MathConstants<float>::pi * 1.18f;
        const float endAngle = juce::MathConstants<float>::twoPi - juce::MathConstants<float>::pi * 0.18f;
        const float meterValue = processor.getFXCompressorGainReduction(slotIndex);

        for (int i = 0; i <= 6; ++i)
        {
            const float proportion = (float) i / 6.0f;
            const float angle = juce::jmap(proportion, 0.0f, 1.0f, startAngle, endAngle);
            const juce::Point<float> outer(centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius);
            const juce::Point<float> inner(centre.x + std::cos(angle) * (radius - 10.0f), centre.y + std::sin(angle) * (radius - 10.0f));
            g.setColour(juce::Colour(0xff5b4331).withAlpha(0.85f));
            g.drawLine({ inner, outer }, 1.2f);

            const juce::Point<float> textPoint(centre.x + std::cos(angle) * (radius - 24.0f),
                                               centre.y + std::sin(angle) * (radius - 24.0f));
            g.setColour(juce::Colour(0xff604634));
            g.setFont(reactorui::bodyFont(10.0f));
            g.drawFittedText(juce::String(i * 4),
                             juce::Rectangle<int>((int) std::round(textPoint.x - 10.0f), (int) std::round(textPoint.y - 7.0f), 20, 14),
                             juce::Justification::centred,
                             1);
        }

        const float needleAngle = juce::jmap(meterValue, 0.0f, 1.0f, startAngle, endAngle);
        const juce::Point<float> needleTip(centre.x + std::cos(needleAngle) * (radius - 13.0f),
                                           centre.y + std::sin(needleAngle) * (radius - 13.0f));
        g.setColour(juce::Colour(0xffc64d3d));
        g.drawLine({ centre, needleTip }, 2.4f);
        g.setColour(juce::Colour(0xff35261e));
        g.fillEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0xff8b5d48));
        g.drawEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 1.2f);

        auto meterLabelBounds = meterWindow.toNearestInt().removeFromBottom(22);
        g.setColour(juce::Colour(0xff3e2c21));
        g.setFont(reactorui::sectionFont(11.0f));
        g.drawFittedText("GAIN REDUCTION", meterLabelBounds, juce::Justification::centred, 1);
    }
}

void CompressorModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(180));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    auto topDeck = area.removeFromTop(138);
    meterBounds = topDeck.removeFromLeft(230);
    topDeck.removeFromLeft(10);
    ratioDeckBounds = topDeck;

    auto ratioArea = ratioDeckBounds.reduced(12, 12);
    ratioLabel.setBounds(ratioArea.removeFromTop(14));
    ratioArea.removeFromTop(10);
    const int buttonGap = 8;
    const int buttonWidth = (ratioArea.getWidth() - buttonGap * 4) / 5;
    for (int i = 0; i < (int) ratioButtons.size(); ++i)
    {
        ratioButtons[(size_t) i].setBounds(ratioArea.removeFromLeft(buttonWidth).withHeight(28));
        if (i < (int) ratioButtons.size() - 1)
            ratioArea.removeFromLeft(buttonGap);
    }

    area.removeFromTop(12);
    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int columnGap = 12;
    const int cellWidth = (controlArea.getWidth() - columnGap * 4) / 5;
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        auto cell = controlArea.removeFromLeft(cellWidth);
        if (i < (int) controls.size() - 1)
            controlArea.removeFromLeft(columnGap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }
}

void CompressorModuleComponent::timerCallback()
{
    syncRatioButtons();
    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    for (auto& button : ratioButtons)
        button.setEnabled(enabled);
    repaint();
}

int CompressorModuleComponent::getSelectedRatio() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jlimit(0, PluginProcessor::getFXCompressorTypeNames().size() - 1, (int) std::round(raw->load()));

    return 0;
}

void CompressorModuleComponent::setSelectedRatio(int ratioIndex)
{
    setChoiceParameterValue(apvts,
                            getFXSlotVariantParamID(slotIndex),
                            juce::jlimit(0, PluginProcessor::getFXCompressorTypeNames().size() - 1, ratioIndex));
    syncRatioButtons();
}

void CompressorModuleComponent::syncRatioButtons()
{
    const int selected = getSelectedRatio();
    for (int i = 0; i < (int) ratioButtons.size(); ++i)
        ratioButtons[(size_t) i].setToggleState(i == selected, juce::dontSendNotification);
}
