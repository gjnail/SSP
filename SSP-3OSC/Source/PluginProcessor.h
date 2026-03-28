#pragma once

#include <JuceHeader.h>
#include "ModulationConfig.h"

class SynthVoice;

class PluginProcessor final : public juce::AudioProcessor
{
public:
    struct OscillatorSampleData final : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<OscillatorSampleData>;

        juce::AudioBuffer<float> buffer;
        double sourceSampleRate = 44100.0;
    };

    struct FactoryPresetInfo
    {
        int index = 0;
        juce::String name;
        juce::String pack;
        juce::String category;
        juce::String subCategory;
        juce::String description;
    };

    struct ModulationSnapshot
    {
        std::vector<reactormod::DynamicLfoData> lfos;
        std::array<int, reactormod::matrixSlotCount> matrixSources{};
        std::array<bool, reactormod::matrixSlotCount> matrixEnabled{};
    };

    struct DestinationModulationInfo
    {
        int sourceIndex = 0;
        float amount = 0.0f;
        int slotIndex = -1;
        int routeCount = 0;
    };

    struct ModulationRouteInfo
    {
        int slotIndex = -1;
        int sourceIndex = 0;
        reactormod::Destination destination = reactormod::Destination::none;
        float amount = 0.0f;
        bool enabled = false;
        juce::String destinationName;
        juce::String parameterID;
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const juce::StringArray& getWaveNames();
    static const juce::StringArray& getOscillatorTypeNames();
    static const juce::StringArray& getWavetableNames();
    static const juce::StringArray& getOctaveNames();
    static const juce::StringArray& getSubWaveNames();
    static const juce::StringArray& getFilterModeNames();
    static const juce::StringArray& getFactoryPresetNames();
    static const juce::Array<FactoryPresetInfo>& getFactoryPresetLibrary();
    static const juce::StringArray& getWarpFMSourceNames();
    static const juce::StringArray& getWarpModeNames();
    static const juce::StringArray& getNoiseTypeNames();
    static const juce::StringArray& getFXModuleNames();
    static bool isSupportedFXModuleType(int moduleType);
    static constexpr int maxFXSlots = 9;
    static constexpr int fxSlotParameterCount = 12;

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::Synthesiser& getSynth() noexcept { return synth; }
    const juce::StringArray& getUserPresetNames() const noexcept { return userPresetNames; }
    juce::String getCurrentPresetName() const noexcept { return currentPresetName; }
    bool isCurrentPresetFactory() const noexcept { return currentPresetIsFactory; }
    juce::File getUserPresetDirectory() const;
    void refreshUserPresets();
    bool loadFactoryPreset(int index);
    void loadBasicInitPatch();
    bool loadUserPreset(int index);
    bool saveUserPreset(const juce::String& presetName);
    juce::Array<int> getFXOrder() const;
    bool addFXModule(int typeIndex);
    void removeFXModuleFromOrder(int orderIndex);
    void moveFXModule(int fromIndex, int toIndex);
    static const juce::StringArray& getFXDriveTypeNames();
    static const juce::StringArray& getFXFilterTypeNames();
    static const juce::StringArray& getFXEQTypeNames();
    static const juce::StringArray& getFXReverbTypeNames();
    static const juce::StringArray& getFXDelayTypeNames();
    static const juce::StringArray& getFXCompressorTypeNames();
    static const juce::StringArray& getFXBitcrusherTypeNames();
    static const juce::StringArray& getFXShiftTypeNames();
    static const juce::StringArray& getFXGateTypeNames();
    static constexpr int fxImagerScopePointCount = 96;
    static constexpr int fxEQSpectrumBinCount = 160;
    int getFXSlotType(int slotIndex) const;
    bool isFXSlotActive(int slotIndex) const;
    bool isFXSlotParallel(int slotIndex) const;
    int getFXSlotVariant(int slotIndex) const;
    void setFXSlotVariant(int slotIndex, int variantIndex);
    float getFXCompressorGainReduction(int slotIndex) const;
    void getFXMultibandMeter(int slotIndex, std::array<float, 3>& upward, std::array<float, 3>& downward) const;
    void getFXEQSpectrum(int slotIndex, std::array<float, fxEQSpectrumBinCount>& spectrum) const;
    void getFXImagerScope(int slotIndex,
                          std::array<float, fxImagerScopePointCount>& xs,
                          std::array<float, fxImagerScopePointCount>& ys) const;
    bool loadOscillatorSample(int oscillatorIndex, const juce::File& file);
    void clearOscillatorSample(int oscillatorIndex);
    juce::String getOscillatorSamplePath(int oscillatorIndex) const;
    juce::String getOscillatorSampleDisplayName(int oscillatorIndex) const;
    OscillatorSampleData::Ptr getOscillatorSampleData(int oscillatorIndex) const;
    ModulationSnapshot getModulationSnapshot() const;
    int getModulationLfoCount() const;
    reactormod::DynamicLfoData getModulationLfo(int index) const;
    juce::StringArray getModulationLfoNames() const;
    float getModulationLfoDisplayPhase(int index) const;
    int addModulationLfo();
    void updateModulationLfo(int index, const reactormod::DynamicLfoData& data);
    int getMatrixSourceForSlot(int slotIndex) const;
    void setMatrixSourceForSlot(int slotIndex, int sourceIndex);
    DestinationModulationInfo getDestinationModulationInfo(reactormod::Destination destination) const;
    std::vector<ModulationRouteInfo> getRoutesForLfo(int lfoIndex) const;
    void setMatrixSlotEnabled(int slotIndex, bool enabled);
    void setMatrixAmountForSlot(int slotIndex, float amount);
    void removeMatrixSlot(int slotIndex);
    int assignLfoToDestination(int sourceIndex, reactormod::Destination destination, float defaultAmount = 1.0f);
    double getCurrentHostBpm() const noexcept { return currentHostBpm; }
    bool isLightThemeEnabled() const;
    void setLightThemeEnabled(bool enabled);

private:
    struct PhaserStage
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    struct DelayState
    {
        std::vector<float> left;
        std::vector<float> right;
        std::array<float, 2> lowpassState{};
        std::array<float, 2> highpassState{};
        std::array<float, 2> previousHighpassInput{};
        int writePosition = 0;
    };

    struct DimensionState
    {
        std::vector<float> left;
        std::vector<float> right;
        int writePosition = 0;
    };

    struct SlotPhaserState
    {
        std::array<PhaserStage, 4> left{};
        std::array<PhaserStage, 4> right{};
        float lfoPhase = 0.0f;
        float feedbackLeft = 0.0f;
        float feedbackRight = 0.0f;
    };

    struct SlotCompressorState
    {
        float envelope = 0.0f;
        float makeup = 1.0f;
        float lowEnvelope = 0.0f;
        float highEnvelope = 0.0f;
        std::array<float, 2> lowpassState{};
        std::array<float, 2> highpassState{};
        std::array<float, 3> downwardEnvelopes{};
        std::array<float, 3> upwardEnvelopes{};
        std::array<float, 3> downwardMeters{};
        std::array<float, 3> upwardMeters{};
    };

    struct SlotBitcrusherState
    {
        std::array<float, 2> heldSample{};
        std::array<int, 2> holdCounter{};
        std::array<int, 2> holdSamplesTarget{};
        std::array<float, 2> toneState{};
    };

    struct SlotImagerState
    {
        std::array<float, 2> lowState1{};
        std::array<float, 2> lowState2{};
        std::array<float, 2> lowState3{};
        int scopeWriteIndex = 0;
        int scopeCounter = 0;
    };

    struct SlotShiftState
    {
        std::vector<float> left;
        std::vector<float> right;
        int writePosition = 0;
        float modPhaseL = 0.0f;
        float modPhaseR = 1.7f;
        float carrierPhaseL = 0.0f;
        float carrierPhaseR = 1.57f;
        std::array<float, 2> feedbackState{};
        std::array<float, 2> allpassX1{};
        std::array<float, 2> allpassY1{};
        std::array<float, 2> allpassX2{};
        std::array<float, 2> allpassY2{};
    };

    struct SlotGateState
    {
        float detectorEnvelope = 0.0f;
        float gateEnvelope = 0.0f;
        int holdSamplesRemaining = 0;
        float rhythmPhase = 0.0f;
    };

    juce::Synthesiser synth;
    juce::MidiKeyboardState keyboardState;
    bool lastMonoMode = false;
    juce::Array<juce::File> userPresetFiles;
    juce::StringArray userPresetNames;
    juce::String currentPresetName;
    bool currentPresetIsFactory = true;
    juce::AudioBuffer<float> effectScratch;
    juce::AudioBuffer<float> effectParallelSerialTap;
    juce::AudioBuffer<float> effectParallelDryMix;
    double currentSampleRate = 44100.0;
    double currentHostBpm = 120.0;
    std::array<juce::dsp::Chorus<float>, maxFXSlots> chorusSlots;
    std::array<juce::dsp::Chorus<float>, maxFXSlots> flangerSlots;
    std::array<juce::dsp::Reverb, maxFXSlots> reverbSlots;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterSlotsL;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterSlotsR;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterStage2SlotsL;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterStage2SlotsR;
    std::array<std::array<juce::IIRFilter, 4>, maxFXSlots> fxEQSlotsL;
    std::array<std::array<juce::IIRFilter, 4>, maxFXSlots> fxEQSlotsR;
    std::array<std::array<float, 2>, maxFXSlots> driveToneStates{};
    std::array<DelayState, maxFXSlots> delayStates{};
    std::array<DimensionState, maxFXSlots> dimensionStates{};
    std::array<SlotPhaserState, maxFXSlots> phaserStates{};
    std::array<SlotCompressorState, maxFXSlots> compressorStates{};
    std::array<SlotBitcrusherState, maxFXSlots> bitcrusherStates{};
    std::array<SlotImagerState, maxFXSlots> imagerStates{};
    std::array<SlotShiftState, maxFXSlots> shiftStates{};
    std::array<float, maxFXSlots> tremoloPhases{};
    std::array<SlotGateState, maxFXSlots> gateStates{};
    std::array<std::atomic<float>, maxFXSlots> compressorGainReductionMeters{};
    std::array<std::array<std::atomic<float>, 3>, maxFXSlots> multibandUpwardMeters{};
    std::array<std::array<std::atomic<float>, 3>, maxFXSlots> multibandDownwardMeters{};
    std::array<std::array<std::atomic<float>, fxImagerScopePointCount>, maxFXSlots> imagerScopeX{};
    std::array<std::array<std::atomic<float>, fxImagerScopePointCount>, maxFXSlots> imagerScopeY{};
    std::array<std::array<std::atomic<float>, fxEQSpectrumBinCount>, maxFXSlots> eqSpectrumBins{};
    juce::dsp::FFT eqSpectrumFFT { 11 };
    juce::dsp::WindowingFunction<float> eqSpectrumWindow { 1 << 11, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, (1 << 12)> eqSpectrumFFTData{};
    juce::Random fxRandom;
    juce::AudioFormatManager audioFormatManager;
    mutable juce::SpinLock oscillatorSampleLock;
    std::array<OscillatorSampleData::Ptr, 3> oscillatorSamples;
    std::array<juce::String, 3> oscillatorSamplePaths;
    mutable juce::SpinLock modulationStateLock;
    std::vector<reactormod::DynamicLfoData> modulationLfos;
    std::array<int, reactormod::matrixSlotCount> matrixSourceIndices{};
    std::array<bool, reactormod::matrixSlotCount> matrixSlotEnableds{};
    std::vector<float> effectModulationLfoPhases;
    std::vector<float> effectModulationLfoHeldValues;
    std::vector<bool> effectModulationOneShotFinished;
    std::array<float, reactormod::fxSlotCount * reactormod::fxParameterCount> effectModulationAmounts{};
    bool effectModulationActive = false;
    std::array<std::atomic<float>, 128> modulationDisplayPhases{};
    std::array<bool, 128> modulationDisplayOneShotFinished{};
    std::array<bool, 128> modulationHeldNotes{};
    int modulationHeldNoteCount = 0;

    void updateVoiceConfiguration();
    void setCurrentPreset(juce::String presetName, bool isFactory);
    void prepareEffectProcessors(double sampleRate, int samplesPerBlock);
    void applyEffects(juce::AudioBuffer<float>& buffer);
    int getFXOrderSlot(int slotIndex) const;
    void setFXOrderSlot(int slotIndex, int moduleTypeIndex);
    void compactFXOrder();
    void initialiseFXSlotDefaults(int slotIndex, int moduleTypeIndex);
    void copyFXSlotState(int sourceIndex, int destinationIndex);
    void clearFXSlotState(int slotIndex);
    void resetPhaserState(int slotIndex);
    void resetShiftState(int slotIndex);
    void resetEffectState();
    void sanitiseFXChain();
    void updateFXEQSpectrum(int slotIndex, const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void clearFXEQSpectrum(int slotIndex);
    void clearFXImagerScope(int slotIndex);
    void migrateLegacyWarpModes(bool forceFromLegacy);
    void initialiseModulationState(bool forceLegacyMatrix);
    juce::ValueTree getOrCreateModulationStateTree();
    juce::ValueTree getOrCreateModulationLfoBankTree();
    juce::ValueTree getOrCreateModMatrixTree();
    void writeModulationStateToTree();
    void restoreOscillatorSampleState();
    void clearAllOscillatorSamples();
    static juce::String getOscillatorSamplePropertyID(int oscillatorIndex);
    void applyStoredThemeMode();
    void updateDisplayedLfoState(const juce::MidiBuffer& midiMessages, int numSamples);
    void updateEffectModulationState(int numSamples);
    float getFXSlotFloatValue(int slotIndex, int parameterIndex) const;
    bool getFXSlotOnValue(int slotIndex) const;
    bool getFXSlotParallelValue(int slotIndex) const;
    static float processPhaserSample(float input, std::array<PhaserStage, 4>& stages, float coeff, float& feedbackState);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
