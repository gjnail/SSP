#pragma once

#include <JuceHeader.h>
#include "ModulationKnob.h"
#include "PluginProcessor.h"

class SamplerOscillatorComponent final : public juce::Component,
                                         public juce::FileDragAndDropTarget,
                                         private juce::Timer
{
public:
    SamplerOscillatorComponent(PluginProcessor&, juce::AudioProcessorValueTreeState&, int oscillatorIndex);
    ~SamplerOscillatorComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void launchLoadChooser();
    void updateSampleState();
    static juce::String noteNameForMidi(int midiNote);
    bool isSupportedSampleFile(const juce::File& file) const;
    bool hasSupportedDroppedSample(const juce::StringArray& files) const;
    bool loadDroppedSample(const juce::StringArray& files);
    juce::Rectangle<int> getPreviewClickBounds() const;

    PluginProcessor& processor;
    int oscIndex = 1;
    juce::Label sampleLabel;
    juce::Label rootNoteLabel;
    juce::Label rootValueLabel;
    juce::Label octaveLabel;
    juce::Label octaveValueLabel;
    juce::TextButton loadButton;
    juce::TextButton clearButton;
    ModulationKnob rootNoteSlider;
    ModulationKnob octaveSlider;
    juce::String currentSampleName;
    juce::String currentRootValueText;
    juce::String currentOctaveValueText;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootNoteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    bool isDraggingFileOver = false;
    std::atomic<float>* mutateParam = nullptr;
    std::atomic<float>* fmSourceParam = nullptr;
    std::array<std::atomic<float>*, 2> warpModeParams{};
    std::array<std::atomic<float>*, 2> warpAmountParams{};
    std::atomic<float>* warpFMParam = nullptr;
    std::atomic<float>* warpSyncParam = nullptr;
    std::atomic<float>* warpBendParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerOscillatorComponent)
};
