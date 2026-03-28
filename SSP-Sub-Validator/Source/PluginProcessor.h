#pragma once

#include <JuceHeader.h>
#include <array>
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

    float getSubPeakDb() const noexcept;
    bool isSubInRange() const noexcept;
    void copySpectrum(std::vector<float>& destination) const;

private:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopePointCount = 512;

    void pushNextSampleIntoFifo(float sample) noexcept;
    void generateSpectrum();
    static float normaliseMagnitude(float magnitude) noexcept;
    static float frequencyAtNormalisedPosition(float position) noexcept;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> fifo{};
    std::array<float, fftSize * 2> fftData{};
    std::array<float, scopePointCount> scopeData{};
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;
    double currentSampleRate = 44100.0;

    mutable juce::SpinLock spectrumLock;
    std::atomic<float> subPeakDb{-100.0f};
    std::atomic<bool> subInRange{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
