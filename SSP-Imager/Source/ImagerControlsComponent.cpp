#include "ImagerControlsComponent.h"

namespace
{
const juce::String bandNames[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
} // namespace

ImagerControlsComponent::ImagerControlsComponent(PluginProcessor& p)
    : processor(p)
{
    for (int i = 0; i < PluginProcessor::numBands; ++i)
    {
        auto& band = bands[i];
        auto si = juce::String(i + 1);
        auto colour = juce::Colour(bandColourValues[i]);

        // Width knob
        band.widthKnob.setRange(0.0, 200.0, 0.1);
        band.widthKnob.setTextValueSuffix("%");
        band.widthKnob.setDoubleClickReturnValue(true, 100.0);
        band.widthKnob.setColour(juce::Slider::rotarySliderFillColourId, colour);
        band.widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "band" + si + "Width", band.widthKnob);
        addAndMakeVisible(band.widthKnob);

        // Pan knob
        band.panKnob.setRange(-100.0, 100.0, 0.1);
        band.panKnob.setDoubleClickReturnValue(true, 0.0);
        band.panKnob.setColour(juce::Slider::rotarySliderFillColourId, colour.withAlpha(0.7f));
        band.panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "band" + si + "Pan", band.panKnob);
        addAndMakeVisible(band.panKnob);

        // Solo button
        band.soloButton.setClickingTogglesState(true);
        band.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "band" + si + "Solo", band.soloButton);
        addAndMakeVisible(band.soloButton);

        // Mute button
        band.muteButton.setClickingTogglesState(true);
        band.muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "band" + si + "Mute", band.muteButton);
        addAndMakeVisible(band.muteButton);

        // Name label
        band.nameLabel.setText(bandNames[i], juce::dontSendNotification);
        band.nameLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        band.nameLabel.setJustificationType(juce::Justification::centred);
        band.nameLabel.setColour(juce::Label::textColourId, colour);
        addAndMakeVisible(band.nameLabel);

        // Range label
        band.rangeLabel.setFont(9.5f);
        band.rangeLabel.setJustificationType(juce::Justification::centred);
        band.rangeLabel.setColour(juce::Label::textColourId, ssp::ui::textMuted());
        addAndMakeVisible(band.rangeLabel);
    }
}

void ImagerControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const int bandWidth = getWidth() / PluginProcessor::numBands;

    for (int i = 0; i < PluginProcessor::numBands; ++i)
    {
        auto bandBounds = juce::Rectangle<float>((float) (i * bandWidth) + 4.0f, bounds.getY(),
                                                  (float) bandWidth - 8.0f, bounds.getHeight());
        ssp::ui::drawPanelBackground(g, bandBounds, juce::Colour(bandColourValues[i]));
    }
}

void ImagerControlsComponent::resized()
{
    const int bandWidth = getWidth() / PluginProcessor::numBands;
    const int knobSize = 80;
    const int smallKnobSize = 64;

    for (int i = 0; i < PluginProcessor::numBands; ++i)
    {
        auto& band = bands[i];
        auto area = juce::Rectangle<int>(i * bandWidth + 4, 0, bandWidth - 8, getHeight());
        area = area.reduced(8, 8);

        auto header = area.removeFromTop(28);
        band.nameLabel.setBounds(header);

        area.removeFromTop(4);

        // Range label
        auto rangeArea = area.removeFromTop(14);
        band.rangeLabel.setBounds(rangeArea);

        // Update range text from crossover frequencies
        juce::String rangeText;
        if (i == 0)
            rangeText = "20 - " + juce::String((int) processor.getCrossoverFrequency(0)) + " Hz";
        else if (i == 1)
            rangeText = juce::String((int) processor.getCrossoverFrequency(0)) + " - " + juce::String((int) processor.getCrossoverFrequency(1)) + " Hz";
        else if (i == 2)
            rangeText = juce::String((int) processor.getCrossoverFrequency(1)) + " - " + juce::String((int) processor.getCrossoverFrequency(2)) + " Hz";
        else
            rangeText = juce::String((int) processor.getCrossoverFrequency(2)) + " - 20k Hz";
        band.rangeLabel.setText(rangeText, juce::dontSendNotification);

        area.removeFromTop(6);

        // Width knob (larger)
        auto widthArea = area.removeFromTop(knobSize + 20);
        band.widthKnob.setBounds(widthArea.withSizeKeepingCentre(knobSize, knobSize + 20));

        area.removeFromTop(2);

        // "Width" label is built into the knob textbox, so just a small gap

        // Pan knob (smaller)
        auto panArea = area.removeFromTop(smallKnobSize + 20);
        band.panKnob.setBounds(panArea.withSizeKeepingCentre(smallKnobSize, smallKnobSize + 20));

        area.removeFromTop(6);

        // Solo/Mute buttons
        auto buttonRow = area.removeFromTop(28);
        auto soloArea = buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(4, 0);
        auto muteArea = buttonRow.reduced(4, 0);
        band.soloButton.setBounds(soloArea);
        band.muteButton.setBounds(muteArea);
    }
}
