#pragma once

#include <array>
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

    struct BandVisual
    {
        float centreHz = 0.0f;
        float attenuation = 0.0f;
    };

    static constexpr size_t numBands = 8;

    std::array<BandVisual, numBands> getBandVisuals() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateBandConfiguration();

    struct BandState
    {
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> filters;
        std::array<float, 2> envelopes{{ 0.0f, 0.0f }};
        float centreHz = 1000.0f;
        float q = 2.5f;
        float attenuationForUi = 0.0f;
    };

    std::array<BandState, numBands> bands;
    juce::SmoothedValue<float> mixSmoothed;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
