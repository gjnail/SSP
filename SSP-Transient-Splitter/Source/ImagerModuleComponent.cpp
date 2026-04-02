#include "ImagerModuleComponent.h"

#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
constexpr int imagerBandCount = 4;
constexpr int imagerHandleCount = 3;
constexpr float minCrossoverGap = 0.07f;

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
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.45f));
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

float normalToFrequency(float value)
{
    constexpr float minFrequency = 40.0f;
    constexpr float maxFrequency = 16000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

juce::String formatFrequency(float value)
{
    const float hz = normalToFrequency(value);
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + " kHz"
                         : juce::String(juce::roundToInt(hz)) + " Hz";
}

float normalToWidthPercent(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.0f, 200.0f);
}
}

ImagerModuleComponent::ImagerModuleComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state, int slot)
    : processor(p),
      apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffa7b8ff);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);

    titleLabel.setText("IMAGER", juce::dontSendNotification);
    subtitleLabel.setText("Drag the crossover handles to split four bands, then widen or narrow each band while the stereo scope shows the post-image field.", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleToggle(onButton, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    const std::array<juce::String, 4> names{ "Low", "Low Mid", "High Mid", "High" };
    for (int i = 0; i < imagerBandCount; ++i)
    {
        addAndMakeVisible(bandLabels[(size_t) i]);
        addAndMakeVisible(widthKnobs[(size_t) i]);
        bandLabels[(size_t) i].setText(names[(size_t) i], juce::dontSendNotification);
        styleLabel(bandLabels[(size_t) i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, i);
        widthKnobs[(size_t) i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        widthKnobs[(size_t) i].textFromValueFunction = [] (double value)
        {
            return juce::String(juce::roundToInt(normalToWidthPercent((float) value))) + "%";
        };
        widthAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                               parameterID,
                                                                                                               widthKnobs[(size_t) i]);
    }

    startTimerHz(30);
}

int ImagerModuleComponent::getPreferredHeight() const
{
    return 452;
}

float ImagerModuleComponent::getParamValue(int parameterIndex) const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, parameterIndex)))
        return raw->load();

    return 0.5f;
}

void ImagerModuleComponent::setParamValue(int parameterIndex, float value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(getFXSlotFloatParamID(slotIndex, parameterIndex))))
        ranged->setValueNotifyingHost(ranged->convertTo0to1(juce::jlimit(0.0f, 1.0f, value)));
}

std::array<float, 3> ImagerModuleComponent::getOrderedCrossovers() const
{
    std::array<float, 3> crossovers{
        juce::jlimit(0.06f, 0.92f, getParamValue(4)),
        juce::jlimit(0.12f, 0.96f, getParamValue(5)),
        juce::jlimit(0.18f, 0.98f, getParamValue(6))
    };

    crossovers[0] = juce::jlimit(0.06f, 0.92f - minCrossoverGap * 2.0f, crossovers[0]);
    crossovers[1] = juce::jlimit(crossovers[0] + minCrossoverGap, 0.96f - minCrossoverGap, crossovers[1]);
    crossovers[2] = juce::jlimit(crossovers[1] + minCrossoverGap, 0.98f, crossovers[2]);
    return crossovers;
}

void ImagerModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffa7b8ff);
    const auto leftColour = juce::Colour(0xff79d7ff);
    const auto rightColour = juce::Colour(0xffffb07d);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    reactorui::drawDisplayPanel(g, graphBounds.toFloat(), accent);
    reactorui::drawSubtleGrid(g, graphBounds.reduced(1).toFloat(), accent.withAlpha(0.10f), 4, 4);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.72f));
    }

    if (graphBounds.isEmpty())
        return;

    const auto crossovers = getOrderedCrossovers();
    auto scopeArea = graphBounds.reduced(18, 14).toFloat();
    const float centreX = scopeArea.getCentreX();
    const float handleY = scopeArea.getBottom() - 12.0f;

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(scopeArea.toNearestInt());

        const std::array<juce::Colour, 4> bandShades{
            accent.withAlpha(0.035f),
            leftColour.withAlpha(0.030f),
            rightColour.withAlpha(0.028f),
            accent.withAlpha(0.040f)
        };

        float previousX = scopeArea.getX();
        for (int i = 0; i <= imagerHandleCount; ++i)
        {
            const float splitX = i < imagerHandleCount
                               ? scopeArea.getX() + scopeArea.getWidth() * crossovers[(size_t) i]
                               : scopeArea.getRight();
            auto bandArea = juce::Rectangle<float>(previousX, scopeArea.getY(), splitX - previousX, scopeArea.getHeight());
            g.setColour(bandShades[(size_t) i]);
            g.fillRect(bandArea);
            previousX = splitX;
        }

        g.setColour(reactorui::textMuted().withAlpha(0.22f));
        g.drawVerticalLine((int) std::round(centreX), scopeArea.getY() + 12.0f, scopeArea.getBottom() - 18.0f);
        g.drawHorizontalLine((int) std::round(scopeArea.getBottom() - 18.0f), scopeArea.getX(), scopeArea.getRight());

        std::array<float, PluginProcessor::fxImagerScopePointCount> xs{};
        std::array<float, PluginProcessor::fxImagerScopePointCount> ys{};
        processor.getFXImagerScope(slotIndex, xs, ys);

        constexpr int scopeBins = 44;
        std::array<float, scopeBins> leftSpread{};
        std::array<float, scopeBins> rightSpread{};
        std::array<float, scopeBins> balance{};
        float overallEnergy = 0.0f;

        for (int i = 0; i < PluginProcessor::fxImagerScopePointCount; ++i)
        {
            const float side = juce::jlimit(-1.0f, 1.0f, xs[(size_t) i]);
            const float mid = juce::jlimit(-1.0f, 1.0f, ys[(size_t) i]);
            const float heightNorm = juce::jlimit(0.0f, 1.0f,
                                                  std::sqrt(std::abs(mid)) * 0.82f
                                                  + std::sqrt(std::abs(side)) * 0.22f);
            const int bin = juce::jlimit(0, scopeBins - 1, juce::roundToInt(heightNorm * (float) (scopeBins - 1)));
            const float spread = std::pow(std::abs(side), 0.78f);

            if (side < 0.0f)
                leftSpread[(size_t) bin] = juce::jmax(leftSpread[(size_t) bin], spread);
            else
                rightSpread[(size_t) bin] = juce::jmax(rightSpread[(size_t) bin], spread);

            balance[(size_t) bin] += side * 0.35f;
            overallEnergy = juce::jmax(overallEnergy, heightNorm);
        }

        for (int pass = 0; pass < 2; ++pass)
        {
            auto previousLeft = leftSpread;
            auto previousRight = rightSpread;
            auto previousBalance = balance;

            for (int i = 1; i < scopeBins - 1; ++i)
            {
                leftSpread[(size_t) i] = juce::jmax(previousLeft[(size_t) i] * 0.72f,
                                                    (previousLeft[(size_t) i - 1] + previousLeft[(size_t) i] + previousLeft[(size_t) i + 1]) / 3.0f);
                rightSpread[(size_t) i] = juce::jmax(previousRight[(size_t) i] * 0.72f,
                                                     (previousRight[(size_t) i - 1] + previousRight[(size_t) i] + previousRight[(size_t) i + 1]) / 3.0f);
                balance[(size_t) i] = (previousBalance[(size_t) i - 1] + previousBalance[(size_t) i] + previousBalance[(size_t) i + 1]) / 3.0f;
            }
        }

        const float baseY = scopeArea.getBottom() - 18.0f;
        const float topY = scopeArea.getY() + 18.0f;
        const float maxSpreadWidth = scopeArea.getWidth() * 0.36f;
        juce::Path leftEdge;
        juce::Path rightEdge;
        juce::Path centreLine;
        juce::Path fillPath;

        leftEdge.startNewSubPath(centreX, baseY);
        rightEdge.startNewSubPath(centreX, baseY);
        centreLine.startNewSubPath(centreX, baseY);
        fillPath.startNewSubPath(centreX, baseY);

        for (int i = 0; i < scopeBins; ++i)
        {
            const float t = (float) i / (float) juce::jmax(1, scopeBins - 1);
            const float y = juce::jmap(t, baseY, topY);
            const float leftX = centreX - leftSpread[(size_t) i] * maxSpreadWidth;
            const float rightX = centreX + rightSpread[(size_t) i] * maxSpreadWidth;
            const float spineX = centreX + juce::jlimit(-1.0f, 1.0f, balance[(size_t) i]) * maxSpreadWidth * 0.20f;

            leftEdge.lineTo(leftX, y);
            rightEdge.lineTo(rightX, y);
            centreLine.lineTo(spineX, y);
            fillPath.lineTo(leftX, y);
        }

        for (int i = scopeBins - 1; i >= 0; --i)
        {
            const float t = (float) i / (float) juce::jmax(1, scopeBins - 1);
            const float y = juce::jmap(t, baseY, topY);
            const float rightX = centreX + rightSpread[(size_t) i] * maxSpreadWidth;
            fillPath.lineTo(rightX, y);
        }

        fillPath.closeSubPath();

        juce::ColourGradient fill(leftColour.withAlpha(0.15f), centreX - maxSpreadWidth, baseY,
                                  rightColour.withAlpha(0.15f), centreX + maxSpreadWidth, baseY, false);
        fill.addColour(0.50, accent.withAlpha(0.20f + overallEnergy * 0.10f));
        g.setGradientFill(fill);
        g.fillPath(fillPath);

        g.setColour(accent.withAlpha(0.10f + overallEnergy * 0.10f));
        g.strokePath(centreLine, juce::PathStrokeType(12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accent.withAlpha(0.24f + overallEnergy * 0.16f));
        g.strokePath(centreLine, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(leftColour.withAlpha(0.18f + overallEnergy * 0.12f));
        g.strokePath(leftEdge, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(rightColour.withAlpha(0.18f + overallEnergy * 0.12f));
        g.strokePath(rightEdge, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(leftColour.withAlpha(0.88f));
        g.strokePath(leftEdge, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(rightColour.withAlpha(0.88f));
        g.strokePath(rightEdge, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accent.brighter(0.18f).withAlpha(0.96f));
        g.strokePath(centreLine, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const std::array<juce::String, 4> names{ "Low", "Low Mid", "High Mid", "High" };
    float previousX = scopeArea.getX();
    for (int i = 0; i <= imagerHandleCount; ++i)
    {
        const float splitX = i < imagerHandleCount
                           ? scopeArea.getX() + scopeArea.getWidth() * crossovers[(size_t) i]
                           : scopeArea.getRight();
        auto bandArea = juce::Rectangle<float>(previousX, scopeArea.getY(), splitX - previousX, 18.0f);
        g.setColour(reactorui::textStrong().withAlpha(0.84f));
        g.setFont(reactorui::bodyFont(10.5f));
        g.drawFittedText(names[(size_t) i], bandArea.toNearestInt(), juce::Justification::centred, 1);
        previousX = splitX;
    }

    for (int i = 0; i < imagerHandleCount; ++i)
    {
        const float x = scopeArea.getX() + scopeArea.getWidth() * crossovers[(size_t) i];
        g.setColour(accent.withAlpha(0.42f));
        g.drawVerticalLine((int) std::round(x), scopeArea.getY() + 18.0f, handleY - 12.0f);

        juce::Path handle;
        handle.addTriangle(x - 7.0f, handleY - 1.0f, x + 7.0f, handleY - 1.0f, x, handleY + 9.0f);
        g.setColour(juce::Colour(0xff0f141a));
        g.fillPath(handle);
        g.setColour(accent.brighter(0.18f));
        g.strokePath(handle, juce::PathStrokeType(1.2f));

        auto textBounds = juce::Rectangle<float>(x - 34.0f, handleY - 20.0f, 68.0f, 14.0f);
        g.setColour(reactorui::textMuted().withAlpha(0.92f));
        g.setFont(reactorui::bodyFont(9.8f));
        g.drawFittedText(formatFrequency(crossovers[(size_t) i]), textBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void ImagerModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(180));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    graphBounds = area.removeFromTop(214);
    area.removeFromTop(12);

    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int gap = 12;
    const int cellWidth = (controlArea.getWidth() - gap * 3) / 4;
    for (int i = 0; i < imagerBandCount; ++i)
    {
        auto cell = controlArea.removeFromLeft(cellWidth);
        if (i < imagerBandCount - 1)
            controlArea.removeFromLeft(gap);

        knobCellBounds[(size_t) i] = cell;
        auto content = cell.reduced(2, 4);
        bandLabels[(size_t) i].setBounds(content.removeFromTop(18));
        widthKnobs[(size_t) i].setBounds(content);
    }
}

int ImagerModuleComponent::findHandleAt(juce::Point<float> position) const
{
    for (int i = 0; i < imagerHandleCount; ++i)
    {
        if (position.getDistanceFrom(getHandlePosition(i)) <= 16.0f)
            return i;
    }

    return -1;
}

juce::Point<float> ImagerModuleComponent::getHandlePosition(int handleIndex) const
{
    if (graphBounds.isEmpty())
        return {};

    const auto crossovers = getOrderedCrossovers();
    const auto scopeArea = graphBounds.reduced(18, 14).toFloat();
    return { scopeArea.getX() + scopeArea.getWidth() * crossovers[(size_t) handleIndex], scopeArea.getBottom() - 12.0f };
}

void ImagerModuleComponent::updateHandleFromPosition(int handleIndex, juce::Point<float> position)
{
    if (graphBounds.isEmpty() || handleIndex < 0 || handleIndex >= imagerHandleCount)
        return;

    const auto scopeArea = graphBounds.reduced(18, 14).toFloat();
    auto crossovers = getOrderedCrossovers();
    float value = juce::jlimit(0.0f, 1.0f, (position.x - scopeArea.getX()) / juce::jmax(1.0f, scopeArea.getWidth()));

    if (handleIndex == 0)
        value = juce::jlimit(0.06f, crossovers[1] - minCrossoverGap, value);
    else if (handleIndex == 1)
        value = juce::jlimit(crossovers[0] + minCrossoverGap, crossovers[2] - minCrossoverGap, value);
    else
        value = juce::jlimit(crossovers[1] + minCrossoverGap, 0.98f, value);

    setParamValue(4 + handleIndex, value);
}

void ImagerModuleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! onButton.getToggleState())
        return;

    draggingHandle = findHandleAt(event.position);
}

void ImagerModuleComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingHandle >= 0)
    {
        updateHandleFromPosition(draggingHandle, event.position);
        repaint();
    }
}

void ImagerModuleComponent::mouseUp(const juce::MouseEvent&)
{
    draggingHandle = -1;
}

void ImagerModuleComponent::timerCallback()
{
    const bool enabled = onButton.getToggleState();
    for (auto& knob : widthKnobs)
        knob.setEnabled(enabled);

    repaint();
}
