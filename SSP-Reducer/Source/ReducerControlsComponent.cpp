#include "ReducerControlsComponent.h"

namespace
{
juce::String formatPercent(double value)
{
    return juce::String(value * 100.0, 1) + " %";
}

juce::String formatBits(double value)
{
    return juce::String(reducerdsp::bitDepthFromNormalised((float) value));
}

juce::String formatRate(double value)
{
    const auto rateHz = reducerdsp::reducedSampleRateFromNormalised((float) value, 44100.0);

    if (rateHz >= 1000.0)
        return juce::String(rateHz / 1000.0, 2) + " kHz";

    return juce::String(juce::roundToInt((float) rateHz)) + " Hz";
}

juce::String formatFilter(double value)
{
    return juce::String(value * 100.0, 1) + " %";
}
}

class ReducerControlsComponent::ReducerKnob final : public juce::Component
{
public:
    ReducerKnob(juce::AudioProcessorValueTreeState& state,
                const juce::String& paramId,
                const juce::String& heading,
                std::function<juce::String(double)> formatterToUse,
                std::function<double(const juce::String&)> parserToUse)
        : attachment(state, paramId, slider),
          formatter(std::move(formatterToUse)),
          parser(std::move(parserToUse))
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);

        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, reducerui::accent());
        slider.setColour(juce::Slider::thumbColourId, reducerui::accentBright());
        slider.textFromValueFunction = [this](double value)
        {
            return formatter != nullptr ? formatter(value) : juce::String(value, 2);
        };
        slider.valueFromTextFunction = [this](const juce::String& text)
        {
            return parser != nullptr ? parser(text) : text.getDoubleValue();
        };
        slider.onValueChange = [this] { refreshValueText(); };

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(reducerui::smallCapsFont(10.5f));
        titleLabel.setColour(juce::Label::textColourId, reducerui::textStrong());

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(reducerui::bodyFont(10.0f));
        valueLabel.setColour(juce::Label::textColourId, reducerui::textMuted().brighter(0.22f));

        refreshValueText();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(31);
        titleLabel.setBounds(footer.removeFromTop(13));
        valueLabel.setBounds(footer);
        slider.setBounds(area.reduced(6, 0));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    reducerui::SSPKnob slider;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    std::function<juce::String(double)> formatter;
    std::function<double(const juce::String&)> parser;
};

class ReducerControlsComponent::ReducerToggle final : public juce::Component
{
public:
    ReducerToggle(juce::AudioProcessorValueTreeState& state,
                  const juce::String& paramId,
                  const juce::String& buttonText)
        : toggle(buttonText),
          attachment(state, paramId, toggle)
    {
        addAndMakeVisible(toggle);
    }

    void resized() override
    {
        toggle.setBounds(getLocalBounds());
    }

private:
    reducerui::SSPToggle toggle;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

ReducerControlsComponent::ReducerControlsComponent(juce::AudioProcessorValueTreeState& state)
{
    const auto parsePercent = [] (const juce::String& text) { return juce::jlimit(0.0, 1.0, text.retainCharacters("0123456789.-").getDoubleValue() / 100.0); };
    const auto parseBits = [] (const juce::String& text)
    {
        const auto bits = juce::jlimit(1.0, 16.0, text.retainCharacters("0123456789.-").getDoubleValue());
        return (bits - 1.0) / 15.0;
    };
    const auto parseRate = [] (const juce::String& text)
    {
        auto cleaned = text.trim();
        double hz = cleaned.retainCharacters("0123456789.-").getDoubleValue();

        if (cleaned.containsIgnoreCase("khz"))
            hz *= 1000.0;

        hz = juce::jlimit(200.0, 44100.0, hz);
        return std::log(hz / 200.0) / std::log(44100.0 / 200.0);
    };

    rateKnob = std::make_unique<ReducerKnob>(state, "rate", "RATE", formatRate, parseRate);
    bitsKnob = std::make_unique<ReducerKnob>(state, "bits", "BITS", formatBits, parseBits);
    jitterKnob = std::make_unique<ReducerKnob>(state, "jitter", "DITHER", formatPercent, parsePercent);
    shapeKnob = std::make_unique<ReducerKnob>(state, "tone", "ADC Q", formatPercent, parsePercent);
    filterKnob = std::make_unique<ReducerKnob>(state, "filter", "DAC Q", formatFilter, parsePercent);
    mixKnob = std::make_unique<ReducerKnob>(state, "mix", "DRY/WET", formatPercent, parsePercent);

    preToggle = std::make_unique<ReducerToggle>(state, "preFilter", "PRE");
    postToggle = std::make_unique<ReducerToggle>(state, "postFilter", "POST");
    dcShiftToggle = std::make_unique<ReducerToggle>(state, "dcShift", "DC SHIFT");

    addAndMakeVisible(*rateKnob);
    addAndMakeVisible(*bitsKnob);
    addAndMakeVisible(*jitterKnob);
    addAndMakeVisible(*shapeKnob);
    addAndMakeVisible(*filterKnob);
    addAndMakeVisible(*mixKnob);

    addAndMakeVisible(*preToggle);
    addAndMakeVisible(*postToggle);
    addAndMakeVisible(*dcShiftToggle);
}

ReducerControlsComponent::~ReducerControlsComponent() = default;

void ReducerControlsComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    reducerui::drawPanelBackground(g, bounds, reducerui::accent(), 14.0f);

    auto guide = bounds.reduced(24.0f, 16.0f);
    guide.removeFromTop(10.0f);
    g.setColour(reducerui::textMuted().withAlpha(0.18f));
    g.drawHorizontalLine((int) guide.getCentreY(), guide.getX(), guide.getRight());
}

void ReducerControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    const int gap = 10;

    auto topRow = area.removeFromTop(128);
    const int topWidth = (topRow.getWidth() - gap * 2) / 3;
    rateKnob->setBounds(topRow.removeFromLeft(topWidth));
    topRow.removeFromLeft(gap);
    bitsKnob->setBounds(topRow.removeFromLeft(topWidth));
    topRow.removeFromLeft(gap);
    jitterKnob->setBounds(topRow);

    area.removeFromTop(8);
    auto secondRow = area.removeFromTop(128);
    const int secondWidth = (secondRow.getWidth() - gap * 2) / 3;
    shapeKnob->setBounds(secondRow.removeFromLeft(secondWidth));
    secondRow.removeFromLeft(gap);
    filterKnob->setBounds(secondRow.removeFromLeft(secondWidth));
    secondRow.removeFromLeft(gap);
    mixKnob->setBounds(secondRow);

    area.removeFromTop(8);
    const int toggleWidth = (area.getWidth() - gap * 2) / 3;
    preToggle->setBounds(area.removeFromLeft(toggleWidth));
    area.removeFromLeft(gap);
    postToggle->setBounds(area.removeFromLeft(toggleWidth));
    area.removeFromLeft(gap);
    dcShiftToggle->setBounds(area.removeFromLeft(toggleWidth));
}
