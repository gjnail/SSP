#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
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
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& getModeNames();
    int getCurrentModeIndex() const noexcept;

private:
    struct PitchShifterChannel
    {
        void prepare(int bufferSize);
        void reset();
        float processSample(float inputSample, float pitchRatio, float grainSizeSamples);

        std::vector<float> delayBuffer;
        int writePosition = 0;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
    };

    struct FrequencyShifterChannel
    {
        static constexpr int hilbertTapCount = 63;

        void reset();
        float processSample(float inputSample,
                            double shiftHz,
                            double sampleRate,
                            const std::array<float, hilbertTapCount>& hilbertCoefficients);

        std::array<float, hilbertTapCount> history{};
        int writePosition = 0;
        double oscillatorPhase = 0.0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void resetProcessingState();
    void rebuildHilbertCoefficients();
    float applyToneFilter(int channel, float inputSample, float cutoffHz);

    std::array<PitchShifterChannel, 2> pitchShifters;
    std::array<FrequencyShifterChannel, 2> frequencyShifters;
    std::array<float, 2> toneFilterStates{{0.0f, 0.0f}};
    std::array<float, FrequencyShifterChannel::hilbertTapCount> hilbertCoefficients{};

    juce::SmoothedValue<float> pitchSemitoneSmoothed;
    juce::SmoothedValue<float> fineTuneSmoothed;
    juce::SmoothedValue<float> grainSizeSmoothed;
    juce::SmoothedValue<float> frequencyShiftSmoothed;
    juce::SmoothedValue<float> toneSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> modeBlendSmoothed;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
