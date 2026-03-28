#include "MasterSectionComponent.h"
#include "ReactorUI.h"

namespace
{
void styleMasterLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

}

MasterSectionComponent::MasterSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    : spreadSlider(processor, "masterSpread", reactormod::Destination::masterSpread, juce::Colour(0xffffbe55), 64, 22),
      gainSlider(processor, "masterGain", reactormod::Destination::masterGain, juce::Colour(0xffffbe55), 64, 22)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(spreadLabel);
    addAndMakeVisible(gainLabel);
    addAndMakeVisible(spreadSlider);
    addAndMakeVisible(gainSlider);

    titleLabel.setText("MASTER", juce::dontSendNotification);
    spreadLabel.setText("Width", juce::dontSendNotification);
    gainLabel.setText("Gain", juce::dontSendNotification);

    styleMasterLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleMasterLabel(spreadLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleMasterLabel(gainLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);

    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "masterSpread", spreadSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "masterGain", gainSlider);
}

void MasterSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffffc45d));
}

void MasterSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);

    const int gap = 14;
    const int cellWidth = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto right = area;

    spreadLabel.setBounds(left.removeFromTop(16));
    spreadSlider.setBounds(left.removeFromTop(118));
    gainLabel.setBounds(right.removeFromTop(16));
    gainSlider.setBounds(right.removeFromTop(118));
}
