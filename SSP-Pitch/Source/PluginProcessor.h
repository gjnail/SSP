#pragma once

#include <JuceHeader.h>

#include "PitchScale.h"
#include "PitchTypes.h"

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
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

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& getFactoryPresetNames();
    static const juce::StringArray& getDetectionAlgorithmNames();

    ssp::pitch::AnalysisSession getSessionCopy() const;
    std::uint32_t getSessionRevision() const noexcept { return sessionRevision.load(); }
    bool hasSessionNotes() const;
    bool hasCapturedAudio() const noexcept;
    ssp::pitch::TransferState getTransferState() const noexcept { return transferState.load(); }
    juce::String getTransferStateLabel() const;

    void startCapture();
    void stopCaptureAndAnalyze();
    bool isCapturing() const noexcept { return captureActive.load(); }
    void analyzeAudioFile(const juce::File& file);
    void loadFactoryCorrectionPreset(const juce::String& presetName);
    void setDetectionAlgorithm(ssp::pitch::DetectionAlgorithm algorithm);
    ssp::pitch::DetectionAlgorithm getDetectionAlgorithm() const noexcept { return selectedAlgorithm.load(); }
    double getReferencePitchHz() const noexcept { return referencePitchHz.load(); }
    void setReferencePitchHz(double hz);

    int getActiveCompareSlot() const noexcept { return activeCompareSlot.load(); }
    void setActiveCompareSlot(int slotIndex);
    void copyActiveCompareSlotToOther();

    void setTransportPlaying(bool shouldPlay);
    bool isTransportPlaying() const noexcept { return transportPlaying.load(); }
    void stopTransport();
    void setLoopEnabled(bool shouldLoop);
    bool isLoopEnabled() const noexcept { return loopEnabled.load(); }
    void advanceTransport(double deltaSeconds);
    double getPlayheadSeconds() const noexcept { return playheadSeconds.load(); }
    bool isHostPlaybackActive() const noexcept { return hostPlaybackActive.load(); }
    bool isLiveEnabled() const noexcept { return apvts.getRawParameterValue("liveEnabled")->load() >= 0.5f; }
    float getLiveDetectedMidi() const noexcept { return liveDetectedMidi.load(); }
    float getLiveTargetMidi() const noexcept { return liveTargetMidi.load(); }
    float getLiveCorrectionCents() const noexcept { return liveCorrectionCents.load(); }
    float getLiveInputLevel() const noexcept { return liveInputLevel.load(); }
    bool isLivePitchDetected() const noexcept { return livePitchDetected.load(); }

    bool undoLastEdit();
    bool redoLastEdit();
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;

    void moveNote(const juce::String& noteId, float semitoneDelta, double timeDeltaSeconds);
    void resizeNote(const juce::String& noteId, double startDeltaSeconds, double endDeltaSeconds);
    void splitNote(const juce::String& noteId, double splitTimeSeconds);
    void mergeNotes(const juce::StringArray& noteIds);
    void addDrawnNote(double startTime, double endTime, float midiNote);
    void setNoteMuted(const juce::String& noteId, bool shouldMute);
    void setNoteGainDb(const juce::String& noteId, float gainDb);
    void setNotePitchCorrection(const juce::String& noteId, float amount);
    void setNoteDriftCorrection(const juce::String& noteId, float amount);
    void setNoteFormantShift(const juce::String& noteId, float semitones);
    void setNoteTransition(const juce::String& noteId, float amount);
    void nudgeNotePitchTrace(const juce::String& noteId, double timeSeconds, float midiNote, float radiusSeconds);
    void muteNotes(const juce::StringArray& noteIds, bool shouldMute);
    void applyCorrectPitchMacro(float centerAmount, float driftAmount, bool includeEditedNotes);
    void applyCurrentScaleToAllNotes();
    void applyQuantizeTimeMacro(float amount, bool includeEditedNotes);
    void applyLevelingMacro(float quietAmount, float loudAmount, bool includeEditedNotes);
    void resetNotes(const juce::StringArray& noteIds);

private:
    struct CaptureClip
    {
        juce::String id;
        juce::String sourceName;
        int sampleRate = 44100;
        int numChannels = 0;
        int maxSamples = 0;
        int numCapturedSamples = 0;
        std::int64_t dawStartSample = 0;
        std::int64_t dawEndSample = 0;
        double bpmAtStart = 120.0;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;
        ssp::pitch::DetectionAlgorithm algorithm = ssp::pitch::DetectionAlgorithm::melodic;
        juce::AudioBuffer<float> originalAudio;
        juce::AudioBuffer<float> renderedAudio;
        std::vector<ssp::pitch::TransportMapPoint> transportMap;
    };

    struct CompareSlot
    {
        ssp::pitch::AnalysisSession session;
        bool valid = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const juce::Array<ssp::pitch::CorrectionPreset>& getFactoryPresets();

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    void generateDemoSession();
    void setSession(const ssp::pitch::AnalysisSession& newSession);
    void pushUndoState();
    void clearRedoState();
    void rebuildCorrectedNotes();
    void rebuildRenderedClipAudio();
    void updateSessionFromClipState();
    void captureIncomingAudio(const juce::AudioBuffer<float>& buffer, const juce::AudioPlayHead::PositionInfo* positionInfo);
    void renderPlaybackIntoBuffer(juce::AudioBuffer<float>& buffer, const juce::AudioPlayHead::PositionInfo* positionInfo);
    void processLivePitchCorrection(juce::AudioBuffer<float>& buffer);
    void updateLivePitchEstimate(const juce::AudioBuffer<float>& buffer);
    bool updateNote(const juce::String& noteId, const std::function<void(ssp::pitch::DetectedNote&)>& updater);
    juce::var serialiseSessionToVar(const ssp::pitch::AnalysisSession& session) const;
    ssp::pitch::AnalysisSession deserialiseSessionFromVar(const juce::var& value) const;
    juce::var serialiseCaptureClipToVar() const;
    void deserialiseCaptureClipFromVar(const juce::var& value);

    mutable juce::CriticalSection sessionLock;
    ssp::pitch::AnalysisSession session;
    std::array<CompareSlot, 2> compareSlots;
    std::vector<ssp::pitch::AnalysisSession> undoStack;
    std::vector<ssp::pitch::AnalysisSession> redoStack;
    std::atomic<int> activeCompareSlot{0};
    std::atomic<bool> transportPlaying{false};
    std::atomic<bool> hostPlaybackActive{false};
    std::atomic<bool> loopEnabled{false};
    std::atomic<double> playheadSeconds{0.0};
    std::atomic<bool> captureActive{false};
    std::atomic<bool> correctionDirty{false};
    std::atomic<bool> hasClipData{false};
    std::atomic<ssp::pitch::TransferState> transferState{ssp::pitch::TransferState::waiting};
    std::atomic<ssp::pitch::DetectionAlgorithm> selectedAlgorithm{ssp::pitch::DetectionAlgorithm::melodic};
    std::atomic<double> referencePitchHz{440.0};
    std::atomic<std::uint32_t> sessionRevision{0};
    juce::SpinLock renderedAudioLock;
    CaptureClip captureClip;
    int lastCaptureUiSyncSamples = 0;
    double currentSampleRate = 44100.0;
    int currentMaxBlockSize = 512;
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> liveDelayBuffer;
    int liveDelayBufferSize = 0;
    int liveDelayWritePosition = 0;
    float livePhaseA = 0.0f;
    float livePhaseB = 0.5f;
    float liveHeadStartA = 0.0f;
    float liveHeadStartB = 0.0f;
    bool liveGrainsInitialised = false;
    float liveBaseDelaySamples = 768.0f;
    float liveGrainSizeSamples = 256.0f;
    static constexpr int liveAnalysisWindowSize = 1024;
    static constexpr int liveAnalysisHopSize = 128;
    static constexpr int liveAnalysisRingSize = 4096;
    std::array<float, liveAnalysisRingSize> liveAnalysisRing{};
    std::array<float, liveAnalysisWindowSize> liveAnalysisScratch{};
    int liveAnalysisWritePosition = 0;
    int liveAnalysisSinceLastEstimate = 0;
    int stablePitchFrames = 0;
    float previousDetectedMidi = 0.0f;
    float smoothedDetectedMidi = 0.0f;
    float smoothedCorrectionSemitones = 0.0f;
    std::atomic<float> liveDetectedMidi{0.0f};
    std::atomic<float> liveTargetMidi{0.0f};
    std::atomic<float> liveCorrectionCents{0.0f};
    std::atomic<float> liveInputLevel{0.0f};
    std::atomic<bool> livePitchDetected{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
