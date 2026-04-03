#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct PresetValue
    {
        juce::String id;
        float value = 0.0f;
    };

    struct PresetRecord
    {
        juce::String id;
        juce::String name;
        juce::String category;
        juce::String author;
        bool isFactory = false;
        juce::Array<PresetValue> values;
        juce::String version = "1.0";
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
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::Array<PresetRecord>& getFactoryPresets();
    const juce::Array<PresetRecord>& getUserPresets() const noexcept { return userPresets; }
    juce::File getUserPresetDirectory() const;
    juce::String getCurrentPresetName() const noexcept;
    juce::String getCurrentPresetCategory() const noexcept;
    juce::String getCurrentPresetAuthor() const noexcept;
    juce::String getCurrentPresetKey() const noexcept;
    bool isCurrentPresetFactory() const noexcept;
    bool isCurrentPresetDirty() const noexcept;
    bool matchesCurrentPreset(const PresetRecord&) const noexcept;
    juce::StringArray getAvailablePresetCategories() const;
    void refreshUserPresets();
    bool loadFactoryPreset(int index);
    bool loadUserPreset(int index);
    bool saveUserPreset(const juce::String& presetName, const juce::String& category);
    bool deleteUserPreset(int index);
    bool importPresetFromFile(const juce::File& file, bool loadAfterImport = true);
    bool exportCurrentPresetToFile(const juce::File& file) const;
    bool resetUserPresetsToFactoryDefaults();
    bool beginPresetPreview(const PresetRecord&);
    void endPresetPreview();
    bool isPresetPreviewActive() const noexcept;
    juce::Array<PresetRecord> getSequentialPresetList() const;
    bool stepPreset(int delta);
    bool loadPresetByKey(const juce::String& presetKey);

private:
    static constexpr int numPresetParameters = 24;

    struct PresetStateSnapshot
    {
        std::array<float, numPresetParameters> values{};
        bool valid = false;
    };

    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void updateScratchBuffers(int numChannels, int numSamples);
    void updateFilterCutoffs(float loCutHz, float hiCutHz, float crossoverHz, float tonePercent);
    void applyInputFilters(juce::AudioBuffer<float>& buffer);
    void applyFocusCutFilters(juce::AudioBuffer<float>& buffer);
    void buildBaseDelayedInput(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& destination,
                               float delaySamples);
    void buildMotionLayer(const juce::AudioBuffer<float>& source,
                          juce::AudioBuffer<float>& destination,
                          float amount,
                          float spreadMs,
                          float shape,
                          float rate,
                          bool spinEnabled);
    void splitIntoVoiceBands(const juce::AudioBuffer<float>& source,
                             juce::AudioBuffer<float>& lowBand,
                             juce::AudioBuffer<float>& highBand);
    void processChorus(juce::AudioBuffer<float>& buffer, juce::dsp::Chorus<float>& chorus);
    void applyDrive(juce::AudioBuffer<float>& buffer, float driveAmount);
    void applyWidth(juce::AudioBuffer<float>& buffer, float widthScale);
    float getValue(const juce::String& parameterID) const;

    void initialisePresetTracking();
    void setCurrentPresetMetadata(juce::String presetKey, juce::String presetName, juce::String category, juce::String author, bool isFactory);
    PresetStateSnapshot captureCurrentPresetSnapshot() const;
    PresetStateSnapshot makeDefaultPresetSnapshot() const;
    void applyPresetSnapshot(const PresetStateSnapshot&);
    void applyPresetRecord(const PresetRecord& preset, bool updateLoadedPresetReference);
    PresetRecord buildCurrentPresetRecord(juce::String presetName, juce::String category, juce::String author, bool isFactory) const;
    void refreshDirtyFlagFromCurrentState();

    std::array<Filter, 2> inputLowPassFilters;
    std::array<Filter, 2> inputHighPassFilters;
    std::array<Filter, 2> outputLowPassFilters;
    std::array<Filter, 2> outputHighPassFilters;
    std::array<Filter, 2> crossoverLowFilters;
    std::array<Filter, 2> crossoverHighFilters;
    std::array<DelayLine, 2> baseDelayLines;
    std::array<DelayLine, 2> motionDelayLines;
    juce::dsp::Chorus<float> lowVoiceChorus;
    juce::dsp::Chorus<float> highVoiceChorus;
    juce::dsp::Chorus<float> shineChorus;
    juce::SmoothedValue<float> mixSmoothed;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> filteredInputBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;
    juce::AudioBuffer<float> motionBuffer;
    double currentSampleRate = 44100.0;
    float motionPhase = 0.0f;

    juce::Array<PresetRecord> userPresets;
    juce::Array<juce::File> userPresetFiles;
    juce::String currentPresetKey;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    juce::String currentPresetAuthor;
    bool currentPresetIsFactory = false;
    std::atomic<bool> currentPresetDirty{false};
    PresetStateSnapshot lastLoadedPresetSnapshot;
    PresetStateSnapshot previewRestoreSnapshot;
    juce::String previewPresetKey;
    bool suppressParameterListener = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
