#include "VoiceSectionComponent.h"
#include "ReactorUI.h"

namespace
{
void styleVoiceLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleVoiceCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111821));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff72c8ff));
}

void styleVoiceToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff72c8ff));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
    button.setClickingTogglesState(true);
}
}

VoiceSectionComponent::VoiceSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    : glideSlider(processor, "voiceGlide", reactormod::Destination::voiceGlide, juce::Colour(0xff72c8ff), 64, 22)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeBox);
    addAndMakeVisible(glideLabel);
    addAndMakeVisible(glideSlider);
    addAndMakeVisible(legatoButton);
    addAndMakeVisible(portamentoButton);

    titleLabel.setText("VOICE MODE", juce::dontSendNotification);
    modeLabel.setText("Mode", juce::dontSendNotification);
    glideLabel.setText("Glide", juce::dontSendNotification);
    legatoButton.setButtonText("Legato");
    portamentoButton.setButtonText("Portamento");

    styleVoiceLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleVoiceLabel(modeLabel, 11.5f, reactorui::textMuted());
    styleVoiceLabel(glideLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleVoiceCombo(modeBox);
    styleVoiceToggle(legatoButton);
    styleVoiceToggle(portamentoButton);
    glideSlider.setTextValueSuffix(" s");
    modeBox.addItem("Poly", 1);
    modeBox.addItem("Mono", 2);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "voiceMode", modeBox);
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "voiceGlide", glideSlider);
    legatoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "voiceLegato", legatoButton);
    portamentoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "voicePortamento", portamentoButton);
}

void VoiceSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff74cbff));
}

void VoiceSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto modeArea = area.removeFromTop(48);
    modeLabel.setBounds(modeArea.removeFromTop(16));
    modeBox.setBounds(modeArea.removeFromTop(28));

    area.removeFromTop(12);
    auto bottomArea = area;
    auto glideArea = bottomArea.removeFromLeft(124);
    bottomArea.removeFromLeft(18);
    auto toggleArea = bottomArea;

    glideLabel.setBounds(glideArea.removeFromTop(16));
    glideSlider.setBounds(glideArea.removeFromTop(118));

    auto toggleTop = toggleArea.removeFromTop(28);
    legatoButton.setBounds(toggleTop.removeFromLeft((toggleTop.getWidth() - 12) / 2));
    toggleTop.removeFromLeft(12);
    portamentoButton.setBounds(toggleTop);
}
