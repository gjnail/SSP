#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthVoice final : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice(PluginProcessor&);

    void prepare(double sampleRate, int samplesPerBlock, int outputChannels);

    bool canPlaySound(juce::SynthesiserSound*) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int controllerNumber, int newValue) override;
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    enum class FilterMode
    {
        low12 = 0,
        low24,
        high12,
        high24,
        band12,
        band24,
        notch,
        peak,
        lowShelf,
        highShelf,
        allPass,
        combPlus,
        combMinus,
        formant,
        flangePlus,
        flangeMinus,
        vowelA,
        vowelE,
        vowelI,
        vowelO,
        vowelU,
        tilt,
        resonator,
        ladderDrive,
        metalComb,
        phaseWarp
    };

    float renderOscillatorSample(int oscIndex, float phase, const std::array<float, 3>& wavetablePositions) const;
    static float wrapPhase(float phase);
    static float bendPhase(float phase, float amount);
    static float saturate(float sample, float drive);
    static float panLeft(float pan);
    static float panRight(float pan);
    static float resonanceToQ(float resonance);
    static float resonanceToGainDb(float resonance);
    static float resonanceToNormalised(float resonance);
    float applyMutate(int oscIndex, float phase, float sample, float amount,
                      const std::array<float, 3>& wavetablePositions) const;
    float applyWarpMode(int oscIndex, int voiceIndex, float phase, int modeIndex, float amount,
                        float noiseSample, float lfo, float mutateAmount,
                        const std::array<float, 3>& wavetablePositions) const;
    float renderNoiseSample(int noiseTypeIndex);
    float renderFMSourceSample(int sourceIndex, int targetOsc, int voiceIndex, float noiseSample, float lfo,
                               const std::array<float, 3>& wavetablePositions) const;

    void updateFilterState(FilterMode mode, float cutoffHz, float resonance);
    float processFilterSample(int channel, float input, FilterMode mode, float cutoffHz, float resonance);
    float processCombSample(float input, std::vector<float>& delayBuffer, int& writePosition, float delaySamples, float feedback, bool invert);

    PluginProcessor& processor;
    static constexpr int maxUnisonVoices = 7;

    std::array<std::array<float, maxUnisonVoices>, 3> oscPhases{};
    std::array<std::array<float, maxUnisonVoices>, 3> oscSyncPhases{};
    std::array<std::array<double, maxUnisonVoices>, 3> oscSamplePositions{};
    std::array<std::atomic<float>*, 3> oscType{};
    std::array<std::atomic<float>*, 3> oscWave{};
    std::array<std::atomic<float>*, 3> oscWavetable{};
    std::array<std::atomic<float>*, 3> oscWTPosition{};
    std::array<std::atomic<float>*, 3> oscLevel{};
    std::array<std::atomic<float>*, 3> oscOctave{};
    std::array<std::atomic<float>*, 3> oscSampleRoot{};
    std::array<std::atomic<float>*, 3> oscDetune{};
    std::array<std::atomic<float>*, 3> oscPan{};
    std::array<std::atomic<float>*, 3> oscWidth{};
    std::array<std::atomic<float>*, 3> oscToFilter{};
    std::array<std::atomic<float>*, 3> oscUnisonOn{};
    std::array<std::atomic<float>*, 3> oscUnisonVoices{};
    std::array<std::atomic<float>*, 3> oscUnison{};
    std::array<std::atomic<float>*, 3> oscWarpFM{};
    std::array<std::atomic<float>*, 3> oscWarpFMSource{};
    std::array<std::array<std::atomic<float>*, 2>, 3> oscWarpMode{};
    std::array<std::array<std::atomic<float>*, 2>, 3> oscWarpAmount{};
    std::array<std::atomic<float>*, 3> oscWarpSync{};
    std::array<std::atomic<float>*, 3> oscWarpBend{};
    std::atomic<float>* subWave = nullptr;
    std::atomic<float>* subLevel = nullptr;
    std::atomic<float>* subOctave = nullptr;
    std::atomic<float>* subDirectOut = nullptr;
    std::atomic<float>* voiceMode = nullptr;
    std::atomic<float>* voiceLegato = nullptr;
    std::atomic<float>* voicePortamento = nullptr;
    std::atomic<float>* voiceGlide = nullptr;
    std::atomic<float>* pitchBendUp = nullptr;
    std::atomic<float>* pitchBendDown = nullptr;
    std::atomic<float>* modWheelCutoff = nullptr;
    std::atomic<float>* modWheelVibrato = nullptr;
    std::atomic<float>* warpSaturator = nullptr;
    std::atomic<float>* warpSaturatorPostFilter = nullptr;
    std::atomic<float>* warpMutate = nullptr;
    std::atomic<float>* noiseAmount = nullptr;
    std::atomic<float>* noiseType = nullptr;
    std::atomic<float>* noiseToFilter = nullptr;
    std::atomic<float>* noiseToAmpEnv = nullptr;
    std::atomic<float>* filterOn = nullptr;
    std::atomic<float>* filterMode = nullptr;
    std::atomic<float>* filterCutoff = nullptr;
    std::atomic<float>* filterResonance = nullptr;
    std::atomic<float>* filterDrive = nullptr;
    std::atomic<float>* filterEnvAmount = nullptr;
    std::atomic<float>* filterAttack = nullptr;
    std::atomic<float>* filterDecay = nullptr;
    std::atomic<float>* filterSustain = nullptr;
    std::atomic<float>* filterRelease = nullptr;
    std::atomic<float>* ampAttack = nullptr;
    std::atomic<float>* ampDecay = nullptr;
    std::atomic<float>* ampSustain = nullptr;
    std::atomic<float>* ampRelease = nullptr;
    std::array<std::atomic<float>*, reactormod::matrixSlotCount> modMatrixDestinations{};
    std::array<std::atomic<float>*, reactormod::matrixSlotCount> modMatrixAmounts{};
    std::atomic<float>* masterSpread = nullptr;
    std::atomic<float>* masterGain = nullptr;

    juce::ADSR ampEnvelope;
    juce::ADSR filterEnvelope;
    juce::ADSR::Parameters adsrParameters;
    juce::ADSR::Parameters filterAdsrParameters;
    std::array<juce::dsp::IIR::Filter<float>, 2> filterA;
    std::array<juce::dsp::IIR::Filter<float>, 2> filterB;
    juce::Random random;
    std::vector<float> combBufferL;
    std::vector<float> combBufferR;
    double sampleRate = 44100.0;
    float noteFrequency = 440.0f;
    float currentNoteFrequency = 440.0f;
    float targetNoteFrequency = 440.0f;
    float noteVelocity = 0.0f;
    std::vector<float> lfoPhases;
    std::vector<bool> lfoOneShotFinished;
    float pitchWheelNormalised = 0.0f;
    float modWheelValue = 0.0f;
    float smoothedCutoff = 2200.0f;
    float lastFilterCutoff = 2200.0f;
    float lastFilterResonance = 0.7f;
    int filterUpdateCountdown = 0;
    int lastFilterModeIndex = -1;
    int combWritePositionL = 0;
    int combWritePositionR = 0;
    int outputChannels = 2;
    float pinkNoiseState = 0.0f;
    float brownNoiseState = 0.0f;
    float digitalNoiseState = 0.0f;
    float crackleNoiseState = 0.0f;
    float radioNoisePhase = 0.0f;
    float ventNoisePhase = 0.0f;
    float lastWhiteNoise = 0.0f;
    float previousWhiteNoise = 0.0f;
    int digitalNoiseCounter = 0;
    float subOscPhase = 0.0f;
};
