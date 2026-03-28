#include "EqControlsComponent.h"
#include "MusicNoteUtils.h"

namespace
{
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
    setWantsKeyboardFocus(true);

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

    for (auto* label : {&typeLabel, &slopeLabel, &stereoLabel, &freqLabel, &gainLabel, &qLabel,
                        &processingLabel, &analyzerLabel, &resolutionLabel, &decayLabel, &outputLabel, &pointStateLabel})
        addAndMakeVisible(*label);

    styleReadout(gainReadout);
    styleReadout(qReadout);
    styleReadout(outputReadout);
    addAndMakeVisible(gainReadout);
    addAndMakeVisible(qReadout);
    addAndMakeVisible(outputReadout);

    for (auto* box : {&typeBox, &slopeBox, &stereoModeBox, &processingModeBox, &analyzerModeBox, &analyzerResolutionBox})
        addAndMakeVisible(*box);

    typeBox.addItemList(PluginProcessor::getPointTypeDisplayNames(), 1);
    slopeBox.addItemList(PluginProcessor::getSlopeNames(), 1);
    stereoModeBox.addItemList(PluginProcessor::getStereoModeNames(), 1);
    processingModeBox.addItemList(PluginProcessor::getProcessingModeNames(), 1);
    analyzerModeBox.addItemList(PluginProcessor::getAnalyzerModeNames(), 1);
    analyzerResolutionBox.addItemList(PluginProcessor::getAnalyzerResolutionNames(), 1);

    frequencySlider.setRange(20.0, 20000.0, 0.01);
    frequencySlider.setSkewFactor(0.25);
    frequencySlider.setDoubleClickReturnValue(true, 1000.0);
    gainSlider.setRange(-24.0, 24.0, 0.01);
    gainSlider.setDoubleClickReturnValue(true, 0.0);
    qSlider.setRange(0.2, 18.0, 0.01);
    qSlider.setSkewFactor(0.35);
    qSlider.setDoubleClickReturnValue(true, 1.0);

    for (auto* knob : {&frequencySlider, &gainSlider, &qSlider})
    {
        knob->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff63d0ff));
        knob->setColour(juce::Slider::thumbColourId, juce::Colour(0xff9de7ff));
        knob->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        knob->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111822));
        addAndMakeVisible(*knob);
    }

    analyzerDecaySlider.setLookAndFeel(&ssp::ui::getVectorLookAndFeel());
    analyzerDecaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerDecaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
    analyzerDecaySlider.setRange(0.05, 0.98, 0.01);
    analyzerDecaySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff63d0ff));
    analyzerDecaySlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9de7ff));
    analyzerDecaySlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111822));
    analyzerDecaySlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(analyzerDecaySlider);

    frequencyInput.setFont(juce::Font(13.0f, juce::Font::bold));
    frequencyInput.setTextToShowWhenEmpty("Enter Hz or note", juce::Colour(0xff6f849b));
    frequencyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111822));
    frequencyInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff31475d));
    frequencyInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff63d0ff));
    frequencyInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    frequencyInput.onReturnKey = [this] { commitFrequencyText(); };
    frequencyInput.onEscapeKey = [this]
    {
        refreshFromPoint();
        noteSuggestionList.setVisible(false);
    };
    frequencyInput.onTextChange = [this] { refreshFrequencySuggestions(); };
    frequencyInput.onFocusLost = [this]
    {
        commitFrequencyText();
        noteSuggestionList.setVisible(false);
    };
    addAndMakeVisible(frequencyInput);

    noteSuggestionList.setModel(this);
    noteSuggestionList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff111822));
    noteSuggestionList.setRowHeight(20);
    noteSuggestionList.setVisible(false);
    addAndMakeVisible(noteSuggestionList);

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
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("processingMode")))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) processingModeBox.getSelectedItemIndex()));
    };

    analyzerModeBox.onChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("analyzerMode")))
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) analyzerModeBox.getSelectedItemIndex()));
    };

    analyzerResolutionBox.onChange = [this]
    {
        if (syncing)
            return;
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("analyzerResolution")))
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

    soloButton.onClick = [this]
    {
        if (selectedPoint >= 0)
            processor.toggleSoloPoint(selectedPoint);
        refreshFromPoint();
    };

    previousBandButton.onClick = [this] { selectRelativePoint(-1); };
    nextBandButton.onClick = [this] { selectRelativePoint(1); };
    addAndMakeVisible(pointEnableToggle);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(globalBypassToggle);
    addAndMakeVisible(previousBandButton);
    addAndMakeVisible(nextBandButton);

    startTimerHz(24);
    refreshFromPoint();
}

bool EqControlsComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N')
    {
        focusFrequencyInput();
        return true;
    }

    return juce::Component::keyPressed(key);
}

void EqControlsComponent::focusFrequencyInput()
{
    frequencyInput.grabKeyboardFocus();
    frequencyInput.selectAll();
    refreshFrequencySuggestions();
}

int EqControlsComponent::getNumRows()
{
    return noteSuggestions.size();
}

void EqControlsComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    g.fillAll(rowIsSelected ? juce::Colour(0xff1a2a3c) : juce::Colour(0xff111822));
    g.setColour(rowIsSelected ? juce::Colour(0xff9de7ff) : juce::Colours::white);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    if (juce::isPositiveAndBelow(rowNumber, noteSuggestions.size()))
        g.drawText(noteSuggestions[rowNumber], 8, 0, width - 16, height, juce::Justification::centredLeft, false);
}

void EqControlsComponent::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    applySuggestion(row);
}

void EqControlsComponent::returnKeyPressed(int row)
{
    applySuggestion(row);
}

void EqControlsComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff63d0ff), 18.0f);
    ssp::ui::drawPanelBackground(g, selectedPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    ssp::ui::drawPanelBackground(g, typePanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    ssp::ui::drawPanelBackground(g, bandPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    ssp::ui::drawPanelBackground(g, statusPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
}

void EqControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(42);
    selectedLabel.setBounds(topRow.removeFromLeft(250));
    helperLabel.setBounds(topRow);

    area.removeFromTop(10);
    auto rowOne = area.removeFromTop(160);
    selectedPanelBounds = rowOne.removeFromLeft(180);
    rowOne.removeFromLeft(14);
    typePanelBounds = rowOne.removeFromLeft(360);
    rowOne.removeFromLeft(14);
    bandPanelBounds = rowOne;

    area.removeFromTop(12);
    statusPanelBounds = area;

    auto selectedArea = selectedPanelBounds.reduced(14);
    pointStateLabel.setBounds(selectedArea.removeFromTop(18));
    selectedArea.removeFromTop(8);
    auto toggleRow = selectedArea.removeFromTop(28);
    pointEnableToggle.setBounds(toggleRow.removeFromLeft(116));
    toggleRow.removeFromLeft(8);
    soloButton.setBounds(toggleRow.removeFromLeft(34));
    selectedArea.removeFromTop(12);
    auto navRow = selectedArea.removeFromTop(30);
    previousBandButton.setBounds(navRow.removeFromLeft(38));
    navRow.removeFromLeft(8);
    nextBandButton.setBounds(navRow.removeFromLeft(38));

    auto typeArea = typePanelBounds.reduced(14);
    auto typeRow = typeArea.removeFromTop(18);
    typeLabel.setBounds(typeRow.removeFromLeft(120));
    slopeLabel.setBounds(typeRow.removeFromLeft(88));
    stereoLabel.setBounds(typeRow);
    typeArea.removeFromTop(6);

    auto comboRow = typeArea.removeFromTop(34);
    typeBox.setBounds(comboRow.removeFromLeft(140));
    comboRow.removeFromLeft(10);
    slopeBox.setBounds(comboRow.removeFromLeft(96));
    comboRow.removeFromLeft(10);
    stereoModeBox.setBounds(comboRow.removeFromLeft(116));
    typeArea.removeFromTop(10);

    freqLabel.setBounds(typeArea.removeFromTop(18));
    typeArea.removeFromTop(4);
    frequencyInput.setBounds(typeArea.removeFromTop(30));
    noteSuggestionList.setBounds(frequencyInput.getX(), frequencyInput.getBottom() + 4, frequencyInput.getWidth(), juce::jmin(108, noteSuggestions.size() * 20 + 2));

    auto modeReadouts = typeArea.removeFromTop(22);
    gainReadout.setBounds(modeReadouts.removeFromLeft(110));
    qReadout.setBounds(modeReadouts.removeFromLeft(90));

    auto bandArea = bandPanelBounds.reduced(14);
    auto bandHeader = bandArea.removeFromTop(18);
    gainLabel.setBounds(bandHeader.removeFromLeft(90));
    qLabel.setBounds(bandHeader.removeFromLeft(90));
    bandArea.removeFromTop(6);

    auto knobs = bandArea.removeFromTop(112);
    const int knobWidth = juce::jmin(118, knobs.getWidth() / 3);
    frequencySlider.setBounds(knobs.removeFromLeft(knobWidth));
    gainSlider.setBounds(knobs.removeFromLeft(knobWidth));
    qSlider.setBounds(knobs.removeFromLeft(knobWidth));

    auto statusArea = statusPanelBounds.reduced(14);
    auto statusLabels = statusArea.removeFromTop(18);
    processingLabel.setBounds(statusLabels.removeFromLeft(126));
    analyzerLabel.setBounds(statusLabels.removeFromLeft(108));
    resolutionLabel.setBounds(statusLabels.removeFromLeft(130));
    decayLabel.setBounds(statusLabels.removeFromLeft(110));
    outputLabel.setBounds(statusLabels);
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
    globalBypassToggle.setBounds(statusControls.removeFromLeft(124));
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
    if (!frequencyInput.hasKeyboardFocus(true))
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
            helperLabel.setText("Drag in the graph for placement, press N to type a note, and use the vector controls below to refine the band.", juce::dontSendNotification);
            typeBox.setSelectedItemIndex(point.type, juce::dontSendNotification);
            slopeBox.setSelectedItemIndex(point.slopeIndex, juce::dontSendNotification);
            stereoModeBox.setSelectedItemIndex(point.stereoMode, juce::dontSendNotification);
            frequencySlider.setValue(point.frequency, juce::dontSendNotification);
            gainSlider.setValue(point.gainDb, juce::dontSendNotification);
            qSlider.setValue(point.q, juce::dontSendNotification);
            pointEnableToggle.setToggleState(point.enabled, juce::dontSendNotification);
            soloButton.setToggleState(processor.getSoloPointIndex() == selectedPoint, juce::dontSendNotification);
            if (!frequencyInput.hasKeyboardFocus(true))
                frequencyInput.setText(ssp::notes::formatFrequencyWithNote(point.frequency), juce::dontSendNotification);
            gainReadout.setText((point.gainDb > 0.0f ? "+" : "") + juce::String(point.gainDb, 1) + " dB", juce::dontSendNotification);
            qReadout.setText("Q " + juce::String(point.q, 2), juce::dontSendNotification);
            syncing = false;
            updateEnabledState();
            resized();
            return;
        }
    }

    selectedLabel.setText("No Point Selected", juce::dontSendNotification);
    helperLabel.setText("Double-click the graph to add a band, then type a note or drag the vector knob to set frequency.", juce::dontSendNotification);
    typeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    slopeBox.setSelectedItemIndex(1, juce::dontSendNotification);
    stereoModeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    frequencySlider.setValue(1000.0, juce::dontSendNotification);
    gainSlider.setValue(0.0, juce::dontSendNotification);
    qSlider.setValue(1.0, juce::dontSendNotification);
    pointEnableToggle.setToggleState(false, juce::dontSendNotification);
    soloButton.setToggleState(false, juce::dontSendNotification);
    if (!frequencyInput.hasKeyboardFocus(true))
        frequencyInput.setText({}, juce::dontSendNotification);
    gainReadout.setText("--", juce::dontSendNotification);
    qReadout.setText("--", juce::dontSendNotification);
    noteSuggestionList.setVisible(false);
    syncing = false;
    updateEnabledState();
    resized();
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
    frequencyInput.setEnabled(hasPoint);
    pointEnableToggle.setEnabled(hasPoint);
    soloButton.setEnabled(hasPoint);
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

void EqControlsComponent::styleComboBox(juce::ComboBox&) {}

void EqControlsComponent::styleLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff8da4bb));
    label.setJustificationType(juce::Justification::centredLeft);
}

void EqControlsComponent::commitFrequencyText()
{
    if (syncing || selectedPoint < 0)
        return;

    double parsedFrequency = 0.0;
    if (!ssp::notes::tryParseFrequencyInput(frequencyInput.getText(), parsedFrequency))
        return;

    auto point = processor.getPoint(selectedPoint);
    point.frequency = (float) juce::jlimit(20.0, 20000.0, parsedFrequency);
    processor.setPoint(selectedPoint, point);
    noteSuggestionList.setVisible(false);
    refreshFromPoint();
}

void EqControlsComponent::refreshFrequencySuggestions()
{
    if (!frequencyInput.hasKeyboardFocus(true))
        return;

    noteSuggestions = ssp::notes::buildNoteSuggestions(frequencyInput.getText());
    noteSuggestionList.updateContent();
    noteSuggestionList.setVisible(!noteSuggestions.isEmpty());
    resized();
}

void EqControlsComponent::applySuggestion(int row)
{
    if (!juce::isPositiveAndBelow(row, noteSuggestions.size()))
        return;

    const auto token = noteSuggestions[row].upToFirstOccurrenceOf(" ", false, false);
    frequencyInput.setText(token, juce::dontSendNotification);
    commitFrequencyText();
    frequencyInput.grabKeyboardFocus();
}
