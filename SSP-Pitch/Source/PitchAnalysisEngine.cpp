#include "PitchAnalysisEngine.h"
#include "PitchScale.h"

namespace
{
constexpr int analysisWindowSize = 1024;
constexpr int analysisHopSize = 128;
constexpr float minimumPitchHz = 55.0f;
constexpr float maximumPitchHz = 1400.0f;
constexpr float silenceThreshold = 0.006f;
constexpr float pitchConfidenceThreshold = 0.42f;

float hzToMidi(float hz)
{
    return 69.0f + 12.0f * std::log2(juce::jmax(1.0f, hz) / 440.0f);
}

float median(std::vector<float> values)
{
    if (values.empty())
        return 0.0f;

    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if ((values.size() % 2) == 0)
        return 0.5f * (values[middle - 1] + values[middle]);

    return values[middle];
}

juce::String noteNameFromClass(int noteClass)
{
    static const juce::StringArray names{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[(noteClass % 12 + 12) % 12];
}
} // namespace

namespace ssp::pitch
{
PitchAnalysisEngine::PitchAnalysisEngine(Listener& listenerToUse)
    : juce::Thread("SSP Pitch Analysis"),
      listener(listenerToUse)
{
    startThread();
}

PitchAnalysisEngine::~PitchAnalysisEngine()
{
    signalThreadShouldExit();
    wakeEvent.signal();
    stopThread(3000);
}

void PitchAnalysisEngine::requestAnalysis(juce::AudioBuffer<float> monoBuffer, double sampleRate, juce::String sourceName, DetectionAlgorithm algorithm)
{
    const juce::ScopedLock lock(requestLock);
    pendingRequest = Request{std::move(monoBuffer), sampleRate, std::move(sourceName), algorithm};
    wakeEvent.signal();
}

void PitchAnalysisEngine::run()
{
    while (! threadShouldExit())
    {
        wakeEvent.wait(-1);

        if (threadShouldExit())
            break;

        std::optional<Request> request;
        {
            const juce::ScopedLock lock(requestLock);
            request.swap(pendingRequest);
        }

        if (! request.has_value())
            continue;

        auto result = analyse(*request);
        listener.pitchAnalysisFinished(result);
    }
}

AnalysisSession PitchAnalysisEngine::analyse(const Request& request)
{
    AnalysisSession session;
    session.sourceName = request.sourceName;
    session.durationSeconds = request.buffer.getNumSamples() / juce::jmax(1.0, request.sampleRate);
    session.algorithm = request.algorithm;
    session.analyzing = true;
    session.progress = 0.02f;
    session.progressLabel = "Scanning pitch";
    publishProgress(session);

    if (request.buffer.getNumSamples() <= analysisWindowSize)
    {
        session.analyzing = false;
        session.progress = 1.0f;
        session.progressLabel = "No usable audio";
        return session;
    }

    const auto* mono = request.buffer.getReadPointer(0);
    const int totalFrames = juce::jmax(1, (request.buffer.getNumSamples() - analysisWindowSize) / analysisHopSize);
    std::vector<float> pitches((size_t) totalFrames, 0.0f);
    std::vector<float> amplitudes((size_t) totalFrames, 0.0f);

    for (int frame = 0; frame < totalFrames; ++frame)
    {
        if (threadShouldExit())
            return session;

        const int startSample = frame * analysisHopSize;
        const float* frameData = mono + startSample;

        double sumSquares = 0.0;
        for (int i = 0; i < analysisWindowSize; ++i)
            sumSquares += frameData[i] * frameData[i];

        const float rms = std::sqrt((float) (sumSquares / (double) analysisWindowSize));
        amplitudes[(size_t) frame] = rms;

        float confidence = 0.0f;
        if (rms > silenceThreshold)
        {
            const float hz = estimatePitchHz(frameData, analysisWindowSize, request.sampleRate, confidence);
            if (hz >= minimumPitchHz && hz <= maximumPitchHz && confidence > pitchConfidenceThreshold)
                pitches[(size_t) frame] = hzToMidi(hz);
        }

        if (frame % 120 == 0)
        {
            session.progress = juce::jmap((float) frame, 0.0f, (float) totalFrames, 0.03f, 0.68f);
            session.notes = buildNotes(stabilisePitches(pitches, amplitudes), amplitudes, (float) analysisHopSize / (float) request.sampleRate, frame);
            session.progressLabel = "Detecting notes";
            publishProgress(session);
        }
    }

    auto smoothed = stabilisePitches(pitches, amplitudes);
    session.progress = 0.72f;
    session.progressLabel = "Building note blobs";
    session.notes = buildNotes(smoothed, amplitudes, (float) analysisHopSize / (float) request.sampleRate, totalFrames);
    publishProgress(session);

    session.progress = 0.82f;
    session.progressLabel = "Drawing waveform";
    session.waveform = buildWaveform(request.buffer, request.sampleRate);
    publishProgress(session);

    session.progress = 0.9f;
    session.progressLabel = "Rendering spectrogram";
    std::atomic<bool> cancelled{false};
    session.spectrogram = buildSpectrogram(request.buffer, request.sampleRate, cancelled);
    applyAlgorithmShaping(session, request.algorithm);
    updateHeatmap(session);
    detectTempoAndHarmony(session);
    session.statistics = buildStatistics(session);

    const int voicedFrames = (int) std::count_if(smoothed.begin(), smoothed.end(), [](float value) { return value > 1.0f; });
    const int lowEnergyFrames = (int) std::count_if(amplitudes.begin(), amplitudes.end(), [](float value) { return value < silenceThreshold; });
    session.polyphonicWarning = voicedFrames > 0 && (session.notes.size() < (size_t) juce::jmax(1, voicedFrames / 180) || lowEnergyFrames < voicedFrames / 6);
    session.analyzing = false;
    session.progress = 1.0f;
    session.progressLabel = "Analysis complete";
    session.revision += 1;
    return session;
}

void PitchAnalysisEngine::publishProgress(const AnalysisSession& partialSession)
{
    listener.pitchAnalysisUpdated(partialSession);
}

float PitchAnalysisEngine::estimatePitchHz(const float* data, int size, double sampleRate, float& confidence)
{
    const int minLag = juce::jlimit(2, size / 2, (int) std::floor(sampleRate / maximumPitchHz));
    const int maxLag = juce::jlimit(minLag + 1, size - 2, (int) std::ceil(sampleRate / minimumPitchHz));

    std::vector<float> difference((size_t) maxLag + 1, 0.0f);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        float value = 0.0f;
        for (int i = 0; i < size - lag; ++i)
        {
            const float delta = data[i] - data[i + lag];
            value += delta * delta;
        }
        difference[(size_t) lag] = value;
    }

    std::vector<float> cumulative((size_t) maxLag + 1, 0.0f);
    float running = 0.0f;
    float bestValue = 1.0f;
    int bestLag = -1;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        running += difference[(size_t) lag];
        if (running <= 0.0f)
            continue;

        cumulative[(size_t) lag] = difference[(size_t) lag] * (float) lag / running;
        if (cumulative[(size_t) lag] < bestValue)
        {
            bestValue = cumulative[(size_t) lag];
            bestLag = lag;
        }
    }

    confidence = juce::jlimit(0.0f, 1.0f, 1.0f - bestValue);
    if (bestLag <= 0)
        return 0.0f;

    return (float) (sampleRate / (double) bestLag);
}

std::vector<float> PitchAnalysisEngine::smoothPitches(const std::vector<float>& pitches)
{
    std::vector<float> smoothed = pitches;
    for (size_t i = 0; i < pitches.size(); ++i)
    {
        std::vector<float> neighbourhood;
        for (int offset = -2; offset <= 2; ++offset)
        {
            const auto index = (int) i + offset;
            if (index >= 0 && index < (int) pitches.size() && pitches[(size_t) index] > 1.0f)
                neighbourhood.push_back(pitches[(size_t) index]);
        }

        if (! neighbourhood.empty())
            smoothed[i] = median(neighbourhood);
    }
    return smoothed;
}

std::vector<float> PitchAnalysisEngine::stabilisePitches(const std::vector<float>& pitches,
                                                         const std::vector<float>& amplitudes)
{
    auto stabilised = smoothPitches(pitches);
    const auto size = (int) stabilised.size();

    for (int i = 0; i < size; ++i)
    {
        if (stabilised[(size_t) i] > 1.0f || amplitudes[(size_t) i] < silenceThreshold * 0.65f)
            continue;

        int previous = -1;
        int next = -1;
        for (int offset = 1; offset <= 3; ++offset)
        {
            if (previous < 0 && i - offset >= 0 && stabilised[(size_t) (i - offset)] > 1.0f)
                previous = i - offset;
            if (next < 0 && i + offset < size && stabilised[(size_t) (i + offset)] > 1.0f)
                next = i + offset;
        }

        if (previous >= 0 && next >= 0)
        {
            const float previousPitch = stabilised[(size_t) previous];
            const float nextPitch = stabilised[(size_t) next];
            if (std::abs(previousPitch - nextPitch) <= 1.25f)
                stabilised[(size_t) i] = 0.5f * (previousPitch + nextPitch);
        }
        else if (next >= 0 && next - i <= 2)
        {
            stabilised[(size_t) i] = stabilised[(size_t) next];
        }
        else if (previous >= 0 && i - previous <= 2)
        {
            stabilised[(size_t) i] = stabilised[(size_t) previous];
        }
    }

    return stabilised;
}

std::vector<DetectedNote> PitchAnalysisEngine::buildNotes(const std::vector<float>& pitches,
                                                          const std::vector<float>& amplitudes,
                                                          float hopSeconds,
                                                          int framesToUse)
{
    std::vector<DetectedNote> notes;
    const int maxFrame = juce::jlimit(0, (int) pitches.size(), framesToUse);
    int startFrame = -1;

    auto emitNote = [&](int endFrame)
    {
        if (startFrame < 0 || endFrame <= startFrame)
            return;

        std::vector<float> voicedPitches;
        std::vector<PitchPoint> trace;
        std::vector<float> envelope;
        float ampSum = 0.0f;

        for (int i = startFrame; i < endFrame; ++i)
        {
            const float pitch = pitches[(size_t) i];
            const float amplitude = amplitudes[(size_t) i];
            if (pitch <= 1.0f)
                continue;

            voicedPitches.push_back(pitch);
            const float cents = (pitch - std::round(pitch)) * 100.0f;
            const bool sibilant = amplitude < 0.028f;
            trace.push_back(PitchPoint{ i * hopSeconds, pitch, amplitude, cents, sibilant });
            envelope.push_back(amplitude);
            ampSum += amplitude;
        }

        if (trace.size() < 2 || voicedPitches.empty())
            return;

        DetectedNote note;
        note.id = juce::Uuid().toString();
        const float startPadding = hopSeconds * 1.5f;
        const float endPadding = hopSeconds * 0.75f;
        const float paddedStart = juce::jmax(0.0f, trace.front().time - startPadding);
        const float paddedEnd = trace.back().time + hopSeconds + endPadding;
        note.startTime = paddedStart;
        note.endTime = paddedEnd;
        note.originalStartTime = note.startTime;
        note.originalEndTime = note.endTime;
        note.originalPitch = median(voicedPitches);
        note.correctedPitch = note.originalPitch;
        note.pitchCenter = note.originalPitch;
        note.pitchOffsetCents = (note.originalPitch - std::round(note.originalPitch)) * 100.0f;
        if (trace.front().time > paddedStart)
            trace.insert(trace.begin(), PitchPoint{ paddedStart, trace.front().pitch, trace.front().amplitude, trace.front().cents, trace.front().sibilant });
        if (trace.back().time < paddedEnd)
            trace.push_back(PitchPoint{ paddedEnd, trace.back().pitch, trace.back().amplitude, trace.back().cents, trace.back().sibilant });

        note.pitchTrace = trace;
        note.correctedPitchTrace = trace;
        note.amplitudeEnvelope = envelope;
        note.amplitude = ampSum / (float) juce::jmax(1, (int) envelope.size());
        const auto vibrato = estimateVibrato(trace);
        note.vibratoRate = vibrato.first;
        note.vibratoDepth = vibrato.second;
        note.sibilantFlag = std::count_if(trace.begin(), trace.end(), [](const auto& point) { return point.sibilant; }) > (int) trace.size() / 5;
        note.timeHandles = { 0.25f, 0.5f, 0.75f };
        notes.push_back(std::move(note));
    };

    for (int frame = 0; frame < maxFrame; ++frame)
    {
        const float pitch = pitches[(size_t) frame];
        const float previousPitch = frame > 0 ? pitches[(size_t) (frame - 1)] : pitch;
        const bool voiced = pitch > 1.0f && amplitudes[(size_t) frame] > silenceThreshold;
        const bool previousVoiced = previousPitch > 1.0f;

        if (! voiced)
        {
            emitNote(frame);
            startFrame = -1;
            continue;
        }

        if (startFrame < 0)
        {
            startFrame = frame;
            continue;
        }

        const float jump = std::abs(pitch - previousPitch);
        if (previousVoiced && jump > 0.85f)
        {
            emitNote(frame);
            startFrame = frame;
        }
    }

    emitNote(maxFrame);
    return notes;
}

std::vector<WaveformPoint> PitchAnalysisEngine::buildWaveform(const juce::AudioBuffer<float>& monoBuffer,
                                                              double sampleRate)
{
    std::vector<WaveformPoint> waveform;
    const auto* mono = monoBuffer.getReadPointer(0);
    const int step = 512;
    for (int start = 0; start < monoBuffer.getNumSamples(); start += step)
    {
        float minimum = 1.0f;
        float maximum = -1.0f;
        const int end = juce::jmin(start + step, monoBuffer.getNumSamples());
        for (int i = start; i < end; ++i)
        {
            minimum = juce::jmin(minimum, mono[i]);
            maximum = juce::jmax(maximum, mono[i]);
        }

        waveform.push_back({
            (float) ((double) start / sampleRate),
            minimum,
            maximum
        });
    }
    return waveform;
}

std::vector<SpectrogramFrame> PitchAnalysisEngine::buildSpectrogram(const juce::AudioBuffer<float>& monoBuffer,
                                                                    double sampleRate,
                                                                    std::atomic<bool>& shouldCancel)
{
    std::vector<SpectrogramFrame> spectrogram;
    juce::dsp::FFT fft(9);
    juce::dsp::WindowingFunction<float> window(512, juce::dsp::WindowingFunction<float>::hann, false);
    std::array<float, 1024> fftData{};
    const auto* mono = monoBuffer.getReadPointer(0);
    const int hop = 192;

    for (int start = 0; start + 512 < monoBuffer.getNumSamples(); start += hop)
    {
        if (shouldCancel.load())
            break;

        std::fill(fftData.begin(), fftData.end(), 0.0f);
        std::copy(mono + start, mono + start + 512, fftData.begin());
        window.multiplyWithWindowingTable(fftData.data(), 512);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        SpectrogramFrame frame;
        frame.time = (float) ((double) start / sampleRate);
        frame.bins.resize(48, 0.0f);

        for (size_t bin = 0; bin < frame.bins.size(); ++bin)
        {
            const int spectrumIndex = juce::jlimit(0, 255, (int) juce::jmap((float) bin, 0.0f, 47.0f, 1.0f, 220.0f));
            const float magnitude = juce::Decibels::gainToDecibels(fftData[(size_t) spectrumIndex] / 256.0f, -72.0f);
            frame.bins[bin] = juce::jmap(magnitude, -72.0f, 0.0f, 0.0f, 1.0f);
        }

        spectrogram.push_back(std::move(frame));
    }

    return spectrogram;
}

void PitchAnalysisEngine::updateHeatmap(AnalysisSession& session)
{
    session.pitchHeatmap.fill(0.0f);
    for (const auto& note : session.notes)
    {
        for (const auto& point : note.pitchTrace)
        {
            const int index = juce::jlimit(0, 127, juce::roundToInt(point.pitch));
            session.pitchHeatmap[(size_t) index] += point.amplitude;
        }
    }

    const auto maxValue = *std::max_element(session.pitchHeatmap.begin(), session.pitchHeatmap.end());
    if (maxValue > 0.0f)
    {
        for (auto& value : session.pitchHeatmap)
            value /= maxValue;
    }
}

std::pair<float, float> PitchAnalysisEngine::estimateVibrato(const std::vector<PitchPoint>& points)
{
    if (points.size() < 4)
        return { 0.0f, 0.0f };

    std::vector<float> centred;
    centred.reserve(points.size());
    std::vector<float> pitchValues;
    pitchValues.reserve(points.size());
    for (const auto& point : points)
        pitchValues.push_back(point.pitch);

    const float centre = median(pitchValues);
    for (const auto& point : points)
        centred.push_back(point.pitch - centre);

    int zeroCrossings = 0;
    for (size_t i = 1; i < centred.size(); ++i)
    {
        if ((centred[i - 1] < 0.0f && centred[i] >= 0.0f) || (centred[i - 1] > 0.0f && centred[i] <= 0.0f))
            ++zeroCrossings;
    }

    const float duration = points.back().time - points.front().time;
    const float rate = duration > 0.0f ? (0.5f * (float) zeroCrossings) / duration : 0.0f;

    float depth = 0.0f;
    for (auto value : centred)
        depth += std::abs(value);

    depth = points.empty() ? 0.0f : (depth / (float) points.size()) * 100.0f;
    return { rate, depth };
}

void PitchAnalysisEngine::applyAlgorithmShaping(AnalysisSession& session, DetectionAlgorithm algorithm)
{
    for (auto& note : session.notes)
        note.algorithm = algorithm;

    switch (algorithm)
    {
        case DetectionAlgorithm::percussive:
        case DetectionAlgorithm::universal:
            for (auto& note : session.notes)
            {
                note.originalPitch = 60.0f;
                note.correctedPitch = 60.0f;
                note.pitchCenter = 60.0f;
                for (auto& point : note.pitchTrace)
                    point.pitch = 60.0f;
                note.correctedPitchTrace = note.pitchTrace;
            }
            break;

        case DetectionAlgorithm::percussivePitched:
            for (auto& note : session.notes)
                note.attackSpeed = 135.0f;
            break;

        case DetectionAlgorithm::polyphonicDecay:
        case DetectionAlgorithm::polyphonicSustain:
            for (auto& note : session.notes)
            {
                note.separationType = SeparationType::hard;
                note.fadeOut = algorithm == DetectionAlgorithm::polyphonicDecay ? 0.2f : 0.08f;
            }
            session.polyphonicWarning = false;
            break;

        case DetectionAlgorithm::automatic:
        case DetectionAlgorithm::melodic:
        default:
            break;
    }
}

void PitchAnalysisEngine::detectTempoAndHarmony(AnalysisSession& session)
{
    session.tempoMap.clear();
    session.chordTrack.clear();
    session.keyTrack.clear();

    if (session.notes.empty())
        return;

    std::vector<double> onsetDiffs;
    for (size_t i = 1; i < session.notes.size(); ++i)
        onsetDiffs.push_back(session.notes[i].startTime - session.notes[i - 1].startTime);

    double bpm = 120.0;
    if (! onsetDiffs.empty())
    {
        std::vector<float> diffFloats;
        diffFloats.reserve(onsetDiffs.size());
        for (auto diff : onsetDiffs)
            if (diff > 0.04)
                diffFloats.push_back((float) diff);

        if (! diffFloats.empty())
            bpm = juce::jlimit(50.0, 220.0, 60.0 / juce::jmax(0.15f, median(diffFloats)));
    }

    session.tempoMap.push_back({ 0.0, bpm, 4, 4 });

    std::array<float, 12> histogram{};
    for (const auto& note : session.notes)
        histogram[(size_t) ((juce::roundToInt(note.originalPitch) % 12 + 12) % 12)] += juce::jmax(0.15f, note.amplitude);

    int tonic = 0;
    for (int i = 1; i < 12; ++i)
        if (histogram[(size_t) i] > histogram[(size_t) tonic])
            tonic = i;

    float majorScore = histogram[(size_t) ((tonic + 4) % 12)] + histogram[(size_t) ((tonic + 7) % 12)];
    float minorScore = histogram[(size_t) ((tonic + 3) % 12)] + histogram[(size_t) ((tonic + 7) % 12)];
    const juce::String scaleName = majorScore >= minorScore ? "Major" : "Minor";
    session.keyTrack.push_back({ 0.0, session.durationSeconds, noteNameFromClass(tonic), scaleName });

    const double chordWindow = 1.0;
    for (double start = 0.0; start < session.durationSeconds; start += chordWindow)
    {
        std::array<float, 12> chordHistogram{};
        for (const auto& note : session.notes)
        {
            if (note.endTime < start || note.startTime > start + chordWindow)
                continue;
            chordHistogram[(size_t) ((juce::roundToInt(note.correctedPitch) % 12 + 12) % 12)] += juce::jmax(0.2f, note.amplitude);
        }

        int root = 0;
        for (int i = 1; i < 12; ++i)
            if (chordHistogram[(size_t) i] > chordHistogram[(size_t) root])
                root = i;

        const bool hasMinorThird = chordHistogram[(size_t) ((root + 3) % 12)] > 0.1f;
        const bool hasMajorThird = chordHistogram[(size_t) ((root + 4) % 12)] > 0.1f;
        const bool hasFifth = chordHistogram[(size_t) ((root + 7) % 12)] > 0.1f;
        juce::String symbol = noteNameFromClass(root);
        if (hasMinorThird && hasFifth)
            symbol << "m";
        else if (hasMajorThird && hasFifth)
            symbol << "maj";
        else
            symbol << "5";

        session.chordTrack.push_back({ start, juce::jmin(session.durationSeconds, start + chordWindow), symbol,
                                       { root, (root + (hasMinorThird ? 3 : 4)) % 12, (root + 7) % 12 } });
    }
}

StatisticsSummary PitchAnalysisEngine::buildStatistics(const AnalysisSession& session)
{
    StatisticsSummary stats;
    stats.totalNotes = (int) session.notes.size();
    if (session.notes.empty())
        return stats;

    const auto tonic = session.keyTrack.empty() ? 0 : juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }.indexOf(session.keyTrack.front().tonic);
    const auto scale = session.keyTrack.empty() ? juce::String("Major") : session.keyTrack.front().scale;

    for (const auto& note : session.notes)
    {
        stats.averagePitchAccuracyBefore += std::abs((note.originalPitch - std::round(note.originalPitch)) * 100.0f);
        stats.averagePitchAccuracyAfter += std::abs((note.correctedPitch - std::round(note.correctedPitch)) * 100.0f);
        stats.correctionAmountAverage += std::abs(note.correctedPitch - note.originalPitch) * 100.0f;
        if (isMidiNoteInScale(juce::roundToInt(note.correctedPitch), tonic < 0 ? 0 : tonic, scale))
            stats.inScalePercent += 1.0f;
    }

    const float denom = (float) session.notes.size();
    stats.averagePitchAccuracyBefore /= denom;
    stats.averagePitchAccuracyAfter /= denom;
    stats.correctionAmountAverage /= denom;
    stats.inScalePercent = (stats.inScalePercent / denom) * 100.0f;
    return stats;
}
} // namespace ssp::pitch
