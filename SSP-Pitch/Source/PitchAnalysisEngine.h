#pragma once

#include <JuceHeader.h>

#include "PitchTypes.h"

namespace ssp::pitch
{
class PitchAnalysisEngine final : private juce::Thread
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void pitchAnalysisUpdated(const AnalysisSession& session) = 0;
        virtual void pitchAnalysisFinished(const AnalysisSession& session) = 0;
    };

    explicit PitchAnalysisEngine(Listener& listenerToUse);
    ~PitchAnalysisEngine() override;

    void requestAnalysis(juce::AudioBuffer<float> monoBuffer, double sampleRate, juce::String sourceName);

private:
    struct Request
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String sourceName;
    };

    void run() override;
    AnalysisSession analyse(const Request& request);
    void publishProgress(const AnalysisSession& partialSession);

    static float estimatePitchHz(const float* data, int size, double sampleRate, float& confidence);
    static std::vector<float> smoothPitches(const std::vector<float>& pitches);
    static std::vector<DetectedNote> buildNotes(const std::vector<float>& pitches,
                                                const std::vector<float>& amplitudes,
                                                float hopSeconds,
                                                int framesToUse);
    static std::vector<WaveformPoint> buildWaveform(const juce::AudioBuffer<float>& monoBuffer,
                                                    double sampleRate);
    static std::vector<SpectrogramFrame> buildSpectrogram(const juce::AudioBuffer<float>& monoBuffer,
                                                          double sampleRate,
                                                          std::atomic<bool>& shouldCancel);
    static void updateHeatmap(AnalysisSession& session);
    static std::pair<float, float> estimateVibrato(const std::vector<PitchPoint>& points);

    Listener& listener;
    juce::CriticalSection requestLock;
    juce::WaitableEvent wakeEvent;
    std::optional<Request> pendingRequest;
};
} // namespace ssp::pitch
