#pragma once

#include <JuceHeader.h>

namespace ssp::pitch
{
enum class DetectionAlgorithm
{
    automatic = 0,
    melodic,
    polyphonicDecay,
    polyphonicSustain,
    percussive,
    percussivePitched,
    universal
};

enum class TransferState
{
    waiting = 0,
    capture,
    playback
};

enum class SeparationType
{
    soft = 0,
    hard
};

struct PitchPoint
{
    float time = 0.0f;
    float pitch = 69.0f;
    float amplitude = 0.0f;
    float cents = 0.0f;
    bool sibilant = false;
};

struct WaveformPoint
{
    float time = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
};

struct SpectrogramFrame
{
    float time = 0.0f;
    std::vector<float> bins;
};

struct DetectedNote
{
    juce::String id;
    DetectionAlgorithm algorithm = DetectionAlgorithm::melodic;
    SeparationType separationType = SeparationType::soft;
    double startTime = 0.0;
    double endTime = 0.0;
    double originalStartTime = 0.0;
    double originalEndTime = 0.0;
    float originalPitch = 69.0f;
    float correctedPitch = 69.0f;
    float pitchCenter = 69.0f;
    float pitchOffsetCents = 0.0f;
    std::vector<PitchPoint> pitchTrace;
    std::vector<PitchPoint> correctedPitchTrace;
    std::vector<float> amplitudeEnvelope;
    float amplitude = 0.0f;
    float vibratoRate = 0.0f;
    float vibratoDepth = 0.0f;
    float formantShift = 0.0f;
    float sibilantBalance = 0.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    float pitchModulation = 100.0f;
    float pitchDrift = 100.0f;
    float attackSpeed = 100.0f;
    bool muted = false;
    bool sibilantFlag = false;
    bool manuallyEdited = false;
    float manualPitchOffset = 0.0f;
    float driftCorrection = -1.0f;
    float pitchCorrection = -1.0f;
    float transitionToNext = 0.0f;
    float gainDb = 0.0f;
    std::vector<float> timeHandles;
};

struct TransportMapPoint
{
    std::int64_t captureSampleIndex = 0;
    std::int64_t dawSamplePosition = 0;
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
};

struct CaptureClipSummary
{
    juce::String id;
    juce::String sourceName;
    DetectionAlgorithm algorithm = DetectionAlgorithm::melodic;
    std::int64_t dawStartSample = 0;
    std::int64_t dawEndSample = 0;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
    int numChannels = 0;
    int numSamples = 0;
};

struct TempoPoint
{
    double timeSeconds = 0.0;
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
};

struct ChordEvent
{
    double startTime = 0.0;
    double endTime = 0.0;
    juce::String symbol;
    juce::Array<int> chordTones;
};

struct ScaleEvent
{
    double startTime = 0.0;
    double endTime = 0.0;
    juce::String tonic = "C";
    juce::String scale = "Major";
};

struct StatisticsSummary
{
    int totalNotes = 0;
    float averagePitchAccuracyBefore = 0.0f;
    float averagePitchAccuracyAfter = 0.0f;
    float inScalePercent = 0.0f;
    float correctionAmountAverage = 0.0f;
};

struct AnalysisSession
{
    juce::String sourceName;
    double durationSeconds = 0.0;
    bool analyzing = false;
    float progress = 0.0f;
    juce::String progressLabel = "Idle";
    bool polyphonicWarning = false;
    bool hasCapturedAudio = false;
    TransferState transferState = TransferState::waiting;
    DetectionAlgorithm algorithm = DetectionAlgorithm::melodic;
    double referencePitchHz = 440.0;
    std::int64_t playheadSample = 0;
    double playheadSeconds = 0.0;
    std::vector<DetectedNote> notes;
    std::vector<WaveformPoint> waveform;
    std::vector<SpectrogramFrame> spectrogram;
    std::vector<CaptureClipSummary> clips;
    std::vector<TransportMapPoint> transportMapPreview;
    std::vector<TempoPoint> tempoMap;
    std::vector<ChordEvent> chordTrack;
    std::vector<ScaleEvent> keyTrack;
    StatisticsSummary statistics;
    std::array<float, 128> pitchHeatmap{};
    std::uint32_t revision = 0;
};

struct CorrectionPreset
{
    juce::String name;
    float correctPitch = 0.0f;
    float correctDrift = 0.0f;
    float correctTiming = 0.0f;
    float vibratoDepth = 100.0f;
    float vibratoSpeed = 100.0f;
    float formantShift = 0.0f;
    bool formantPreserve = true;
};
} // namespace ssp::pitch
