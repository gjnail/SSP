#include "ModulationKnob.h"

namespace
{
constexpr float rotaryStartAngle = juce::MathConstants<float>::pi * 1.18f;
constexpr float rotaryEndAngle = juce::MathConstants<float>::pi * 2.82f;
}

ModulationKnob::ModulationKnob(PluginProcessor& p,
                               juce::String id,
                               reactormod::Destination destinationToUse,
                               juce::Colour accent,
                               int textBoxWidth,
                               int textBoxHeight)
{
    setupModulation(p, std::move(id), destinationToUse, accent, textBoxWidth, textBoxHeight);
}

void ModulationKnob::setupModulation(PluginProcessor& p,
                                     juce::String id,
                                     reactormod::Destination destinationToUse,
                                     juce::Colour accent,
                                     int textBoxWidth,
                                     int textBoxHeight)
{
    stopTimer();

    processor = &p;
    parameterID = std::move(id);
    destination = destinationToUse == reactormod::Destination::none
                    ? reactormod::destinationForParameterID(parameterID)
                    : destinationToUse;
    modulationInfo = {};
    dragActive = false;
    dragAccepting = false;
    dragSourceIndex = 0;
    targetHovered = false;
    editingTargetDepth = false;
    targetDragStartAmount = 0.0f;
    targetDragStartPosition = {};
    valueLabel = nullptr;

    reactorui::styleKnobSlider(*this, accent, textBoxWidth, textBoxHeight);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setInterceptsMouseClicks(true, true);
    initialiseValueLabel();

    if (destination != reactormod::Destination::none)
    {
        refreshModulationState();
        startTimerHz(10);
    }
}

bool ModulationKnob::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    dragAccepting = processor != nullptr
        && destination != reactormod::Destination::none
        && isModulationSourceDragDescription(dragSourceDetails.description);
    return dragAccepting;
}

void ModulationKnob::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    dragAccepting = isInterestedInDragSource(dragSourceDetails);
    dragSourceIndex = sourceIndexFromDescription(dragSourceDetails.description);
    dragActive = dragAccepting;
    repaint();
}

void ModulationKnob::itemDragExit(const SourceDetails&)
{
    dragActive = false;
    dragAccepting = false;
    dragSourceIndex = 0;
    repaint();
}

void ModulationKnob::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragActive = false;
    dragAccepting = false;

    const int sourceIndex = sourceIndexFromDescription(dragSourceDetails.description);
    dragSourceIndex = 0;
    if (processor != nullptr && sourceIndex > 0 && destination != reactormod::Destination::none)
    {
        processor->setSelectedModulationSourceIndex(sourceIndex);
        processor->assignSourceToDestination(sourceIndex, destination, 1.0f);
    }

    refreshModulationState();
    repaint();
}

void ModulationKnob::mouseEnter(const juce::MouseEvent& event)
{
    updateTargetHover(event.position);
    juce::Slider::mouseEnter(event);
}

void ModulationKnob::mouseMove(const juce::MouseEvent& event)
{
    updateTargetHover(event.position);
    juce::Slider::mouseMove(event);
}

void ModulationKnob::mouseExit(const juce::MouseEvent& event)
{
    targetHovered = false;
    if (! editingTargetDepth)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    repaint();
    juce::Slider::mouseExit(event);
}

void ModulationKnob::mouseDown(const juce::MouseEvent& event)
{
    if (onPrimaryInteract)
        onPrimaryInteract();

    if (shouldShowTargetChip() && getTargetChipBounds().contains(event.position))
    {
        editingTargetDepth = true;
        targetDragStartAmount = modulationInfo.amount;
        targetDragStartPosition = event.position;
        repaint();
        return;
    }

    setValueLabelVisible(true);
    juce::Slider::mouseDown(event);
}

void ModulationKnob::mouseDrag(const juce::MouseEvent& event)
{
    if (editingTargetDepth)
    {
        const float delta = (event.position.x - targetDragStartPosition.x) / 120.0f;
        applyTargetAmount(targetDragStartAmount + delta);
        updateTargetHover(event.position);
        return;
    }

    setValueLabelVisible(true);
    juce::Slider::mouseDrag(event);
}

void ModulationKnob::mouseUp(const juce::MouseEvent& event)
{
    if (editingTargetDepth)
    {
        editingTargetDepth = false;
        updateTargetHover(event.position);
        repaint();
        return;
    }

    setValueLabelVisible(false);
    juce::Slider::mouseUp(event);
}

void ModulationKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (shouldShowTargetChip() && getTargetChipBounds().contains(event.position))
    {
        applyTargetAmount(modulationInfo.amount + wheel.deltaY * 0.18f);
        updateTargetHover(event.position);
        return;
    }

    showValueLabelTemporarily();
    juce::Slider::mouseWheelMove(event, wheel);
}

void ModulationKnob::paintOverChildren(juce::Graphics& g)
{
    auto knobBounds = getKnobBounds();
    if (knobBounds.isEmpty())
        return;

    const auto centre = knobBounds.getCentre();
    const float radius = knobBounds.getWidth() * 0.56f;
    const float lineWidth = juce::jmin(6.0f, knobBounds.getWidth() * 0.08f);

    juce::Path outerTrack;
    outerTrack.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff090c10).withAlpha(0.9f));
    g.strokePath(outerTrack, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (dragActive)
    {
        g.setColour(reactorui::modulationSourceColour(juce::jmax(1, dragSourceIndex)).withAlpha(0.85f));
        g.strokePath(outerTrack, juce::PathStrokeType(lineWidth + 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (modulationInfo.sourceIndex > 0 && std::abs(modulationInfo.amount) > 0.0001f)
    {
        const float knobProportion = (float) valueToProportionOfLength(getValue());
        const float targetProportion = juce::jlimit(0.0f, 1.0f, knobProportion + modulationInfo.amount);
        const float modStart = rotaryStartAngle
            + juce::jmin(knobProportion, targetProportion) * (rotaryEndAngle - rotaryStartAngle);
        const float modEnd = rotaryStartAngle
            + juce::jmax(knobProportion, targetProportion) * (rotaryEndAngle - rotaryStartAngle);

        juce::Path modulationArc;
        modulationArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, modStart, modEnd, true);
        g.setColour(reactorui::modulationSourceColour(modulationInfo.sourceIndex).withAlpha(isEnabled() ? 0.95f : 0.4f));
        g.strokePath(modulationArc, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (shouldShowTargetChip())
    {
        const auto chipBounds = getTargetChipBounds();
        const auto chipColour = reactorui::modulationSourceColour(modulationInfo.sourceIndex);

        g.setColour(juce::Colour(0xff0b1117).withAlpha(editingTargetDepth ? 0.98f : 0.94f));
        g.fillRoundedRectangle(chipBounds, 8.0f);
        g.setColour(chipColour.withAlpha(targetHovered || editingTargetDepth ? 0.96f : 0.74f));
        g.drawRoundedRectangle(chipBounds, 8.0f, editingTargetDepth ? 1.5f : 1.0f);

        auto textBounds = chipBounds.getSmallestIntegerContainer().reduced(8, 0);
        const auto amountPercent = juce::roundToInt(std::abs(modulationInfo.amount) * 100.0f);
        auto amountText = juce::String(amountPercent) + "%";
        if (modulationInfo.amount < 0.0f)
            amountText = "-" + amountText;

        g.setColour(reactorui::textStrong());
        g.setFont(reactorui::sectionFont(8.7f));
        g.drawFittedText("TARGET", textBounds.removeFromLeft(52), juce::Justification::centredLeft, 1);
        g.setFont(reactorui::bodyFont(8.8f));
        g.drawFittedText(amountText, textBounds, juce::Justification::centredRight, 1);
    }
}

void ModulationKnob::refreshModulationState()
{
    if (destination == reactormod::Destination::none)
        return;

    if (processor == nullptr)
        return;

    const auto latestInfo = processor->getDestinationModulationInfo(destination,
                                                                   processor->getSelectedModulationSourceIndex());
    if (latestInfo.sourceIndex != modulationInfo.sourceIndex
        || std::abs(latestInfo.amount - modulationInfo.amount) > 0.0001f
        || latestInfo.slotIndex != modulationInfo.slotIndex
        || latestInfo.routeCount != modulationInfo.routeCount)
    {
        modulationInfo = latestInfo;
        repaint();
    }
}

void ModulationKnob::timerCallback()
{
    refreshModulationState();

    if (valueLabel != nullptr
        && valueLabel->isVisible()
        && ! isMouseButtonDown()
        && juce::Time::getMillisecondCounter() >= valueLabelHideTimeMs)
    {
        setValueLabelVisible(false);
    }
}

bool ModulationKnob::isModulationSourceDragDescription(const juce::var& description) const
{
    return reactormod::isModulationSourceDragDescription(description.toString());
}

int ModulationKnob::sourceIndexFromDescription(const juce::var& description) const
{
    return reactormod::sourceIndexFromDragDescription(description.toString());
}

juce::Rectangle<float> ModulationKnob::getKnobBounds() const
{
    auto layout = getLookAndFeel().getSliderLayout(const_cast<ModulationKnob&>(*this));
    auto available = layout.sliderBounds.toFloat().reduced(1.0f);
    const float squareSize = juce::jmin(available.getWidth(), available.getHeight()) * 0.98f;
    return juce::Rectangle<float>(squareSize, squareSize).withCentre(available.getCentre()).reduced(squareSize * 0.04f);
}

juce::Rectangle<float> ModulationKnob::getTargetChipBounds() const
{
    const auto knobBounds = getKnobBounds();
    const float width = juce::jlimit(72.0f, 90.0f, knobBounds.getWidth() * 0.95f);
    const float height = 18.0f;
    return juce::Rectangle<float>(width, height).withCentre({ knobBounds.getCentreX(), knobBounds.getY() + 10.0f });
}

bool ModulationKnob::shouldShowTargetChip() const
{
    return modulationInfo.sourceIndex > 0
        && modulationInfo.slotIndex >= 0
        && (editingTargetDepth || isMouseOver(true));
}

void ModulationKnob::updateTargetHover(const juce::Point<float>& position)
{
    const bool hoveringChip = shouldShowTargetChip() && getTargetChipBounds().contains(position);
    if (hoveringChip != targetHovered)
    {
        targetHovered = hoveringChip;
        repaint();
    }

    setMouseCursor((hoveringChip || editingTargetDepth) ? juce::MouseCursor::LeftRightResizeCursor
                                                        : juce::MouseCursor::PointingHandCursor);
}

void ModulationKnob::applyTargetAmount(float amount)
{
    if (modulationInfo.slotIndex < 0)
        return;

    if (processor == nullptr)
        return;

    processor->setMatrixAmountForSlot(modulationInfo.slotIndex, amount);
    refreshModulationState();
}

void ModulationKnob::initialiseValueLabel()
{
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        if (auto* label = dynamic_cast<juce::Label*>(getChildComponent(i)))
        {
            valueLabel = label;
            break;
        }
    }

    setValueLabelVisible(false);
}

void ModulationKnob::setValueLabelVisible(bool shouldBeVisible)
{
    if (valueLabel == nullptr)
        return;

    valueLabel->setVisible(shouldBeVisible);
    if (shouldBeVisible)
    {
        valueLabel->toFront(false);
        valueLabelHideTimeMs = juce::Time::getMillisecondCounter() + 900;
    }
}

void ModulationKnob::showValueLabelTemporarily(int durationMs)
{
    if (valueLabel == nullptr)
        return;

    valueLabelHideTimeMs = juce::Time::getMillisecondCounter() + (uint32_t) juce::jmax(100, durationMs);
    setValueLabelVisible(true);
}
