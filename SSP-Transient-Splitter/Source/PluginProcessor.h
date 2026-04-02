#pragma once

#include <JuceHeader.h>
#include "ModulationConfig.h"

class PluginProcessor final : public juce::AudioProcessor
{
public:
    enum PathId
    {
        transientPath = 0,
        bodyPath = 1
    };

    struct DestinationModulationInfo
    {
        int sourceIndex = 0;
        float amount = 0.0f;
        int slotIndex = -1;
        int routeCount = 0;
    };

    struct StereoMeterSnapshot
    {
        float leftPeak = 0.0f;
        float rightPeak = 0.0f;
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        bool clipped = false;
    };

    static constexpr int slotsPerPath = 8;
    static constexpr int maxFXSlots = slotsPerPath * 2;
    static constexpr int fxSlotParameterCount = 12;
    static constexpr int fxImagerScopePointCount = 96;
    static constexpr int fxEQSpectrumBinCount = 160;
    static constexpr int visualizerHistorySize = 320;
    static constexpr int splitEditorDisplayPointCount = 16384;
    static constexpr int manualMarkerCount = 4;
    static constexpr float splitEditorCaptureDurationMs = 12000.0f;

    enum SplitMode
    {
        splitModeAuto = 0,
        splitModeAssisted,
        splitModeManual
    };

    enum ManualMarkerId
    {
        manualMarkerTransientStart = 0,
        manualMarkerTransientEnd,
        manualMarkerBodyStart,
        manualMarkerBodyEnd
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
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static const juce::StringArray& getFXModuleNames();
    static bool isSupportedFXModuleType(int moduleType);
    static const juce::StringArray& getFXDriveTypeNames();
    static const juce::StringArray& getFXFilterTypeNames();
    static const juce::StringArray& getFXEQTypeNames();
    static const juce::StringArray& getFXReverbTypeNames();
    static const juce::StringArray& getFXDelayTypeNames();
    static const juce::StringArray& getFXCompressorTypeNames();
    static const juce::StringArray& getFXBitcrusherTypeNames();
    static const juce::StringArray& getFXShiftTypeNames();
    static const juce::StringArray& getFXGateTypeNames();

    int getFXSlotType(int slotIndex) const;
    bool isFXSlotActive(int slotIndex) const;
    int getFXSlotVariant(int slotIndex) const;
    void setFXSlotVariant(int slotIndex, int variantIndex);
    float getFXCompressorGainReduction(int slotIndex) const;
    void getFXMultibandMeter(int slotIndex, std::array<float, 3>& upward, std::array<float, 3>& downward) const;
    void getFXEQSpectrum(int slotIndex, std::array<float, fxEQSpectrumBinCount>& spectrum) const;
    void getFXImagerScope(int slotIndex,
                          std::array<float, fxImagerScopePointCount>& xs,
                          std::array<float, fxImagerScopePointCount>& ys) const;

    int getGlobalSlotIndex(int path, int localSlotIndex) const noexcept;
    juce::Array<int> getFXOrderForPath(int path) const;
    bool addFXModule(int path, int typeIndex);
    void removeFXModuleFromPath(int path, int localSlotIndex);
    void moveFXModuleWithinPath(int path, int fromLocalIndex, int toLocalIndex);
    void copyPathChain(int sourcePath, int destinationPath);

    const juce::String& getCurrentPresetName() const noexcept;
    juce::String getCurrentPresetDescription() const;
    int getActiveABSlot() const noexcept;
    bool isABStateValid(int slotIndex) const noexcept;
    static const juce::StringArray& getFactoryPresetNames();
    static const juce::StringArray& getFactoryPresetCategories();
    static juce::String getFactoryPresetCategory(int presetIndex);
    juce::StringArray getUserPresetNames() const;
    bool loadFactoryPreset(int presetIndex);
    bool saveUserPreset(const juce::String& presetName);
    bool loadUserPreset(const juce::String& presetName);
    bool deleteUserPreset(const juce::String& presetName);
    void storeABState(int slotIndex);
    bool recallABState(int slotIndex);
    int getSplitModeIndex() const noexcept;
    void armSplitEditorCapture() noexcept;
    void clearSplitEditorCapture() noexcept;
    bool isSplitEditorCaptureFrozen() const noexcept;
    bool isSplitEditorCaptureArmed() const noexcept;
    float getSplitEditorCaptureDurationMs() const noexcept;
    void getSplitEditorWaveform(std::array<float, splitEditorDisplayPointCount>& waveform, bool& frozen) const;
    std::array<float, manualMarkerCount> getManualMarkerValuesMs() const;
    void setManualMarkerValueMs(int markerIndex, float valueMs);
    bool hasPendingSplitEditorSuggestedMarkers() const noexcept;
    void applyPendingSplitEditorSuggestedMarkers();

    StereoMeterSnapshot getInputMeterSnapshot() const noexcept;
    StereoMeterSnapshot getTransientMeterSnapshot() const noexcept;
    StereoMeterSnapshot getBodyMeterSnapshot() const noexcept;
    StereoMeterSnapshot getOutputMeterSnapshot() const noexcept;
    float getDetectionActivity() const noexcept;
    void clearOutputClipLatch() noexcept;
    void getVisualizerData(std::array<float, visualizerHistorySize>& waveform,
                           std::array<float, visualizerHistorySize>& transientEnvelope,
                           std::array<float, visualizerHistorySize>& bodyEnvelope,
                           int& writePosition) const;
    void getBeforeAfterVisualizerData(std::array<float, visualizerHistorySize>& before,
                                      std::array<float, visualizerHistorySize>& after,
                                      int& writePosition) const;

    DestinationModulationInfo getDestinationModulationInfo(reactormod::Destination, int preferredSourceIndex = 0) const;
    int getSelectedModulationSourceIndex() const noexcept;
    void setSelectedModulationSourceIndex(int sourceIndex) noexcept;
    int assignSourceToDestination(int sourceIndex, reactormod::Destination destination, float defaultAmount = 1.0f);
    void setMatrixAmountForSlot(int slotIndex, float amount);

    enum FXModuleType
    {
        fxOff = 0,
        fxDrive,
        fxFilter,
        fxChorus,
        fxFlanger,
        fxPhaser,
        fxDelay,
        fxReverb,
        fxCompressor,
        fxEQ,
        fxMultiband,
        fxBitcrusher,
        fxImager,
        fxShift,
        fxTremolo,
        fxGate
    };

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
        std::array<float, 2> lowpassState{};
        std::array<float, 2> highpassState{};
        std::array<float, 3> downwardEnvelopes{};
        std::array<float, 3> upwardEnvelopes{};
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

    struct MeterState
    {
        std::atomic<float> leftPeak{0.0f};
        std::atomic<float> rightPeak{0.0f};
        std::atomic<float> leftRms{0.0f};
        std::atomic<float> rightRms{0.0f};
        std::atomic<bool> clipped{false};
    };

    static constexpr int maxInternalBlockSize = 32768;

    void updateHostTempo();
    void updateLatencyFromParameters();
    void updateSidechainFilters();
    void processChunk(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyEffects(juce::AudioBuffer<float>& buffer, int slotStart, int slotEnd);
    void prepareEffectProcessors(double sampleRate, int samplesPerBlock);
    void resetEffectState();
    void resetPhaserState(int slotIndex);
    void resetShiftState(int slotIndex);
    void clearFXEQSpectrum(int slotIndex);
    void clearFXImagerScope(int slotIndex);
    void updateFXEQSpectrum(int slotIndex, const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void copyFXSlotState(int sourceIndex, int destinationIndex);
    void clearFXSlotState(int slotIndex);
    void initialiseFXSlotDefaults(int slotIndex, int moduleTypeIndex);
    void compactPathSlots(int path);
    void resetSplitterParametersToDefaults();
    void resetDetectionState();
    std::array<float, manualMarkerCount> getOrderedManualMarkerValuesMs() const;
    void setManualMarkerValuesMs(const std::array<float, manualMarkerCount>& valuesMs);
    float getConfiguredSplitEditorCaptureDurationMs() const noexcept;
    void updateSplitEditorCaptureWindow() noexcept;
    void pushSplitEditorLiveSample(float sample) noexcept;
    void finaliseSplitEditorCapture(bool updateSuggestedMarkers);
    void applySuggestedManualMarkersFromCapture();
    float getManualTransientWeightForAgeSamples(int ageSamples) const;
    float getFXSlotFloatValue(int slotIndex, int parameterIndex) const;
    bool getFXSlotOnValue(int slotIndex) const;
    int getFXOrderSlot(int slotIndex) const;
    void setFXOrderSlot(int slotIndex, int moduleTypeIndex);
    float processPhaserSample(float input, std::array<PhaserStage, 4>& stages, float coeff, float& feedbackState);
    void updateMeterState(MeterState& meter, const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples, bool clipLatch);
    StereoMeterSnapshot makeMeterSnapshot(const MeterState& state) const noexcept;
    void pushVisualizerSample(float waveform, float transientEnv, float bodyEnv) noexcept;
    void pushBeforeAfterVisualizerSample(float before, float after) noexcept;
    juce::ValueTree capturePresetState() const;
    bool applyPresetState(const juce::ValueTree& state, const juce::String& presetNameHint);
    juce::File getUserPresetDirectory() const;

    double currentSampleRate = 44100.0;
    double currentHostBpm = 120.0;

    juce::AudioBuffer<float> workInput;
    juce::AudioBuffer<float> transientBuffer;
    juce::AudioBuffer<float> bodyBuffer;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> effectScratch;
    juce::AudioBuffer<float> pogoEnvelopeBuffer;
    juce::AudioBuffer<float> lookaheadBuffer;

    int lookaheadBufferSize = 0;
    int lookaheadWritePosition = 0;
    int currentLatencySamples = 0;

    juce::IIRFilter sidechainHPF;
    juce::IIRFilter sidechainBassLPF;
    juce::IIRFilter sidechainHighHPF;
    float fastEnvelopeDb = -120.0f;
    float previousFastEnvelopeDb = -120.0f;
    float slowEnvelopeDb = -120.0f;
    float transientHoldValue = 0.0f;
    int transientHoldSamplesRemaining = 0;
    float transientWeightState = 0.0f;
    float bodyWeightState = 1.0f;
    float manualTransientWeightState = 0.0f;
    bool onsetTriggerArmed = true;
    bool manualEventActive = false;
    int manualEventAgeSamples = 0;
    int manualRetriggerCooldownSamples = 0;
    int visualizerCounter = 0;
    int visualizerStride = 64;
    int beforeAfterVisualizerCounter = 0;
    int splitEditorLiveCounter = 0;
    int splitEditorLiveStride = 1;
    int splitEditorCaptureMaxLengthSamples = 0;
    int splitEditorCaptureLengthSamples = 0;
    int splitEditorCaptureWritePosition = 0;
    bool splitEditorCaptureRecording = false;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> dryWetSmoothed;
    juce::SmoothedValue<float> pogoAmountSmoothed;
    std::array<juce::SmoothedValue<float>, 2> pathLevelSmoothed;
    std::array<juce::SmoothedValue<float>, 2> pathPanSmoothed;

    MeterState inputMeter;
    MeterState transientMeter;
    MeterState bodyMeter;
    MeterState outputMeter;
    std::atomic<float> detectionActivity{0.0f};
    std::atomic<bool> outputClipLatched{false};

    std::array<std::atomic<float>, visualizerHistorySize> waveformHistory{};
    std::array<std::atomic<float>, visualizerHistorySize> transientHistory{};
    std::array<std::atomic<float>, visualizerHistorySize> bodyHistory{};
    std::atomic<int> visualizerWritePosition{0};
    std::array<std::atomic<float>, visualizerHistorySize> beforeSignalHistory{};
    std::array<std::atomic<float>, visualizerHistorySize> afterSignalHistory{};
    std::atomic<int> beforeAfterVisualizerWritePosition{0};
    std::array<std::atomic<float>, splitEditorDisplayPointCount> splitEditorLiveWaveform{};
    std::array<std::atomic<float>, splitEditorDisplayPointCount> splitEditorFrozenWaveform{};
    std::atomic<int> splitEditorLiveWritePosition{0};
    std::atomic<bool> splitEditorCaptureArmed{false};
    std::atomic<bool> splitEditorCaptureFrozen{false};
    std::atomic<float> splitEditorCurrentCaptureDurationMs{1000.0f};
    std::array<std::atomic<float>, manualMarkerCount> splitEditorSuggestedMarkerValues{};
    std::atomic<bool> splitEditorSuggestedMarkersPending{false};
    juce::AudioBuffer<float> splitEditorCaptureBuffer;

    bool pogoEventActive = false;
    int pogoEventAgeSamples = 0;
    bool onsetNeedsRearm = false;
    float onsetRearmThresholdDb = -120.0f;

    std::array<std::array<float, 2>, maxFXSlots> driveToneStates{};
    std::array<juce::dsp::Chorus<float>, maxFXSlots> chorusSlots;
    std::array<juce::dsp::Chorus<float>, maxFXSlots> flangerSlots;
    std::array<juce::dsp::Reverb, maxFXSlots> reverbSlots;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterSlotsL;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterSlotsR;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterStage2SlotsL;
    std::array<juce::IIRFilter, maxFXSlots> fxFilterStage2SlotsR;
    std::array<std::array<juce::IIRFilter, 4>, maxFXSlots> fxEQSlotsL;
    std::array<std::array<juce::IIRFilter, 4>, maxFXSlots> fxEQSlotsR;
    std::array<SlotPhaserState, maxFXSlots> phaserStates;
    std::array<SlotCompressorState, maxFXSlots> compressorStates;
    std::array<SlotBitcrusherState, maxFXSlots> bitcrusherStates;
    std::array<SlotImagerState, maxFXSlots> imagerStates;
    std::array<SlotShiftState, maxFXSlots> shiftStates;
    std::array<SlotGateState, maxFXSlots> gateStates;
    std::array<DelayState, maxFXSlots> delayStates;
    std::array<float, maxFXSlots> tremoloPhases{};
    std::array<std::atomic<float>, maxFXSlots> compressorGainReductionMeters{};
    std::array<std::array<std::atomic<float>, 3>, maxFXSlots> multibandUpwardMeters{};
    std::array<std::array<std::atomic<float>, 3>, maxFXSlots> multibandDownwardMeters{};
    std::array<std::array<std::atomic<float>, fxEQSpectrumBinCount>, maxFXSlots> eqSpectrumBins{};
    std::array<std::array<std::atomic<float>, fxImagerScopePointCount>, maxFXSlots> imagerScopeX{};
    std::array<std::array<std::atomic<float>, fxImagerScopePointCount>, maxFXSlots> imagerScopeY{};
    juce::dsp::FFT eqSpectrumFFT{11};
    juce::dsp::WindowingFunction<float> eqSpectrumWindow{1 << 11, juce::dsp::WindowingFunction<float>::hann};
    std::array<float, (1 << 11) * 2> eqSpectrumFFTData{};
    juce::Random fxRandom;

    std::atomic<int> selectedModulationSourceIndex{0};
    juce::String currentPresetName{"Init"};
    std::array<juce::ValueTree, 2> abStateSnapshots;
    std::array<bool, 2> abStateValid{{ false, false }};
    std::atomic<int> activeABSlot{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
