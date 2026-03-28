#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
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

    float getInputMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;
    float getHeatMeterLevel() const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateMeters(float inputPeak, float outputPeak, float heatAmount) noexcept;

    std::array<float, 2> toneState{{0.0f, 0.0f}};
    std::array<float, 2> dcInputState{{0.0f, 0.0f}};
    std::array<float, 2> dcOutputState{{0.0f, 0.0f}};

    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> colorSmoothed;
    juce::SmoothedValue<float> biasSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};
    std::atomic<float> heatMeter{0.0f};
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
