#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 4;
    static constexpr int numCrossovers = 3;
    static constexpr int vectorscopeSize = 2048;

    struct BandMeter
    {
        std::atomic<float> correlation{1.0f};
        std::atomic<float> levelL{0.0f};
        std::atomic<float> levelR{0.0f};
        std::atomic<float> width{0.0f};
    };

    // Vectorscope sample point (M/S coordinates + band index for colouring)
    struct VectorscopePoint
    {
        float mid = 0.0f;
        float side = 0.0f;
        int band = -1; // -1 = mixed, 0-3 = band index
    };

    // Preset system
    struct PresetBandData
    {
        float width = 100.0f;
        float pan = 0.0f;
    };

    struct PresetRecord
    {
        juce::String id;
        juce::String name;
        juce::String category;
        juce::String author;
        bool isFactory = false;
        float crossover1 = 200.0f;
        float crossover2 = 2000.0f;
        float crossover3 = 8000.0f;
        std::array<PresetBandData, numBands> bands{};
        float outputGain = 0.0f;
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

    float getCrossoverFrequency(int index) const;
    float getBandWidth(int bandIndex) const;
    float getBandPan(int bandIndex) const;
    bool isBandSoloed(int bandIndex) const;
    bool isBandMuted(int bandIndex) const;
    float getOutputGainDb() const;
    const BandMeter& getBandMeter(int bandIndex) const { return bandMeters[(size_t) bandIndex]; }
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    // Vectorscope access
    void getVectorscopeSnapshot(std::array<VectorscopePoint, vectorscopeSize>& dest) const;

    // Preset management
    static const juce::Array<PresetRecord>& getFactoryPresets();
    const juce::Array<PresetRecord>& getUserPresets() const noexcept { return userPresets; }
    juce::File getUserPresetDirectory() const;
    void refreshUserPresets();
    bool loadFactoryPreset(int index);
    bool loadUserPreset(int index);
    bool saveUserPreset(const juce::String& presetName, const juce::String& category);
    bool deleteUserPreset(int index);
    bool importPresetFromFile(const juce::File& file, bool loadAfterImport = true);
    bool exportCurrentPresetToFile(const juce::File& file) const;
    bool resetUserPresetsToFactoryDefaults();
    juce::String getCurrentPresetName() const noexcept { return currentPresetName; }
    juce::String getCurrentPresetCategory() const noexcept { return currentPresetCategory; }
    bool isCurrentPresetDirty() const noexcept { return currentPresetDirty.load(); }
    bool isCurrentPresetFactory() const noexcept { return currentPresetIsFactory; }
    bool matchesCurrentPreset(const PresetRecord& preset) const noexcept { return currentPresetKey == preset.id; }
    juce::StringArray getAvailablePresetCategories() const;
    juce::Array<PresetRecord> getSequentialPresetList() const;
    bool stepPreset(int delta);
    bool loadPresetByKey(const juce::String& presetKey);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateCrossoverFilters();
    void applyPresetRecord(const PresetRecord& preset);
    PresetRecord captureCurrentState() const;
    void setCurrentPresetMetadata(const juce::String& key, const juce::String& name, const juce::String& category, bool isFactory);

    double currentSampleRate = 44100.0;

    // Linkwitz-Riley 4th order crossover filters (stereo via channel index)
    // Crossover 1: splits into Low | Rest
    // Crossover 2: splits Rest into LowMid | UpperRest
    // Crossover 3: splits UpperRest into HighMid | High
    juce::dsp::LinkwitzRileyFilter<float> crossover1;
    juce::dsp::LinkwitzRileyFilter<float> crossover2;
    juce::dsp::LinkwitzRileyFilter<float> crossover3;

    // Allpass compensation filters for phase alignment in cascaded splits
    juce::dsp::LinkwitzRileyFilter<float> allpass2ForLow;
    juce::dsp::LinkwitzRileyFilter<float> allpass3ForLow;
    juce::dsp::LinkwitzRileyFilter<float> allpass3ForLowMid;

    std::array<BandMeter, numBands> bandMeters;
    std::array<float, numCrossovers> lastCrossoverFreqs{};

    // Smoothed parameters
    std::array<juce::SmoothedValue<float>, numBands> smoothedWidth;
    std::array<juce::SmoothedValue<float>, numBands> smoothedPan;
    juce::SmoothedValue<float> smoothedOutputGain;

    // Vectorscope ring buffer
    std::array<VectorscopePoint, vectorscopeSize> vectorscopeBuffer{};
    std::atomic<int> vectorscopeWriteIndex{0};
    int vectorscopeDecimationCounter = 0;

    // Preset state
    juce::Array<PresetRecord> userPresets;
    juce::Array<juce::File> userPresetFiles;
    juce::String currentPresetKey;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    bool currentPresetIsFactory = false;
    std::atomic<bool> currentPresetDirty{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
