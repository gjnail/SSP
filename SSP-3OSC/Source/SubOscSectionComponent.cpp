#include "SubOscSectionComponent.h"
#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
juce::Rectangle<float> getSubPreviewBounds(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(12, 10).toFloat();
    area.removeFromTop(22.0f);
    return area.removeFromTop(52.0f);
}

void styleSubLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleSubCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10161d));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff95c7ff));
}

void styleSubToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff95c7ff));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
    button.setClickingTogglesState(true);
}
}

SubOscSectionComponent::SubOscSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    : levelSlider(processor, "subLevel", reactormod::Destination::subLevel, juce::Colour(0xff8bbcff), 74, 22),
      octaveSlider(processor, "subOctave", reactormod::Destination::none, juce::Colour(0xff8bbcff), 96, 24)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(waveLabel);
    addAndMakeVisible(levelLabel);
    addAndMakeVisible(octaveLabel);
    addAndMakeVisible(waveBox);
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(octaveSlider);
    addAndMakeVisible(directOutButton);

    titleLabel.setText("SUB OSC", juce::dontSendNotification);
    waveLabel.setText("Wave", juce::dontSendNotification);
    levelLabel.setText("Level", juce::dontSendNotification);
    octaveLabel.setText("Octave", juce::dontSendNotification);
    directOutButton.setButtonText("Direct Out");

    styleSubLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleSubLabel(waveLabel, 11.5f, reactorui::textMuted());
    styleSubLabel(levelLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleSubLabel(octaveLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleSubCombo(waveBox);
    styleSubToggle(directOutButton);

    waveBox.addItemList(PluginProcessor::getWaveNames(), 1);
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

    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "subWave", waveBox);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "subLevel", levelSlider);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "subOctave", octaveSlider);
    directOutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "subDirectOut", directOutButton);

    waveParam = apvts.getRawParameterValue("subWave");
    levelParam = apvts.getRawParameterValue("subLevel");

    startTimerHz(18);
}

void SubOscSectionComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xff8fbaff);
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent);

    const auto preview = getSubPreviewBounds(getLocalBounds());
    reactorui::drawDisplayPanel(g, preview, accent);
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accent.withAlpha(0.10f), 4, 2);

    const int waveIndex = waveParam != nullptr ? juce::roundToInt(waveParam->load()) : 0;
    const float level = levelParam != nullptr ? levelParam->load() : 0.0f;
    const float centreY = preview.getCentreY();
    const float amplitude = preview.getHeight() * (0.14f + level * 0.34f);
    const int width = juce::jmax(1, (int) preview.getWidth());

    juce::Path path;
    for (int x = 0; x < width; ++x)
    {
        const float phase = (float) x / (float) juce::jmax(1, width - 1);
        float sample = 0.0f;
        switch (waveIndex)
        {
            case 0: sample = std::sin(juce::MathConstants<float>::twoPi * phase); break;
            case 1: sample = 2.0f * phase - 1.0f; break;
            case 2: sample = phase < 0.5f ? 1.0f : -1.0f; break;
            case 3: sample = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
            default: break;
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

void SubOscSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    titleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    auto previewArea = area.removeFromTop(52);
    juce::ignoreUnused(previewArea);
    area.removeFromTop(8);

    auto topRow = area.removeFromTop(26);
    waveLabel.setBounds(topRow.removeFromLeft(42));
    topRow.removeFromLeft(8);
    waveBox.setBounds(topRow.removeFromLeft(118));
    topRow.removeFromLeft(12);
    directOutButton.setBounds(topRow);

    area.removeFromTop(8);
    auto knobRow = area;
    auto left = knobRow.removeFromLeft((knobRow.getWidth() - 12) / 2);
    knobRow.removeFromLeft(12);
    auto right = knobRow;

    levelLabel.setBounds(left.removeFromTop(16));
    left.removeFromTop(4);
    levelSlider.setBounds(left.reduced(0, 2));
    octaveLabel.setBounds(right.removeFromTop(16));
    right.removeFromTop(4);
    octaveSlider.setBounds(right.reduced(0, 2));
}

void SubOscSectionComponent::timerCallback()
{
    repaint();
}
