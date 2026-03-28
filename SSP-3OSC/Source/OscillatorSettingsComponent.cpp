#include "OscillatorSettingsComponent.h"
#include "ReactorUI.h"

namespace
{
void styleOscSettingLabel(juce::Label& label, float size, juce::Colour colour,
                          juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}
}

OscillatorSettingsComponent::OscillatorSettingsComponent(PluginProcessor& processor,
                                                         juce::AudioProcessorValueTreeState& apvts)
    : widthSliders{
        ModulationKnob(processor, "osc1Width", reactormod::Destination::none, juce::Colour(0xff8fd7ff), 62, 16),
        ModulationKnob(processor, "osc2Width", reactormod::Destination::none, juce::Colour(0xff8fd7ff), 62, 16),
        ModulationKnob(processor, "osc3Width", reactormod::Destination::none, juce::Colour(0xff8fd7ff), 62, 16)
      }
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("OSC STEREO", juce::dontSendNotification);
    styleOscSettingLabel(titleLabel, 15.0f, reactorui::textStrong());

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(oscLabels[(size_t) i]);
        addAndMakeVisible(widthLabels[(size_t) i]);
        addAndMakeVisible(widthSliders[(size_t) i]);

        oscLabels[(size_t) i].setText("OSC " + juce::String(i + 1), juce::dontSendNotification);
        widthLabels[(size_t) i].setText("Width", juce::dontSendNotification);
        styleOscSettingLabel(oscLabels[(size_t) i], 12.0f, juce::Colour(0xffa7ddff), juce::Justification::centred);
        styleOscSettingLabel(widthLabels[(size_t) i], 11.5f, reactorui::textMuted(), juce::Justification::centred);

        widthAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "osc" + juce::String(i + 1) + "Width", widthSliders[(size_t) i]);
    }
}

void OscillatorSettingsComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff7dcfff));
}

void OscillatorSettingsComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    const int gap = 16;
    const int cellWidth = (area.getWidth() - gap * 2) / 3;

    for (int i = 0; i < 3; ++i)
    {
        auto cell = area.removeFromLeft(cellWidth);
        if (i < 2)
            area.removeFromLeft(gap);

        oscLabels[(size_t) i].setBounds(cell.removeFromTop(16));
        cell.removeFromTop(6);
        widthLabels[(size_t) i].setBounds(cell.removeFromTop(16));
        widthSliders[(size_t) i].setBounds(cell.removeFromTop(124));
    }
}
