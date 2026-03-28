#include "SamplerOscillatorComponent.h"
#include "ReactorUI.h"

namespace
{
void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111821));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff7de9ff));
}

void styleButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff111821));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff18303a));
    button.setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
    button.setColour(juce::TextButton::textColourOnId, reactorui::textStrong());
}
}

SamplerOscillatorComponent::SamplerOscillatorComponent(PluginProcessor& p,
                                                       juce::AudioProcessorValueTreeState& state,
                                                       int oscillatorIndex)
    : processor(p),
      oscIndex(oscillatorIndex),
      rootNoteSlider(p, "osc" + juce::String(oscillatorIndex) + "SampleRoot",
                     reactormod::Destination::none, juce::Colour(0xff56cbe3), 96, 24),
      octaveSlider(p, "osc" + juce::String(oscillatorIndex) + "Octave",
                   reactormod::Destination::none, juce::Colour(0xff56cbe3), 96, 24)
{
    addAndMakeVisible(sampleLabel);
    addAndMakeVisible(rootNoteLabel);
    addAndMakeVisible(rootValueLabel);
    addAndMakeVisible(octaveLabel);
    addAndMakeVisible(octaveValueLabel);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(rootNoteSlider);
    addAndMakeVisible(octaveSlider);

    styleLabel(sampleLabel, 12.5f, reactorui::textStrong());
    styleLabel(rootNoteLabel, 11.5f, reactorui::textMuted());
    styleLabel(rootValueLabel, 12.0f, reactorui::textStrong(), juce::Justification::centred);
    styleLabel(octaveLabel, 11.5f, reactorui::textMuted());
    styleLabel(octaveValueLabel, 12.0f, reactorui::textStrong(), juce::Justification::centred);

    rootNoteLabel.setText("Root", juce::dontSendNotification);
    octaveLabel.setText("Octave", juce::dontSendNotification);

    loadButton.setButtonText("Load");
    clearButton.setButtonText("Clear");
    styleButton(loadButton);
    styleButton(clearButton);

    rootNoteSlider.setRange(12.0, 127.0, 1.0);
    rootNoteSlider.setNumDecimalPlacesToDisplay(0);
    rootNoteSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    octaveSlider.setRange(0.0, (double) (PluginProcessor::getOctaveNames().size() - 1), 1.0);
    octaveSlider.setNumDecimalPlacesToDisplay(0);
    octaveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    const auto prefix = "osc" + juce::String(oscIndex);
    rootNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "SampleRoot", rootNoteSlider);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Octave", octaveSlider);

    loadButton.onClick = [this] { launchLoadChooser(); };
    clearButton.onClick = [this]
    {
        processor.clearOscillatorSample(oscIndex);
        updateSampleState();
    };

    updateSampleState();
    startTimerHz(8);
}

void SamplerOscillatorComponent::paint(juce::Graphics& g)
{
    auto preview = getPreviewClickBounds().toFloat();
    const auto accent = juce::Colour(0xff6ce5ff);

    reactorui::drawDisplayPanel(g, preview, accent);
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accent.withAlpha(0.10f), 6, 3);

    const auto sampleData = processor.getOscillatorSampleData(oscIndex);
    if (sampleData != nullptr && sampleData->buffer.getNumSamples() > 1)
    {
        juce::Path path;
        const auto& buffer = sampleData->buffer;
        const int totalSamples = buffer.getNumSamples();
        const float centreY = preview.getCentreY();
        const float amplitude = preview.getHeight() * 0.42f;
        const int width = juce::jmax(1, (int) preview.getWidth());

        for (int x = 0; x < width; ++x)
        {
            const float progress = (float) x / (float) juce::jmax(1, width - 1);
            const int sampleIndex = juce::jlimit(0, totalSamples - 1, juce::roundToInt(progress * (float) (totalSamples - 1)));
            float mono = 0.0f;
            const int channels = juce::jmin(2, buffer.getNumChannels());
            for (int channel = 0; channel < channels; ++channel)
                mono += buffer.getSample(channel, sampleIndex);
            mono /= (float) juce::jmax(1, channels);

            const float drawX = preview.getX() + (float) x;
            const float drawY = centreY - mono * amplitude;
            if (x == 0)
                path.startNewSubPath(drawX, drawY);
            else
                path.lineTo(drawX, drawY);
        }

        g.setColour(accent);
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour(reactorui::textMuted().withAlpha(0.75f));
        g.setFont(reactorui::bodyFont(12.0f));
        g.drawFittedText("Click or drop an audio file here to load a sample",
                         preview.getSmallestIntegerContainer().reduced(12, 8),
                         juce::Justification::centred,
                         2);
    }

    g.setColour(isDraggingFileOver ? accent.withAlpha(0.22f) : accent.withAlpha(0.10f));
    g.drawRoundedRectangle(preview.reduced(1.0f), 10.0f, isDraggingFileOver ? 2.2f : 1.0f);
}

void SamplerOscillatorComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(90);
    auto infoRow = area.removeFromTop(30);
    auto buttonArea = infoRow.removeFromRight(172);
    sampleLabel.setBounds(infoRow);
    loadButton.setBounds(buttonArea.removeFromLeft(82));
    buttonArea.removeFromLeft(10);
    clearButton.setBounds(buttonArea);

    area.removeFromTop(6);
    auto labelRow = area.removeFromTop(16);
    auto rootLabelArea = labelRow.removeFromLeft((labelRow.getWidth() - 14) / 2);
    labelRow.removeFromLeft(14);
    auto octaveLabelArea = labelRow;

    rootNoteLabel.setBounds(rootLabelArea);
    octaveLabel.setBounds(octaveLabelArea);

    area.removeFromTop(2);
    auto valueRow = area.removeFromTop(18);
    auto rootValueArea = valueRow.removeFromLeft((valueRow.getWidth() - 14) / 2);
    valueRow.removeFromLeft(14);
    auto octaveValueArea = valueRow;
    rootValueLabel.setBounds(rootValueArea);
    octaveValueLabel.setBounds(octaveValueArea);

    area.removeFromTop(4);
    auto knobRow = area;
    auto rootKnobArea = knobRow.removeFromLeft((knobRow.getWidth() - 14) / 2);
    knobRow.removeFromLeft(14);
    auto octaveKnobArea = knobRow;

    rootNoteSlider.setBounds(rootKnobArea);
    octaveSlider.setBounds(octaveKnobArea);
}

void SamplerOscillatorComponent::mouseUp(const juce::MouseEvent& event)
{
    if (event.eventComponent != this)
        return;

    if (getPreviewClickBounds().contains(event.getPosition()))
        launchLoadChooser();
}

bool SamplerOscillatorComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return hasSupportedDroppedSample(files);
}

void SamplerOscillatorComponent::fileDragEnter(const juce::StringArray& files, int, int)
{
    isDraggingFileOver = hasSupportedDroppedSample(files);
    repaint();
}

void SamplerOscillatorComponent::fileDragExit(const juce::StringArray&)
{
    isDraggingFileOver = false;
    repaint();
}

void SamplerOscillatorComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingFileOver = false;
    if (loadDroppedSample(files))
        updateSampleState();

    repaint();
}

void SamplerOscillatorComponent::timerCallback()
{
    updateSampleState();
    repaint();
}

void SamplerOscillatorComponent::launchLoadChooser()
{
    auto startPath = processor.getOscillatorSamplePath(oscIndex);
    auto startFile = startPath.isNotEmpty() ? juce::File(startPath)
                                            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    chooser = std::make_unique<juce::FileChooser>("Load sample for oscillator " + juce::String(oscIndex),
                                                  startFile,
                                                  "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    auto* chooserPtr = chooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooserPtr] (const juce::FileChooser& chooserRef)
                         {
                             juce::ignoreUnused(chooserRef);
                             if (chooserPtr != chooser.get())
                                 return;

                             const auto file = chooserPtr->getResult();
                             if (file.existsAsFile())
                                 processor.loadOscillatorSample(oscIndex, file);

                             updateSampleState();
                             chooser.reset();
                         });
}

void SamplerOscillatorComponent::updateSampleState()
{
    const auto sampleName = processor.getOscillatorSampleDisplayName(oscIndex);
    if (sampleName != currentSampleName)
    {
        currentSampleName = sampleName;
        sampleLabel.setText(currentSampleName, juce::dontSendNotification);
    }

    const auto rootValue = noteNameForMidi(juce::roundToInt((float) rootNoteSlider.getValue()));
    if (rootValue != currentRootValueText)
    {
        currentRootValueText = rootValue;
        rootValueLabel.setText(currentRootValueText, juce::dontSendNotification);
    }

    const auto octaveIndex = juce::jlimit(0, PluginProcessor::getOctaveNames().size() - 1,
                                          juce::roundToInt((float) octaveSlider.getValue()));
    const auto octaveValue = PluginProcessor::getOctaveNames()[octaveIndex];
    if (octaveValue != currentOctaveValueText)
    {
        currentOctaveValueText = octaveValue;
        octaveValueLabel.setText(currentOctaveValueText, juce::dontSendNotification);
    }

    clearButton.setEnabled(processor.getOscillatorSamplePath(oscIndex).isNotEmpty());
}

juce::String SamplerOscillatorComponent::noteNameForMidi(int midiNote)
{
    static const std::array<const char*, 12> noteNames{{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    }};

    const int clamped = juce::jlimit(0, 127, midiNote);
    const int noteIndex = clamped % 12;
    const int octave = clamped / 12 - 1;
    return juce::String(noteNames[(size_t) noteIndex]) + juce::String(octave);
}

bool SamplerOscillatorComponent::isSupportedSampleFile(const juce::File& file) const
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav"
        || extension == ".aif"
        || extension == ".aiff"
        || extension == ".flac"
        || extension == ".ogg"
        || extension == ".mp3";
}

bool SamplerOscillatorComponent::hasSupportedDroppedSample(const juce::StringArray& files) const
{
    for (const auto& path : files)
    {
        const auto file = juce::File(path);
        if (file.existsAsFile() && isSupportedSampleFile(file))
            return true;
    }

    return false;
}

bool SamplerOscillatorComponent::loadDroppedSample(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const auto file = juce::File(path);
        if (file.existsAsFile() && isSupportedSampleFile(file))
            return processor.loadOscillatorSample(oscIndex, file);
    }

    return false;
}

juce::Rectangle<int> SamplerOscillatorComponent::getPreviewClickBounds() const
{
    return getLocalBounds().removeFromTop(82);
}
