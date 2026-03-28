#include "WavetableOscillatorComponent.h"
#include "PreviewWarpUtils.h"
#include "ReactorUI.h"
#include "WavetableLibrary.h"

namespace
{
void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111821));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff8ff3c8));
}

}

WavetableOscillatorComponent::WavetableOscillatorComponent(PluginProcessor& processor,
                                                           juce::AudioProcessorValueTreeState& state,
                                                           int oscillatorIndex)
    : oscIndex(oscillatorIndex),
      positionSlider(processor,
                     "osc" + juce::String(oscillatorIndex) + "WTPos",
                     oscIndex == 1 ? reactormod::Destination::osc1WTPos
                                   : oscIndex == 2 ? reactormod::Destination::osc2WTPos
                                                   : reactormod::Destination::osc3WTPos,
                     juce::Colour(0xff70e7b1),
                     72, 22),
      octaveSlider(processor,
                   "osc" + juce::String(oscillatorIndex) + "Octave",
                   reactormod::Destination::none,
                   juce::Colour(0xff70e7b1),
                   96, 24)
{
    addAndMakeVisible(tableLabel);
    addAndMakeVisible(positionLabel);
    addAndMakeVisible(octaveLabel);
    addAndMakeVisible(tableBox);
    addAndMakeVisible(positionSlider);
    addAndMakeVisible(octaveSlider);

    tableLabel.setText("Table", juce::dontSendNotification);
    positionLabel.setText("WT Pos", juce::dontSendNotification);
    octaveLabel.setText("Octave", juce::dontSendNotification);
    styleLabel(tableLabel, 11.5f, reactorui::textMuted());
    styleLabel(positionLabel, 11.5f, reactorui::textMuted());
    styleLabel(octaveLabel, 11.5f, reactorui::textMuted());
    styleCombo(tableBox);

    tableBox.addItemList(PluginProcessor::getWavetableNames(), 1);

    positionSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
    };
    positionSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, (double) text.retainCharacters("0123456789.").getDoubleValue() / 100.0);
    };
    octaveSlider.setRange(0.0, (double) (PluginProcessor::getOctaveNames().size() - 1), 1.0);
    octaveSlider.setNumDecimalPlacesToDisplay(0);
    octaveSlider.textFromValueFunction = [] (double value)
    {
        const auto index = juce::jlimit(0, PluginProcessor::getOctaveNames().size() - 1, juce::roundToInt((float) value));
        return PluginProcessor::getOctaveNames()[index];
    };
    octaveSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto names = PluginProcessor::getOctaveNames();
        for (int i = 0; i < names.size(); ++i)
            if (text.trim().equalsIgnoreCase(names[i]))
                return (double) i;

        return juce::jlimit(0.0, (double) (names.size() - 1),
                            (double) text.retainCharacters("0123456789-+").getIntValue());
    };

    const auto prefix = "osc" + juce::String(oscIndex);
    tableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, prefix + "Wavetable", tableBox);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Octave", octaveSlider);
    positionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "WTPos", positionSlider);
    tableParam = state.getRawParameterValue(prefix + "Wavetable");
    positionParam = state.getRawParameterValue(prefix + "WTPos");
    mutateParam = state.getRawParameterValue("warpMutate");
    fmSourceParam = state.getRawParameterValue(prefix + "WarpFMSource");
    warpModeParams[0] = state.getRawParameterValue(prefix + "Warp1Mode");
    warpModeParams[1] = state.getRawParameterValue(prefix + "Warp2Mode");
    warpAmountParams[0] = state.getRawParameterValue(prefix + "Warp1Amount");
    warpAmountParams[1] = state.getRawParameterValue(prefix + "Warp2Amount");
    warpFMParam = state.getRawParameterValue(prefix + "WarpFM");
    warpSyncParam = state.getRawParameterValue(prefix + "WarpSync");
    warpBendParam = state.getRawParameterValue(prefix + "WarpBend");

    startTimerHz(20);
}

void WavetableOscillatorComponent::paint(juce::Graphics& g)
{
    const auto preview = getPreviewBounds().toFloat();
    const auto accent = juce::Colour(0xff78efb8);

    reactorui::drawDisplayPanel(g, preview, accent);
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accent.withAlpha(0.10f), 6, 2);

    const int tableIndex = tableParam != nullptr ? juce::roundToInt(tableParam->load()) : 0;
    const float position = positionParam != nullptr ? positionParam->load() : 0.0f;
    const float mutateAmount = mutateParam != nullptr ? mutateParam->load() : 0.0f;
    const int fmSourceIndex = fmSourceParam != nullptr ? juce::roundToInt(fmSourceParam->load()) : 0;
    const float legacyFM = warpFMParam != nullptr ? warpFMParam->load() : 0.0f;
    const float legacySync = warpSyncParam != nullptr ? warpSyncParam->load() : 0.0f;
    const float legacyBend = warpBendParam != nullptr ? warpBendParam->load() : 0.0f;
    const float centreY = preview.getCentreY();
    const float amplitude = preview.getHeight() * 0.40f;
    const int width = juce::jmax(1, (int) preview.getWidth());

    juce::Path path;
    for (int x = 0; x < width; ++x)
    {
        const float phase = (float) x / (float) juce::jmax(1, width - 1);
        float warpedPhase = previewwarp::applyLegacyWarp(phase, legacyFM, legacySync, legacyBend, fmSourceIndex, mutateAmount);
        for (int slot = 0; slot < 2; ++slot)
        {
            const int modeIndex = warpModeParams[(size_t) slot] != nullptr ? juce::roundToInt(warpModeParams[(size_t) slot]->load()) : 0;
            const float amount = warpAmountParams[(size_t) slot] != nullptr ? warpAmountParams[(size_t) slot]->load() : 0.0f;
            warpedPhase = previewwarp::applyWarpMode(warpedPhase, modeIndex, amount, fmSourceIndex, mutateAmount);
        }

        float sample = wavetable::renderSample(tableIndex, position, previewwarp::wrapPhase(warpedPhase));
        if (mutateAmount > 0.0001f)
        {
            const float harmonicPhase = previewwarp::wrapPhase(warpedPhase + 0.11f * mutateAmount
                * std::sin(juce::MathConstants<float>::twoPi * warpedPhase * (2.0f + (float) oscIndex)));
            const float harmonic = wavetable::renderSample(tableIndex, position, harmonicPhase);
            const float folded = std::sin((sample + harmonic * 0.45f) * (1.0f + mutateAmount * 4.5f));
            sample = juce::jlimit(-1.0f, 1.0f, juce::jmap(mutateAmount, sample, folded));
        }
        const float drawX = preview.getX() + (float) x;
        const float drawY = centreY - sample * amplitude;
        if (x == 0)
            path.startNewSubPath(drawX, drawY);
        else
            path.lineTo(drawX, drawY);
    }

    g.setColour(accent.withAlpha(0.14f));
    g.fillRoundedRectangle(preview.reduced(1.0f), 10.0f);
    g.setColour(accent);
    g.strokePath(path, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WavetableOscillatorComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(96);

    auto tableRow = area.removeFromTop(42);
    tableLabel.setBounds(tableRow.removeFromTop(16));
    tableBox.setBounds(tableRow.removeFromTop(28));

    area.removeFromTop(6);
    auto lowerRow = area;
    auto left = lowerRow.removeFromLeft((lowerRow.getWidth() - 14) / 2);
    lowerRow.removeFromLeft(14);
    auto right = lowerRow;

    positionLabel.setBounds(left.removeFromTop(16));
    left.removeFromTop(4);
    positionSlider.setBounds(left.reduced(0, 2));
    octaveLabel.setBounds(right.removeFromTop(16));
    right.removeFromTop(4);
    octaveSlider.setBounds(right.reduced(0, 2));
}

void WavetableOscillatorComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<int> WavetableOscillatorComponent::getPreviewBounds() const
{
    return getLocalBounds().removeFromTop(88);
}
