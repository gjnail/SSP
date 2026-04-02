#pragma once

#include <array>
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
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputMeterLevel() const noexcept;
    float getResonatorMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;

private:
    static constexpr int maxChordNotes = 5;
    static constexpr int partialsPerNote = 4;
    static constexpr int maxVoices = maxChordNotes * partialsPerNote;

    struct BiquadCoefficients
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    struct BiquadState
    {
        void reset() noexcept;
        float process(float input, const BiquadCoefficients& coefficients) noexcept;

        float s1 = 0.0f;
        float s2 = 0.0f;
    };

    struct VoiceConfig
    {
        float baseGain = 0.0f;
        int noteIndex = 0;
        BiquadCoefficients leftCoefficients;
        BiquadCoefficients rightCoefficients;
        float leftPhaseDelta = 0.0f;
        float rightPhaseDelta = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static BiquadCoefficients makeBandPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept;
    static BiquadCoefficients makeHighPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept;
    static BiquadCoefficients makeLowPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept;

    void refreshTempo() noexcept;
    void refreshDiscreteParameters() noexcept;
    void refreshExciterFilters(float lowCutHz, float highCutHz, float q) noexcept;
    void refreshVoices(float warp, float drift, float shiftHz, float tilt, float depth, float decayMs) noexcept;
    void updateArpTargets() noexcept;
    void stepArpeggiator() noexcept;
    void updateMeters(float inputPeak, float resonatorPeak, float outputPeak) noexcept;

    double currentSampleRate = 44100.0;
    float hostBpm = 120.0f;
    int arpDirection = 1;
    int arpStepIndex = 0;
    int currentChordNoteCount = 3;
    int activeVoiceCount = 0;
    double samplesUntilArpStep = 0.0;

    int rootNoteIndex = 0;
    int chordIndex = 0;
    int octaveIndex = 2;
    int spreadIndex = 3;
    int arpPatternIndex = 0;
    bool arpEnabled = false;
    bool arpSync = true;
    bool motionSync = false;

    std::array<float, maxChordNotes> chordFrequencies{};
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, maxChordNotes> noteWeightSmoothers;
    std::array<VoiceConfig, maxVoices> voiceConfigs{};
    std::array<std::array<BiquadState, maxVoices>, 2> voiceStates{};
    std::array<std::array<float, maxVoices>, 2> voiceDriftPhases{};
    std::array<std::array<float, maxVoices>, 2> oscillatorPhases{};
    std::array<float, maxVoices> voiceDriftRates{};

    std::array<BiquadState, 2> inputHighPassStates;
    std::array<BiquadState, 2> inputLowPassStates;
    std::array<BiquadCoefficients, 2> inputHighPassCoefficients;
    std::array<BiquadCoefficients, 2> inputLowPassCoefficients;

    std::array<float, 2> excitationEnvelopes{};
    std::array<float, 2> motionPhases{};

    juce::SmoothedValue<float> warpSmoothed;
    juce::SmoothedValue<float> driftSmoothed;
    juce::SmoothedValue<float> shiftSmoothed;
    juce::SmoothedValue<float> tiltSmoothed;
    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> lowCutSmoothed;
    juce::SmoothedValue<float> highCutSmoothed;
    juce::SmoothedValue<float> filterQSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    juce::SmoothedValue<float> motionStrengthSmoothed;
    juce::SmoothedValue<float> motionShapeSmoothed;

    std::atomic<float> inputMeter{0.0f};
    std::atomic<float> resonatorMeter{0.0f};
    std::atomic<float> outputMeter{0.0f};

    juce::Random random { 0x51534d50 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
