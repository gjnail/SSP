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
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateScratchBuffers(int numChannels, int numSamples);
    void updateFilterCutoffs(float loCutHz, float hiCutHz, float crossoverHz);
    void applyInputFilters(juce::AudioBuffer<float>& buffer);
    void applyFlatCutFilters(juce::AudioBuffer<float>& buffer);
    void buildPredelayedInput(const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& destination, float predelaySamples);
    void buildEarlyReflections(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& destination,
                               float amount,
                               float reflect,
                               float shape,
                               float rate,
                               bool spinEnabled);
    void splitIntoDiffusionBands(const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& lowBand,
                                 juce::AudioBuffer<float>& highBand);
    void processChorus(juce::AudioBuffer<float>& buffer, juce::dsp::Chorus<float>& chorus);
    void applyWidth(juce::AudioBuffer<float>& buffer, float widthScale);
    float getValue(const juce::String& parameterID) const;

    std::array<Filter, 2> inputLowPassFilters;
    std::array<Filter, 2> inputHighPassFilters;
    std::array<Filter, 2> outputLowPassFilters;
    std::array<Filter, 2> outputHighPassFilters;
    std::array<Filter, 2> crossoverLowFilters;
    std::array<Filter, 2> crossoverHighFilters;
    std::array<DelayLine, 2> preDelayLines;
    std::array<DelayLine, 2> earlyReflectionLines;
    juce::dsp::Chorus<float> lowDiffusionChorus;
    juce::dsp::Chorus<float> highDiffusionChorus;
    juce::dsp::Chorus<float> wetChorus;
    juce::Reverb reverb;
    juce::SmoothedValue<float> mixSmoothed;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> filteredInputBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;
    juce::AudioBuffer<float> earlyBuffer;
    double currentSampleRate = 44100.0;
    float earlyPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
