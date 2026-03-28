#include "EQ3ControlsComponent.h"

namespace
{
juce::String formatDb(double value)
{
    const auto rounded = juce::roundToInt((float) value * 10.0f) / 10.0f;

    if (std::abs(rounded) < 0.05f)
        return "0.0 dB";

    return juce::String(rounded >= 0.0f ? "+" : "") + juce::String(rounded, 1) + " dB";
}
}

class EQ3ControlsComponent::BandKnob final : public juce::Component
{
public:
    BandKnob(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
             const juce::String& bandName, juce::Colour accentColour)
        : attachment(state, parameterId, slider)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(valueLabel);

        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.onValueChange = [this] { refreshValue(); };

        nameLabel.setText(bandName, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(eq3ui::smallCapsFont(10.5f));
        nameLabel.setColour(juce::Label::textColourId, eq3ui::textStrong());

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(eq3ui::bodyFont(10.5f));
        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.12f));

        refreshValue();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(28);
        nameLabel.setBounds(footer.removeFromTop(13));
        valueLabel.setBounds(footer);
        slider.setBounds(area.reduced(6, 0));
    }

private:
    void refreshValue()
    {
        valueLabel.setText(formatDb(slider.getValue()), juce::dontSendNotification);
    }

    eq3ui::SSPKnob slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

EQ3ControlsComponent::EQ3ControlsComponent(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    lowKnob = std::make_unique<BandKnob>(apvts, "low", "LOW", eq3ui::brandAmber());
    midKnob = std::make_unique<BandKnob>(apvts, "mid", "MID", eq3ui::brandTeal());
    highKnob = std::make_unique<BandKnob>(apvts, "high", "HIGH", eq3ui::brandIce());

    addAndMakeVisible(*lowKnob);
    addAndMakeVisible(*midKnob);
    addAndMakeVisible(*highKnob);
}

EQ3ControlsComponent::~EQ3ControlsComponent() = default;

void EQ3ControlsComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    eq3ui::drawPanelBackground(g, bounds, eq3ui::brandAmber(), 14.0f);

    auto guide = bounds.reduced(26.0f, 16.0f);
    guide.removeFromTop(10.0f);
    g.setColour(eq3ui::textMuted().withAlpha(0.28f));
    g.drawHorizontalLine((int) guide.getY(), guide.getX(), guide.getRight());
}

void EQ3ControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(16, 8);
    const int gap = 10;
    const int columnWidth = (area.getWidth() - gap * 2) / 3;

    lowKnob->setBounds(area.removeFromLeft(columnWidth));
    area.removeFromLeft(gap);
    midKnob->setBounds(area.removeFromLeft(columnWidth));
    area.removeFromLeft(gap);
    highKnob->setBounds(area);
}
