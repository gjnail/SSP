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
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputMeterLevel() const noexcept;
    float getDelayMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;

    static const juce::StringArray& getTimeModeNames() noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float readDelaySample(const std::vector<float>& buffer, float delayInSamples) const noexcept;
    float getDelayTimeMs() const noexcept;
    float getHostBpm() const noexcept;
    void updateMeters(float inputPeak, float delayPeak, float outputPeak) noexcept;

    std::array<std::vector<float>, 2> delayBuffers;
    std::array<float, 2> toneStates{{0.0f, 0.0f}};
    int delayBufferSize = 0;
    int writePosition = 0;
    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float> timeSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> toneSmoothed;
    juce::SmoothedValue<float> widthSmoothed;
    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> delayMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
