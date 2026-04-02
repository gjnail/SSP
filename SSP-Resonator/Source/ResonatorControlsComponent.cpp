#include "ResonatorControlsComponent.h"

namespace
{
void styleSummaryLabel(juce::Label& label, float size)
{
    label.setFont(juce::Font(size, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

ResonatorControlsComponent::ResonatorControlsComponent(PluginProcessor& p)
    : processor(p)
{
    sectionLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    sectionLabel.setText("Colorbass Controls", juce::dontSendNotification);
    sectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sectionLabel);

    helperLabel.setFont(12.0f);
    helperLabel.setText("Build the chord, pick the vowel color, add motion, then keep the subs and attacks clean.", juce::dontSendNotification);
    helperLabel.setColour(juce::Label::textColourId, juce::Colour(0xff89a3c1));
    addAndMakeVisible(helperLabel);

    styleSummaryLabel(harmonySummaryLabel, 18.0f);
    styleSummaryLabel(voiceSummaryLabel, 13.0f);
    addAndMakeVisible(harmonySummaryLabel);
    addAndMakeVisible(voiceSummaryLabel);

    for (auto* label : {&sourceStatusLabel, &motionStatusLabel, &protectStatusLabel})
    {
        styleStatusLabel(*label);
        addAndMakeVisible(*label);
    }

    styleLabel(rootLabel, "Root");
    styleLabel(octaveLabel, "Octave");
    styleLabel(chordLabel, "Chord");
    styleLabel(voicingLabel, "Voicing");
    styleLabel(vowelLabel, "Vowel");
    styleLabel(driveLabel, "Excite");
    styleLabel(resonanceLabel, "Bloom");
    styleLabel(brightnessLabel, "Sparkle");
    styleLabel(formantLabel, "Prism");
    styleLabel(detuneLabel, "Detune");
    styleLabel(widthLabel, "Width");
    styleLabel(motionDepthLabel, "Motion");
    styleLabel(motionRateLabel, "Rate");
    styleLabel(divisionLabel, "Division");
    styleLabel(protectLabel, "Sub");
    styleLabel(crossoverLabel, "Xover");
    styleLabel(transientLabel, "Attack");
    styleLabel(mixLabel, "Mix");
    styleLabel(outputLabel, "Output");

    for (auto* label : {&rootLabel, &octaveLabel, &chordLabel, &voicingLabel, &vowelLabel, &driveLabel,
                        &resonanceLabel, &brightnessLabel, &formantLabel, &detuneLabel, &widthLabel,
                        &motionDepthLabel, &motionRateLabel, &divisionLabel, &protectLabel, &crossoverLabel, &transientLabel,
                        &mixLabel, &outputLabel})
        addAndMakeVisible(*label);

    rootBox.addItemList(PluginProcessor::getRootNoteNames(), 1);
    octaveBox.addItemList(PluginProcessor::getOctaveNames(), 1);
    chordBox.addItemList(PluginProcessor::getChordNames(), 1);
    voicingBox.addItemList(PluginProcessor::getVoicingNames(), 1);
    vowelBox.addItemList(PluginProcessor::getVowelNames(), 1);
    divisionBox.addItemList(PluginProcessor::getMotionDivisionNames(), 1);

    for (auto* box : {&rootBox, &octaveBox, &chordBox, &voicingBox, &vowelBox, &divisionBox})
        addAndMakeVisible(*box);

    styleKnob(driveKnob, juce::Colour(0xffff8d5b));
    styleKnob(resonanceKnob, juce::Colour(0xffffd45a));
    styleKnob(brightnessKnob, juce::Colour(0xff4ce8ff));
    styleKnob(formantKnob, juce::Colour(0xff8f87ff));
    styleKnob(detuneKnob, juce::Colour(0xffa57cff));
    styleKnob(widthKnob, juce::Colour(0xff55f3cb));
    styleKnob(motionDepthKnob, juce::Colour(0xff6be2ff));
    styleKnob(motionRateKnob, juce::Colour(0xff71ff9f));
    styleKnob(protectKnob, juce::Colour(0xff75b9ff));
    styleKnob(crossoverKnob, juce::Colour(0xff9bcbff));
    styleKnob(transientKnob, juce::Colour(0xffffb96a));
    styleKnob(mixKnob, juce::Colour(0xff8be16e));
    styleKnob(outputKnob, juce::Colour(0xff7bb6ff));

    driveKnob.setTextValueSuffix(" x");
    detuneKnob.setTextValueSuffix(" ct");
    crossoverKnob.setTextValueSuffix(" Hz");
    outputKnob.setTextValueSuffix(" dB");

    for (auto* knob : {&driveKnob, &resonanceKnob, &brightnessKnob, &formantKnob, &detuneKnob, &widthKnob,
                       &motionDepthKnob, &motionRateKnob, &protectKnob, &crossoverKnob, &transientKnob,
                       &mixKnob, &outputKnob})
        addAndMakeVisible(*knob);

    addAndMakeVisible(midiFollowToggle);
    addAndMakeVisible(motionSyncToggle);

    rootAttachment = std::make_unique<ComboAttachment>(processor.apvts, "rootNote", rootBox);
    octaveAttachment = std::make_unique<ComboAttachment>(processor.apvts, "rootOctave", octaveBox);
    chordAttachment = std::make_unique<ComboAttachment>(processor.apvts, "chordType", chordBox);
    voicingAttachment = std::make_unique<ComboAttachment>(processor.apvts, "voicing", voicingBox);
    vowelAttachment = std::make_unique<ComboAttachment>(processor.apvts, "vowelMode", vowelBox);
    divisionAttachment = std::make_unique<ComboAttachment>(processor.apvts, "motionDivision", divisionBox);
    driveAttachment = std::make_unique<SliderAttachment>(processor.apvts, "drive", driveKnob);
    resonanceAttachment = std::make_unique<SliderAttachment>(processor.apvts, "resonance", resonanceKnob);
    brightnessAttachment = std::make_unique<SliderAttachment>(processor.apvts, "brightness", brightnessKnob);
    formantAttachment = std::make_unique<SliderAttachment>(processor.apvts, "formantMix", formantKnob);
    detuneAttachment = std::make_unique<SliderAttachment>(processor.apvts, "detune", detuneKnob);
    widthAttachment = std::make_unique<SliderAttachment>(processor.apvts, "stereoWidth", widthKnob);
    motionDepthAttachment = std::make_unique<SliderAttachment>(processor.apvts, "motionDepth", motionDepthKnob);
    motionRateAttachment = std::make_unique<SliderAttachment>(processor.apvts, "motionRateHz", motionRateKnob);
    protectAttachment = std::make_unique<SliderAttachment>(processor.apvts, "lowProtect", protectKnob);
    crossoverAttachment = std::make_unique<SliderAttachment>(processor.apvts, "crossoverHz", crossoverKnob);
    transientAttachment = std::make_unique<SliderAttachment>(processor.apvts, "transientPreserve", transientKnob);
    mixAttachment = std::make_unique<SliderAttachment>(processor.apvts, "mix", mixKnob);
    outputAttachment = std::make_unique<SliderAttachment>(processor.apvts, "outputDb", outputKnob);
    midiFollowAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "midiFollow", midiFollowToggle);
    motionSyncAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "motionSync", motionSyncToggle);

    startTimerHz(12);
}

void ResonatorControlsComponent::styleLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff8ca4be));
    label.setJustificationType(juce::Justification::centredLeft);
}

void ResonatorControlsComponent::styleKnob(ssp::ui::SSPKnob& knob, juce::Colour accent)
{
    knob.setColour(juce::Slider::rotarySliderFillColourId, accent);
    knob.setColour(juce::Slider::thumbColourId, accent.brighter(0.22f));
    knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    knob.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111822));
}

void ResonatorControlsComponent::styleStatusLabel(juce::Label& label)
{
    label.setFont(juce::Font(13.0f));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd8e4f1));
    label.setJustificationType(juce::Justification::centredLeft);
}

void ResonatorControlsComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff55cfff), 18.0f);
    ssp::ui::drawPanelBackground(g, harmonyPanel.toFloat(), juce::Colour(0xff55cfff), 16.0f);
    ssp::ui::drawPanelBackground(g, tonePanel.toFloat(), juce::Colour(0xffffb555), 16.0f);
    ssp::ui::drawPanelBackground(g, motionPanel.toFloat(), juce::Colour(0xff64f0cb), 16.0f);
    ssp::ui::drawPanelBackground(g, protectPanel.toFloat(), juce::Colour(0xff8fa2ff), 16.0f);
    ssp::ui::drawPanelBackground(g, statusPanel.toFloat(), juce::Colour(0xff8f87ff), 16.0f);
}

void ResonatorControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(44);
    sectionLabel.setBounds(topRow.removeFromLeft(230));
    helperLabel.setBounds(topRow);

    area.removeFromTop(8);
    auto summaryRow = area.removeFromTop(52);
    harmonySummaryLabel.setBounds(summaryRow.removeFromTop(22));
    voiceSummaryLabel.setBounds(summaryRow);

    area.removeFromTop(10);
    auto panelRow = area.removeFromTop(268);
    harmonyPanel = panelRow.removeFromLeft(312);
    panelRow.removeFromLeft(12);
    tonePanel = panelRow.removeFromLeft(298);
    panelRow.removeFromLeft(12);
    motionPanel = panelRow.removeFromLeft(298);
    panelRow.removeFromLeft(12);
    protectPanel = panelRow;

    area.removeFromTop(12);
    statusPanel = area;

    auto harmonyArea = harmonyPanel.reduced(14);
    auto harmonyLabels = harmonyArea.removeFromTop(18);
    rootLabel.setBounds(harmonyLabels.removeFromLeft(54));
    octaveLabel.setBounds(harmonyLabels.removeFromLeft(56));
    chordLabel.setBounds(harmonyLabels.removeFromLeft(70));
    voicingLabel.setBounds(harmonyLabels);
    harmonyArea.removeFromTop(6);
    auto harmonyControls = harmonyArea.removeFromTop(34);
    rootBox.setBounds(harmonyControls.removeFromLeft(60));
    harmonyControls.removeFromLeft(6);
    octaveBox.setBounds(harmonyControls.removeFromLeft(58));
    harmonyControls.removeFromLeft(6);
    chordBox.setBounds(harmonyControls.removeFromLeft(70));
    harmonyControls.removeFromLeft(6);
    voicingBox.setBounds(harmonyControls.removeFromLeft(86));
    harmonyArea.removeFromTop(10);
    vowelLabel.setBounds(harmonyArea.removeFromTop(18));
    harmonyArea.removeFromTop(6);
    vowelBox.setBounds(harmonyArea.removeFromTop(32));
    harmonyArea.removeFromTop(10);
    auto toggleRow = harmonyArea.removeFromTop(24);
    midiFollowToggle.setBounds(toggleRow.removeFromLeft(100));

    auto toneArea = tonePanel.reduced(14);
    auto toneLabels = toneArea.removeFromTop(18);
    driveLabel.setBounds(toneLabels.removeFromLeft(56));
    resonanceLabel.setBounds(toneLabels.removeFromLeft(62));
    brightnessLabel.setBounds(toneLabels.removeFromLeft(62));
    formantLabel.setBounds(toneLabels);
    toneArea.removeFromTop(8);
    auto toneKnobs = toneArea.removeFromTop(132);
    const int toneKnobWidth = juce::jmin(56, toneKnobs.getWidth() / 4);
    driveKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));
    resonanceKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));
    brightnessKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));
    formantKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));

    auto motionArea = motionPanel.reduced(14);
    auto motionLabels = motionArea.removeFromTop(18);
    detuneLabel.setBounds(motionLabels.removeFromLeft(56));
    widthLabel.setBounds(motionLabels.removeFromLeft(56));
    motionDepthLabel.setBounds(motionLabels.removeFromLeft(58));
    motionRateLabel.setBounds(motionLabels);
    motionArea.removeFromTop(8);
    auto motionKnobs = motionArea.removeFromTop(132);
    const int motionKnobWidth = juce::jmin(56, motionKnobs.getWidth() / 4);
    detuneKnob.setBounds(motionKnobs.removeFromLeft(motionKnobWidth));
    widthKnob.setBounds(motionKnobs.removeFromLeft(motionKnobWidth));
    motionDepthKnob.setBounds(motionKnobs.removeFromLeft(motionKnobWidth));
    motionRateKnob.setBounds(motionKnobs.removeFromLeft(motionKnobWidth));
    motionArea.removeFromTop(8);
    auto motionBottom = motionArea.removeFromTop(52);
    motionSyncToggle.setBounds(motionBottom.removeFromTop(24).removeFromLeft(72));
    motionBottom.removeFromTop(4);
    divisionLabel.setBounds(motionBottom.removeFromTop(16));
    divisionBox.setBounds(motionBottom.removeFromTop(30));

    auto protectArea = protectPanel.reduced(14);
    auto protectLabels = protectArea.removeFromTop(18);
    protectLabel.setBounds(protectLabels.removeFromLeft(48));
    crossoverLabel.setBounds(protectLabels.removeFromLeft(52));
    transientLabel.setBounds(protectLabels.removeFromLeft(52));
    mixLabel.setBounds(protectLabels.removeFromLeft(44));
    outputLabel.setBounds(protectLabels);
    protectArea.removeFromTop(8);
    auto protectKnobs = protectArea.removeFromTop(132);
    const int protectKnobWidth = juce::jmin(48, protectKnobs.getWidth() / 5);
    protectKnob.setBounds(protectKnobs.removeFromLeft(protectKnobWidth));
    crossoverKnob.setBounds(protectKnobs.removeFromLeft(protectKnobWidth));
    transientKnob.setBounds(protectKnobs.removeFromLeft(protectKnobWidth));
    mixKnob.setBounds(protectKnobs.removeFromLeft(protectKnobWidth));
    outputKnob.setBounds(protectKnobs.removeFromLeft(protectKnobWidth));

    auto statusArea = statusPanel.reduced(16);
    sourceStatusLabel.setBounds(statusArea.removeFromTop(18));
    statusArea.removeFromTop(4);
    motionStatusLabel.setBounds(statusArea.removeFromTop(18));
    statusArea.removeFromTop(4);
    protectStatusLabel.setBounds(statusArea.removeFromTop(18));
}

void ResonatorControlsComponent::timerCallback()
{
    harmonySummaryLabel.setText(processor.getCurrentChordLabel() + " / " + processor.getCurrentVowelName(), juce::dontSendNotification);
    voiceSummaryLabel.setText(processor.getCurrentFactoryPresetName() + "  |  " + processor.getCurrentChordNoteSummary(), juce::dontSendNotification);
    sourceStatusLabel.setText(processor.getCurrentSourceStatus() + "  |  " + processor.getCurrentMonitorStatus(), juce::dontSendNotification);
    motionStatusLabel.setText(processor.getCurrentMotionStatus(), juce::dontSendNotification);
    protectStatusLabel.setText(processor.getCurrentProtectionStatus(), juce::dontSendNotification);
    motionRateKnob.setEnabled(! motionSyncToggle.getToggleState());
    divisionBox.setEnabled(motionSyncToggle.getToggleState());
}
