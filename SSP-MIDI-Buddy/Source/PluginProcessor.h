#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    struct ScaleDefinition
    {
        juce::String name;
        std::vector<int> intervals;
    };

    struct GeneratedChord
    {
        int degree = 0;
        int noteCount = 3;
        int octaveOffset = 0;
        int inversion = 0;
        int qualityIndex = 0;
        std::array<bool, 24> customNoteMask{};
    };

    struct PreviewNote
    {
        double startBeat = 0.0;
        double durationBeats = 0.0;
        int midiNote = 60;
    };

    struct SlotDisplayData
    {
        juce::String heading;
        juce::String primary;
        juce::String secondary;
    };

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
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

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const juce::StringArray& getKeyNames();
    static const juce::StringArray& getScaleNames();
    static const juce::StringArray& getGenerationModeNames();
    static const juce::StringArray& getStyleNames();
    static const juce::StringArray& getChordFeelNames();
    static const juce::StringArray& getChordCountNames();
    static const juce::StringArray& getChordDurationNames();
    static const juce::StringArray& getRegisterNames();
    static const juce::StringArray& getSpreadNames();
    static const juce::StringArray& getPlaybackModeNames();
    static const juce::StringArray& getArpRateNames();
    static const juce::StringArray& getArpOctaveNames();
    static const juce::StringArray& getMelodyRangeNames();
    static const juce::StringArray& getChordQualityNames();
    static const std::vector<ScaleDefinition>& getScaleDefinitions();

    void randomizeProgression();
    juce::File exportGeneratedMidiFile();
    juce::File exportGeneratedMidiFileTo(const juce::File& destination);
    juce::File getDefaultExportFile() const;

    juce::String getSettingsSummary() const;
    juce::String getProgressionSummary() const;
    juce::String getExportStatus() const;
    std::vector<juce::String> getProgressionChordLabels() const;
    SlotDisplayData getProgressionSlotDisplay(int slotIndex) const;
    juce::StringArray getScaleChordReplacementNames(int slotIndex) const;
    std::vector<PreviewNote> getClipPreviewNotes() const;
    double getClipPreviewLengthInQuarterNotes() const;
    int getVisibleSlotCount() const;
    juce::StringArray getScaleDegreeNames() const;
    GeneratedChord getProgressionSlot(int slotIndex) const;
    int getSlotRootMidi(int slotIndex) const;
    std::vector<int> getSlotEditorIntervals(int slotIndex) const;
    void setProgressionSlotDegree(int slotIndex, int degree);
    void replaceProgressionSlotWithScaleChord(int slotIndex, int degree);
    void setProgressionSlotQuality(int slotIndex, int qualityIndex);
    void cycleProgressionSlotInversion(int slotIndex);
    void setProgressionSlotInversion(int slotIndex, int inversion);
    void toggleCustomNoteForSlot(int slotIndex, int semitoneIndex);
    bool moveCustomNoteForSlot(int slotIndex, int fromSemitoneIndex, int toSemitoneIndex);
    bool isChordModeActive() const;
    bool isMelodyModeActive() const;
    bool isBasslineModeActive() const;
    bool isMotifModeActive() const;

private:
    static constexpr int maxProgressionChords = 8;
    static constexpr int customNoteGridSize = 24;

    int getChoiceValue(const juce::String& parameterID, int fallback = 0) const;
    float getFloatValue(const juce::String& parameterID, float fallback = 0.0f) const;
    int getRequestedChordCount() const;
    double getChordDurationInQuarterNotes() const;
    int getRegisterBaseOctave() const;
    int getMelodyRangeOctaves() const;
    double getArpStepInQuarterNotes() const;
    const ScaleDefinition& getSelectedScaleDefinition() const;
    juce::String serialiseProgression() const;
    bool restoreProgression(const juce::String& serialised);
    std::array<GeneratedChord, maxProgressionChords> makeRandomProgression() const;
    std::array<GeneratedChord, maxProgressionChords> makeRandomMelody() const;
    std::array<GeneratedChord, maxProgressionChords> makeRandomBassline() const;
    std::array<GeneratedChord, maxProgressionChords> makeRandomMotif() const;
    int mapCanonicalDegreeToScaleStep(int canonicalDegree) const;
    int getChordToneCountForStyle(int styleIndex, bool isFinalChord) const;
    int getDegreeRootMidi(int degree) const;
    int getMelodyNoteMidi(const GeneratedChord& melodyNote) const;
    int getScalePositionMidi(int scalePosition, int octaveBias) const;
    std::vector<int> buildScaleStackIntervals(const GeneratedChord& chord, int forcedNoteCount = -1) const;
    std::vector<int> buildChordIntervals(const GeneratedChord& chord) const;
    std::vector<int> buildVoicingForChord(const GeneratedChord& chord,
                                          const std::vector<int>* previousVoicing) const;
    bool hasCustomIntervals(const GeneratedChord& chord) const;
    void seedCustomMaskFromCurrentShape(GeneratedChord& chord) const;
    int getMaxInversionForChord(const GeneratedChord& chord) const;
    double applySwingToBeat(double beat, double stepLength, int stepIndex) const;
    juce::String getInversionLabel(const GeneratedChord& chord) const;
    juce::String detectChordSuffix(const std::vector<int>& chordIntervals) const;
    juce::String buildChordName(const GeneratedChord& chord) const;
    juce::String buildChordDisplayLabel(const GeneratedChord& chord) const;
    juce::String buildMelodyNoteLabel(const GeneratedChord& melodyNote) const;
    std::vector<int> makeArpPattern(const std::vector<int>& voicing, juce::Random& random) const;
    juce::MidiMessageSequence buildGeneratedSequence(double bpm) const;
    double getHostBpmFallback120() const;
    void syncProgressionIntoState();
    juce::String makeSuggestedExportBaseName() const;
    juce::File writeGeneratedMidiFileTo(const juce::File& destination, bool temporaryExport);

    juce::CriticalSection progressionLock;
    std::array<GeneratedChord, maxProgressionChords> progression{};
    juce::String lastExportStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
