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

    juce::AudioProcessorValueTreeState apvts;

    float getInputMeterLevel() const noexcept;
    float getResonanceMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;

private:
    static constexpr int maxStages = 10;

    struct BiquadCoefficients
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    struct BiquadState
    {
        void reset() noexcept;
        float process(float input, const BiquadCoefficients& coefficients) noexcept;

        float s1 = 0.0f;
        float s2 = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static BiquadCoefficients makeAllPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept;
    void refreshCoefficients(float frequencyHz, float pinch, float amount, float spread) noexcept;
    void updateMeters(float inputPeak, float resonancePeak, float outputPeak) noexcept;

    std::array<std::array<BiquadCoefficients, maxStages>, 2> stageCoefficients;
    std::array<std::array<BiquadState, maxStages>, 2> stageStates;
    int activeStageCount = 2;
    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float> frequencySmoothed;
    juce::SmoothedValue<float> amountSmoothed;
    juce::SmoothedValue<float> pinchSmoothed;
    juce::SmoothedValue<float> spreadSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> resonanceMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
