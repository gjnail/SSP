#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

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
    static constexpr int maxPartialsPerVoice = 3;
    static constexpr int maxResonatorBands = maxResonatorVoices * maxPartialsPerVoice;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    using VoiceArray = std::array<ResonatorVoice, maxResonatorVoices>;
    using SpectrumArray = std::array<float, fftSize / 2>;

    struct AnalyzerFrame
    {
        SpectrumArray pre{};
        SpectrumArray post{};
    };

    struct FactoryPreset
    {
        juce::String name;
        int rootNote = 0;
        int rootOctave = 2;
        int chordType = 0;
        int voicing = 0;
        int vowelMode = 0;
        int motionDivision = 3;
        bool midiFollow = false;
        bool motionSync = true;
        float drive = 1.0f;
        float resonance = 0.94f;
        float brightness = 0.7f;
        float formantMix = 0.7f;
        float detune = 8.0f;
        float stereoWidth = 0.75f;
        float motionDepth = 0.5f;
        float motionRateHz = 2.0f;
        float lowProtect = 0.8f;
        float crossoverHz = 110.0f;
        float transientPreserve = 0.55f;
        float mix = 0.58f;
        float outputDb = -1.0f;
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

    bool acceptsMidi() const override { return true; }
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
    static const juce::StringArray& getVowelNames();
    static const juce::StringArray& getMotionDivisionNames();
    static const juce::Array<FactoryPreset>& getFactoryPresets();
    static juce::StringArray getFactoryPresetNames();

    VoiceArray getVoicesSnapshot() const;
    AnalyzerFrame getAnalyzerFrameCopy() const;
    bool loadFactoryPreset(int index);
    bool stepFactoryPreset(int delta);
    int getCurrentFactoryPresetIndex() const noexcept { return currentFactoryPresetIndex.load(); }
    juce::String getCurrentFactoryPresetName() const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    juce::String getCurrentChordLabel() const;
    juce::String getCurrentChordNoteSummary() const;
    juce::String getCurrentMotionStatus() const;
    juce::String getCurrentProtectionStatus() const;
    juce::String getCurrentSourceStatus() const;
    juce::String getCurrentVowelName() const;
    juce::String getCurrentMonitorStatus() const;
    float getDriveAmount() const;
    float getResonanceAmount() const;
    float getBrightnessAmount() const;
    float getDetuneAmount() const;
    float getWidthAmount() const;
    float getMixAmount() const;
    float getOutputGainDb() const;
    float getFormantMixAmount() const;
    float getMotionDepthAmount() const;
    float getLowProtectAmount() const;
    float getLowProtectCrossoverHz() const;
    float getTransientPreserveAmount() const;
    bool isDifferenceSoloEnabled() const;
    void setDifferenceSoloEnabled(bool shouldBeEnabled);
    float getVisualMotionPhase() const noexcept { return visualMotionPhase.load(); }

private:
    struct ResonatorBand
    {
        bool active = false;
        float frequency[2]{};
        float b0[2]{};
        float b1[2]{};
        float b2[2]{};
        float a1[2]{};
        float a2[2]{};
        float phaseIncrement[2]{};
        float leftLevel = 0.0f;
        float rightLevel = 0.0f;
        float excitationScale = 0.0f;
        float decayCoefficient = 0.999f;
        float textureAmount = 0.0f;
    };

    struct ResonatorState
    {
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;
        float envelope = 0.0f;
        float phase = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidiInput(const juce::MidiBuffer& midiMessages);
    void rebuildResonatorCache();
    void resetResonatorStates();
    int getDefaultRootMidi() const;
    int getEffectiveRootMidi(bool* cameFromMidi = nullptr) const;
    float getMotionRateHz(bool syncEnabled, int divisionIndex) const;
    void updateHostBpmFromPlayHead() noexcept;
    void pushPreSpectrumSample(float sample) noexcept;
    void pushPostSpectrumSample(float sample) noexcept;
    void performSpectrumAnalysis(const std::array<float, fftSize>& source,
                                 SpectrumArray& destination,
                                 float decayAmount);

    double currentSampleRate = 44100.0;
    mutable juce::SpinLock stateLock;
    VoiceArray voiceCache{};
    std::array<ResonatorBand, maxResonatorBands> bandCache{};
    std::array<std::array<ResonatorState, 2>, maxResonatorBands> resonatorStates{};
    std::array<float, 2> inputLowState{{0.0f, 0.0f}};
    std::array<float, 2> wetLowState{{0.0f, 0.0f}};
    std::array<float, 2> transientFastEnv{{0.0f, 0.0f}};
    std::array<float, 2> transientSlowEnv{{0.0f, 0.0f}};
    std::array<std::array<juce::dsp::StateVariableTPTFilter<float>, 2>, 2> formantFilters;
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    std::array<float, fftSize> preSpectrumFifo{};
    std::array<float, fftSize> postSpectrumFifo{};
    std::array<float, fftSize * 2> fftData{};
    AnalyzerFrame analyzerFrame{};
    int preSpectrumFifoIndex = 0;
    int postSpectrumFifoIndex = 0;
    std::vector<int> heldMidiNotes;
    double motionPhase = 0.0;
    std::atomic<int> currentEffectiveRootMidi{60};
    std::atomic<bool> currentRootFromMidi{false};
    std::atomic<float> visualMotionPhase{0.0f};
    std::atomic<float> currentMotionRateHz{2.0f};
    std::atomic<float> currentHostBpm{140.0f};
    std::atomic<int> currentFactoryPresetIndex{-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
