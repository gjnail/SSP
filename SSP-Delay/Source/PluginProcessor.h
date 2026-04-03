#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

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
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputMeterLevel() const noexcept;
    float getDelayMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;

    static const juce::StringArray& getTimeModeNames() noexcept;
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
    static constexpr int numPresetParameters = 10;

    struct PresetStateSnapshot
    {
        std::array<float, numPresetParameters> values{};
        bool valid = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    float readDelaySample(const std::vector<float>& buffer, float delayInSamples) const noexcept;
    float getDelayTimeMsFor(bool rightChannel) const noexcept;
    float getHostBpm() const noexcept;
    void updateMeters(float inputPeak, float delayPeak, float outputPeak) noexcept;
    void initialisePresetTracking();
    void setCurrentPresetMetadata(juce::String presetKey, juce::String presetName, juce::String category, juce::String author, bool isFactory);
    PresetStateSnapshot captureCurrentPresetSnapshot() const;
    PresetStateSnapshot makeDefaultPresetSnapshot() const;
    void normalisePresetSnapshot(PresetStateSnapshot&) const;
    void applyPresetSnapshot(const PresetStateSnapshot&);
    void applyPresetRecord(const PresetRecord& preset, bool updateLoadedPresetReference);
    PresetRecord buildCurrentPresetRecord(juce::String presetName, juce::String category, juce::String author, bool isFactory) const;
    void refreshDirtyFlagFromCurrentState();

    std::array<std::vector<float>, 2> delayBuffers;
    std::array<float, 2> toneStates{{0.0f, 0.0f}};
    int delayBufferSize = 0;
    int writePosition = 0;
    double currentSampleRate = 44100.0;

    std::array<juce::SmoothedValue<float>, 2> timeSmoothed;
    std::array<juce::SmoothedValue<float>, 2> feedbackSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> toneSmoothed;
    juce::SmoothedValue<float> widthSmoothed;
    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> delayMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};

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
