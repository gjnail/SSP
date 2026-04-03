#include "SpectrumControlsComponent.h"

SpectrumControlsComponent::SpectrumControlsComponent(PluginProcessor& p)
    : processor(p)
{
    auto populateCombo = [](spectrumui::SSPDropdown& box, const juce::StringArray& values)
    {
        box.clear();
        for (int i = 0; i < values.size(); ++i)
            box.addItem(values[i], i + 1);
    };

    populateCombo(displayModeBox, PluginProcessor::getDisplayModeNames());
    populateCombo(fftSizeBox, PluginProcessor::getFftSizeNames());
    populateCombo(overlapBox, PluginProcessor::getOverlapNames());
    populateCombo(windowBox, PluginProcessor::getWindowNames());
    populateCombo(themeBox, PluginProcessor::getThemeNames());
    populateCombo(presetBox, [&]()
    {
        juce::StringArray names;
        const auto& presets = PluginProcessor::getFactoryPresets();
        for (int i = 0; i < presets.size(); ++i)
            names.add(presets.getReference(i).name);
        return names;
    }());

    presetBox.setTextWhenNothingSelected("Custom");
    presetBox.onChange = [this]
    {
        if (updatingPresetUi)
            return;

        const int id = presetBox.getSelectedId();
        if (id > 0)
            processor.loadFactoryPreset(id - 1);

        refreshPresetUi();
    };

    previousPresetButton.onClick = [this]
    {
        processor.stepFactoryPreset(-1);
        refreshPresetUi();
    };

    nextPresetButton.onClick = [this]
    {
        processor.stepFactoryPreset(1);
        refreshPresetUi();
    };

    displayModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "displayMode", displayModeBox);
    fftSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "fftSize", fftSizeBox);
    overlapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "overlap", overlapBox);
    windowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "window", windowBox);
    themeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "theme", themeBox);

    averageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "average", averageKnob);
    smoothAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "smooth", smoothKnob);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "slope", slopeKnob);
    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "range", rangeKnob);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "hold", holdKnob);

    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "freeze", freezeToggle);
    waterfallAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "waterfall", waterfallToggle);

    presetLabel.setText("Producer Preset", juce::dontSendNotification);
    presetMetaLabel.setText(processor.getCurrentPresetDescription(), juce::dontSendNotification);
    presetMetaLabel.setFont(spectrumui::bodyFont(10.3f));
    presetMetaLabel.setJustificationType(juce::Justification::centredLeft);
    presetMetaLabel.setInterceptsMouseClicks(false, false);

    displayModeLabel.setText("Display", juce::dontSendNotification);
    fftSizeLabel.setText("FFT", juce::dontSendNotification);
    overlapLabel.setText("Overlap", juce::dontSendNotification);
    windowLabel.setText("Window", juce::dontSendNotification);
    themeLabel.setText("Theme", juce::dontSendNotification);

    averageLabel.setText("Average", juce::dontSendNotification);
    smoothLabel.setText("Smooth", juce::dontSendNotification);
    slopeLabel.setText("Slope", juce::dontSendNotification);
    rangeLabel.setText("Range", juce::dontSendNotification);
    holdLabel.setText("Hold", juce::dontSendNotification);

    for (auto* label : { &presetLabel, &displayModeLabel, &fftSizeLabel, &overlapLabel, &windowLabel, &themeLabel,
                         &averageLabel, &smoothLabel, &slopeLabel, &rangeLabel, &holdLabel })
    {
        styleLabel(*label);
        addAndMakeVisible(*label);
    }

    addAndMakeVisible(presetMetaLabel);

    for (auto* component : { static_cast<juce::Component*>(&previousPresetButton),
                             static_cast<juce::Component*>(&presetBox),
                             static_cast<juce::Component*>(&nextPresetButton),
                             static_cast<juce::Component*>(&displayModeBox),
                             static_cast<juce::Component*>(&fftSizeBox),
                             static_cast<juce::Component*>(&overlapBox),
                             static_cast<juce::Component*>(&windowBox),
                             static_cast<juce::Component*>(&themeBox),
                             static_cast<juce::Component*>(&averageKnob),
                             static_cast<juce::Component*>(&smoothKnob),
                             static_cast<juce::Component*>(&slopeKnob),
                             static_cast<juce::Component*>(&rangeKnob),
                             static_cast<juce::Component*>(&holdKnob),
                             static_cast<juce::Component*>(&freezeToggle),
                             static_cast<juce::Component*>(&waterfallToggle),
                             static_cast<juce::Component*>(&clearMaxButton),
                             static_cast<juce::Component*>(&clearClipButton) })
        addAndMakeVisible(*component);

    averageKnob.setTextValueSuffix(" %");
    smoothKnob.setTextValueSuffix(" %");
    slopeKnob.setTextValueSuffix(" dB/oct");
    rangeKnob.setTextValueSuffix(" dB");
    holdKnob.setTextValueSuffix(" s");

    averageKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff6bd8ff));
    smoothKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff75e2b1));
    slopeKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffffc66d));
    rangeKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9fb0ff));
    holdKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff8f7b));

    clearMaxButton.onClick = [this] { processor.clearAnalyzerPeaks(); };
    clearClipButton.onClick = [this] { processor.clearClipFlag(); };

    refreshPresetUi();
    startTimerHz(8);
}

void SpectrumControlsComponent::styleLabel(juce::Label& label)
{
    label.setFont(spectrumui::smallCapsFont(10.0f));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setInterceptsMouseClicks(false, false);
}

void SpectrumControlsComponent::paint(juce::Graphics& g)
{
    const auto palette = spectrumui::getPalette(processor.getThemeIndex());
    auto bounds = getLocalBounds().toFloat();
    spectrumui::drawPanelBackground(g, bounds, palette, palette.accentSecondary, 18.0f);

    for (auto* label : { &presetLabel, &displayModeLabel, &fftSizeLabel, &overlapLabel, &windowLabel, &themeLabel,
                         &averageLabel, &smoothLabel, &slopeLabel, &rangeLabel, &holdLabel })
        label->setColour(juce::Label::textColourId, palette.textMuted);
    presetMetaLabel.setColour(juce::Label::textColourId, palette.textStrong.withAlpha(0.88f));

    auto area = bounds.reduced(18.0f, 14.0f);
    auto header = area.removeFromTop(18.0f);
    g.setColour(palette.textStrong);
    g.setFont(spectrumui::smallCapsFont(11.0f));
    g.drawText("Analyzer Controls + Producer Presets", header.toNearestInt(), juce::Justification::left);

    auto divider = header.withTrimmedLeft(232.0f).withTrimmedRight(8.0f).withHeight(1.0f).withCentre({ header.getCentreX(), header.getCentreY() + 1.0f });
    g.setColour(palette.outlineSoft.withAlpha(0.7f));
    g.fillRect(divider);
}

void SpectrumControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 14);
    area.removeFromTop(26);

    auto presetRow = area.removeFromTop(54);
    presetLabel.setBounds(presetRow.removeFromTop(14));
    auto presetControls = presetRow.removeFromTop(30);
    previousPresetButton.setBounds(presetControls.removeFromLeft(34));
    presetControls.removeFromLeft(8);
    presetBox.setBounds(presetControls.removeFromLeft(246));
    presetControls.removeFromLeft(8);
    nextPresetButton.setBounds(presetControls.removeFromLeft(34));
    presetControls.removeFromLeft(12);
    presetMetaLabel.setBounds(presetControls);

    area.removeFromTop(8);

    auto comboRow = area.removeFromTop(58);
    const int comboColumns = 5;
    const int comboGap = 10;
    const int comboWidth = (comboRow.getWidth() - comboGap * (comboColumns - 1)) / comboColumns;

    auto placeCombo = [&](juce::Label& label, juce::ComboBox& box)
    {
        auto cell = comboRow.removeFromLeft(comboWidth);
        label.setBounds(cell.removeFromTop(14));
        box.setBounds(cell.removeFromTop(30));
        comboRow.removeFromLeft(comboGap);
    };

    placeCombo(displayModeLabel, displayModeBox);
    placeCombo(fftSizeLabel, fftSizeBox);
    placeCombo(overlapLabel, overlapBox);
    placeCombo(windowLabel, windowBox);
    placeCombo(themeLabel, themeBox);

    area.removeFromTop(8);

    auto knobRow = area.removeFromTop(188);
    const int knobColumns = 5;
    const int knobGap = 12;
    const int knobWidth = (knobRow.getWidth() - knobGap * (knobColumns - 1)) / knobColumns;

    auto placeKnob = [&](juce::Label& label, juce::Slider& knob)
    {
        auto cell = knobRow.removeFromLeft(knobWidth);
        label.setBounds(cell.removeFromTop(14));
        knob.setBounds(cell);
        knobRow.removeFromLeft(knobGap);
    };

    placeKnob(averageLabel, averageKnob);
    placeKnob(smoothLabel, smoothKnob);
    placeKnob(slopeLabel, slopeKnob);
    placeKnob(rangeLabel, rangeKnob);
    placeKnob(holdLabel, holdKnob);

    area.removeFromTop(6);

    auto footer = area.removeFromTop(36);
    freezeToggle.setBounds(footer.removeFromLeft(180));
    footer.removeFromLeft(8);
    waterfallToggle.setBounds(footer.removeFromLeft(200));
    footer.removeFromLeft(12);
    clearMaxButton.setBounds(footer.removeFromLeft(124));
    footer.removeFromLeft(8);
    clearClipButton.setBounds(footer.removeFromLeft(124));
}

void SpectrumControlsComponent::refreshPresetUi()
{
    const int presetIndex = processor.getMatchingFactoryPresetIndex();
    updatingPresetUi = true;
    if (presetIndex >= 0)
        presetBox.setSelectedId(presetIndex + 1, juce::dontSendNotification);
    else
        presetBox.setSelectedId(0, juce::dontSendNotification);
    updatingPresetUi = false;

    presetMetaLabel.setText(processor.getCurrentPresetDescription(), juce::dontSendNotification);
}

void SpectrumControlsComponent::timerCallback()
{
    refreshPresetUi();
    repaint();
}
