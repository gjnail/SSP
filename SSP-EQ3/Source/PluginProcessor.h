#pragma once

#include <array>
#include <complex>
#include <limits>
#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    struct BiquadCoefficients
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

    static constexpr float minGainDb = -12.0f;
    static constexpr float maxGainDb = 12.0f;
    static constexpr float lowShelfFrequencyHz = 200.0f;
    static constexpr float midFrequencyHz = 1000.0f;
    static constexpr float midQ = 0.7f;
    static constexpr float highShelfFrequencyHz = 5000.0f;

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

    double getResponseForFrequency(double frequency) const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    bool isPoweredOn() const;

    static BiquadCoefficients makeLowShelfCoefficients(double sampleRate, double frequency, double gainDb);
    static BiquadCoefficients makePeakCoefficients(double sampleRate, double frequency, double q, double gainDb);
    static BiquadCoefficients makeHighShelfCoefficients(double sampleRate, double frequency, double gainDb);
    static double getMagnitudeForFrequency(const BiquadCoefficients& coefficients, double frequency, double sampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getParameterValue(const juce::String& parameterId) const;

    struct BiquadState
    {
        double z1 = 0.0;
        double z2 = 0.0;

        void reset() noexcept
        {
            z1 = 0.0;
            z2 = 0.0;
        }

        float processSample(float input, const BiquadCoefficients& coefficients) noexcept
        {
            const double output = coefficients.b0 * input + z1;
            z1 = coefficients.b1 * input - coefficients.a1 * output + z2;
            z2 = coefficients.b2 * input - coefficients.a2 * output;
            return (float) output;
        }
    };

    std::array<std::array<BiquadState, 2>, 3> filterStates{};
    juce::SmoothedValue<float> lowGainSmoothed;
    juce::SmoothedValue<float> midGainSmoothed;
    juce::SmoothedValue<float> highGainSmoothed;
    juce::SmoothedValue<float> powerMixSmoothed;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
