#pragma once

#include <JuceHeader.h>

namespace ssp::pitch
{
struct PitchPoint
{
    float time = 0.0f;
    float pitch = 69.0f;
    float amplitude = 0.0f;
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
    double startTime = 0.0;
    double endTime = 0.0;
    double originalStartTime = 0.0;
    double originalEndTime = 0.0;
    float originalPitch = 69.0f;
    float correctedPitch = 69.0f;
    std::vector<PitchPoint> pitchTrace;
    std::vector<PitchPoint> correctedPitchTrace;
    std::vector<float> amplitudeEnvelope;
    float amplitude = 0.0f;
    float vibratoRate = 0.0f;
    float vibratoDepth = 0.0f;
    float formantShift = 0.0f;
    bool muted = false;
    float driftCorrection = -1.0f;
    float pitchCorrection = -1.0f;
    float transitionToNext = 0.0f;
    float gainDb = 0.0f;
};

struct AnalysisSession
{
    juce::String sourceName;
    double durationSeconds = 0.0;
    bool analyzing = false;
    float progress = 0.0f;
    juce::String progressLabel = "Idle";
    bool polyphonicWarning = false;
    std::vector<DetectedNote> notes;
    std::vector<WaveformPoint> waveform;
    std::vector<SpectrogramFrame> spectrogram;
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
