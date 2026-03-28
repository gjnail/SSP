#include "ModSectionComponent.h"
#include "ReactorUI.h"

namespace
{
void styleModLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleModSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe5bcff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffa66cff));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, reactorui::outline());
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f151c));
    slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
}
}

ModSectionComponent::ModSectionComponent(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subLabel);
    addAndMakeVisible(rateLabel);
    addAndMakeVisible(depthLabel);
    addAndMakeVisible(rateSlider);
    addAndMakeVisible(depthSlider);

    titleLabel.setText("MOD", juce::dontSendNotification);
    subLabel.setText("LFO 1 routes to the filter cutoff.", juce::dontSendNotification);
    rateLabel.setText("Rate", juce::dontSendNotification);
    depthLabel.setText("Depth", juce::dontSendNotification);

    styleModLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleModLabel(subLabel, 11.5f, reactorui::textMuted());
    styleModLabel(rateLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleModLabel(depthLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);

    styleModSlider(rateSlider);
    styleModSlider(depthSlider);
    rateSlider.setTextValueSuffix(" Hz");

    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoRate", rateSlider);
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfoDepth", depthSlider);
}

void ModSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffa66cff));
}

void ModSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(22));
    subLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    const int gap = 12;
    auto left = area.removeFromLeft((area.getWidth() - gap) / 2);
    area.removeFromLeft(gap);
    auto right = area;

    rateLabel.setBounds(left.removeFromTop(16));
    rateSlider.setBounds(left.removeFromTop(118));
    depthLabel.setBounds(right.removeFromTop(16));
    depthSlider.setBounds(right.removeFromTop(118));
}
