#include "ResonatorControlsComponent.h"

namespace
{
void styleSummaryLabel(juce::Label& label)
{
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

ResonatorControlsComponent::ResonatorControlsComponent(PluginProcessor& p)
    : processor(p)
{
    sectionLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    sectionLabel.setText("Resonator Controls", juce::dontSendNotification);
    sectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sectionLabel);

    helperLabel.setFont(12.0f);
    helperLabel.setText("Dial in a chord, choose how wide it blooms, then shape how bright and unstable the ringing feels.",
                        juce::dontSendNotification);
    helperLabel.setColour(juce::Label::textColourId, juce::Colour(0xff89a3c1));
    addAndMakeVisible(helperLabel);

    harmonySummaryLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    styleSummaryLabel(harmonySummaryLabel);
    addAndMakeVisible(harmonySummaryLabel);

    voiceSummaryLabel.setFont(13.0f);
    voiceSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e9ff));
    voiceSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(voiceSummaryLabel);

    statusCopyLabel.setFont(juce::Font(14.0f));
    statusCopyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8e4f1));
    statusCopyLabel.setJustificationType(juce::Justification::topLeft);
    statusCopyLabel.setText("Use Tight, Open, Wide or Cascade to move the chord from compact filter-bank tones into layered shimmer. Higher Bloom increases sustain, Sparkle keeps the ringing airy, and Detune plus Width widen the stereo bloom.",
                            juce::dontSendNotification);
    addAndMakeVisible(statusCopyLabel);

    styleLabel(rootLabel, "Root");
    styleLabel(octaveLabel, "Octave");
    styleLabel(chordLabel, "Chord");
    styleLabel(voicingLabel, "Voicing");
    styleLabel(driveLabel, "Excite");
    styleLabel(resonanceLabel, "Bloom");
    styleLabel(brightnessLabel, "Sparkle");
    styleLabel(detuneLabel, "Detune");
    styleLabel(widthLabel, "Width");
    styleLabel(mixLabel, "Mix");
    styleLabel(outputLabel, "Output");

    for (auto* label : {&rootLabel, &octaveLabel, &chordLabel, &voicingLabel, &driveLabel, &resonanceLabel,
                        &brightnessLabel, &detuneLabel, &widthLabel, &mixLabel, &outputLabel})
        addAndMakeVisible(*label);

    rootBox.addItemList(PluginProcessor::getRootNoteNames(), 1);
    octaveBox.addItemList(PluginProcessor::getOctaveNames(), 1);
    chordBox.addItemList(PluginProcessor::getChordNames(), 1);
    voicingBox.addItemList(PluginProcessor::getVoicingNames(), 1);

    for (auto* box : {&rootBox, &octaveBox, &chordBox, &voicingBox})
        addAndMakeVisible(*box);

    styleKnob(driveKnob, juce::Colour(0xffff8d5b));
    styleKnob(resonanceKnob, juce::Colour(0xffffd45a));
    styleKnob(brightnessKnob, juce::Colour(0xff4ce8ff));
    styleKnob(detuneKnob, juce::Colour(0xff8f87ff));
    styleKnob(widthKnob, juce::Colour(0xff55f3cb));
    styleKnob(mixKnob, juce::Colour(0xff6bdd7f));
    styleKnob(outputKnob, juce::Colour(0xff75b9ff));

    driveKnob.setTextValueSuffix(" x");
    detuneKnob.setTextValueSuffix(" ct");
    outputKnob.setTextValueSuffix(" dB");

    for (auto* knob : {&driveKnob, &resonanceKnob, &brightnessKnob, &detuneKnob, &widthKnob, &mixKnob, &outputKnob})
        addAndMakeVisible(*knob);

    rootAttachment = std::make_unique<ComboAttachment>(processor.apvts, "rootNote", rootBox);
    octaveAttachment = std::make_unique<ComboAttachment>(processor.apvts, "rootOctave", octaveBox);
    chordAttachment = std::make_unique<ComboAttachment>(processor.apvts, "chordType", chordBox);
    voicingAttachment = std::make_unique<ComboAttachment>(processor.apvts, "voicing", voicingBox);
    driveAttachment = std::make_unique<SliderAttachment>(processor.apvts, "drive", driveKnob);
    resonanceAttachment = std::make_unique<SliderAttachment>(processor.apvts, "resonance", resonanceKnob);
    brightnessAttachment = std::make_unique<SliderAttachment>(processor.apvts, "brightness", brightnessKnob);
    detuneAttachment = std::make_unique<SliderAttachment>(processor.apvts, "detune", detuneKnob);
    widthAttachment = std::make_unique<SliderAttachment>(processor.apvts, "stereoWidth", widthKnob);
    mixAttachment = std::make_unique<SliderAttachment>(processor.apvts, "mix", mixKnob);
    outputAttachment = std::make_unique<SliderAttachment>(processor.apvts, "outputDb", outputKnob);

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

void ResonatorControlsComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff55cfff), 18.0f);
    ssp::ui::drawPanelBackground(g, harmonyPanel.toFloat(), juce::Colour(0xff55cfff), 16.0f);
    ssp::ui::drawPanelBackground(g, tonePanel.toFloat(), juce::Colour(0xffffb555), 16.0f);
    ssp::ui::drawPanelBackground(g, mixPanel.toFloat(), juce::Colour(0xff60f0b7), 16.0f);
    ssp::ui::drawPanelBackground(g, statusPanel.toFloat(), juce::Colour(0xff8d86ff), 16.0f);
}

void ResonatorControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(46);
    sectionLabel.setBounds(topRow.removeFromLeft(260));
    helperLabel.setBounds(topRow);

    area.removeFromTop(8);
    auto summaryRow = area.removeFromTop(42);
    harmonySummaryLabel.setBounds(summaryRow.removeFromTop(22));
    voiceSummaryLabel.setBounds(summaryRow);

    area.removeFromTop(10);
    auto row = area.removeFromTop(192);
    harmonyPanel = row.removeFromLeft(310);
    row.removeFromLeft(14);
    tonePanel = row.removeFromLeft(330);
    row.removeFromLeft(14);
    mixPanel = row;

    area.removeFromTop(12);
    statusPanel = area;

    auto harmonyArea = harmonyPanel.reduced(16);
    auto labelRow = harmonyArea.removeFromTop(18);
    rootLabel.setBounds(labelRow.removeFromLeft(64));
    octaveLabel.setBounds(labelRow.removeFromLeft(64));
    chordLabel.setBounds(labelRow.removeFromLeft(80));
    voicingLabel.setBounds(labelRow);
    harmonyArea.removeFromTop(6);

    auto comboRow = harmonyArea.removeFromTop(34);
    rootBox.setBounds(comboRow.removeFromLeft(70));
    comboRow.removeFromLeft(8);
    octaveBox.setBounds(comboRow.removeFromLeft(70));
    comboRow.removeFromLeft(8);
    chordBox.setBounds(comboRow.removeFromLeft(88));
    comboRow.removeFromLeft(8);
    voicingBox.setBounds(comboRow.removeFromLeft(102));

    auto toneArea = tonePanel.reduced(16);
    auto toneHeader = toneArea.removeFromTop(18);
    driveLabel.setBounds(toneHeader.removeFromLeft(92));
    resonanceLabel.setBounds(toneHeader.removeFromLeft(98));
    brightnessLabel.setBounds(toneHeader);
    toneArea.removeFromTop(8);

    auto toneKnobs = toneArea.removeFromTop(130);
    const int toneKnobWidth = juce::jmin(100, toneKnobs.getWidth() / 3);
    driveKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));
    resonanceKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));
    brightnessKnob.setBounds(toneKnobs.removeFromLeft(toneKnobWidth));

    auto mixArea = mixPanel.reduced(16);
    auto mixHeader = mixArea.removeFromTop(18);
    detuneLabel.setBounds(mixHeader.removeFromLeft(78));
    widthLabel.setBounds(mixHeader.removeFromLeft(78));
    mixLabel.setBounds(mixHeader.removeFromLeft(70));
    outputLabel.setBounds(mixHeader);
    mixArea.removeFromTop(8);

    auto mixKnobs = mixArea.removeFromTop(130);
    const int mixKnobWidth = juce::jmin(86, mixKnobs.getWidth() / 4);
    detuneKnob.setBounds(mixKnobs.removeFromLeft(mixKnobWidth));
    widthKnob.setBounds(mixKnobs.removeFromLeft(mixKnobWidth));
    mixKnob.setBounds(mixKnobs.removeFromLeft(mixKnobWidth));
    outputKnob.setBounds(mixKnobs.removeFromLeft(mixKnobWidth));

    statusCopyLabel.setBounds(statusPanel.reduced(16));
}

void ResonatorControlsComponent::timerCallback()
{
    harmonySummaryLabel.setText(processor.getCurrentChordLabel(), juce::dontSendNotification);
    voiceSummaryLabel.setText(processor.getCurrentChordNoteSummary(), juce::dontSendNotification);
}
