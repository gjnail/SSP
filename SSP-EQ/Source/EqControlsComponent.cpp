#include "EqControlsComponent.h"

EqControlsComponent::EqControlsComponent(PluginProcessor& p)
    : processor(p)
{
    selectedLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    selectedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    selectedLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectedLabel);

    auto setupCaption = [](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff8da4bb));
        label.setJustificationType(juce::Justification::centredLeft);
    };

    setupCaption(typeLabel, "Point Type");
    addAndMakeVisible(typeLabel);

    setupCaption(qLabel, "Q / Width");
    addAndMakeVisible(qLabel);

    helperLabel.setText("Pick bell, shelf, or cut shapes here. Use the graph for frequency and gain, then tighten or widen the selected point with Q.",
                        juce::dontSendNotification);
    helperLabel.setFont(12.0f);
    helperLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6f849b));
    helperLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(helperLabel);

    typeBox.addItemList(PluginProcessor::getPointTypeNames(), 1);
    typeBox.onChange = [this] { commitTypeChange(); };
    addAndMakeVisible(typeBox);

    qSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 22);
    qSlider.setRange(0.2, 10.0, 0.01);
    qSlider.setSkewFactor(0.45);
    qSlider.onValueChange = [this] { commitQChange(); };
    qSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff63d0ff));
    qSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9de7ff));
    qSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(qSlider);

    removeButton.onClick = [this]
    {
        if (selectedPoint < 0)
            return;

        processor.removePoint(selectedPoint);
        selectedPoint = -1;
        refreshFromPoint();
        if (onSelectionChanged)
            onSelectionChanged(-1);
    };
    addAndMakeVisible(removeButton);

    startTimerHz(20);
    refreshFromPoint();
}

void EqControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff101721));
    g.fillRoundedRectangle(bounds, 14.0f);

    juce::ColourGradient sheen(juce::Colour(0x1638a2ff), bounds.getX(), bounds.getY(),
                               juce::Colour(0x0022c8ff), bounds.getRight(), bounds.getBottom(), false);
    sheen.addColour(0.6, juce::Colour(0x1018e4ff));
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 14.0f);

    g.setColour(juce::Colour(0xff233445));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.0f);

    auto drawPanel = [&g](juce::Rectangle<int> area)
    {
        g.setColour(juce::Colour(0xff141d28));
        g.fillRoundedRectangle(area.toFloat(), 12.0f);
        g.setColour(juce::Colour(0xff2b3e52));
        g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 12.0f, 1.0f);
    };

    drawPanel(typePanelBounds);
    drawPanel(qPanelBounds);
    drawPanel(actionPanelBounds);
}

void EqControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(46);
    selectedLabel.setBounds(topRow.removeFromLeft(220));
    helperLabel.setBounds(topRow);

    area.removeFromTop(8);
    auto panelRow = area.removeFromTop(150);

    typePanelBounds = panelRow.removeFromLeft(300);
    panelRow.removeFromLeft(16);
    qPanelBounds = panelRow.removeFromLeft(200);
    panelRow.removeFromLeft(16);
    actionPanelBounds = panelRow;

    auto typeArea = typePanelBounds.reduced(16);
    typeLabel.setBounds(typeArea.removeFromTop(18));
    typeArea.removeFromTop(8);
    typeBox.setBounds(typeArea.removeFromTop(34));

    auto qArea = qPanelBounds.reduced(14);
    qLabel.setBounds(qArea.removeFromTop(18));
    qArea.removeFromTop(2);
    qSlider.setBounds(qArea.withSizeKeepingCentre(118, 118));

    auto actionArea = actionPanelBounds.reduced(16);
    removeButton.setBounds(actionArea.removeFromTop(36).withWidth(160));
}

void EqControlsComponent::setSelectedPoint(int index)
{
    if (selectedPoint == index)
        return;

    selectedPoint = index;
    refreshFromPoint();
}

void EqControlsComponent::timerCallback()
{
    refreshFromPoint();
}

void EqControlsComponent::refreshFromPoint()
{
    syncing = true;

    if (selectedPoint >= 0)
    {
        const auto point = processor.getPoint(selectedPoint);
        if (point.enabled)
        {
            selectedLabel.setText("Point " + juce::String(selectedPoint + 1), juce::dontSendNotification);
            typeBox.setSelectedItemIndex(point.type, juce::dontSendNotification);
            qSlider.setValue(point.q, juce::dontSendNotification);
            syncing = false;
            updateEnabledState();
            return;
        }
    }

    selectedLabel.setText("No Point Selected", juce::dontSendNotification);
    typeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    qSlider.setValue(1.0, juce::dontSendNotification);
    syncing = false;
    updateEnabledState();
}

void EqControlsComponent::commitTypeChange()
{
    if (syncing || selectedPoint < 0)
        return;

    auto point = processor.getPoint(selectedPoint);
    if (!point.enabled)
        return;

    point.type = typeBox.getSelectedItemIndex();
    processor.setPoint(selectedPoint, point);
}

void EqControlsComponent::commitQChange()
{
    if (syncing || selectedPoint < 0)
        return;

    auto point = processor.getPoint(selectedPoint);
    if (!point.enabled)
        return;

    point.q = (float) qSlider.getValue();
    processor.setPoint(selectedPoint, point);
}

void EqControlsComponent::updateEnabledState()
{
    const bool hasPoint = selectedPoint >= 0 && processor.getPoint(selectedPoint).enabled;
    typeLabel.setEnabled(hasPoint);
    qLabel.setEnabled(hasPoint);
    typeBox.setEnabled(hasPoint);
    removeButton.setEnabled(hasPoint);
    qSlider.setEnabled(hasPoint);
    qSlider.setAlpha(hasPoint ? 1.0f : 0.75f);
}
