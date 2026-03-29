#pragma once

#include <array>
#include <limits>
#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    static constexpr float minGainDb = -96.0f;
    static constexpr float maxGainDb = 12.0f;
    static constexpr float lowShelfFrequencyHz = 200.0f;
    static constexpr float midFrequencyHz = 1000.0f;
    static constexpr float midQ = 0.7f;
    static constexpr float shelfQ = 0.70710678f;
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

    static juce::IIRCoefficients makeLowShelfCoefficients(double sampleRate, double frequency, double gainDb);
    static juce::IIRCoefficients makePeakCoefficients(double sampleRate, double frequency, double q, double gainDb);
    static juce::IIRCoefficients makeHighShelfCoefficients(double sampleRate, double frequency, double gainDb);
    static double getMagnitudeForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getParameterValue(const juce::String& parameterId) const;
    void updateFilterCoefficients(float lowGainDb, float midGainDb, float highGainDb);

    std::array<std::array<juce::IIRFilter, 2>, 3> filters{};
    std::array<juce::IIRCoefficients, 3> currentCoefficients{};
    juce::SmoothedValue<float> lowGainSmoothed;
    juce::SmoothedValue<float> midGainSmoothed;
    juce::SmoothedValue<float> highGainSmoothed;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
