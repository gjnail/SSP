#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    struct EqPoint
    {
        bool enabled = false;
        float frequency = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        int type = 0;
    };

    static constexpr int maxPoints = 8;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    using PointArray = std::array<EqPoint, maxPoints>;
    using SpectrumArray = std::array<float, fftSize / 2>;

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

    static const juce::StringArray& getPointTypeNames();

    EqPoint getPoint(int index) const;
    PointArray getPointsSnapshot() const;
    void setPoint(int index, const EqPoint& point);
    void setPointPosition(int index, float frequency, float gainDb);
    int addPoint(float frequency, float gainDb);
    void removePoint(int index);
    float getResponseForFrequency(float frequency) const;
    int getEnabledPointCount() const;
    SpectrumArray getSpectrumDataCopy() const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncPointCacheFromParameters();
    void pushSpectrumSample(float sample) noexcept;
    void performSpectrumAnalysis();

    double currentSampleRate = 44100.0;
    std::array<std::array<juce::IIRFilter, maxPoints>, 2> filters;
    mutable juce::SpinLock stateLock;
    PointArray pointCache{};
    std::array<juce::IIRCoefficients, maxPoints> coefficientCache{};
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    std::array<float, fftSize> spectrumFifo{};
    std::array<float, fftSize * 2> fftData{};
    SpectrumArray spectrumData{};
    int spectrumFifoIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
