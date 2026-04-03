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
        std::array<float, visualizerPointCount> waveformBody{};
        std::array<float, visualizerPointCount> waveformPeak{};
        float clipAmount = 0.0f;
        float softness = 0.25f;
        int oversamplingIndex = 2;
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

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::StringArray getOversamplingChoices();
    static juce::StringArray getClipTypeChoices();
    static const juce::Array<PresetRecord>& getFactoryPresets();
    float getInputMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;
    float getClipMeterLevel() const noexcept;
    VisualizerSnapshot getVisualizerSnapshot() const noexcept;
    int getActiveOversamplingIndex() const noexcept;
    int getActiveOversamplingFactor() const noexcept;
    int getActiveClipTypeIndex() const noexcept;
    juce::String getActiveOversamplingLabel() const;
    juce::String getActiveClipTypeLabel() const;
    bool isOversamplingEnabled() const noexcept { return getActiveOversamplingFactor() > 1; }
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
    static int oversamplingIndexToFactor(int oversamplingIndex) noexcept;
    void pushVisualizerSample(float waveformPeak, float waveformBody) noexcept;
    void updateMeters(float inputPeak, float outputPeak, float clipAmount) noexcept;
    void updateVisualizerSnapshot(const juce::AudioBuffer<float>& drivenBuffer,
                                  int numChannels,
                                  float clipAmount,
                                  float softness) noexcept;
    void prepareOversamplingEngines(int samplesPerBlock);
    void updateActiveOversampling(int newOversamplingIndex, bool resetEngine) noexcept;
    void parameterChanged(const juce::String&, float) override;
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

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 8> oversamplingEngines;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> dcFilters;

    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> ceilingSmoothed;
    juce::SmoothedValue<float> trimSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};
    std::atomic<float> clipMeter{0.0f};
    std::array<std::atomic<float>, visualizerPointCount> waveformVisualizerBody;
    std::array<std::atomic<float>, visualizerPointCount> waveformVisualizerPeak;
    std::atomic<float> visualizerClipAmount{0.0f};
    std::atomic<float> visualizerSoftness{0.25f};
    std::atomic<int> visualizerWritePosition{0};
    std::atomic<int> activeOversamplingIndex{2};
    int lastOversamplingIndex = 2;
    int lastLatencySamples = 0;
    double currentSampleRate = 44100.0;
    int visualizerSamplesAccumulated = 0;
    float waveformPeakAccumulator = 0.0f;
    float waveformEnergyAccumulator = 0.0f;
    float heldWaveformPeak = 0.0f;
    float heldWaveformBody = 0.0f;
    juce::Array<PresetRecord> userPresets;
    juce::Array<juce::File> userPresetFiles;
    std::atomic<bool> currentPresetDirty{false};
    juce::String currentPresetKey;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    juce::String currentPresetAuthor;
    bool currentPresetIsFactory = true;
    juce::String lastLoadedPresetSnapshot;

    static constexpr int visualizerStride = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
