#pragma once

#include <JuceHeader.h>
#include <array>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    enum StereoMode
    {
        stereo = 0,
        left,
        right,
        mid,
        side
    };

    struct ResonatorVoice
    {
        bool active = false;
        float frequency = 440.0f;
        float level = 0.0f;
        float pan = 0.0f;
        int midiNote = 69;
    };

    static constexpr int maxResonatorVoices = 8;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    using VoiceArray = std::array<ResonatorVoice, maxResonatorVoices>;
    using SpectrumArray = std::array<float, fftSize / 2>;

    struct AnalyzerFrame
    {
        SpectrumArray pre{};
        SpectrumArray post{};
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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& getRootNoteNames();
    static const juce::StringArray& getOctaveNames();
    static const juce::StringArray& getChordNames();
    static const juce::StringArray& getVoicingNames();

    VoiceArray getVoicesSnapshot() const;
    AnalyzerFrame getAnalyzerFrameCopy() const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    juce::String getCurrentChordLabel() const;
    juce::String getCurrentChordNoteSummary() const;
    float getDriveAmount() const;
    float getResonanceAmount() const;
    float getBrightnessAmount() const;
    float getDetuneAmount() const;
    float getWidthAmount() const;
    float getMixAmount() const;
    float getOutputGainDb() const;

private:
    struct ResonatorCoefficients
    {
        bool active = false;
        float a1[2]{};
        float a2[2]{};
        float leftLevel = 0.0f;
        float rightLevel = 0.0f;
        float exciteGain = 0.0f;
        float dampingAlpha = 0.1f;
    };

    struct ResonatorState
    {
        float y1 = 0.0f;
        float y2 = 0.0f;
        float damped = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void rebuildResonatorCache();
    void resetResonatorStates();
    void pushPreSpectrumSample(float sample) noexcept;
    void pushPostSpectrumSample(float sample) noexcept;
    void performSpectrumAnalysis(const std::array<float, fftSize>& source,
                                 SpectrumArray& destination,
                                 float decayAmount);

    double currentSampleRate = 44100.0;
    mutable juce::SpinLock stateLock;
    VoiceArray voiceCache{};
    std::array<ResonatorCoefficients, maxResonatorVoices> coefficientCache{};
    std::array<std::array<ResonatorState, 2>, maxResonatorVoices> resonatorStates{};
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    std::array<float, fftSize> preSpectrumFifo{};
    std::array<float, fftSize> postSpectrumFifo{};
    std::array<float, fftSize * 2> fftData{};
    AnalyzerFrame analyzerFrame{};
    int preSpectrumFifoIndex = 0;
    int postSpectrumFifoIndex = 0;
    float feedbackMemory = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
