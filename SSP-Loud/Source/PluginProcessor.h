#pragma once

#include <JuceHeader.h>
#include <array>
#include <deque>
#include <vector>

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

    float getMomentaryLufs() const noexcept;
    float getShortTermLufs() const noexcept;
    float getIntegratedLufs() const noexcept;
    float getPeakDb() const noexcept;
    float getLeftMeterLevel() const noexcept;
    float getRightMeterLevel() const noexcept;
    void requestReset() noexcept;
    void copyHistory(std::vector<float>& destination) const;

private:
    struct WindowEntry
    {
        double durationSeconds = 0.0;
        double energyIntegral = 0.0;
    };

    void resetMeasurementState();
    void pushWindowEntry(std::deque<WindowEntry>& entries,
                         double& timeTotal,
                         double& energyTotal,
                         WindowEntry entry,
                         double maxWindowSeconds);
    void pushHistoryPoint(float value);
    float energyToLufs(double energy) const noexcept;
    void updateMeters(float leftPeak, float rightPeak, float peakDb) noexcept;

    std::array<juce::dsp::IIR::Filter<float>, 2> highPassFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> highShelfFilters;
    double currentSampleRate = 44100.0;

    std::deque<WindowEntry> momentaryEntries;
    std::deque<WindowEntry> shortTermEntries;
    double momentaryTimeTotal = 0.0;
    double momentaryEnergyTotal = 0.0;
    double shortTermTimeTotal = 0.0;
    double shortTermEnergyTotal = 0.0;
    double integratedTimeTotal = 0.0;
    double integratedEnergyTotal = 0.0;
    double historyIntervalAccumulator = 0.0;

    mutable juce::SpinLock historyLock;
    std::array<float, 360> historyValues{};
    int historyCount = 0;
    int historyWriteIndex = 0;

    std::atomic<float> momentaryLufs{-100.0f};
    std::atomic<float> shortTermLufs{-100.0f};
    std::atomic<float> integratedLufs{-100.0f};
    std::atomic<float> peakDb{-60.0f};
    std::atomic<float> leftMeterLevel{0.0f};
    std::atomic<float> rightMeterLevel{0.0f};
    std::atomic<bool> resetRequested{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
