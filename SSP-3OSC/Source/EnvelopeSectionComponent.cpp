#include "EnvelopeSectionComponent.h"
#include "ReactorUI.h"

namespace
{
juce::Rectangle<float> getEnvelopePreviewBounds(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(14).toFloat();
    area.removeFromTop(34.0f);
    return area.removeFromTop(68.0f);
}

void styleEnvelopeLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

}

EnvelopeSectionComponent::EnvelopeSectionComponent(PluginProcessor& processor,
                                                   juce::AudioProcessorValueTreeState& apvts,
                                                   juce::String title,
                                                   juce::String prefix,
                                                   juce::Colour accent)
    : titleText(std::move(title)),
      parameterPrefix(std::move(prefix)),
      accentColour(accent),
      attackSlider(processor, parameterPrefix + "Attack",
                   reactormod::destinationForParameterID(parameterPrefix + "Attack"), accentColour, 62, 16),
      decaySlider(processor, parameterPrefix + "Decay",
                  reactormod::destinationForParameterID(parameterPrefix + "Decay"), accentColour, 62, 16),
      sustainSlider(processor, parameterPrefix + "Sustain",
                    reactormod::destinationForParameterID(parameterPrefix + "Sustain"), accentColour, 62, 16),
      releaseSlider(processor, parameterPrefix + "Release",
                    reactormod::destinationForParameterID(parameterPrefix + "Release"), accentColour, 62, 16)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(attackLabel);
    addAndMakeVisible(decayLabel);
    addAndMakeVisible(sustainLabel);
    addAndMakeVisible(releaseLabel);
    addAndMakeVisible(attackSlider);
    addAndMakeVisible(decaySlider);
    addAndMakeVisible(sustainSlider);
    addAndMakeVisible(releaseSlider);

    titleLabel.setText(titleText, juce::dontSendNotification);
    styleEnvelopeLabel(titleLabel, 15.0f, reactorui::textStrong());

    attackLabel.setText("Attack", juce::dontSendNotification);
    decayLabel.setText("Decay", juce::dontSendNotification);
    sustainLabel.setText("Sustain", juce::dontSendNotification);
    releaseLabel.setText("Release", juce::dontSendNotification);

    styleEnvelopeLabel(attackLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleEnvelopeLabel(decayLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleEnvelopeLabel(sustainLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleEnvelopeLabel(releaseLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);

    attackSlider.setTextValueSuffix(" s");
    decaySlider.setTextValueSuffix(" s");
    releaseSlider.setTextValueSuffix(" s");

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterPrefix + "Attack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterPrefix + "Decay", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterPrefix + "Sustain", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterPrefix + "Release", releaseSlider);

    startTimerHz(30);
}

void EnvelopeSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    reactorui::drawPanelBackground(g, bounds, accentColour);

    auto preview = getEnvelopePreviewBounds(getLocalBounds());
    reactorui::drawDisplayPanel(g, preview, accentColour);
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accentColour.withAlpha(0.13f), 4, 1);

    const float attack = (float) attackSlider.getValue();
    const float decay = (float) decaySlider.getValue();
    const float sustain = (float) sustainSlider.getValue();
    const float release = (float) releaseSlider.getValue();

    const float total = juce::jmax(0.05f, attack + decay + release + 0.25f);
    const float attackPortion = attack / total;
    const float decayPortion = decay / total;
    const float releasePortion = release / total;
    const float sustainPortion = juce::jlimit(0.12f, 0.35f, 1.0f - attackPortion - decayPortion - releasePortion);

    const float startX = preview.getX() + 8.0f;
    const float endX = preview.getRight() - 8.0f;
    const float usableWidth = endX - startX;
    const float bottomY = preview.getBottom() - 9.0f;
    const float topY = preview.getY() + 10.0f;
    const float sustainY = juce::jmap(sustain, 0.0f, 1.0f, bottomY, topY);

    juce::Path env;
    env.startNewSubPath(startX, bottomY);
    env.lineTo(startX + usableWidth * attackPortion, topY);
    env.lineTo(startX + usableWidth * (attackPortion + decayPortion), sustainY);
    env.lineTo(startX + usableWidth * (attackPortion + decayPortion + sustainPortion), sustainY);
    env.lineTo(endX, bottomY);

    juce::Path fill(env);
    fill.lineTo(endX, bottomY);
    fill.lineTo(startX, bottomY);
    fill.closeSubPath();
    g.setColour(accentColour.withAlpha(0.10f));
    g.fillPath(fill);
    g.setColour(accentColour);
    g.strokePath(env, juce::PathStrokeType(2.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EnvelopeSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(82);
    area.removeFromTop(8);

    const int gap = 8;
    const int cellWidth = (area.getWidth() - gap * 3) / 4;
    auto a = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto d = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto s = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto r = area;

    attackLabel.setBounds(a.removeFromTop(16));
    attackSlider.setBounds(a);
    decayLabel.setBounds(d.removeFromTop(16));
    decaySlider.setBounds(d);
    sustainLabel.setBounds(s.removeFromTop(16));
    sustainSlider.setBounds(s);
    releaseLabel.setBounds(r.removeFromTop(16));
    releaseSlider.setBounds(r);
}

void EnvelopeSectionComponent::timerCallback()
{
    repaint();
}
