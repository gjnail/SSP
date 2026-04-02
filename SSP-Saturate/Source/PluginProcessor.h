#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int maxBands = 6;
    static constexpr int maxCrossovers = maxBands - 1;
    static constexpr int analyzerOrder = 11;
    static constexpr int analyzerSize = 1 << analyzerOrder;
    static constexpr int analyzerBinCount = 96;
    enum class FactoryPreset
    {
        gentleWarmth = 0,
        masteringGlow,
        softConsole,
        tubeKiss,
        transformerGlue,
        presenceShine,
        tubePush,
        hotBus,
        edgeCrunch,
        ampStack,
        brokenCombo,
        tapeLift,
        parallelAir,
        cassetteDust,
        brokenBus,
        speakerCrush,
        fuzzMotion,
        rectifierBloom
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

    float getInputMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;
    float getHeatMeterLevel() const noexcept;
    float getBandMeterLevel(int bandIndex) const noexcept;
    std::array<float, analyzerBinCount> getSpectrumData() const noexcept;
    int getActiveBandCount() const noexcept;
    int getSoloBand() const noexcept;
    void setSoloBand(int bandIndex);
    static juce::StringArray getFactoryPresetNames();
    static juce::String getFactoryPresetName(int presetIndex);
    static juce::String getFactoryPresetCategory(int presetIndex);
    static juce::String getFactoryPresetTags(int presetIndex);
    void applyFactoryPreset(int presetIndex);
    int getCurrentFactoryPresetIndex() const noexcept;
    static juce::String bandParamId(int bandIndex, const juce::String& suffix);
    static juce::String crossoverParamId(int crossoverIndex);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct BandProcessorState
    {
        std::array<float, 2> toneLowpass{{0.0f, 0.0f}};
        std::array<float, 2> feedback{{0.0f, 0.0f}};
        std::array<float, 2> dcIn{{0.0f, 0.0f}};
        std::array<float, 2> dcOut{{0.0f, 0.0f}};
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParameterValue(const juce::String& paramId, float plainValue);
    void updateMeters(float inputPeak, float outputPeak, float heatAmount,
                      const std::array<float, maxBands>& bandPeaks) noexcept;
    void updateCrossoverFilters();
    void pushNextAnalyzerSample(float sample) noexcept;
    void updateSpectrumData() noexcept;
    float processBandSample(float sample, int bandIndex, int channel,
                            float globalDriveDb, bool autoGainEnabled) noexcept;

    std::array<float, maxBands> splitBands(float inputSample, int channel, int activeBands);

    std::array<std::array<juce::dsp::LinkwitzRileyFilter<float>, maxCrossovers>, 2> lowpassFilters;
    std::array<std::array<juce::dsp::LinkwitzRileyFilter<float>, maxCrossovers>, 2> highpassFilters;
    std::array<BandProcessorState, maxBands> bandStates;

    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};
    std::atomic<float> heatMeter{0.0f};
    std::array<std::atomic<float>, maxBands> bandMeters;
    std::array<std::atomic<float>, analyzerBinCount> spectrumBins;
    juce::dsp::FFT analyzerFft{analyzerOrder};
    juce::dsp::WindowingFunction<float> analyzerWindow{analyzerSize, juce::dsp::WindowingFunction<float>::hann};
    std::array<float, analyzerSize> analyzerFifo{};
    std::array<float, analyzerSize * 2> analyzerData{};
    int analyzerWritePosition = 0;
    bool analyzerBlockReady = false;
    int currentFactoryPreset = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
