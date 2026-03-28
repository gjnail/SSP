#pragma once

#include <JuceHeader.h>

#include "PitchAnalysisEngine.h"
#include "PitchScale.h"
#include "PitchTypes.h"

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater,
                              private ssp::pitch::PitchAnalysisEngine::Listener
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

    ssp::pitch::AnalysisSession getSessionCopy() const;
    bool hasSessionNotes() const;

    void startCapture();
    void stopCaptureAndAnalyze();
    bool isCapturing() const noexcept { return captureActive.load(); }
    void analyzeAudioFile(const juce::File& file);
    void loadFactoryCorrectionPreset(const juce::String& presetName);

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

private:
    struct CompareSlot
    {
        ssp::pitch::AnalysisSession session;
        bool valid = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const juce::Array<ssp::pitch::CorrectionPreset>& getFactoryPresets();

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void pitchAnalysisUpdated(const ssp::pitch::AnalysisSession& session) override;
    void pitchAnalysisFinished(const ssp::pitch::AnalysisSession& session) override;

    void generateDemoSession();
    void setSession(const ssp::pitch::AnalysisSession& newSession);
    void pushUndoState();
    void clearRedoState();
    void rebuildCorrectedNotes();
    void captureIncomingAudio(const juce::AudioBuffer<float>& buffer);
    bool updateNote(const juce::String& noteId, const std::function<void(ssp::pitch::DetectedNote&)>& updater);
    juce::var serialiseSessionToVar(const ssp::pitch::AnalysisSession& session) const;
    ssp::pitch::AnalysisSession deserialiseSessionFromVar(const juce::var& value) const;

    mutable juce::CriticalSection sessionLock;
    ssp::pitch::AnalysisSession session;
    std::array<CompareSlot, 2> compareSlots;
    std::vector<ssp::pitch::AnalysisSession> undoStack;
    std::vector<ssp::pitch::AnalysisSession> redoStack;
    std::atomic<int> activeCompareSlot{0};
    std::atomic<bool> transportPlaying{false};
    std::atomic<bool> loopEnabled{false};
    std::atomic<double> playheadSeconds{0.0};
    std::atomic<bool> captureActive{false};
    std::atomic<bool> correctionDirty{false};
    std::vector<float> capturedMono;
    double currentSampleRate = 44100.0;
    juce::AudioFormatManager formatManager;
    ssp::pitch::PitchAnalysisEngine analysisEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
