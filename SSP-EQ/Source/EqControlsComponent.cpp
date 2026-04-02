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
    styleLabel(latencyLabel, "Latency");
    styleLabel(analyzerLabel, "Analyzer Mode");
    styleLabel(resolutionLabel, "Analyzer Resolution");
    styleLabel(decayLabel, "Analyzer Decay");
    styleLabel(outputLabel, "Output");
    styleLabel(pointStateLabel, "Band State");
    styleLabel(dynamicSectionLabel, "Dynamic EQ");
    styleLabel(thresholdLabel, "Threshold");
    styleLabel(ratioLabel, "Ratio");
    styleLabel(attackLabel, "Attack");
    styleLabel(releaseLabel, "Release");
    styleLabel(kneeLabel, "Knee");
    styleLabel(rangeLabel, "Range");
    styleLabel(sidechainSourceLabel, "SC Source");
    styleLabel(sidechainFilterLabel, "Detector Filters");
    styleLabel(gainReductionLabel, "Dynamic Meter");

    for (auto* label : {&typeLabel, &slopeLabel, &stereoLabel, &freqLabel, &gainLabel, &qLabel,
                        &processingLabel, &latencyLabel, &analyzerLabel, &resolutionLabel, &decayLabel, &outputLabel,
                        &pointStateLabel, &dynamicSectionLabel, &thresholdLabel, &ratioLabel, &attackLabel,
                        &releaseLabel, &kneeLabel, &rangeLabel, &sidechainSourceLabel, &sidechainFilterLabel,
                        &gainReductionLabel})
        addAndMakeVisible(*label);

    styleReadout(gainReadout);
    styleReadout(qReadout);
    styleReadout(outputReadout);
    styleReadout(latencyReadout);
    styleReadout(sidechainHighPassReadout);
    styleReadout(sidechainLowPassReadout);
    addAndMakeVisible(gainReadout);
    addAndMakeVisible(qReadout);
    addAndMakeVisible(outputReadout);
    addAndMakeVisible(latencyReadout);
    addAndMakeVisible(sidechainHighPassReadout);
    addAndMakeVisible(sidechainLowPassReadout);

    for (auto* box : {&typeBox, &slopeBox, &stereoModeBox, &processingModeBox, &analyzerModeBox, &analyzerResolutionBox,
                      &sidechainSourceBox, &sidechainBandBox})
        addAndMakeVisible(*box);

    typeBox.addItemList(PluginProcessor::getPointTypeDisplayNames(), 1);
    slopeBox.addItemList(PluginProcessor::getSlopeNames(), 1);
    stereoModeBox.addItemList(PluginProcessor::getStereoModeNames(), 1);
    processingModeBox.addItemList(PluginProcessor::getProcessingModeNames(), 1);
    analyzerModeBox.addItemList(PluginProcessor::getAnalyzerModeNames(), 1);
    analyzerResolutionBox.addItemList(PluginProcessor::getAnalyzerResolutionNames(), 1);
    sidechainSourceBox.addItemList(PluginProcessor::getSidechainSourceNames(), 1);
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
        sidechainBandBox.addItem("Band " + juce::String(i + 1), i + 1);

    frequencySlider.setRange(20.0, 20000.0, 0.01);
    frequencySlider.setSkewFactor(0.25);
    frequencySlider.setDoubleClickReturnValue(true, 1000.0);
    frequencySlider.textFromValueFunction = [](double value)
    {
        return ssp::notes::formatFrequencyWithNote((float) value);
    };
    gainSlider.setRange(-24.0, 24.0, 0.01);
    gainSlider.setDoubleClickReturnValue(true, 0.0);
    gainSlider.setTextValueSuffix(" dB");
    qSlider.setRange(0.2, 18.0, 0.01);
    qSlider.setSkewFactor(0.35);
    qSlider.setDoubleClickReturnValue(true, 1.0);
    thresholdSlider.setRange(-60.0, 0.0, 0.01);
    thresholdSlider.setDoubleClickReturnValue(true, -24.0);
    thresholdSlider.setTextValueSuffix(" dB");
    ratioSlider.setRange(1.0, 20.0, 0.01);
    ratioSlider.setSkewFactor(0.35);
    ratioSlider.setDoubleClickReturnValue(true, 4.0);
    ratioSlider.textFromValueFunction = [](double value)
    {
        return value >= 19.95 ? "Inf:1" : juce::String(value, value < 10.0 ? 2 : 1) + ":1";
    };
    attackSlider.setRange(0.1, 200.0, 0.01);
    attackSlider.setSkewFactor(0.3);
    attackSlider.setDoubleClickReturnValue(true, 10.0);
    attackSlider.setTextValueSuffix(" ms");
    releaseSlider.setRange(10.0, 2000.0, 0.1);
    releaseSlider.setSkewFactor(0.3);
    releaseSlider.setDoubleClickReturnValue(true, 100.0);
    releaseSlider.setTextValueSuffix(" ms");
    kneeSlider.setRange(0.0, 12.0, 0.01);
    kneeSlider.setDoubleClickReturnValue(true, 6.0);
    rangeSlider.setRange(0.0, 24.0, 0.01);
    rangeSlider.setDoubleClickReturnValue(true, 24.0);
    rangeSlider.textFromValueFunction = [this](double value)
    {
        const bool negativeRange = selectedPoint >= 0 && processor.getPoint(selectedPoint).gainDb < 0.0f;
        const float signedValue = negativeRange ? -(float) value : (float) value;
        return (signedValue > 0.0f ? "+" : "") + juce::String(signedValue, 1) + " dB";
    };
    sidechainHighPassSlider.setRange(20.0, 20000.0, 0.01);
    sidechainHighPassSlider.setSkewFactor(0.25);
    sidechainHighPassSlider.setDoubleClickReturnValue(true, 20.0);
    sidechainLowPassSlider.setRange(20.0, 20000.0, 0.01);
    sidechainLowPassSlider.setSkewFactor(0.25);
    sidechainLowPassSlider.setDoubleClickReturnValue(true, 20000.0);

    for (auto* knob : {&frequencySlider, &gainSlider, &qSlider, &thresholdSlider, &ratioSlider, &attackSlider,
                       &releaseSlider, &kneeSlider, &rangeSlider, &sidechainHighPassSlider, &sidechainLowPassSlider})
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
    thresholdSlider.onValueChange = commitPoint;
    ratioSlider.onValueChange = commitPoint;
    attackSlider.onValueChange = commitPoint;
    releaseSlider.onValueChange = commitPoint;
    kneeSlider.onValueChange = commitPoint;
    rangeSlider.onValueChange = commitPoint;
    sidechainHighPassSlider.onValueChange = commitPoint;
    sidechainLowPassSlider.onValueChange = commitPoint;
    sidechainSourceBox.onChange = commitPoint;
    sidechainBandBox.onChange = commitPoint;

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

    dynamicToggle.setClickingTogglesState(true);
    dynamicToggle.onClick = [this]
    {
        commitPointChange();
        dynamicSectionTarget = dynamicToggle.getToggleState() ? 1.0f : 0.0f;
        if (dynamicSectionTarget > 0.5f)
            dynamicSectionAmount = juce::jmax(dynamicSectionAmount, 0.12f);
        refreshFromPoint();
        repaint();
    };
    dynamicAboveButton.setClickingTogglesState(true);
    dynamicBelowButton.setClickingTogglesState(true);
    dynamicAboveButton.onClick = [this] { commitDynamicDirection(PluginProcessor::dynamicAbove); };
    dynamicBelowButton.onClick = [this] { commitDynamicDirection(PluginProcessor::dynamicBelow); };
    sidechainListenButton.setClickingTogglesState(true);
    sidechainListenButton.onClick = [this]
    {
        processor.setBandSidechainListen(selectedPoint, sidechainListenButton.getToggleState());
        refreshFromPoint();
    };

    previousBandButton.onClick = [this] { selectRelativePoint(-1); };
    nextBandButton.onClick = [this] { selectRelativePoint(1); };
    addAndMakeVisible(pointEnableToggle);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(dynamicToggle);
    addAndMakeVisible(dynamicAboveButton);
    addAndMakeVisible(dynamicBelowButton);
    addAndMakeVisible(sidechainListenButton);
    addAndMakeVisible(globalBypassToggle);
    addAndMakeVisible(previousBandButton);
    addAndMakeVisible(nextBandButton);

    for (auto* component : {
             static_cast<juce::Component*>(&kneeLabel),
             static_cast<juce::Component*>(&sidechainSourceLabel),
             static_cast<juce::Component*>(&sidechainFilterLabel),
             static_cast<juce::Component*>(&gainReductionLabel),
             static_cast<juce::Component*>(&kneeSlider),
             static_cast<juce::Component*>(&sidechainSourceBox),
             static_cast<juce::Component*>(&sidechainBandBox),
             static_cast<juce::Component*>(&sidechainListenButton),
             static_cast<juce::Component*>(&sidechainHighPassSlider),
             static_cast<juce::Component*>(&sidechainLowPassSlider),
             static_cast<juce::Component*>(&sidechainHighPassReadout),
             static_cast<juce::Component*>(&sidechainLowPassReadout),
             static_cast<juce::Component*>(&dynamicAboveButton),
             static_cast<juce::Component*>(&dynamicBelowButton) })
        component->setVisible(false);

    startTimerHz(60);
    refreshFromPoint();
}

bool EqControlsComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N')
    {
        focusFrequencyInput();
        return true;
    }

    if ((key.getTextCharacter() == 'd' || key.getTextCharacter() == 'D') && selectedPoint >= 0)
    {
        auto point = processor.getPoint(selectedPoint);
        if (point.enabled)
        {
            point.dynamicEnabled = ! point.dynamicEnabled;
            processor.setPoint(selectedPoint, point);
            refreshFromPoint();
            return true;
        }
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
    if (! selectedPanelBounds.isEmpty())
        ssp::ui::drawPanelBackground(g, selectedPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    if (! typePanelBounds.isEmpty())
        ssp::ui::drawPanelBackground(g, typePanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    if (! bandPanelBounds.isEmpty())
        ssp::ui::drawPanelBackground(g, bandPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);
    if (dynamicPanelBounds.getHeight() > 8)
    {
        ssp::ui::drawPanelBackground(g, dynamicPanelBounds.toFloat(), juce::Colour(0xff3ddfb5), 16.0f);
        g.setColour(juce::Colour(0xff567085).withAlpha(0.72f));
        g.drawLine((float) dynamicPanelBounds.getX() + 14.0f,
                   (float) dynamicPanelBounds.getY() + 8.0f,
                   (float) dynamicPanelBounds.getRight() - 14.0f,
                   (float) dynamicPanelBounds.getY() + 8.0f,
                   1.0f);
    }
    ssp::ui::drawPanelBackground(g, statusPanelBounds.toFloat(), juce::Colour(0xff63d0ff), 16.0f);

}

void EqControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(42);
    selectedLabel.setBounds(topRow.removeFromLeft(250));
    helperLabel.setBounds(topRow);

    area.removeFromTop(10);
    const bool showBandRows = hasSelectedBand();
    auto rowOne = area.removeFromTop(showBandRows ? 160 : 0);
    if (showBandRows)
    {
        selectedPanelBounds = rowOne.removeFromLeft(180);
        rowOne.removeFromLeft(14);
        typePanelBounds = rowOne.removeFromLeft(360);
        rowOne.removeFromLeft(14);
        bandPanelBounds = rowOne;
    }
    else
    {
        selectedPanelBounds = {};
        typePanelBounds = {};
        bandPanelBounds = {};
    }

    area.removeFromTop(12);
    const int minStatusHeight = 96;
    const int dynamicTargetHeight = showBandRows ? juce::roundToInt(304.0f * dynamicSectionAmount) : 0;
    const int dynamicHeight = juce::jmin(dynamicTargetHeight, juce::jmax(0, area.getHeight() - minStatusHeight));
    dynamicPanelBounds = dynamicHeight > 0 ? area.removeFromTop(dynamicHeight) : juce::Rectangle<int>();
    if (dynamicHeight > 0)
        area.removeFromTop(12);
    statusPanelBounds = area;

    auto selectedArea = selectedPanelBounds.reduced(14);
    pointStateLabel.setBounds(selectedArea.removeFromTop(18));
    selectedArea.removeFromTop(8);
    auto toggleRow = selectedArea.removeFromTop(28);
    pointEnableToggle.setBounds(toggleRow.removeFromLeft(116));
    toggleRow.removeFromLeft(8);
    soloButton.setBounds(toggleRow.removeFromLeft(34));
    toggleRow.removeFromLeft(8);
    dynamicToggle.setBounds(toggleRow.removeFromLeft(56));
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

    if (dynamicHeight > 0)
    {
        auto dynamicArea = dynamicPanelBounds.reduced(18, 16);
        dynamicSectionLabel.setBounds(dynamicArea.removeFromTop(18));
        dynamicArea.removeFromTop(18);

        auto dynamicHeader = dynamicArea.removeFromTop(18);
        thresholdLabel.setBounds(dynamicHeader.removeFromLeft(84));
        attackLabel.setBounds(dynamicHeader.removeFromLeft(76));
        releaseLabel.setBounds(dynamicHeader.removeFromLeft(76));
        ratioLabel.setBounds(dynamicHeader.removeFromLeft(76));
        rangeLabel.setBounds(dynamicHeader.removeFromLeft(76));
        dynamicArea.removeFromTop(16);

        auto dynamicControls = dynamicArea.removeFromTop(150);
        const int dynamicKnobWidth = juce::jmin(96, juce::jmax(86, dynamicControls.getWidth() / 5));
        thresholdSlider.setBounds(dynamicControls.removeFromLeft(dynamicKnobWidth));
        attackSlider.setBounds(dynamicControls.removeFromLeft(dynamicKnobWidth));
        releaseSlider.setBounds(dynamicControls.removeFromLeft(dynamicKnobWidth));
        ratioSlider.setBounds(dynamicControls.removeFromLeft(dynamicKnobWidth));
        rangeSlider.setBounds(dynamicControls.removeFromLeft(dynamicKnobWidth));
        gainReductionMeterBounds = {};
    }
    else
    {
        gainReductionMeterBounds = {};
    }

    auto statusArea = statusPanelBounds.reduced(14);
    auto statusLabels = statusArea.removeFromTop(18);
    processingLabel.setBounds(statusLabels.removeFromLeft(126));
    latencyLabel.setBounds(statusLabels.removeFromLeft(96));
    analyzerLabel.setBounds(statusLabels.removeFromLeft(108));
    resolutionLabel.setBounds(statusLabels.removeFromLeft(130));
    decayLabel.setBounds(statusLabels.removeFromLeft(110));
    outputLabel.setBounds(statusLabels);
    statusArea.removeFromTop(6);

    auto statusControls = statusArea.removeFromTop(32);
    processingModeBox.setBounds(statusControls.removeFromLeft(126));
    statusControls.removeFromLeft(10);
    latencyReadout.setBounds(statusControls.removeFromLeft(110));
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
    const bool wasDynamic = selectedBandIsDynamic();
    selectedPoint = index;
    const bool isDynamic = selectedBandIsDynamic();
    dynamicSectionTarget = isDynamic ? 1.0f : 0.0f;
    if (wasDynamic && isDynamic)
        dynamicSectionAmount = 1.0f;
    refreshFromPoint();
}

void EqControlsComponent::timerCallback()
{
    if (!frequencyInput.hasKeyboardFocus(true))
        refreshFromPoint();

    dynamicSectionAmount += (dynamicSectionTarget - dynamicSectionAmount) * 0.25f;
    if (std::abs(dynamicSectionAmount - dynamicSectionTarget) <= 0.01f)
        dynamicSectionAmount = dynamicSectionTarget;
    updateDynamicSectionVisibility();
    if (std::abs(dynamicSectionAmount - dynamicSectionTarget) > 0.01f)
    {
        resized();
        repaint();
    }
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
    latencyReadout.setText(juce::String(processor.getCurrentLatencySamples()) + " smp / " + juce::String(processor.getCurrentLatencyMs(), 1) + " ms",
                           juce::dontSendNotification);

    if (selectedPoint >= 0)
    {
        const auto point = processor.getPoint(selectedPoint);
        if (point.enabled)
        {
            rangeSlider.setRange(0.0, juce::jmax(0.01, std::abs((double) point.gainDb)), 0.01);
            rangeSlider.setDoubleClickReturnValue(true, std::abs((double) point.gainDb));
            selectedLabel.setText("Point " + juce::String(selectedPoint + 1), juce::dontSendNotification);
            auto helperText = juce::String("Drag in the graph for placement, press N to type a note, and use the vector controls below to refine the band.");
            if (point.dynamicEnabled && processor.getProcessingMode() == PluginProcessor::linearPhase)
                helperText = "Dynamic bands use minimum-phase correction on top of the linear-phase FIR path.";
            helperLabel.setText(helperText, juce::dontSendNotification);
            typeBox.setSelectedItemIndex(point.type, juce::dontSendNotification);
            slopeBox.setSelectedItemIndex(point.slopeIndex, juce::dontSendNotification);
            stereoModeBox.setSelectedItemIndex(point.stereoMode, juce::dontSendNotification);
            frequencySlider.setValue(point.frequency, juce::dontSendNotification);
            gainSlider.setValue(point.gainDb, juce::dontSendNotification);
            qSlider.setValue(point.q, juce::dontSendNotification);
            pointEnableToggle.setToggleState(point.enabled, juce::dontSendNotification);
            soloButton.setToggleState(processor.getSoloPointIndex() == selectedPoint, juce::dontSendNotification);
            dynamicToggle.setToggleState(point.dynamicEnabled, juce::dontSendNotification);
            thresholdSlider.setValue(point.dynamicThresholdDb, juce::dontSendNotification);
            ratioSlider.setValue(point.dynamicRatio, juce::dontSendNotification);
            attackSlider.setValue(point.dynamicAttackMs, juce::dontSendNotification);
            releaseSlider.setValue(point.dynamicReleaseMs, juce::dontSendNotification);
            kneeSlider.setValue(point.dynamicKneeDb, juce::dontSendNotification);
            rangeSlider.setValue(juce::jmin(std::abs(point.gainDb), point.dynamicRangeDb), juce::dontSendNotification);
            sidechainSourceBox.setSelectedItemIndex(point.sidechainSource, juce::dontSendNotification);
            sidechainBandBox.setSelectedItemIndex(point.sidechainBandIndex, juce::dontSendNotification);
            sidechainHighPassSlider.setValue(point.sidechainHighPassHz, juce::dontSendNotification);
            sidechainLowPassSlider.setValue(point.sidechainLowPassHz, juce::dontSendNotification);
            dynamicAboveButton.setToggleState(point.dynamicDirection == PluginProcessor::dynamicAbove, juce::dontSendNotification);
            dynamicBelowButton.setToggleState(point.dynamicDirection == PluginProcessor::dynamicBelow, juce::dontSendNotification);
            sidechainListenButton.setToggleState(processor.isBandSidechainListening(selectedPoint), juce::dontSendNotification);
            sidechainHighPassReadout.setText(ssp::notes::formatFrequencyWithNote(point.sidechainHighPassHz), juce::dontSendNotification);
            sidechainLowPassReadout.setText(ssp::notes::formatFrequencyWithNote(point.sidechainLowPassHz), juce::dontSendNotification);
            if (!frequencyInput.hasKeyboardFocus(true))
                frequencyInput.setText(ssp::notes::formatFrequencyWithNote(point.frequency), juce::dontSendNotification);
            gainReadout.setText((point.gainDb > 0.0f ? "+" : "") + juce::String(point.gainDb, 1) + " dB", juce::dontSendNotification);
            qReadout.setText("Q " + juce::String(point.q, 2), juce::dontSendNotification);
            dynamicSectionTarget = point.dynamicEnabled ? 1.0f : 0.0f;
            syncing = false;
            updateEnabledState();
            updateSectionVisibility();
            resized();
            return;
        }
    }

    selectedLabel.setText("No Point Selected", juce::dontSendNotification);
    helperLabel.setText("Double-click the graph to add a band, then type a note or drag the vector knob to set frequency.", juce::dontSendNotification);
    rangeSlider.setRange(0.0, 24.0, 0.01);
    rangeSlider.setDoubleClickReturnValue(true, 24.0);
    typeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    slopeBox.setSelectedItemIndex(1, juce::dontSendNotification);
    stereoModeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    frequencySlider.setValue(1000.0, juce::dontSendNotification);
    gainSlider.setValue(0.0, juce::dontSendNotification);
    qSlider.setValue(1.0, juce::dontSendNotification);
    pointEnableToggle.setToggleState(false, juce::dontSendNotification);
    soloButton.setToggleState(false, juce::dontSendNotification);
    dynamicToggle.setToggleState(false, juce::dontSendNotification);
    dynamicAboveButton.setToggleState(true, juce::dontSendNotification);
    dynamicBelowButton.setToggleState(false, juce::dontSendNotification);
    thresholdSlider.setValue(-24.0, juce::dontSendNotification);
    ratioSlider.setValue(4.0, juce::dontSendNotification);
    attackSlider.setValue(10.0, juce::dontSendNotification);
    releaseSlider.setValue(100.0, juce::dontSendNotification);
    kneeSlider.setValue(6.0, juce::dontSendNotification);
    rangeSlider.setValue(24.0, juce::dontSendNotification);
    sidechainSourceBox.setSelectedItemIndex(0, juce::dontSendNotification);
    sidechainBandBox.setSelectedItemIndex(0, juce::dontSendNotification);
    sidechainHighPassSlider.setValue(20.0, juce::dontSendNotification);
    sidechainLowPassSlider.setValue(20000.0, juce::dontSendNotification);
    sidechainListenButton.setToggleState(false, juce::dontSendNotification);
    sidechainHighPassReadout.setText("--", juce::dontSendNotification);
    sidechainLowPassReadout.setText("--", juce::dontSendNotification);
    if (!frequencyInput.hasKeyboardFocus(true))
        frequencyInput.setText({}, juce::dontSendNotification);
    gainReadout.setText("--", juce::dontSendNotification);
    qReadout.setText("--", juce::dontSendNotification);
    noteSuggestionList.setVisible(false);
    dynamicSectionTarget = 0.0f;
    syncing = false;
    updateEnabledState();
    updateSectionVisibility();
    resized();
}

void EqControlsComponent::updateEnabledState()
{
    const bool hasPoint = selectedPoint >= 0 && processor.getPoint(selectedPoint).enabled;
    const bool hasDynamicPoint = hasPoint && dynamicToggle.getToggleState();
    typeBox.setEnabled(hasPoint);
    slopeBox.setEnabled(hasPoint);
    stereoModeBox.setEnabled(hasPoint);
    frequencySlider.setEnabled(hasPoint);
    gainSlider.setEnabled(hasPoint);
    qSlider.setEnabled(hasPoint);
    frequencyInput.setEnabled(hasPoint);
    pointEnableToggle.setEnabled(hasPoint);
    soloButton.setEnabled(hasPoint);
    dynamicToggle.setEnabled(hasPoint);
    thresholdSlider.setEnabled(hasDynamicPoint);
    ratioSlider.setEnabled(hasDynamicPoint);
    attackSlider.setEnabled(hasDynamicPoint);
    releaseSlider.setEnabled(hasDynamicPoint);
    kneeSlider.setEnabled(hasDynamicPoint);
    rangeSlider.setEnabled(hasDynamicPoint);
    sidechainSourceBox.setEnabled(hasDynamicPoint);
    sidechainBandBox.setEnabled(hasDynamicPoint && sidechainSourceBox.getSelectedItemIndex() == PluginProcessor::sidechainBand);
    sidechainListenButton.setEnabled(hasDynamicPoint);
    sidechainHighPassSlider.setEnabled(hasDynamicPoint);
    sidechainLowPassSlider.setEnabled(hasDynamicPoint);
    dynamicAboveButton.setEnabled(hasDynamicPoint);
    dynamicBelowButton.setEnabled(hasDynamicPoint);
    updateDynamicSectionVisibility();
}

void EqControlsComponent::updateSectionVisibility()
{
    const bool showBandRows = hasSelectedBand();
    for (auto* component : {
             static_cast<juce::Component*>(&pointStateLabel),
             static_cast<juce::Component*>(&pointEnableToggle),
             static_cast<juce::Component*>(&soloButton),
             static_cast<juce::Component*>(&dynamicToggle),
             static_cast<juce::Component*>(&previousBandButton),
             static_cast<juce::Component*>(&nextBandButton),
             static_cast<juce::Component*>(&typeLabel),
             static_cast<juce::Component*>(&slopeLabel),
             static_cast<juce::Component*>(&stereoLabel),
             static_cast<juce::Component*>(&freqLabel),
             static_cast<juce::Component*>(&gainLabel),
             static_cast<juce::Component*>(&qLabel),
             static_cast<juce::Component*>(&typeBox),
             static_cast<juce::Component*>(&slopeBox),
             static_cast<juce::Component*>(&stereoModeBox),
             static_cast<juce::Component*>(&frequencyInput),
             static_cast<juce::Component*>(&gainReadout),
             static_cast<juce::Component*>(&qReadout),
             static_cast<juce::Component*>(&frequencySlider),
             static_cast<juce::Component*>(&gainSlider),
             static_cast<juce::Component*>(&qSlider) })
        component->setVisible(showBandRows);

    if (! showBandRows)
        noteSuggestionList.setVisible(false);

    updateDynamicSectionVisibility();
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
    point.dynamicEnabled = dynamicToggle.getToggleState();
    point.dynamicThresholdDb = (float) thresholdSlider.getValue();
    point.dynamicRatio = (float) ratioSlider.getValue();
    point.dynamicAttackMs = (float) attackSlider.getValue();
    point.dynamicReleaseMs = (float) releaseSlider.getValue();
    point.dynamicKneeDb = (float) kneeSlider.getValue();
    point.dynamicDirection = dynamicBelowButton.getToggleState() ? PluginProcessor::dynamicBelow : PluginProcessor::dynamicAbove;
    point.dynamicRangeDb = juce::jlimit(0.0f, std::abs(point.gainDb), (float) rangeSlider.getValue());
    point.sidechainSource = sidechainSourceBox.getSelectedItemIndex();
    point.sidechainBandIndex = sidechainBandBox.getSelectedItemIndex();
    point.sidechainHighPassHz = (float) sidechainHighPassSlider.getValue();
    point.sidechainLowPassHz = (float) juce::jmax(sidechainHighPassSlider.getValue() + 1.0, sidechainLowPassSlider.getValue());
    processor.setPoint(selectedPoint, point);
    dynamicSectionTarget = point.dynamicEnabled ? 1.0f : 0.0f;
    rangeSlider.setRange(0.0, juce::jmax(0.01, std::abs((double) point.gainDb)), 0.01);
    rangeSlider.setDoubleClickReturnValue(true, std::abs((double) point.gainDb));
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
            setSelectedPoint(index);
            if (onSelectionChanged)
                onSelectionChanged(index);
            return;
        }
    }
}

void EqControlsComponent::styleComboBox(juce::ComboBox&) {}

void EqControlsComponent::commitDynamicDirection(int direction)
{
    if (syncing || selectedPoint < 0)
        return;

    dynamicAboveButton.setToggleState(direction == PluginProcessor::dynamicAbove, juce::dontSendNotification);
    dynamicBelowButton.setToggleState(direction == PluginProcessor::dynamicBelow, juce::dontSendNotification);
    commitPointChange();
}

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

void EqControlsComponent::updateDynamicSectionVisibility()
{
    const bool visible = dynamicSectionAmount > 0.02f
        && hasSelectedBand()
        && dynamicToggle.getToggleState();

    for (auto* component : {
             static_cast<juce::Component*>(&dynamicSectionLabel),
             static_cast<juce::Component*>(&thresholdLabel),
             static_cast<juce::Component*>(&attackLabel),
             static_cast<juce::Component*>(&releaseLabel),
             static_cast<juce::Component*>(&ratioLabel),
             static_cast<juce::Component*>(&rangeLabel),
             static_cast<juce::Component*>(&thresholdSlider),
             static_cast<juce::Component*>(&attackSlider),
             static_cast<juce::Component*>(&releaseSlider),
             static_cast<juce::Component*>(&ratioSlider),
             static_cast<juce::Component*>(&rangeSlider) })
        component->setVisible(visible);
}

bool EqControlsComponent::hasSelectedBand() const
{
    return selectedPoint >= 0 && processor.getPoint(selectedPoint).enabled;
}

bool EqControlsComponent::selectedBandIsDynamic() const
{
    return hasSelectedBand() && processor.getPoint(selectedPoint).dynamicEnabled;
}
