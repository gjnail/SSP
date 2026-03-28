#include "TremoloModuleComponent.h"

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

float normalToRateHz(float value)
{
    constexpr float minRate = 0.05f;
    constexpr float maxRate = 8.0f;
    return minRate * std::pow(maxRate / minRate, juce::jlimit(0.0f, 1.0f, value));
}

float evaluateTremoloShape(float phase, float shape)
{
    const float sine = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * phase);
    const float driven = 0.5f + 0.5f * std::tanh(std::sin(juce::MathConstants<float>::twoPi * phase) * (1.0f + shape * 10.0f));
    return juce::jmap(shape, sine, driven);
}

struct TremoloSyncDivision
{
    const char* name;
    float beats = 1.0f;
};

const std::array<TremoloSyncDivision, 13>& getTremoloSyncDivisions()
{
    static const std::array<TremoloSyncDivision, 13> divisions{{
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
        { "3 Bars", 12.0f },
        { "4 Bars", 16.0f }
    }};
    return divisions;
}

int normalToSyncDivisionIndex(float value)
{
    const auto maxIndex = (int) getTremoloSyncDivisions().size() - 1;
    return juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
}

float syncDivisionIndexToNormal(int index)
{
    const auto maxIndex = (int) getTremoloSyncDivisions().size() - 1;
    if (maxIndex <= 0)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, (float) juce::jlimit(0, maxIndex, index) / (float) maxIndex);
}

void setChoiceParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1((float) value));
}
}

TremoloModuleComponent::TremoloModuleComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int slot)
    : apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xfff9a56c);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);
    addAndMakeVisible(syncButton);

    titleLabel.setText("TREMOLO", juce::dontSendNotification);
    subtitleLabel.setText("Syncable stereo amplitude modulation with shape and spread control for gentle movement through hard chopper pulses.", juce::dontSendNotification);
    syncButton.setButtonText("Sync");
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleToggle(onButton, accent);
    styleModeButton(syncButton, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);
    syncButton.onClick = [this]
    {
        setSyncEnabled(syncButton.getToggleState());
    };

    const std::array<juce::String, 5> names{ "Rate", "Depth", "Shape", "Stereo", "Mix" };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        addAndMakeVisible(controlLabels[i]);
        addAndMakeVisible(controls[i]);
        controlLabels[i].setText(names[i], juce::dontSendNotification);
        styleLabel(controlLabels[i], 11.0f, reactorui::textMuted(), juce::Justification::centred);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, (int) i);
        controls[i].setupModulation(processor, parameterID, reactormod::Destination::none, accent);
        controlAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                        parameterID,
                                                                                                        controls[i]);
    }

    for (int i = 1; i < 5; ++i)
    {
        controls[(size_t) i].textFromValueFunction = [i] (double value)
        {
        if (i == 3)
                return juce::String(juce::roundToInt((float) value * 180.0f)) + " deg";

            return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
        };
    }

    syncSyncButton();
    updateRateFormatting();
    startTimerHz(24);
}

int TremoloModuleComponent::getPreferredHeight() const
{
    return 430;
}

void TremoloModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xfff9a56c);
    const auto leftColour = accent.brighter(0.18f);
    const auto rightColour = juce::Colour(0xffffd08a);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    reactorui::drawDisplayPanel(g, visualizerBounds.toFloat(), accent);
    reactorui::drawSubtleGrid(g, visualizerBounds.reduced(1).toFloat(), accent.withAlpha(0.10f), 6, 4);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (const auto& cell : knobCellBounds)
    {
        if (! cell.isEmpty())
            drawDeck(g, cell.toFloat(), accent.withAlpha(0.74f));
    }

    if (! visualizerBounds.isEmpty())
    {
        const float depth = (float) controls[1].getValue();
        const float shape = (float) controls[2].getValue();
        const float stereoNorm = (float) controls[3].getValue();
        const float phaseOffset = stereoNorm * 0.5f;
        auto area = visualizerBounds.reduced(16, 14).toFloat();
        const float centreY = area.getCentreY();
        const float amplitude = area.getHeight() * 0.34f * depth;

        g.setColour(reactorui::textMuted().withAlpha(0.24f));
        g.drawHorizontalLine((int) std::round(centreY), area.getX(), area.getRight());

        juce::Path leftPath;
        juce::Path rightPath;
        for (int x = 0; x < area.getWidth(); ++x)
        {
            const float phase = (float) x / juce::jmax(1.0f, area.getWidth() - 1.0f);
            const float leftLfo = evaluateTremoloShape(phase, shape);
            const float rightLfo = evaluateTremoloShape(std::fmod(phase + phaseOffset, 1.0f), shape);
            const float plotX = area.getX() + (float) x;
            const float leftY = centreY + amplitude * (1.0f - 2.0f * leftLfo);
            const float rightY = centreY + amplitude * (1.0f - 2.0f * rightLfo);

            if (x == 0)
            {
                leftPath.startNewSubPath(plotX, leftY);
                rightPath.startNewSubPath(plotX, rightY);
            }
            else
            {
                leftPath.lineTo(plotX, leftY);
                rightPath.lineTo(plotX, rightY);
            }
        }

        g.setColour(leftColour.withAlpha(0.18f));
        g.strokePath(leftPath, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(rightColour.withAlpha(0.15f));
        g.strokePath(rightPath, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(leftColour);
        g.strokePath(leftPath, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(rightColour);
        g.strokePath(rightPath, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto legend = area.removeFromTop(16.0f);
        g.setFont(reactorui::bodyFont(10.0f));
        g.setColour(leftColour.withAlpha(0.92f));
        g.drawFittedText("Left", legend.removeFromLeft(44).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour(rightColour.withAlpha(0.92f));
        g.drawFittedText("Right", legend.removeFromLeft(50).toNearestInt(), juce::Justification::centredLeft, 1);
    }
}

void TremoloModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(160));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    visualizerBounds = area.removeFromTop(106);
    area.removeFromTop(10);

    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int rowGap = 12;
    const int columnGap = 14;
    auto topRow = controlArea.removeFromTop(juce::jmax(102, (int) std::round(controlArea.getHeight() * 0.46f)));
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
        if (i == 0)
        {
            auto rateHeader = content.removeFromTop(24);
            controlLabels[(size_t) i].setBounds(rateHeader.removeFromLeft(56));
            syncButton.setBounds(rateHeader.removeFromRight(60).reduced(0, 1));
        }
        else
        {
            controlLabels[(size_t) i].setBounds(content.removeFromTop(18));
        }
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

void TremoloModuleComponent::setSyncEnabled(bool shouldSync)
{
    setChoiceParameterValue(apvts, getFXSlotVariantParamID(slotIndex), shouldSync ? 1 : 0);
    syncSyncButton();
    updateRateFormatting();
}

bool TremoloModuleComponent::isSyncEnabled() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return raw->load() >= 0.5f;

    return false;
}

void TremoloModuleComponent::syncSyncButton()
{
    const bool syncEnabled = isSyncEnabled();
    if (syncButton.getToggleState() != syncEnabled)
        syncButton.setToggleState(syncEnabled, juce::dontSendNotification);
}

void TremoloModuleComponent::updateRateFormatting()
{
    if (isSyncEnabled())
    {
        controls[0].textFromValueFunction = [] (double value)
        {
            return juce::String(getTremoloSyncDivisions()[(size_t) normalToSyncDivisionIndex((float) value)].name);
        };
        controls[0].valueFromTextFunction = [] (const juce::String& text)
        {
            for (size_t i = 0; i < getTremoloSyncDivisions().size(); ++i)
                if (text.trim().equalsIgnoreCase(getTremoloSyncDivisions()[i].name))
                    return (double) syncDivisionIndexToNormal((int) i);

            return 0.0;
        };
    }
    else
    {
        controls[0].textFromValueFunction = [] (double value)
        {
            const float hz = normalToRateHz((float) value);
            return hz >= 1.0f ? juce::String(hz, hz >= 10.0f ? 1 : 2) + " Hz"
                              : juce::String(hz, 2) + " Hz";
        };
        controls[0].valueFromTextFunction = nullptr;
    }

    controls[0].setValue(controls[0].getValue(), juce::dontSendNotification);
}

void TremoloModuleComponent::timerCallback()
{
    syncSyncButton();
    updateRateFormatting();

    const bool enabled = onButton.getToggleState();
    for (auto& control : controls)
        control.setEnabled(enabled);
    syncButton.setEnabled(enabled);

    repaint();
}
