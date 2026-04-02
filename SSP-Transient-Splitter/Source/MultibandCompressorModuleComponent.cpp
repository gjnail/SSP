#include "MultibandCompressorModuleComponent.h"

#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
constexpr int multibandBandCount = 3;

juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotFloatParamID(int slotIndex, int parameterIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
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

float normalToTimeMs(float value)
{
    return 5.0f * std::pow(36.0f, juce::jlimit(0.0f, 1.0f, value));
}

void setFloatParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1(juce::jlimit(0.0f, 1.0f, value)));
}
}

MultibandCompressorModuleComponent::MultibandCompressorModuleComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state, int slot)
    : processor(p),
      apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffc3ff5b);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);

    titleLabel.setText("MULTIBAND COMP", juce::dontSendNotification);
    subtitleLabel.setText("OTT-style three-band dynamics. Drag the top handles for upward compression and the bottom handles for downward compression.", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleToggle(onButton, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    const std::array<juce::String, 4> names{ "Depth", "Time", "Output", "Mix" };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        addAndMakeVisible(controlLabels[i]);
        addAndMakeVisible(controls[i]);
        controlLabels[i].setText(names[i], juce::dontSendNotification);
        styleLabel(controlLabels[i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, 6 + (int) i);
        controls[i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        controlAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                        parameterID,
                                                                                                        controls[i]);
    }

    controls[0].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    controls[1].textFromValueFunction = [] (double value)
    {
        return juce::String(normalToTimeMs((float) value), 0) + " ms";
    };
    controls[2].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::jmap((float) value, -8.0f, 8.0f), 1) + " dB";
    };
    controls[3].textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };

    startTimerHz(60);
}

int MultibandCompressorModuleComponent::getPreferredHeight() const
{
    return 438;
}

void MultibandCompressorModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffc3ff5b);
    const auto upwardColour = juce::Colour(0xff79d7ff);
    const auto downwardColour = juce::Colour(0xffff8861);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    reactorui::drawDisplayPanel(g, graphBounds.toFloat(), accent);
    reactorui::drawSubtleGrid(g, graphBounds.reduced(1).toFloat(), accent.withAlpha(0.10f), 3, 4);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.74f));
    }

    if (! graphBounds.isEmpty())
    {
        const float centreY = graphBounds.getCentreY();
        g.setColour(reactorui::textMuted().withAlpha(0.32f));
        g.drawHorizontalLine((int) std::round(centreY), (float) graphBounds.getX(), (float) graphBounds.getRight());

        const float bandWidth = (float) graphBounds.getWidth() / 3.0f;
        const float topLimit = (float) graphBounds.getY() + 16.0f;
        const float bottomLimit = (float) graphBounds.getBottom() - 16.0f;

        for (int band = 0; band < multibandBandCount; ++band)
        {
            const float x = (float) graphBounds.getX() + bandWidth * (float) band;
            auto bandArea = juce::Rectangle<float>(x, (float) graphBounds.getY(), bandWidth, (float) graphBounds.getHeight()).reduced(8.0f, 8.0f);
            if (band > 0)
            {
                g.setColour(reactorui::outlineSoft().withAlpha(0.28f));
                g.drawVerticalLine((int) std::round(x), (float) graphBounds.getY() + 8.0f, (float) graphBounds.getBottom() - 8.0f);
            }

            const float upValue = getBandValue(3 + band);
            const float downValue = getBandValue(band);
            const float upY = juce::jmap(upValue, 0.0f, 1.0f, centreY - 8.0f, topLimit);
            const float downY = juce::jmap(downValue, 0.0f, 1.0f, centreY + 8.0f, bottomLimit);
            const float meterUpY = juce::jmap(displayedUpwardMeters[(size_t) band], 0.0f, 1.0f, centreY - 6.0f, upY);
            const float meterDownY = juce::jmap(displayedDownwardMeters[(size_t) band], 0.0f, 1.0f, centreY + 6.0f, downY);
            const float peakUpY = juce::jmap(peakUpwardMeters[(size_t) band], 0.0f, 1.0f, centreY - 6.0f, upY);
            const float peakDownY = juce::jmap(peakDownwardMeters[(size_t) band], 0.0f, 1.0f, centreY + 6.0f, downY);

            g.setColour(upwardColour.withAlpha(0.20f));
            g.fillRoundedRectangle(bandArea.withY(upY).withBottom(centreY - 3.0f), 4.0f);
            g.setColour(downwardColour.withAlpha(0.20f));
            g.fillRoundedRectangle(bandArea.withY(centreY + 3.0f).withBottom(downY), 4.0f);

            auto upwardMeterArea = juce::Rectangle<float>(bandArea.getX() + 10.0f, meterUpY,
                                                          juce::jmax(24.0f, bandArea.getWidth() - 20.0f),
                                                          juce::jmax(3.0f, centreY - meterUpY - 3.0f));
            auto downwardMeterArea = juce::Rectangle<float>(bandArea.getX() + 10.0f, centreY + 3.0f,
                                                            juce::jmax(24.0f, bandArea.getWidth() - 20.0f),
                                                            juce::jmax(3.0f, meterDownY - centreY - 3.0f));

            g.setColour(upwardColour.withAlpha(0.12f));
            g.fillRoundedRectangle(upwardMeterArea.expanded(8.0f, 10.0f), 9.0f);
            g.setColour(upwardColour.withAlpha(0.22f));
            g.fillRoundedRectangle(upwardMeterArea.expanded(4.0f, 5.0f), 7.0f);

            juce::ColourGradient upwardFill(upwardColour.withAlpha(0.92f), upwardMeterArea.getCentreX(), upwardMeterArea.getBottom(),
                                            upwardColour.brighter(0.55f).withAlpha(1.0f), upwardMeterArea.getCentreX(), upwardMeterArea.getY(), false);
            upwardFill.addColour(0.45, upwardColour.withAlpha(0.74f));
            g.setGradientFill(upwardFill);
            g.fillRoundedRectangle(upwardMeterArea, 5.0f);
            g.setColour(upwardColour.brighter(0.9f).withAlpha(0.72f));
            g.drawHorizontalLine((int) std::round(upwardMeterArea.getY() + 1.0f), upwardMeterArea.getX() + 4.0f, upwardMeterArea.getRight() - 4.0f);

            g.setColour(downwardColour.withAlpha(0.12f));
            g.fillRoundedRectangle(downwardMeterArea.expanded(8.0f, 10.0f), 9.0f);
            g.setColour(downwardColour.withAlpha(0.22f));
            g.fillRoundedRectangle(downwardMeterArea.expanded(4.0f, 5.0f), 7.0f);

            juce::ColourGradient downwardFill(downwardColour.brighter(0.30f).withAlpha(1.0f), downwardMeterArea.getCentreX(), downwardMeterArea.getY(),
                                              downwardColour.withAlpha(0.88f), downwardMeterArea.getCentreX(), downwardMeterArea.getBottom(), false);
            downwardFill.addColour(0.55, downwardColour.withAlpha(0.74f));
            g.setGradientFill(downwardFill);
            g.fillRoundedRectangle(downwardMeterArea, 5.0f);
            g.setColour(downwardColour.brighter(0.7f).withAlpha(0.68f));
            g.drawHorizontalLine((int) std::round(downwardMeterArea.getBottom() - 1.0f), downwardMeterArea.getX() + 4.0f, downwardMeterArea.getRight() - 4.0f);

            g.setColour(upwardColour.brighter(0.9f).withAlpha(0.96f));
            g.drawHorizontalLine((int) std::round(peakUpY), bandArea.getX() + 12.0f, bandArea.getRight() - 12.0f);
            g.setColour(downwardColour.brighter(0.8f).withAlpha(0.96f));
            g.drawHorizontalLine((int) std::round(peakDownY), bandArea.getX() + 12.0f, bandArea.getRight() - 12.0f);

            const auto upHandle = getHandlePosition(band, true);
            const auto downHandle = getHandlePosition(band, false);
            g.setColour(juce::Colour(0xff0d1116));
            g.fillEllipse(upHandle.x - 7.0f, upHandle.y - 7.0f, 14.0f, 14.0f);
            g.fillEllipse(downHandle.x - 7.0f, downHandle.y - 7.0f, 14.0f, 14.0f);
            g.setColour(upwardColour);
            g.fillEllipse(upHandle.x - 5.6f, upHandle.y - 5.6f, 11.2f, 11.2f);
            g.setColour(downwardColour);
            g.fillEllipse(downHandle.x - 5.6f, downHandle.y - 5.6f, 11.2f, 11.2f);

            const juce::String name = band == 0 ? "Low" : band == 1 ? "Mid" : "High";
            g.setColour(reactorui::textStrong().withAlpha(0.88f));
            g.setFont(reactorui::bodyFont(11.0f));
            g.drawFittedText(name, bandArea.toNearestInt().removeFromBottom(16), juce::Justification::centred, 1);
        }
    }
}

void MultibandCompressorModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(220));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    graphBounds = area.removeFromTop(206);
    area.removeFromTop(12);

    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int gap = 12;
    const int cellWidth = (controlArea.getWidth() - gap * 3) / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = controlArea.removeFromLeft(cellWidth);
        if (i < 3)
            controlArea.removeFromLeft(gap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
        controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        controls[(size_t) i].setBounds(content);
    }
}

void MultibandCompressorModuleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! graphBounds.contains(event.getPosition()))
        return;

    const auto hit = findHandleAt(event.position);
    draggingBand = hit.first;
    draggingUpward = hit.second;
}

void MultibandCompressorModuleComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingBand < 0 || ! graphBounds.contains(event.getPosition()))
        return;

    const float centreY = graphBounds.getCentreY();
    if (draggingUpward)
    {
        const float normal = juce::jlimit(0.0f, 1.0f, (centreY - event.position.y) / juce::jmax(1.0f, centreY - ((float) graphBounds.getY() + 16.0f)));
        setBandValue(3 + draggingBand, normal);
    }
    else
    {
        const float normal = juce::jlimit(0.0f, 1.0f, (event.position.y - centreY) / juce::jmax(1.0f, ((float) graphBounds.getBottom() - 16.0f) - centreY));
        setBandValue(draggingBand, normal);
    }

    repaint(graphBounds.expanded(8));
}

void MultibandCompressorModuleComponent::mouseUp(const juce::MouseEvent&)
{
    draggingBand = -1;
}

void MultibandCompressorModuleComponent::timerCallback()
{
    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    updateMeterDisplays();
}

void MultibandCompressorModuleComponent::updateMeterDisplays()
{
    std::array<float, 3> upMeters{};
    std::array<float, 3> downMeters{};
    processor.getFXMultibandMeter(slotIndex, upMeters, downMeters);

    bool changed = false;

    for (int band = 0; band < multibandBandCount; ++band)
    {
        const auto visualBoost = [] (float value)
        {
            const float clamped = juce::jlimit(0.0f, 1.0f, value);
            return juce::jlimit(0.0f, 1.0f, std::pow(clamped, 0.72f) * 1.22f);
        };

        const float upTarget = visualBoost(upMeters[(size_t) band]);
        const float downTarget = visualBoost(downMeters[(size_t) band]);

        auto smoothMeter = [&changed] (float current, float target)
        {
            const float coefficient = target > current ? 0.70f : 0.26f;
            const float next = current + (target - current) * coefficient;
            changed = changed || std::abs(next - current) > 0.0015f;
            return next;
        };

        displayedUpwardMeters[(size_t) band] = smoothMeter(displayedUpwardMeters[(size_t) band], upTarget);
        displayedDownwardMeters[(size_t) band] = smoothMeter(displayedDownwardMeters[(size_t) band], downTarget);

        const float nextPeakUp = juce::jmax(displayedUpwardMeters[(size_t) band], peakUpwardMeters[(size_t) band] - 0.010f);
        const float nextPeakDown = juce::jmax(displayedDownwardMeters[(size_t) band], peakDownwardMeters[(size_t) band] - 0.010f);
        changed = changed || std::abs(nextPeakUp - peakUpwardMeters[(size_t) band]) > 0.0015f;
        changed = changed || std::abs(nextPeakDown - peakDownwardMeters[(size_t) band]) > 0.0015f;
        peakUpwardMeters[(size_t) band] = nextPeakUp;
        peakDownwardMeters[(size_t) band] = nextPeakDown;
    }

    if (changed)
        repaint(graphBounds.expanded(6));
}

float MultibandCompressorModuleComponent::getBandValue(int parameterIndex) const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, parameterIndex)))
        return juce::jlimit(0.0f, 1.0f, raw->load());

    return 0.0f;
}

void MultibandCompressorModuleComponent::setBandValue(int parameterIndex, float value)
{
    setFloatParameter(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), value);
}

std::pair<int, bool> MultibandCompressorModuleComponent::findHandleAt(juce::Point<float> position) const
{
    for (int band = 0; band < multibandBandCount; ++band)
    {
        if (getHandlePosition(band, true).getDistanceFrom(position) <= 10.0f)
            return { band, true };
        if (getHandlePosition(band, false).getDistanceFrom(position) <= 10.0f)
            return { band, false };
    }

    return { -1, false };
}

juce::Point<float> MultibandCompressorModuleComponent::getHandlePosition(int bandIndex, bool upward) const
{
    const float bandWidth = (float) graphBounds.getWidth() / 3.0f;
    const float x = (float) graphBounds.getX() + bandWidth * ((float) bandIndex + 0.5f);
    const float centreY = graphBounds.getCentreY();

    if (upward)
    {
        const float value = getBandValue(3 + bandIndex);
        const float y = juce::jmap(value, 0.0f, 1.0f, centreY - 8.0f, (float) graphBounds.getY() + 16.0f);
        return { x, y };
    }

    const float value = getBandValue(bandIndex);
    const float y = juce::jmap(value, 0.0f, 1.0f, centreY + 8.0f, (float) graphBounds.getBottom() - 16.0f);
    return { x, y };
}
