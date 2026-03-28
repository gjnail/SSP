#include "NoiseSectionComponent.h"
#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
juce::Rectangle<float> getNoisePreviewBounds(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(12, 10).toFloat();
    area.removeFromTop(22.0f);
    return area.removeFromTop(34.0f);
}

void styleNoiseLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleNoiseCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10161d));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffd06c));
}

void styleNoiseToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffd06c));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
    button.setClickingTogglesState(true);
}
}

NoiseSectionComponent::NoiseSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    : amountSlider(processor, "noiseAmount", reactormod::Destination::noiseAmount, juce::Colour(0xffffd06c), 64, 22)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(amountLabel);
    addAndMakeVisible(typeBox);
    addAndMakeVisible(amountSlider);
    addAndMakeVisible(toFilterButton);
    addAndMakeVisible(toAmpButton);

    titleLabel.setText("NOISE", juce::dontSendNotification);
    typeLabel.setText("Type", juce::dontSendNotification);
    amountLabel.setText("Amount", juce::dontSendNotification);
    toFilterButton.setButtonText("To Filter");
    toAmpButton.setButtonText("Use Amp Envelope");

    styleNoiseLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleNoiseLabel(typeLabel, 11.5f, reactorui::textMuted());
    styleNoiseLabel(amountLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    typeBox.addItemList(PluginProcessor::getNoiseTypeNames(), 1);
    styleNoiseCombo(typeBox);
    styleNoiseToggle(toFilterButton);
    styleNoiseToggle(toAmpButton);

    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "noiseType", typeBox);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noiseAmount", amountSlider);
    toFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "noiseToFilter", toFilterButton);
    toAmpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "noiseToAmpEnv", toAmpButton);

    startTimerHz(18);
}

void NoiseSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffffcb68));

    auto preview = getNoisePreviewBounds(getLocalBounds());
    reactorui::drawDisplayPanel(g, preview, juce::Colour(0xffffcb68));
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), juce::Colour(0xffffcb68).withAlpha(0.10f), 4, 2);

    juce::Path previewStroke(previewPath);
    auto transform = previewStroke.getTransformToScaleToFit(preview.reduced(8.0f, 6.0f), true);
    previewStroke.applyTransform(transform);
    g.setColour(juce::Colour(0xffffdf91).withAlpha(0.16f));
    g.strokePath(previewStroke, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xffffd06c));
    g.strokePath(previewStroke, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void NoiseSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    titleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    auto previewArea = area.removeFromTop(34);
    juce::ignoreUnused(previewArea);
    area.removeFromTop(6);

    auto typeRow = area.removeFromTop(22);
    typeLabel.setBounds(typeRow.removeFromLeft(34));
    typeRow.removeFromLeft(6);
    typeBox.setBounds(typeRow);

    area.removeFromTop(4);
    auto toggleRow = area.removeFromTop(22);
    toFilterButton.setBounds(toggleRow.removeFromLeft((toggleRow.getWidth() - 8) / 2));
    toggleRow.removeFromLeft(8);
    toAmpButton.setBounds(toggleRow);

    area.removeFromTop(4);
    amountLabel.setBounds(area.removeFromTop(14));
    amountSlider.setBounds(area);
}

void NoiseSectionComponent::timerCallback()
{
    const int typeIndex = juce::jmax(0, typeBox.getSelectedItemIndex());
    previewPath.clear();

    float y = 36.0f;
    float held = 0.0f;
    float lastWhite = 0.0f;
    float previousWhite = 0.0f;
    float crackle = 0.0f;
    for (int x = 0; x < 180; ++x)
    {
        float sample = 0.0f;
        const float white = previewRandom.nextFloat() * 2.0f - 1.0f;
        const float blue = juce::jlimit(-1.0f, 1.0f, white - lastWhite);
        const float violet = juce::jlimit(-1.0f, 1.0f, white - 2.0f * lastWhite + previousWhite);
        previousWhite = lastWhite;
        lastWhite = white;

        switch (typeIndex)
        {
            case 1: sample = white * 0.55f + std::sin((float) x * 0.19f) * 0.10f; break;
            case 2: y = juce::jlimit(8.0f, 64.0f, y + white * 2.0f); sample = 1.0f - (y / 36.0f); break;
            case 3:
                if ((x % 7) == 0)
                    held = white;
                sample = held;
                break;
            case 4: sample = previewRandom.nextFloat() < 0.06f ? white * 1.4f : 0.0f; break;
            case 5: sample = white * 0.30f + std::sin((float) x * 0.34f) * 0.26f; break;
            case 6: sample = white * 0.20f + std::sin((float) x * 0.07f) * 0.55f; break;
            case 7: sample = blue * 1.15f; break;
            case 8: sample = violet * 1.25f; break;
            case 9:
                crackle *= 0.82f;
                if (previewRandom.nextFloat() < 0.05f)
                    crackle = white * 1.6f;
                sample = crackle;
                break;
            case 10:
                y = juce::jlimit(10.0f, 62.0f, y + white * 1.0f);
                sample = (1.0f - (y / 36.0f)) * 0.68f + std::sin((float) x * 0.045f) * 0.24f;
                break;
            case 11:
                sample = (previewRandom.nextFloat() < 0.35f ? held : white);
                held = white;
                sample = sample * 0.70f + std::sin((float) x * 0.48f) * 0.20f + blue * 0.12f;
                break;
            default: sample = white; break;
        }

        if (typeIndex != 2 && typeIndex != 10)
            y = 36.0f - sample * 20.0f;

        if (x == 0)
            previewPath.startNewSubPath((float) x, y);
        else
            previewPath.lineTo((float) x, y);
    }

    repaint();
}
