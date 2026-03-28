#include "TriggerControlsComponent.h"

namespace
{
juce::Colour panelBorder() { return juce::Colour(0xff28323d); }
juce::Colour activeButton() { return juce::Colour(0xffffd83d); }
juce::Colour activeText() { return juce::Colour(0xff151515); }
juce::Colour idleButton() { return juce::Colour(0xff212a34); }
juce::Colour idleText() { return juce::Colour(0xffd9e1e8); }

const juce::StringArray& getTriggerRateOptions()
{
    static const juce::StringArray options{"1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/32"};
    return options;
}
} // namespace

TriggerControlsComponent::TriggerControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(rateLabel);
    addAndMakeVisible(audioButton);
    addAndMakeVisible(bpmButton);
    addAndMakeVisible(midiButton);
    addAndMakeVisible(rateBox);

    titleLabel.setText("Trigger Controls", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(juce::Font(10.5f));
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    modeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8693a0));

    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.setFont(juce::Font(10.5f));
    rateLabel.setJustificationType(juce::Justification::centredLeft);
    rateLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8693a0));

    auto setupButton = [this](juce::TextButton& button, int mode)
    {
        button.onClick = [this, mode] { setTriggerMode(mode); };
        button.setColour(juce::TextButton::buttonOnColourId, activeButton());
        button.setColour(juce::TextButton::textColourOffId, idleText());
        button.setColour(juce::TextButton::textColourOnId, activeText());
    };

    setupButton(audioButton, 0);
    setupButton(bpmButton, 1);
    setupButton(midiButton, 2);

    rateBox.addItemList(getTriggerRateOptions(), 1);
    rateBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    rateBox.setColour(juce::ComboBox::outlineColourId, panelBorder());
    rateBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffeef4fa));
    rateBox.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffeef4fa));
    rateBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1a222c));
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "triggerRate", rateBox);

    startTimerHz(20);
    refreshState();
}

TriggerControlsComponent::~TriggerControlsComponent()
{
    stopTimer();
}

void TriggerControlsComponent::timerCallback()
{
    refreshState();
}

void TriggerControlsComponent::setTriggerMode(int modeIndex)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("triggerMode")))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1((float)modeIndex);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
        selectedMode = modeIndex;
        updateButtonStyles();
        repaint();
    }
}

void TriggerControlsComponent::refreshState()
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("triggerMode")))
        selectedMode = param->getIndex();

    currentTriggerActivity = processor.getTriggerActivity();
    updateButtonStyles();

    rateBox.setEnabled(true);
    rateLabel.setAlpha(1.0f);
    repaint();
}

void TriggerControlsComponent::updateButtonStyles()
{
    auto apply = [&](juce::TextButton& button, int index)
    {
        const bool active = selectedMode == index;
        button.setColour(juce::TextButton::buttonColourId, active ? activeButton() : idleButton());
    };

    apply(audioButton, 0);
    apply(bpmButton, 1);
    apply(midiButton, 2);
}

void TriggerControlsComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient fill(juce::Colour(0xff18212a), area.getTopLeft(), juce::Colour(0xff11161d), area.getBottomRight(), false);
    fill.addColour(0.5, juce::Colour(0xff151d27));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(area, 14.0f);

    g.setColour(panelBorder());
    g.drawRoundedRectangle(area.reduced(0.5f), 14.0f, 1.0f);

    auto indicatorArea = area.reduced(14.0f, 12.0f).removeFromTop(18.0f).removeFromRight(72.0f);
    const auto ledBounds = indicatorArea.removeFromLeft(14.0f).withSizeKeepingCentre(10.0f, 10.0f);
    const auto ledColour = juce::Colour(0xffffd83d).withAlpha(0.18f + 0.82f * juce::jlimit(0.0f, 1.0f, currentTriggerActivity));
    g.setColour(juce::Colour(0x33ffd83d));
    g.fillEllipse(ledBounds.expanded(4.0f));
    g.setColour(ledColour);
    g.fillEllipse(ledBounds);
    g.setColour(juce::Colour(0xffa9b4bf));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("TRIGGER", indicatorArea, juce::Justification::centredRight);

    auto helper = area.reduced(14.0f, 12.0f);
    helper.removeFromTop(66.0f);
    g.setColour(juce::Colour(0xff7f8c99));
    g.setFont(juce::Font(10.0f));
    g.drawText("Audio -> sidechain trigger. BPM Sync -> host tempo cycle. Midi -> note sidechain trigger.",
               helper.removeFromTop(14.0f), juce::Justification::centredLeft);
}

void TriggerControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);

    titleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);

    auto controlsRow = area.removeFromTop(58);
    auto sourceArea = controlsRow.removeFromLeft(controlsRow.getWidth() - 180);
    auto rateArea = controlsRow;

    modeLabel.setBounds(sourceArea.removeFromTop(12));
    sourceArea.removeFromTop(8);

    const int gap = 8;
    const int buttonWidth = (sourceArea.getWidth() - gap * 2) / 3;
    audioButton.setBounds(sourceArea.removeFromLeft(buttonWidth));
    sourceArea.removeFromLeft(gap);
    bpmButton.setBounds(sourceArea.removeFromLeft(buttonWidth));
    sourceArea.removeFromLeft(gap);
    midiButton.setBounds(sourceArea);

    rateLabel.setBounds(rateArea.removeFromTop(12));
    rateArea.removeFromTop(8);
    rateBox.setBounds(rateArea.removeFromTop(34));
}
