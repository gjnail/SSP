#pragma once

#include <JuceHeader.h>
#include <array>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    enum class QuickPreset
    {
        defaultStereo,
        swap,
        mono,
        dualMono,
        cross,
        leftOnly,
        rightOnly
    };

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    juce::String getDescription() const;
    void applyQuickPreset(QuickPreset);
    void resetToDefault();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParameterValue(const juce::String& parameterID, float value);

    juce::SmoothedValue<float> outputTrimSmoothed;
    std::array<juce::SmoothedValue<float>, 4> matrixSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
