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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static constexpr int visualizerPointCount = 180;

    struct VisualizerData
    {
        std::array<float, visualizerPointCount> input{};
        std::array<float, visualizerPointCount> output{};
        std::array<float, visualizerPointCount> difference{};
        std::array<float, visualizerPointCount> gainMotion{};
        std::array<float, visualizerPointCount> panMotion{};
    };

    static int getFactoryPresetCount() noexcept;
    static juce::StringArray getFactoryPresetNames();
    static juce::StringArray getFactoryPresetCategories();
    static juce::String getFactoryPresetName(int presetIndex);
    static juce::String getFactoryPresetCategory(int presetIndex);
    static juce::String getFactoryPresetTags(int presetIndex);
    static juce::String getRateLabel(int rateIndex);

    void applyFactoryPreset(int presetIndex);
    bool stepFactoryPreset(int delta);
    int getCurrentFactoryPresetIndex() const noexcept;
    juce::String getCurrentPresetName() const;
    juce::String getCurrentPresetCategory() const;
    juce::String getCurrentPresetTags() const;
    bool isCurrentPresetDirty() const noexcept;

    float getCurrentGainOffsetDb() const noexcept;
    float getCurrentPanOffset() const noexcept;
    VisualizerData getVisualizerData() const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    double getRateSeconds() const noexcept;
    void pushVisualizerSample(float inputLevel, float outputLevel, float gainDb, float pan) noexcept;
    void resetVisualizerData() noexcept;

    double currentSampleRate = 44100.0;
    double modulationPhase = 0.0;
    std::atomic<float> currentGainOffsetDb{0.0f};
    std::atomic<float> currentPanOffset{0.0f};
    int currentFactoryPreset = 0;
    int visualizerSamplesPerPoint = 64;
    int visualizerAccumulatorCount = 0;
    float visualizerInputAccumulator = 0.0f;
    float visualizerOutputAccumulator = 0.0f;
    float visualizerDifferenceAccumulator = 0.0f;
    float visualizerGainAccumulator = 0.0f;
    float visualizerPanAccumulator = 0.0f;
    std::array<std::atomic<float>, visualizerPointCount> inputHistory;
    std::array<std::atomic<float>, visualizerPointCount> outputHistory;
    std::array<std::atomic<float>, visualizerPointCount> differenceHistory;
    std::array<std::atomic<float>, visualizerPointCount> gainMotionHistory;
    std::array<std::atomic<float>, visualizerPointCount> panMotionHistory;
    std::atomic<int> visualizerWritePosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
