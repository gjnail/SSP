#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int visualizerPointCount = 320;

    struct PresetRecord
    {
        juce::String id;
        juce::String name;
        juce::String category;
        juce::String author;
        bool isFactory = true;
        juce::ValueTree state;
    };

    struct VisualizerSnapshot
    {
        std::array<float, visualizerPointCount> inputBody{};
        std::array<float, visualizerPointCount> inputPeak{};
        std::array<float, visualizerPointCount> outputBody{};
        std::array<float, visualizerPointCount> outputPeak{};
        std::array<float, visualizerPointCount> gainReductionHistory{};
        float gainReductionDb = 0.0f;
        int writePosition = 0;
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

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::Array<PresetRecord>& getFactoryPresets();
    static juce::StringArray getLimitTypeChoices();
    float getInputMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;
    float getGainReductionDb() const noexcept;
    float getGainReductionMeterLevel() const noexcept;
    juce::String getActiveLimitTypeLabel() const;
    VisualizerSnapshot getVisualizerSnapshot() const noexcept;
    const juce::Array<PresetRecord>& getUserPresets() const noexcept { return userPresets; }
    juce::Array<PresetRecord> getSequentialPresetList() const;
    juce::StringArray getAvailablePresetCategories() const;
    juce::String getCurrentPresetName() const noexcept { return currentPresetName; }
    juce::String getCurrentPresetCategory() const noexcept { return currentPresetCategory; }
    bool isCurrentPresetDirty() const noexcept { return currentPresetDirty.load(); }
    bool matchesCurrentPreset(const PresetRecord& preset) const noexcept;
    void stepPreset(int delta);
    bool loadFactoryPreset(int index);
    bool loadUserPreset(int index);
    bool saveUserPreset(const juce::String& presetName, const juce::String& category);
    bool deleteUserPreset(int index);
    bool importPresetFromFile(const juce::File& file, bool loadAfterImport);
    bool exportCurrentPresetToFile(const juce::File& file) const;
    void refreshUserPresets();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String&, float) override;
    void updateLatencyCompensation();
    void updateMeters(float inputPeak, float outputPeak, float gainReductionDb) noexcept;
    void pushVisualizerSample(float inputPeak, float inputBody, float outputPeak, float outputBody, float gainReductionDb) noexcept;
    void updateVisualizerSnapshot(const juce::AudioBuffer<float>& inputBuffer,
                                  const juce::AudioBuffer<float>& outputBuffer,
                                  int numChannels,
                                  float gainReductionDb) noexcept;

    juce::File getUserPresetDirectory() const;
    PresetRecord buildCurrentPresetRecord(const juce::String& presetName,
                                          const juce::String& category,
                                          const juce::String& author,
                                          bool isFactory) const;
    void applyPresetRecord(const PresetRecord& preset);
    juce::String captureCurrentPresetSnapshot() const;
    void updatePresetDirtyState();
    void setCurrentPresetMetadata(const PresetRecord& preset);
    void initialisePresetTracking();

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};
    std::atomic<float> gainReductionDbAtomic{0.0f};
    std::array<std::atomic<float>, visualizerPointCount> inputVisualizerBody;
    std::array<std::atomic<float>, visualizerPointCount> inputVisualizerPeak;
    std::array<std::atomic<float>, visualizerPointCount> outputVisualizerBody;
    std::array<std::atomic<float>, visualizerPointCount> outputVisualizerPeak;
    std::array<std::atomic<float>, visualizerPointCount> gainReductionVisualizer;
    std::atomic<int> visualizerWritePosition{0};
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> limitedPreviewBuffer;
    juce::AudioBuffer<float> lookaheadBuffer;
    std::array<float, 2> channelGain{1.0f, 1.0f};
    int lookaheadWritePosition = 0;
    int maxLookaheadSamples = 0;
    double currentSampleRate = 44100.0;

    int visualizerSamplesAccumulated = 0;
    float inputPeakAccumulator = 0.0f;
    float inputEnergyAccumulator = 0.0f;
    float outputPeakAccumulator = 0.0f;
    float outputEnergyAccumulator = 0.0f;
    float gainReductionAccumulator = 0.0f;
    float heldInputPeak = 0.0f;
    float heldInputBody = 0.0f;
    float heldOutputPeak = 0.0f;
    float heldOutputBody = 0.0f;
    float heldGainReduction = 0.0f;

    juce::Array<PresetRecord> userPresets;
    juce::Array<juce::File> userPresetFiles;
    std::atomic<bool> currentPresetDirty{false};
    juce::String currentPresetKey;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    juce::String currentPresetAuthor;
    bool currentPresetIsFactory = true;
    juce::String lastLoadedPresetSnapshot;

    static constexpr int visualizerStride = 96;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
