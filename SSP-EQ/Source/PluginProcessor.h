#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum PointType
    {
        bell = 0,
        lowShelf,
        highShelf,
        lowCut,
        highCut,
        notch,
        bandPass,
        tiltShelf
    };

    enum StereoMode
    {
        stereo = 0,
        left,
        right,
        mid,
        side
    };

    enum AnalyzerMode
    {
        analyzerPre = 0,
        analyzerPost,
        analyzerPrePost,
        analyzerSidechain
    };

    enum ProcessingMode
    {
        zeroLatency = 0,
        naturalPhase,
        linearPhase
    };

    enum DynamicDirection
    {
        dynamicAbove = 0,
        dynamicBelow
    };

    enum SidechainSource
    {
        sidechainInternal = 0,
        sidechainExternal,
        sidechainBand
    };

    struct EqPoint
    {
        bool enabled = false;
        float frequency = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        int type = bell;
        int slopeIndex = 1;
        int stereoMode = stereo;
        bool dynamicEnabled = false;
        float dynamicThresholdDb = -24.0f;
        float dynamicRatio = 4.0f;
        float dynamicAttackMs = 10.0f;
        float dynamicReleaseMs = 100.0f;
        float dynamicKneeDb = 6.0f;
        int dynamicDirection = dynamicAbove;
        float dynamicRangeDb = 24.0f;
        int sidechainSource = sidechainInternal;
        int sidechainBandIndex = 0;
        float sidechainHighPassHz = 20.0f;
        float sidechainLowPassHz = 20000.0f;
    };

    static constexpr int maxPoints = 24;
    static constexpr int maxStagesPerPoint = 8;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    using PointArray = std::array<EqPoint, maxPoints>;
    using SpectrumArray = std::array<float, fftSize / 2>;

    struct AnalyzerFrame
    {
        SpectrumArray pre{};
        SpectrumArray post{};
        SpectrumArray sidechain{};
    };

    struct PointFilterSetup
    {
        std::array<juce::IIRCoefficients, maxStagesPerPoint> coeffs{};
        int numStages = 0;
    };

    struct DynamicVisualState
    {
        bool enabled = false;
        float detectorLevelDb = -96.0f;
        float activity = 0.0f;
        float effectiveGainDb = 0.0f;
        float maxGainDb = 0.0f;
        float activeResponseDb = 0.0f;
    };

    struct PresetBand
    {
        bool enabled = true;
        juce::String type = "bell";
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int slope = 12;
        juce::String stereoMode = "stereo";
        bool dynamicEnabled = false;
        float dynamicThresholdDb = -24.0f;
        float dynamicRatio = 4.0f;
        float dynamicAttackMs = 10.0f;
        float dynamicReleaseMs = 100.0f;
        float dynamicKneeDb = 6.0f;
        juce::String dynamicDirection = "above";
        float dynamicRangeDb = 24.0f;
        juce::String sidechainSource = "internal";
        int sidechainBandIndex = 0;
        float sidechainHighPassHz = 20.0f;
        float sidechainLowPassHz = 20000.0f;
    };

    struct PresetRecord
    {
        juce::String id;
        juce::String name;
        juce::String category;
        juce::String author;
        bool isFactory = false;
        juce::Array<PresetBand> bands;
        juce::String version = "1.0";
    };

    enum class RandomizeMode
    {
        subtle = 0,
        standard,
        extreme
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
    static const juce::StringArray& getPointTypeDisplayNames();
    static const juce::StringArray& getSlopeNames();
    static const juce::StringArray& getStereoModeNames();
    static const juce::StringArray& getAnalyzerModeNames();
    static const juce::StringArray& getProcessingModeNames();
    static const juce::StringArray& getAnalyzerResolutionNames();
    static const juce::StringArray& getDynamicDirectionNames();
    static const juce::StringArray& getSidechainSourceNames();
    static const juce::Array<PresetRecord>& getFactoryPresets();

    EqPoint getPoint(int index) const;
    PointArray getPointsSnapshot() const;
    void setPoint(int index, const EqPoint& point);
    void setPointPosition(int index, float frequency, float gainDb);
    int addPoint(float frequency, float gainDb);
    void removePoint(int index);
    float getResponseForFrequency(float frequency) const;
    float getResponseForFrequencyByStereoMode(float frequency, int stereoMode) const;
    float getBandResponseForFrequency(int index, float frequency) const;
    float getBandSetResponseForFrequency(int index, float frequency) const;
    DynamicVisualState getDynamicVisualState(int index) const;
    int getEnabledPointCount() const;
    AnalyzerFrame getAnalyzerFrameCopy() const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    float getOutputGainDb() const;
    bool isGlobalBypassed() const;
    int getAnalyzerMode() const;
    int getProcessingMode() const;
    int getAnalyzerResolution() const;
    float getAnalyzerDecay() const;
    const juce::Array<PresetRecord>& getUserPresets() const noexcept { return userPresets; }
    juce::File getUserPresetDirectory() const;
    juce::String getCurrentPresetName() const noexcept;
    juce::String getCurrentPresetCategory() const noexcept;
    juce::String getCurrentPresetAuthor() const noexcept;
    juce::String getCurrentPresetKey() const noexcept;
    bool isCurrentPresetFactory() const noexcept;
    bool isCurrentPresetDirty() const noexcept;
    bool matchesCurrentPreset(const PresetRecord&) const noexcept;
    juce::StringArray getAvailablePresetCategories() const;
    void refreshUserPresets();
    bool loadFactoryPreset(int index);
    bool loadUserPreset(int index);
    bool saveUserPreset(const juce::String& presetName, const juce::String& category);
    bool deleteUserPreset(int index);
    bool importPresetFromFile(const juce::File& file, bool loadAfterImport = true);
    bool exportCurrentPresetToFile(const juce::File& file) const;
    bool resetUserPresetsToFactoryDefaults();
    bool hasLoadedPresetState() const noexcept;
    bool hasABComparisonTarget() const noexcept;
    bool isABComparisonActive() const noexcept;
    bool toggleABComparison();
    bool beginPresetPreview(const PresetRecord&);
    void endPresetPreview();
    bool isPresetPreviewActive() const noexcept;
    PointArray getPreviewReferencePointsSnapshot() const;
    int getActiveCompareSlot() const noexcept;
    bool setActiveCompareSlot(int slotIndex);
    bool copyActiveCompareSlotToOther();
    int getSoloPointIndex() const noexcept;
    void setSoloPointIndex(int pointIndex);
    void toggleSoloPoint(int pointIndex);
    juce::Array<PresetRecord> getSequentialPresetList() const;
    bool stepPreset(int delta);
    bool loadPresetByKey(const juce::String& presetKey);
    void randomizeCurrentSlot(RandomizeMode mode);
    bool undoRandomizeCurrentSlot();
    bool hasRandomizeUndo() const noexcept;
    int consumePendingVisualTransitionMs(int fallbackMs);
    int getCurrentLatencySamples() const noexcept;
    double getCurrentLatencyMs() const noexcept;
    void setBandSidechainListen(int pointIndex, bool shouldListen);
    bool isBandSidechainListening(int pointIndex) const noexcept;
    bool hasDynamicPointEnabled() const;

private:
    struct PresetStateSnapshot
    {
        PointArray points{};
        bool valid = false;
    };

    struct CompareSlotState
    {
        PresetStateSnapshot snapshot;
        PresetStateSnapshot baselineSnapshot;
        juce::String presetKey;
        juce::String presetName;
        juce::String presetCategory;
        juce::String presetAuthor;
        bool isFactory = false;
        bool dirty = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void syncPointCacheFromParameters();
    void updateVisualPointCache();
    void initialisePresetTracking();
    void restorePresetMetadataFromState();
    void applyPresetRecord(const PresetRecord& preset, bool updateLoadedPresetReference);
    void applyPresetSnapshot(const PresetStateSnapshot& snapshot);
    PresetStateSnapshot captureCurrentPresetSnapshot() const;
    void syncCurrentStateIntoActiveCompareSlot();
    void applyCompareSlotState(int slotIndex);
    void setCompareSlotState(int slotIndex, const CompareSlotState& state);
    CompareSlotState captureCurrentCompareSlotState() const;
    void setCurrentPresetMetadata(juce::String presetKey, juce::String presetName, juce::String category, juce::String author, bool isFactory);
    PresetRecord buildCurrentPresetRecord(juce::String presetName, juce::String category, juce::String author, bool isFactory) const;
    void clearABState();
    void requestVisualTransitionMs(int durationMs);
    void updateSoloMonitoringSetup();
    void pushSpectrumSample(float sample) noexcept;
    void pushPreSpectrumSample(float sample) noexcept;
    void pushPostSpectrumSample(float sample) noexcept;
    void pushSidechainSpectrumSample(float sample) noexcept;
    void performSpectrumAnalysis(const std::array<float, fftSize>& source, SpectrumArray& destination, float decayAmount);
    float processPointForDomain(int modeIndex, int domainIndex, int pointIndex, float sample, float wetMix = 1.0f);
    void updateFilterStates();
    void resetDynamicState();
    void ensureScratchBuffers(int numSamples);
    void computeDynamicControlSignals(const juce::AudioBuffer<float>& mainBuffer,
                                      const juce::AudioBuffer<float>& sidechainBuffer);
    void renderProcessingMode(int processingMode,
                              const juce::AudioBuffer<float>& source,
                              juce::AudioBuffer<float>& destination);
    void renderZeroLatencyMode(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& destination);
    void renderNaturalPhaseMode(const juce::AudioBuffer<float>& source,
                                juce::AudioBuffer<float>& destination);
    void renderLinearPhaseMode(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& destination);
    void renderDynamicIirOverlay(int modeIndex,
                                 const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& destination);
    void updateDynamicBandSetup(int pointIndex, float effectiveGainDb);
    void updateNaturalPhaseCompensation();
    void updateLatencyState();
    void scheduleLinearFirRebuild();
    void linearFirWorkerLoop();
    void loadPendingLinearImpulseResponses();

    double currentSampleRate = 44100.0;
    std::array<std::array<std::array<std::array<juce::IIRFilter, maxStagesPerPoint>, maxPoints>, 4>, 3> filters;
    std::array<std::array<std::array<juce::IIRFilter, maxStagesPerPoint>, maxPoints>, 4> naturalPhaseCompensationFilters;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> naturalPhaseDelayLines;
    mutable juce::SpinLock stateLock;
    PointArray pointCache{};
    PointArray visualPointCache{};
    std::array<PointFilterSetup, maxPoints> coefficientCache{};
    std::array<PointFilterSetup, maxPoints> setCoefficientCache{};
    std::array<DynamicVisualState, maxPoints> dynamicVisualStates{};
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    std::array<float, fftSize> preSpectrumFifo{};
    std::array<float, fftSize> postSpectrumFifo{};
    std::array<float, fftSize> sidechainSpectrumFifo{};
    std::array<float, fftSize * 2> fftData{};
    AnalyzerFrame analyzerFrame{};
    int preSpectrumFifoIndex = 0;
    int postSpectrumFifoIndex = 0;
    int sidechainSpectrumFifoIndex = 0;
    juce::StringArray parameterIDs;
    juce::Array<PresetRecord> userPresets;
    juce::Array<juce::File> userPresetFiles;
    juce::String currentPresetKey;
    juce::String currentPresetName;
    juce::String currentPresetCategory;
    juce::String currentPresetAuthor;
    bool currentPresetIsFactory = false;
    std::atomic<bool> currentPresetDirty{false};
    std::atomic<int> dirtyTrackingSuppressionDepth{0};
    PresetStateSnapshot lastLoadedPresetSnapshot;
    PresetStateSnapshot previewRestoreSnapshot;
    PresetStateSnapshot abComparisonSnapshot;
    bool previewActive = false;
    juce::String previewPresetKey;
    bool abComparisonActive = false;
    std::array<CompareSlotState, 2> compareSlots;
    int activeCompareSlot = 0;
    CompareSlotState randomizeUndoState;
    bool hasRandomizeUndoState = false;
    int soloPointIndex = -1;
    juce::IIRFilter soloHighPassLeft;
    juce::IIRFilter soloHighPassRight;
    juce::IIRFilter soloLowPassLeft;
    juce::IIRFilter soloLowPassRight;
    std::atomic<int> pendingVisualTransitionMs{90};
    struct DynamicRuntime
    {
        juce::IIRFilter detectorPrimary;
        juce::IIRFilter detectorHighPass;
        juce::IIRFilter detectorLowPass;
        float smoothedPower = 0.0f;
        float lastEffectiveGainDb = 0.0f;
        float lastActivity = 0.0f;
        float lastMonitorSample = 0.0f;
    };
    std::array<DynamicRuntime, maxPoints> dynamicRuntime;
    std::array<std::vector<float>, maxPoints> blockEffectiveGainDb;
    std::array<std::vector<float>, maxPoints> blockActivity;
    std::vector<float> blockSidechainListen;
    juce::AudioBuffer<float> scratchInputBuffer;
    juce::AudioBuffer<float> scratchRenderBufferA;
    juce::AudioBuffer<float> scratchRenderBufferB;
    juce::AudioBuffer<float> scratchRenderBufferC;
    juce::AudioBuffer<float> linearMidBuffer;
    juce::AudioBuffer<float> linearSideBuffer;
    juce::AudioBuffer<float> linearTempMonoBuffer;
    juce::dsp::ConvolutionMessageQueue convolutionQueue;
    std::array<std::unique_ptr<juce::dsp::Convolution>, 6> linearPhaseConvolvers;
    int linearPhaseKernelSize = 4096;
    int linearPhaseGroupDelaySamples = 2048;
    int naturalPhaseLatencySamples = 0;
    std::atomic<int> currentLatencySamples{0};
    int activeProcessingMode = zeroLatency;
    int previousProcessingMode = zeroLatency;
    int processingModeFadeTotalSamples = 0;
    int processingModeFadeRemainingSamples = 0;
    int sidechainListenPointIndex = -1;
    struct LinearImpulseSet
    {
        std::array<juce::AudioBuffer<float>, 6> responses;
        int kernelSize = 4096;
        int groupDelaySamples = 2048;
        bool valid = false;
    };
    std::mutex linearIrMutex;
    std::condition_variable linearIrCondition;
    std::thread linearIrWorker;
    PointArray linearIrRequestedPoints{};
    LinearImpulseSet linearIrReadySet;
    bool linearIrDirty = false;
    bool linearIrReady = false;
    bool linearIrExit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
