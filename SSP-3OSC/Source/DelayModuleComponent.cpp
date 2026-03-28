#include "DelayModuleComponent.h"

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

void setFloatParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1(juce::jlimit(0.0f, 1.0f, value)));
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
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 22);
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.4f));
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

float normalToDelayFreeMs(float value)
{
    constexpr float minDelayMs = 1.0f;
    constexpr float maxDelayMs = 1800.0f;
    return minDelayMs * std::pow(maxDelayMs / minDelayMs, juce::jlimit(0.0f, 1.0f, value));
}

float delayFreeMsToNormal(float milliseconds)
{
    constexpr float minDelayMs = 1.0f;
    constexpr float maxDelayMs = 1800.0f;
    return juce::jlimit(0.0f, 1.0f, (float) juce::mapFromLog10(juce::jlimit(minDelayMs, maxDelayMs, milliseconds), minDelayMs, maxDelayMs));
}

struct DelaySyncDivision
{
    const char* name;
    float beats = 1.0f;
};

const std::array<DelaySyncDivision, 12>& getDelaySyncDivisions()
{
    static const std::array<DelaySyncDivision, 12> divisions{{
        { "1/32", 0.125f },
        { "1/16T", 1.0f / 6.0f },
        { "1/16", 0.25f },
        { "1/8T", 1.0f / 3.0f },
        { "1/8", 0.5f },
        { "1/4T", 2.0f / 3.0f },
        { "1/4", 1.0f },
        { "1/2T", 4.0f / 3.0f },
        { "1/2", 2.0f },
        { "1 Bar", 4.0f },
        { "2 Bars", 8.0f },
        { "4 Bars", 16.0f }
    }};
    return divisions;
}

int normalToSyncDivisionIndex(float value)
{
    const auto maxIndex = (int) getDelaySyncDivisions().size() - 1;
    return juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
}

float syncDivisionIndexToNormal(int index)
{
    const auto maxIndex = (int) getDelaySyncDivisions().size() - 1;
    if (maxIndex <= 0)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, (float) juce::jlimit(0, maxIndex, index) / (float) maxIndex);
}

float normalToHighPassFrequency(float value)
{
    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 2400.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToLowPassFrequency(float value)
{
    constexpr float minFrequency = 700.0f;
    constexpr float maxFrequency = 20000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

juce::String formatFrequency(float frequency)
{
    return frequency >= 1000.0f ? juce::String(frequency / 1000.0f, 1) + " kHz"
                                : juce::String(juce::roundToInt(frequency)) + " Hz";
}

juce::String getTypeDescription(int typeIndex)
{
    switch (typeIndex)
    {
        case 1: return "Ping-pong bounce that trades repeats between channels for a wider stereo echo path.";
        case 2: return "Darker tape-style repeats with extra saturation and softer top end on feedback.";
        case 3: return "Diffused delay cloud with blurred repeats and extra stereo smear.";
        default: return "Clean stereo delay with independent left and right timing, filtered repeats, and broad sweet spots.";
    }
}
}

DelayModuleComponent::DelayModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffd6a3ff);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(syncButton);
    addAndMakeVisible(linkButton);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeSelector);

    titleLabel.setText("DELAY", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleLabel(typeLabel, 10.5f, reactorui::textMuted());
    typeLabel.setText("Type", juce::dontSendNotification);
    syncButton.setButtonText("Sync");
    linkButton.setButtonText("Link");
    styleToggle(onButton, accent);
    styleModeButton(syncButton, accent);
    styleModeButton(linkButton, accent);
    styleSelector(typeSelector, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    const std::array<juce::String, 2> timeNames{ "Left", "Right" };
    for (size_t i = 0; i < timeControls.size(); ++i)
    {
        addAndMakeVisible(timeLabels[i]);
        addAndMakeVisible(timeControls[i]);
        timeLabels[i].setText(timeNames[i], juce::dontSendNotification);
        styleLabel(timeLabels[i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, (int) i);
        timeControls[i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        timeAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                     parameterID,
                                                                                                     timeControls[i]);
    }

    addAndMakeVisible(feedbackLabel);
    addAndMakeVisible(feedbackControl);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(mixControl);
    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    mixLabel.setText("Mix", juce::dontSendNotification);
    styleLabel(feedbackLabel, 11.0f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(mixLabel, 11.0f, reactorui::textMuted(), juce::Justification::centred);
    const auto feedbackParameterID = getFXSlotFloatParamID(slotIndex, 4);
    const auto mixParameterID = getFXSlotFloatParamID(slotIndex, 5);
    feedbackControl.setupModulation(processor, feedbackParameterID, reactormod::Destination::none, accent);
    mixControl.setupModulation(processor, mixParameterID, reactormod::Destination::none, accent);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                 feedbackParameterID,
                                                                                                 feedbackControl);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                            mixParameterID,
                                                                                            mixControl);

    feedbackControl.textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    feedbackControl.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, text.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0);
    };

    mixControl.textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    mixControl.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, text.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0);
    };

    syncButton.onClick = [this]
    {
        setSyncEnabled(syncButton.getToggleState());
    };
    linkButton.onClick = [this]
    {
        setLinkEnabled(linkButton.getToggleState());
    };

    timeControls[0].onValueChange = [this]
    {
        mirrorLinkedTime(0);
    };
    timeControls[1].onValueChange = [this]
    {
        mirrorLinkedTime(1);
    };

    typeSelector.addItemList(PluginProcessor::getFXDelayTypeNames(), 1);
    typeSelector.onChange = [this]
    {
        setSelectedType(juce::jmax(0, typeSelector.getSelectedItemIndex()));
    };

    syncButtons();
    syncTypeSelector();
    updateDescription();
    updateTimeFormatting();
    startTimerHz(24);
}

int DelayModuleComponent::getPreferredHeight() const
{
    return 528;
}

void DelayModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffd6a3ff);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    drawDeck(g, rateDeckBounds.toFloat(), accent);
    drawDeck(g, typeDeckBounds.toFloat(), accent);
    drawDeck(g, filterDeckBounds.toFloat(), accent);
    drawDeck(g, bottomDeckBounds.toFloat(), accent);

    for (const auto& cell : timeCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.70f));
    }

    for (const auto& cell : mixCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.70f));
    }

    if (! filterPadBounds.isEmpty())
    {
        auto pad = filterPadBounds.toFloat();
        auto filterTextArea = filterDeckBounds.reduced(12, 10);
        juce::ColourGradient padFill(juce::Colour(0xff11151c), pad.getTopLeft(),
                                     juce::Colour(0xff0a0d13), pad.getBottomLeft(), false);
        padFill.addColour(0.18, accent.withAlpha(0.08f));
        g.setGradientFill(padFill);
        g.fillRoundedRectangle(pad, 8.0f);
        g.setColour(juce::Colour(0xff050608));
        g.drawRoundedRectangle(pad, 8.0f, 1.2f);
        g.setColour(reactorui::outlineSoft().withAlpha(0.9f));
        g.drawRoundedRectangle(pad.reduced(1.2f), 7.0f, 1.0f);

        const float lowNorm = getParamValue(2);
        const float highNorm = getParamValue(3);
        const float leftX = pad.getX() + lowNorm * pad.getWidth();
        const float rightX = pad.getX() + highNorm * pad.getWidth();
        const float centreY = pad.getCentreY();
        const float startY = pad.getBottom() - 12.0f;
        const float topY = centreY - 4.0f;

        g.setColour(accent.withAlpha(0.12f));
        g.fillRoundedRectangle(juce::Rectangle<float>(leftX, pad.getY() + 10.0f, juce::jmax(8.0f, rightX - leftX), pad.getHeight() - 20.0f), 5.0f);

        g.setColour(reactorui::textMuted().withAlpha(0.22f));
        for (int i = 1; i < 5; ++i)
        {
            const float gridX = pad.getX() + pad.getWidth() * ((float) i / 5.0f);
            g.drawVerticalLine(juce::roundToInt(gridX), pad.getY() + 10.0f, pad.getBottom() - 10.0f);
        }

        auto smoothStep = [] (float edge0, float edge1, float x)
        {
            const float width = juce::jmax(0.0001f, edge1 - edge0);
            const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / width);
            return t * t * (3.0f - 2.0f * t);
        };

        juce::Path response;
        constexpr int pointCount = 60;
        for (int i = 0; i < pointCount; ++i)
        {
            const float xNorm = (float) i / (float) (pointCount - 1);
            const float hp = smoothStep(lowNorm - 0.06f, lowNorm + 0.06f, xNorm);
            const float lp = 1.0f - smoothStep(highNorm - 0.06f, highNorm + 0.06f, xNorm);
            const float band = juce::jlimit(0.0f, 1.0f, hp * lp);
            const float y = juce::jmap(band, startY, topY);
            const float x = pad.getX() + xNorm * pad.getWidth();

            if (i == 0)
                response.startNewSubPath(x, y);
            else
                response.lineTo(x, y);
        }
        g.setColour(accent.withAlpha(0.92f));
        g.strokePath(response, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(accent.brighter(0.4f));
        g.fillEllipse(leftX - 5.0f, centreY - 9.0f, 10.0f, 10.0f);
        g.fillEllipse(rightX - 5.0f, centreY - 9.0f, 10.0f, 10.0f);

        g.setColour(reactorui::textMuted());
        g.setFont(reactorui::bodyFont(11.0f));
        g.drawText("Filter", filterTextArea.removeFromTop(14), juce::Justification::topLeft, false);
        g.drawFittedText("HP " + formatFrequency(normalToHighPassFrequency(lowNorm))
                         + "    LP " + formatFrequency(normalToLowPassFrequency(highNorm)),
                         filterTextArea.removeFromBottom(18),
                         juce::Justification::centredRight,
                         1);
    }
}

void DelayModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(160));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    auto topRow = area.removeFromTop(156);
    typeDeckBounds = topRow.removeFromRight(248);
    topRow.removeFromRight(10);
    rateDeckBounds = topRow;

    area.removeFromTop(10);
    filterDeckBounds = area.removeFromTop(122);
    area.removeFromTop(10);
    bottomDeckBounds = area;

    auto rateArea = rateDeckBounds.reduced(10, 8);
    auto rateHeader = rateArea.removeFromTop(30);
    syncButton.setBounds(rateHeader.removeFromRight(66).reduced(0, 2));
    rateHeader.removeFromRight(8);
    linkButton.setBounds(rateHeader.removeFromRight(66).reduced(0, 2));
    area.removeFromTop(0);

    const int columnGap = 14;
    const int knobWidth = (rateArea.getWidth() - columnGap) / 2;
    for (int i = 0; i < 2; ++i)
    {
        auto cell = rateArea.removeFromLeft(knobWidth);
        if (i == 0)
            rateArea.removeFromLeft(columnGap);

        timeCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
        timeLabels[(size_t) i].setBounds(content.removeFromTop(18));
        timeControls[(size_t) i].setBounds(content);
    }

    auto typeArea = typeDeckBounds.reduced(12, 10);
    typeLabel.setBounds(typeArea.removeFromTop(14));
    typeArea.removeFromTop(8);
    typeSelector.setBounds(typeArea.removeFromTop(28));

    filterPadBounds = filterDeckBounds.reduced(12, 12);
    filterPadBounds.removeFromTop(16);
    filterPadBounds.removeFromBottom(20);

    auto bottomArea = bottomDeckBounds.reduced(10, 8);
    const int mixGap = 14;
    const int mixWidth = (bottomArea.getWidth() - mixGap) / 2;
    for (int i = 0; i < 2; ++i)
    {
        auto cell = bottomArea.removeFromLeft(mixWidth);
        if (i == 0)
            bottomArea.removeFromLeft(mixGap);

        mixCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
        if (i == 0)
        {
            feedbackLabel.setBounds(content.removeFromTop(18));
            feedbackControl.setBounds(content);
        }
        else
        {
            mixLabel.setBounds(content.removeFromTop(18));
            mixControl.setBounds(content);
        }
    }
}

void DelayModuleComponent::mouseDown(const juce::MouseEvent& event)
{
    beginFilterDrag(event.position);
}

void DelayModuleComponent::mouseDrag(const juce::MouseEvent& event)
{
    updateFilterDrag(event.position);
}

void DelayModuleComponent::mouseUp(const juce::MouseEvent&)
{
    filterDragTarget = FilterDragTarget::none;
}

void DelayModuleComponent::timerCallback()
{
    syncButtons();
    syncTypeSelector();
    updateDescription();
    updateTimeFormatting();

    const bool enabled = onButton.getToggleState();
    timeControls[0].setEnabled(enabled);
    timeControls[1].setEnabled(enabled);
    feedbackControl.setEnabled(enabled);
    mixControl.setEnabled(enabled);
    syncButton.setEnabled(enabled);
    linkButton.setEnabled(enabled);
    typeSelector.setEnabled(enabled);

    repaint();
}

int DelayModuleComponent::getSelectedType() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jlimit(0, PluginProcessor::getFXDelayTypeNames().size() - 1, (int) std::round(raw->load()));

    return 0;
}

void DelayModuleComponent::setSelectedType(int typeIndex)
{
    setChoiceParameterValue(apvts,
                            getFXSlotVariantParamID(slotIndex),
                            juce::jlimit(0, PluginProcessor::getFXDelayTypeNames().size() - 1, typeIndex));
    syncTypeSelector();
    updateDescription();
}

void DelayModuleComponent::syncTypeSelector()
{
    const int selectedType = getSelectedType();
    if (typeSelector.getSelectedItemIndex() != selectedType)
        typeSelector.setSelectedItemIndex(selectedType, juce::dontSendNotification);
}

void DelayModuleComponent::updateDescription()
{
    subtitleLabel.setText(getTypeDescription(getSelectedType()), juce::dontSendNotification);
}

void DelayModuleComponent::updateTimeFormatting()
{
    const bool syncEnabled = isSyncEnabled();

    for (auto& timeControl : timeControls)
    {
        if (syncEnabled)
        {
            timeControl.textFromValueFunction = [] (double value)
            {
                return juce::String(getDelaySyncDivisions()[(size_t) normalToSyncDivisionIndex((float) value)].name);
            };
            timeControl.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto divisions = getDelaySyncDivisions();
                for (size_t i = 0; i < divisions.size(); ++i)
                {
                    if (text.trim().equalsIgnoreCase(divisions[i].name))
                        return (double) syncDivisionIndexToNormal((int) i);
                }

                return (double) syncDivisionIndexToNormal(6);
            };
        }
        else
        {
            timeControl.textFromValueFunction = [] (double value)
            {
                return juce::String(normalToDelayFreeMs((float) value), 1) + " ms";
            };
            timeControl.valueFromTextFunction = [] (const juce::String& text)
            {
                return (double) delayFreeMsToNormal((float) text.upToFirstOccurrenceOf(" ", false, false).getDoubleValue());
            };
        }

        timeControl.updateText();
    }
}

void DelayModuleComponent::setSyncEnabled(bool shouldSync)
{
    setParamValue(6, shouldSync ? 1.0f : 0.0f);
    syncButtons();
    updateTimeFormatting();
}

void DelayModuleComponent::setLinkEnabled(bool shouldLink)
{
    setParamValue(7, shouldLink ? 1.0f : 0.0f);
    if (shouldLink)
        setParamValue(1, getParamValue(0));
    syncButtons();
}

bool DelayModuleComponent::isSyncEnabled() const
{
    return getParamValue(6) >= 0.5f;
}

bool DelayModuleComponent::isLinkEnabled() const
{
    return getParamValue(7) >= 0.5f;
}

void DelayModuleComponent::syncButtons()
{
    const bool syncEnabled = isSyncEnabled();
    const bool linkEnabled = isLinkEnabled();

    if (syncButton.getToggleState() != syncEnabled)
        syncButton.setToggleState(syncEnabled, juce::dontSendNotification);
    if (linkButton.getToggleState() != linkEnabled)
        linkButton.setToggleState(linkEnabled, juce::dontSendNotification);
}

float DelayModuleComponent::getParamValue(int parameterIndex) const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, parameterIndex)))
        return raw->load();

    return 0.0f;
}

void DelayModuleComponent::setParamValue(int parameterIndex, float value)
{
    setFloatParameterValue(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), value);
}

void DelayModuleComponent::mirrorLinkedTime(int sourceIndex)
{
    if (suppressLinkedUpdate || ! isLinkEnabled())
        return;

    const int destinationIndex = sourceIndex == 0 ? 1 : 0;
    suppressLinkedUpdate = true;
    setParamValue(destinationIndex, (float) timeControls[(size_t) sourceIndex].getValue());
    suppressLinkedUpdate = false;
}

void DelayModuleComponent::beginFilterDrag(juce::Point<float> position)
{
    if (! filterPadBounds.contains(juce::roundToInt(position.x), juce::roundToInt(position.y)))
        return;

    const float lowNorm = getParamValue(2);
    const float highNorm = getParamValue(3);
    const float lowX = (float) filterPadBounds.getX() + lowNorm * (float) filterPadBounds.getWidth();
    const float highX = (float) filterPadBounds.getX() + highNorm * (float) filterPadBounds.getWidth();
    filterDragTarget = std::abs(position.x - lowX) <= std::abs(position.x - highX) ? FilterDragTarget::lowCut
                                                                                     : FilterDragTarget::highCut;
    updateFilterDrag(position);
}

void DelayModuleComponent::updateFilterDrag(juce::Point<float> position)
{
    if (filterDragTarget == FilterDragTarget::none || filterPadBounds.isEmpty())
        return;

    const float normalisedX = juce::jlimit(0.0f, 1.0f, (position.x - (float) filterPadBounds.getX()) / (float) filterPadBounds.getWidth());
    const float minGap = 0.05f;

    if (filterDragTarget == FilterDragTarget::lowCut)
        setParamValue(2, juce::jmin(normalisedX, getParamValue(3) - minGap));
    else if (filterDragTarget == FilterDragTarget::highCut)
        setParamValue(3, juce::jmax(normalisedX, getParamValue(2) + minGap));
}
