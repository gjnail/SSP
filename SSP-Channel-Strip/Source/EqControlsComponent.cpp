#include "EqControlsComponent.h"

namespace
{
void styleKnob(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff63d0ff));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9de7ff));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111822));
}

void styleReadout(juce::Label& label)
{
    label.setFont(juce::Font(16.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

EqControlsComponent::EqControlsComponent(PluginProcessor& p)
    : processor(p)
{
    selectedLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    selectedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    selectedLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectedLabel);

    helperLabel.setFont(12.0f);
    helperLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6f849b));
    helperLabel.setJustificationType(juce::Justification::centredLeft);
    helperLabel.setText("No Point Selected", juce::dontSendNotification);
    addAndMakeVisible(helperLabel);

    styleLabel(typeLabel, "Point Type");
    styleLabel(slopeLabel, "Slope");
    styleLabel(stereoLabel, "Stereo Processing");
    styleLabel(freqLabel, "Frequency");
    styleLabel(gainLabel, "Gain");
    styleLabel(qLabel, "Q / Width");
    styleLabel(processingLabel, "Processing Mode");
    styleLabel(analyzerLabel, "Analyzer Mode");
    styleLabel(resolutionLabel, "Analyzer Resolution");
    styleLabel(decayLabel, "Analyzer Decay");
    styleLabel(outputLabel, "Output");
    styleLabel(pointStateLabel, "Band State");

    addAndMakeVisible(typeLabel);
    addAndMakeVisible(slopeLabel);
    addAndMakeVisible(stereoLabel);
    addAndMakeVisible(freqLabel);
    addAndMakeVisible(gainLabel);
    addAndMakeVisible(qLabel);
    addAndMakeVisible(processingLabel);
    addAndMakeVisible(analyzerLabel);
    addAndMakeVisible(resolutionLabel);
    addAndMakeVisible(decayLabel);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(pointStateLabel);

    styleReadout(freqReadout);
    styleReadout(gainReadout);
    styleReadout(qReadout);
    styleReadout(outputReadout);
    addAndMakeVisible(freqReadout);
    addAndMakeVisible(gainReadout);
    addAndMakeVisible(qReadout);
    addAndMakeVisible(outputReadout);

    styleComboBox(typeBox);
    styleComboBox(slopeBox);
    styleComboBox(stereoModeBox);
    styleComboBox(processingModeBox);
    styleComboBox(analyzerModeBox);
    styleComboBox(analyzerResolutionBox);

    typeBox.addItemList(PluginProcessor::getPointTypeDisplayNames(), 1);
    slopeBox.addItemList(PluginProcessor::getSlopeNames(), 1);
    stereoModeBox.addItemList(PluginProcessor::getStereoModeNames(), 1);
    processingModeBox.addItemList(PluginProcessor::getProcessingModeNames(), 1);
    analyzerModeBox.addItemList(PluginProcessor::getAnalyzerModeNames(), 1);
    analyzerResolutionBox.addItemList(PluginProcessor::getAnalyzerResolutionNames(), 1);

    addAndMakeVisible(typeBox);
    addAndMakeVisible(slopeBox);
    addAndMakeVisible(stereoModeBox);
    addAndMakeVisible(processingModeBox);
    addAndMakeVisible(analyzerModeBox);
    addAndMakeVisible(analyzerResolutionBox);

    frequencySlider.setRange(20.0, 20000.0, 0.01);
    frequencySlider.setSkewFactor(0.25);
    gainSlider.setRange(-24.0, 24.0, 0.01);
    qSlider.setRange(0.2, 18.0, 0.01);
    qSlider.setSkewFactor(0.35);
    analyzerDecaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerDecaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
    analyzerDecaySlider.setRange(0.05, 0.98, 0.01);
    analyzerDecaySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff63d0ff));
    analyzerDecaySlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9de7ff));
    analyzerDecaySlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111822));
    analyzerDecaySlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(analyzerDecaySlider);

    styleKnob(frequencySlider);
    styleKnob(gainSlider);
    styleKnob(qSlider);
    addAndMakeVisible(frequencySlider);
    addAndMakeVisible(gainSlider);
    addAndMakeVisible(qSlider);

    auto commitPoint = [this] { commitPointChange(); };
    typeBox.onChange = commitPoint;
    slopeBox.onChange = commitPoint;
    stereoModeBox.onChange = commitPoint;
    pointEnableToggle.onClick = commitPoint;
    frequencySlider.onValueChange = commitPoint;
    gainSlider.onValueChange = commitPoint;
    qSlider.onValueChange = commitPoint;

    processingModeBox.onChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = processor.apvts.getParameter("processingMode"))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) processingModeBox.getSelectedItemIndex()));
    };

    analyzerModeBox.onChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = processor.apvts.getParameter("analyzerMode"))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) analyzerModeBox.getSelectedItemIndex()));
    };

    analyzerResolutionBox.onChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = processor.apvts.getParameter("analyzerResolution"))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) analyzerResolutionBox.getSelectedItemIndex()));
    };

    analyzerDecaySlider.onValueChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(processor.apvts.getParameter("analyzerDecay")))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) analyzerDecaySlider.getValue()));
    };

    globalBypassToggle.onClick = [this]
    {
        if (syncing)
            return;
        if (auto* param = processor.apvts.getParameter("globalBypass"))
            param->setValueNotifyingHost(globalBypassToggle.getToggleState() ? 1.0f : 0.0f);
    };

    previousBandButton.onClick = [this] { selectRelativePoint(-1); };
    nextBandButton.onClick = [this] { selectRelativePoint(1); };
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

    addAndMakeVisible(pointEnableToggle);
    addAndMakeVisible(globalBypassToggle);
    addAndMakeVisible(previousBandButton);
    addAndMakeVisible(nextBandButton);
    addAndMakeVisible(removeButton);

    startTimerHz(24);
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

    drawPanel(selectedPanelBounds);
    drawPanel(typePanelBounds);
    drawPanel(bandPanelBounds);
    drawPanel(statusPanelBounds);
}

void EqControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(42);
    selectedLabel.setBounds(topRow.removeFromLeft(250));
    helperLabel.setBounds(topRow);

    area.removeFromTop(10);
    auto rowOne = area.removeFromTop(124);
    selectedPanelBounds = rowOne.removeFromLeft(180);
    rowOne.removeFromLeft(14);
    typePanelBounds = rowOne.removeFromLeft(340);
    rowOne.removeFromLeft(14);
    bandPanelBounds = rowOne;

    area.removeFromTop(12);
    statusPanelBounds = area;

    auto selectedArea = selectedPanelBounds.reduced(14);
    pointStateLabel.setBounds(selectedArea.removeFromTop(18));
    selectedArea.removeFromTop(8);
    pointEnableToggle.setBounds(selectedArea.removeFromTop(24));
    selectedArea.removeFromTop(8);
    previousBandButton.setBounds(selectedArea.removeFromTop(28).removeFromLeft(36));
    nextBandButton.setBounds(selectedArea.removeFromTop(0).withTrimmedLeft(46).removeFromLeft(36));
    selectedArea.removeFromTop(8);
    removeButton.setBounds(selectedPanelBounds.reduced(14).removeFromBottom(34).withWidth(140));

    auto typeArea = typePanelBounds.reduced(14);
    auto typeRow = typeArea.removeFromTop(18);
    typeLabel.setBounds(typeRow.removeFromLeft(120));
    slopeLabel.setBounds(typeRow.removeFromLeft(88));
    stereoLabel.setBounds(typeRow);
    typeArea.removeFromTop(6);

    auto comboRow = typeArea.removeFromTop(34);
    typeBox.setBounds(comboRow.removeFromLeft(132));
    comboRow.removeFromLeft(10);
    slopeBox.setBounds(comboRow.removeFromLeft(92));
    comboRow.removeFromLeft(10);
    stereoModeBox.setBounds(comboRow.removeFromLeft(112));

    typeArea.removeFromTop(10);
    auto readoutRow = typeArea.removeFromTop(20);
    freqReadout.setBounds(readoutRow.removeFromLeft(96));
    gainReadout.setBounds(readoutRow.removeFromLeft(96));
    qReadout.setBounds(readoutRow.removeFromLeft(80));

    auto bandArea = bandPanelBounds.reduced(14);
    auto bandHeader = bandArea.removeFromTop(18);
    freqLabel.setBounds(bandHeader.removeFromLeft(90));
    gainLabel.setBounds(bandHeader.removeFromLeft(90));
    qLabel.setBounds(bandHeader);
    bandArea.removeFromTop(6);

    auto knobs = bandArea.removeFromTop(98);
    const int knobWidth = juce::jmin(110, knobs.getWidth() / 3);
    frequencySlider.setBounds(knobs.removeFromLeft(knobWidth));
    gainSlider.setBounds(knobs.removeFromLeft(knobWidth));
    qSlider.setBounds(knobs.removeFromLeft(knobWidth));

    auto statusArea = statusPanelBounds.reduced(14);
    auto statusRowOne = statusArea.removeFromTop(18);
    processingLabel.setBounds(statusRowOne.removeFromLeft(126));
    analyzerLabel.setBounds(statusRowOne.removeFromLeft(108));
    resolutionLabel.setBounds(statusRowOne.removeFromLeft(130));
    decayLabel.setBounds(statusRowOne.removeFromLeft(110));
    outputLabel.setBounds(statusRowOne);
    statusArea.removeFromTop(6);

    auto statusControls = statusArea.removeFromTop(32);
    processingModeBox.setBounds(statusControls.removeFromLeft(126));
    statusControls.removeFromLeft(10);
    analyzerModeBox.setBounds(statusControls.removeFromLeft(108));
    statusControls.removeFromLeft(10);
    analyzerResolutionBox.setBounds(statusControls.removeFromLeft(110));
    statusControls.removeFromLeft(10);
    analyzerDecaySlider.setBounds(statusControls.removeFromLeft(170));
    statusControls.removeFromLeft(10);
    globalBypassToggle.setBounds(statusControls.removeFromLeft(120));
    outputReadout.setBounds(statusControls.removeFromLeft(120));
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

    processingModeBox.setSelectedItemIndex(processor.getProcessingMode(), juce::dontSendNotification);
    analyzerModeBox.setSelectedItemIndex(processor.getAnalyzerMode(), juce::dontSendNotification);
    analyzerResolutionBox.setSelectedItemIndex(processor.getAnalyzerResolution(), juce::dontSendNotification);
    analyzerDecaySlider.setValue(processor.getAnalyzerDecay(), juce::dontSendNotification);
    globalBypassToggle.setToggleState(processor.isGlobalBypassed(), juce::dontSendNotification);
    outputReadout.setText(juce::String(processor.getOutputGainDb(), 1) + " dB", juce::dontSendNotification);

    if (selectedPoint >= 0)
    {
        const auto point = processor.getPoint(selectedPoint);
        if (point.enabled)
        {
            selectedLabel.setText("Point " + juce::String(selectedPoint + 1), juce::dontSendNotification);
            helperLabel.setText("Drag in the graph for frequency and gain, scroll on the node for Q, and tailor the band below.", juce::dontSendNotification);
            typeBox.setSelectedItemIndex(point.type, juce::dontSendNotification);
            slopeBox.setSelectedItemIndex(point.slopeIndex, juce::dontSendNotification);
            stereoModeBox.setSelectedItemIndex(point.stereoMode, juce::dontSendNotification);
            frequencySlider.setValue(point.frequency, juce::dontSendNotification);
            gainSlider.setValue(point.gainDb, juce::dontSendNotification);
            qSlider.setValue(point.q, juce::dontSendNotification);
            pointEnableToggle.setToggleState(point.enabled, juce::dontSendNotification);
            freqReadout.setText(juce::String(point.frequency, point.frequency >= 1000.0f ? 1 : 0) + " Hz", juce::dontSendNotification);
            gainReadout.setText((point.gainDb > 0.0f ? "+" : "") + juce::String(point.gainDb, 1) + " dB", juce::dontSendNotification);
            qReadout.setText("Q " + juce::String(point.q, 2), juce::dontSendNotification);
            syncing = false;
            updateEnabledState();
            return;
        }
    }

    selectedLabel.setText("No Point Selected", juce::dontSendNotification);
    helperLabel.setText("Double-click the graph to add a band, then use the controls below to fine-tune it.", juce::dontSendNotification);
    typeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    slopeBox.setSelectedItemIndex(1, juce::dontSendNotification);
    stereoModeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    frequencySlider.setValue(1000.0, juce::dontSendNotification);
    gainSlider.setValue(0.0, juce::dontSendNotification);
    qSlider.setValue(1.0, juce::dontSendNotification);
    pointEnableToggle.setToggleState(false, juce::dontSendNotification);
    freqReadout.setText("--", juce::dontSendNotification);
    gainReadout.setText("--", juce::dontSendNotification);
    qReadout.setText("--", juce::dontSendNotification);
    syncing = false;
    updateEnabledState();
}

void EqControlsComponent::updateEnabledState()
{
    const bool hasPoint = selectedPoint >= 0 && processor.getPoint(selectedPoint).enabled;
    typeBox.setEnabled(hasPoint);
    slopeBox.setEnabled(hasPoint);
    stereoModeBox.setEnabled(hasPoint);
    frequencySlider.setEnabled(hasPoint);
    gainSlider.setEnabled(hasPoint);
    qSlider.setEnabled(hasPoint);
    pointEnableToggle.setEnabled(hasPoint);
    removeButton.setEnabled(hasPoint);
}

void EqControlsComponent::commitPointChange()
{
    if (syncing || selectedPoint < 0)
        return;

    auto point = processor.getPoint(selectedPoint);
    point.enabled = pointEnableToggle.getToggleState();
    point.type = typeBox.getSelectedItemIndex();
    point.slopeIndex = slopeBox.getSelectedItemIndex();
    point.stereoMode = stereoModeBox.getSelectedItemIndex();
    point.frequency = (float) frequencySlider.getValue();
    point.gainDb = (float) gainSlider.getValue();
    point.q = (float) qSlider.getValue();
    processor.setPoint(selectedPoint, point);
}

void EqControlsComponent::selectRelativePoint(int delta)
{
    auto points = processor.getPointsSnapshot();
    if (processor.getEnabledPointCount() == 0)
        return;

    int start = selectedPoint;
    if (start < 0)
        start = delta > 0 ? -1 : PluginProcessor::maxPoints;

    for (int step = 0; step < PluginProcessor::maxPoints; ++step)
    {
        int index = (start + delta * (step + 1) + PluginProcessor::maxPoints) % PluginProcessor::maxPoints;
        if (points[(size_t) index].enabled)
        {
            selectedPoint = index;
            refreshFromPoint();
            if (onSelectionChanged)
                onSelectionChanged(index);
            return;
        }
    }
}

void EqControlsComponent::styleComboBox(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111822));
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2b3e52));
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff9de7ff));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
}

void EqControlsComponent::styleLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff8da4bb));
    label.setJustificationType(juce::Justification::centredLeft);
}
