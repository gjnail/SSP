#include "TransitionControlsComponent.h"

namespace
{
juce::Colour subtleText() { return transitionui::textMuted(); }
} // namespace

class TransitionControlsComponent::TransitionKnob final : public juce::Component
{
public:
    TransitionKnob(juce::AudioProcessorValueTreeState& state,
                   const juce::String& paramId,
                   const juce::String& heading,
                   const juce::String& caption,
                   juce::Colour accent,
                   bool heroStyle)
        : attachment(state, paramId, slider),
          accentColour(accent),
          hero(heroStyle)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(transitionui::titleFont(hero ? 26.0f : 16.0f));
        titleLabel.setColour(juce::Label::textColourId, transitionui::textStrong());

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(transitionui::titleFont(hero ? 18.0f : 12.0f));
        valueLabel.setColour(juce::Label::textColourId, accentColour.brighter(0.16f));

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(transitionui::bodyFont(hero ? 10.5f : 9.6f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());
        captionLabel.setMinimumHorizontalScale(0.72f);

        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        if (auto* param = state.getParameter(paramId))
            slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    void paint(juce::Graphics& g) override
    {
        transitionui::drawPanelBackground(g, getLocalBounds().toFloat(), accentColour, hero ? 22.0f : 18.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 22 : 14, hero ? 18 : 12);
        titleLabel.setBounds(area.removeFromTop(hero ? 34 : 22));
        valueLabel.setBounds(area.removeFromTop(hero ? 24 : 16));
        captionLabel.setBounds(area.removeFromBottom(hero ? 28 : 16));
        area.removeFromBottom(hero ? 8 : 4);

        const int knobSize = juce::jmin(area.getWidth(), area.getHeight());
        const int actualSize = (int) std::round((hero ? 0.97f : 0.92f) * (float) knobSize);
        slider.setBounds(area.withSizeKeepingCentre(actualSize, actualSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(juce::String(juce::roundToInt((float) slider.getValue() * 100.0f)) + "%", juce::dontSendNotification);
    }

    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    transitionui::SSPKnob slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::Colour accentColour;
    bool hero = false;
};

TransitionControlsComponent::TransitionControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    addAndMakeVisible(presetLabel);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(badgeLabel);
    addAndMakeVisible(descriptionLabel);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setFont(transitionui::smallCapsFont(11.0f));
    presetLabel.setColour(juce::Label::textColourId, subtleText());

    presetBox.addItemList(PluginProcessor::getPresetNames(), 1);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "preset", presetBox);

    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setFont(transitionui::smallCapsFont(10.0f));
    badgeLabel.setColour(juce::Label::textColourId, transitionui::textStrong());
    badgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff17333f));
    badgeLabel.setColour(juce::Label::outlineColourId, transitionui::brandCyan().withAlpha(0.56f));

    descriptionLabel.setFont(transitionui::bodyFont(11.0f));
    descriptionLabel.setColour(juce::Label::textColourId, subtleText());
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);
    descriptionLabel.setMinimumHorizontalScale(0.82f);

    amountKnob = std::make_unique<TransitionKnob>(state, "amount", "Intensity", "Automate for builds or sweep for an instant wash out.", transitionui::brandAmber(), true);
    filterKnob = std::make_unique<TransitionKnob>(state, "filter", "Filter", "How hard the sweep bites.", transitionui::brandAmber(), false);
    spaceKnob = std::make_unique<TransitionKnob>(state, "space", "Space", "Dial the wash and room size.", transitionui::brandCyan(), false);
    widthKnob = std::make_unique<TransitionKnob>(state, "width", "Width", "Push the stereo spread wider.", juce::Colour(0xff7bd9ff), false);
    riseKnob = std::make_unique<TransitionKnob>(state, "rise", "Rise", "Trim the riser energy and climb.", juce::Colour(0xffff9c53), false);
    mixKnob = std::make_unique<TransitionKnob>(state, "mix", "Mix", "Blend processed and dry.", transitionui::brandCyan().brighter(0.10f), false);
    visualizer = std::make_unique<TransitionVisualizerComponent>(processor);
    addAndMakeVisible(*amountKnob);
    addAndMakeVisible(*filterKnob);
    addAndMakeVisible(*spaceKnob);
    addAndMakeVisible(*widthKnob);
    addAndMakeVisible(*riseKnob);
    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*visualizer);

    refreshPresetDetails();
    startTimerHz(12);
}

TransitionControlsComponent::~TransitionControlsComponent()
{
    stopTimer();
}

void TransitionControlsComponent::timerCallback()
{
    refreshPresetDetails();
}

void TransitionControlsComponent::refreshPresetDetails()
{
    const auto state = processor.getVisualState();
    badgeLabel.setText(state.presetBadge, juce::dontSendNotification);
    descriptionLabel.setText(state.presetDescription, juce::dontSendNotification);
}

void TransitionControlsComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    transitionui::drawPanelBackground(g, bounds, transitionui::brandAmber(), 24.0f);

    auto amberWash = bounds.reduced(26.0f, 22.0f).removeFromLeft(bounds.getWidth() * 0.38f);
    g.setColour(transitionui::brandAmber().withAlpha(0.08f));
    g.fillEllipse(amberWash.withTrimmedBottom(amberWash.getHeight() * 0.10f));

    auto cyanWash = bounds.reduced(26.0f, 22.0f).removeFromRight(bounds.getWidth() * 0.42f);
    g.setColour(transitionui::brandCyan().withAlpha(0.07f));
    g.fillEllipse(cyanWash.withTrimmedTop(cyanWash.getHeight() * 0.18f));
}

void TransitionControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(20, 18);

    auto header = area.removeFromTop(84);
    auto labelRow = header.removeFromTop(18);
    presetLabel.setBounds(labelRow.removeFromLeft(62));
    labelRow.removeFromLeft(10);
    badgeLabel.setBounds(labelRow.removeFromLeft(120));

    header.removeFromTop(8);
    auto selectorRow = header;
    presetBox.setBounds(selectorRow.removeFromLeft(248).removeFromTop(40));
    selectorRow.removeFromLeft(18);
    descriptionLabel.setBounds(selectorRow.removeFromTop(40));

    area.removeFromTop(14);
    auto main = area.removeFromTop((int) std::round(area.getHeight() * 0.58f));
    auto controlsRow = area;
    const int panelGap = 14;

    auto heroArea = main.removeFromLeft((int) std::round(main.getWidth() * 0.35f));
    main.removeFromLeft(panelGap);
    amountKnob->setBounds(heroArea);
    visualizer->setBounds(main);

    const int knobGap = 10;
    const int knobWidth = (controlsRow.getWidth() - knobGap * 4) / 5;
    filterKnob->setBounds(controlsRow.removeFromLeft(knobWidth));
    controlsRow.removeFromLeft(knobGap);
    spaceKnob->setBounds(controlsRow.removeFromLeft(knobWidth));
    controlsRow.removeFromLeft(knobGap);
    widthKnob->setBounds(controlsRow.removeFromLeft(knobWidth));
    controlsRow.removeFromLeft(knobGap);
    riseKnob->setBounds(controlsRow.removeFromLeft(knobWidth));
    controlsRow.removeFromLeft(knobGap);
    mixKnob->setBounds(controlsRow);
}
