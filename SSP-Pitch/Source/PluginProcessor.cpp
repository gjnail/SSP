#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using namespace ssp::pitch;

constexpr int maxTransportPoints = 262144;
constexpr float minimumEditableMidi = 12.0f; // C0
constexpr float maximumEditableMidi = 96.0f; // C7
constexpr float neutralPitchThreshold = 0.01f;
constexpr float liveMinimumConfidence = 0.55f;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto percent = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctPitch", "Correct Pitch", percent, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctDrift", "Correct Drift", percent, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctTiming", "Correct Timing", percent, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vibratoDepth", "Vibrato Depth", juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("vibratoSpeed", "Vibrato Speed", juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantShift", "Formant Shift", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("formantPreserve", "Formant Preserve", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("showHeatmap", "Show Heatmap", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("showWaveform", "Show Waveform", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("showSpectrogram", "Show Spectrogram", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("showGhostTrace", "Show Ghost Trace", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("showStats", "Show Stats", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("snapMode", "Snap Mode", getSnapModeNames(), 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scaleKey", "Scale Key", getScaleKeyNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scaleMode", "Scale Mode", getScaleModeNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("algorithmMode", "Algorithm", juce::StringArray{
        "Automatic", "Melodic", "Polyphonic Decay", "Polyphonic Sustain", "Percussive", "Percussive Pitched", "Universal"
    }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("referencePitchHz", "Reference Pitch", juce::NormalisableRange<float>(420.0f, 460.0f, 0.01f), 440.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("liveEnabled", "Live Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctionAmount", "Correction Amount", percent, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("retuneSpeed", "Retune Speed", percent, 35.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", percent, 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", percent, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("inputRange", "Input Range", juce::StringArray{
        "Soprano", "Alto/Tenor", "Low Male", "Instrument"
    }, 1));
    return { params.begin(), params.end() };
}

const juce::Array<CorrectionPreset>& factoryPresets()
{
    static const juce::Array<CorrectionPreset> presets = []()
    {
        juce::Array<CorrectionPreset> list;
        list.add({"Natural Vocal", 30.0f, 20.0f, 0.0f, 100.0f, 100.0f, 0.0f, true});
        list.add({"Polished Vocal", 60.0f, 50.0f, 10.0f, 90.0f, 100.0f, 0.0f, true});
        list.add({"Pop Vocal", 80.0f, 70.0f, 30.0f, 80.0f, 105.0f, 0.0f, true});
        list.add({"Auto-Tune Effect", 100.0f, 100.0f, 0.0f, 0.0f, 110.0f, 0.0f, true});
        list.add({"Gentle Touch", 15.0f, 10.0f, 0.0f, 100.0f, 100.0f, 0.0f, true});
        list.add({"Bass Tighten", 72.0f, 25.0f, 0.0f, 85.0f, 100.0f, 0.0f, true});
        list.add({"Solo Instrument", 48.0f, 35.0f, 10.0f, 95.0f, 100.0f, 0.0f, true});
        list.add({"Creative Formant Up", 55.0f, 40.0f, 0.0f, 100.0f, 100.0f, 4.0f, true});
        list.add({"Creative Formant Down", 55.0f, 40.0f, 0.0f, 100.0f, 100.0f, -4.0f, true});
        list.add({"Vibrato Smooth", 45.0f, 40.0f, 0.0f, 50.0f, 95.0f, 0.0f, true});
        list.add({"Vibrato Exaggerate", 20.0f, 15.0f, 0.0f, 150.0f, 120.0f, 0.0f, true});
        list.add({"Podcast / Dialogue", 12.0f, 30.0f, 0.0f, 95.0f, 100.0f, 0.0f, true});
        return list;
    }();
    return presets;
}

const juce::StringArray& detectionAlgorithmNames()
{
    static const juce::StringArray names{
        "Automatic", "Melodic", "Polyphonic Decay", "Polyphonic Sustain",
        "Percussive", "Percussive Pitched", "Universal"
    };
    return names;
}

float getParam(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return apvts.getRawParameterValue(id)->load();
}

float hzToMidi(float hz, float referenceHz)
{
    return 69.0f + 12.0f * std::log2(juce::jmax(1.0f, hz) / juce::jmax(1.0f, referenceHz));
}

double getPositionSeconds(const juce::AudioPlayHead::PositionInfo* info, double fallbackSampleRate)
{
    if (info == nullptr)
        return 0.0;

    if (auto seconds = info->getTimeInSeconds())
        return *seconds;

    if (auto samples = info->getTimeInSamples())
        return (double) *samples / juce::jmax(1.0, fallbackSampleRate);

    return 0.0;
}

std::int64_t getPositionSamples(const juce::AudioPlayHead::PositionInfo* info)
{
    if (info == nullptr)
        return 0;

    if (auto samples = info->getTimeInSamples())
        return *samples;

    return 0;
}

double getBpm(const juce::AudioPlayHead::PositionInfo* info)
{
    if (info == nullptr)
        return 120.0;

    if (auto bpm = info->getBpm())
        return *bpm;

    return 120.0;
}

int getTimeSigNumerator(const juce::AudioPlayHead::PositionInfo* info)
{
    if (info == nullptr)
        return 4;

    if (auto sig = info->getTimeSignature())
        return sig->numerator;

    return 4;
}

bool isHostPlaying(const juce::AudioPlayHead::PositionInfo* info)
{
    if (info == nullptr)
        return false;

    return info->getIsPlaying();
}

int getTimeSigDenominator(const juce::AudioPlayHead::PositionInfo* info)
{
    if (info == nullptr)
        return 4;

    if (auto sig = info->getTimeSignature())
        return sig->denominator;

    return 4;
}

float readHermiteInterpolated(const juce::AudioBuffer<float>& buffer, int channel, float sourcePosition)
{
    const int size = buffer.getNumSamples();
    if (size <= 1)
        return size == 1 ? buffer.getSample(channel, 0) : 0.0f;

    const int x1 = juce::jlimit(0, size - 1, (int) std::floor(sourcePosition));
    const int x0 = juce::jlimit(0, size - 1, x1 - 1);
    const int x2 = juce::jlimit(0, size - 1, x1 + 1);
    const int x3 = juce::jlimit(0, size - 1, x1 + 2);
    const float t = sourcePosition - (float) x1;

    const float y0 = buffer.getSample(channel, x0);
    const float y1 = buffer.getSample(channel, x1);
    const float y2 = buffer.getSample(channel, x2);
    const float y3 = buffer.getSample(channel, x3);

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

float midiToHz(float midiNote)
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

std::pair<float, float> getInputRangeHz(int inputRangeIndex)
{
    switch (inputRangeIndex)
    {
        case 0: return { 196.0f, 1400.0f }; // soprano
        case 1: return { 98.0f, 880.0f };   // alto/tenor
        case 2: return { 65.0f, 440.0f };   // low male
        case 3:
        default: return { 55.0f, 1760.0f }; // instrument
    }
}

float estimateYinPitchHz(const float* data, int size, double sampleRate, float minHz, float maxHz, float& confidence)
{
    const int minLag = juce::jlimit(2, size / 2, (int) std::floor(sampleRate / juce::jmax(40.0f, maxHz)));
    const int maxLag = juce::jlimit(minLag + 1, size - 2, (int) std::ceil(sampleRate / juce::jmax(1.0f, minHz)));
    std::array<float, 1025> difference{};

    float runningSum = 0.0f;
    float bestValue = 1.0f;
    int bestLag = -1;

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

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        runningSum += difference[(size_t) lag];
        if (runningSum <= 0.0f)
            continue;

        const float normalised = difference[(size_t) lag] * (float) lag / runningSum;
        if (normalised < bestValue)
        {
            bestValue = normalised;
            bestLag = lag;
        }
    }

    confidence = juce::jlimit(0.0f, 1.0f, 1.0f - bestValue);
    if (bestLag <= 0)
        return 0.0f;

    return (float) (sampleRate / (double) bestLag);
}

float hannWindowSample(float position, float length)
{
    if (length <= 1.0f)
        return 1.0f;

    const float phase = juce::MathConstants<float>::twoPi * juce::jlimit(0.0f, 1.0f, position / (length - 1.0f));
    return 0.5f - 0.5f * std::cos(phase);
}

void renderPsolaLikeNote(const juce::AudioBuffer<float>& source,
                         juce::AudioBuffer<float>& destination,
                         const ssp::pitch::DetectedNote& note,
                         int originalStart,
                         int originalLength,
                         int outputLength,
                         int sampleRate)
{
    destination.clear();

    const float originalHz = juce::jlimit(40.0f, 2000.0f, midiToHz(note.originalPitch));
    const float targetHz = juce::jlimit(40.0f, 2000.0f, midiToHz(note.correctedPitch));
    const float originalPeriod = (float) sampleRate / originalHz;
    const float targetPeriod = (float) sampleRate / targetHz;
    const int halfWindow = juce::jlimit(24, 320, juce::roundToInt(juce::jmax(originalPeriod, targetPeriod) * 1.6f));
    const int windowSize = halfWindow * 2 + 1;

    if (originalLength <= windowSize + 2 || outputLength <= windowSize + 2)
    {
        for (int channel = 0; channel < destination.getNumChannels(); ++channel)
            for (int sample = 0; sample < outputLength; ++sample)
            {
                const float progress = outputLength > 1 ? (float) sample / (float) (outputLength - 1) : 0.0f;
                const float sourceIndex = (float) originalStart + progress * (float) juce::jmax(1, originalLength - 1);
                destination.setSample(channel, sample, readHermiteInterpolated(source, channel, sourceIndex));
            }
        return;
    }

    std::vector<float> weights((size_t) outputLength, 0.0f);
    std::vector<int> outputMarks;
    for (float mark = (float) halfWindow; mark < (float) (outputLength - halfWindow); mark += juce::jmax(8.0f, targetPeriod))
        outputMarks.push_back(juce::roundToInt(mark));
    if (outputMarks.empty())
        outputMarks.push_back(outputLength / 2);

    for (const int outputMark : outputMarks)
    {
        const float progress = outputLength > 1 ? (float) outputMark / (float) (outputLength - 1) : 0.0f;
        const float sourceCenter = (float) originalStart + progress * (float) juce::jmax(1, originalLength - 1);

        for (int offset = -halfWindow; offset <= halfWindow; ++offset)
        {
            const int destinationIndex = outputMark + offset;
            if (! juce::isPositiveAndBelow(destinationIndex, outputLength))
                continue;

            const float window = hannWindowSample((float) (offset + halfWindow), (float) windowSize);
            const float sourceIndex = juce::jlimit((float) originalStart,
                                                   (float) (originalStart + originalLength - 1),
                                                   sourceCenter + (float) offset);
            weights[(size_t) destinationIndex] += window;

            for (int channel = 0; channel < destination.getNumChannels(); ++channel)
            {
                const float sample = readHermiteInterpolated(source, channel, sourceIndex);
                destination.addSample(channel, destinationIndex, sample * window);
            }
        }
    }

    for (int channel = 0; channel < destination.getNumChannels(); ++channel)
        for (int sample = 0; sample < outputLength; ++sample)
        {
            const float weight = weights[(size_t) sample];
            if (weight > 0.0001f)
                destination.setSample(channel, sample, destination.getSample(channel, sample) / weight);
        }
}

bool noteHasAudibleEdit(const ssp::pitch::DetectedNote& note)
{
    if (note.muted || std::abs(note.gainDb) > 0.01f || std::abs(note.formantShift) > 0.01f
        || note.fadeIn > 0.001f || note.fadeOut > 0.001f || note.manuallyEdited)
        return true;

    if (std::abs(note.correctedPitch - note.originalPitch) > neutralPitchThreshold)
        return true;

    if (std::abs(note.startTime - note.originalStartTime) > 0.0005
        || std::abs(note.endTime - note.originalEndTime) > 0.0005)
        return true;

    if (note.correctedPitchTrace.size() != note.pitchTrace.size())
        return true;

    for (size_t i = 0; i < note.correctedPitchTrace.size(); ++i)
    {
        const auto& a = note.correctedPitchTrace[i];
        const auto& b = note.pitchTrace[i];
        if (std::abs(a.pitch - b.pitch) > neutralPitchThreshold
            || std::abs(a.time - b.time) > 0.0005f)
            return true;
    }

    return false;
}
} // namespace

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SSPPITCHSTATE", createParameterLayout())
{
    formatManager.registerBasicFormats();

    for (const auto& id : { "correctPitch", "correctDrift", "correctTiming", "vibratoDepth", "vibratoSpeed", "formantShift",
                            "formantPreserve", "snapMode", "scaleKey", "scaleMode", "algorithmMode", "referencePitchHz" })
        apvts.addParameterListener(id, this);

    session.progressLabel = "Waiting for transfer";
    session.transferState = TransferState::waiting;
    session.algorithm = DetectionAlgorithm::melodic;
    session.referencePitchHz = 440.0;
    sessionRevision.store(session.revision);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto& id : { "correctPitch", "correctDrift", "correctTiming", "vibratoDepth", "vibratoSpeed", "formantShift",
                            "formantPreserve", "snapMode", "scaleKey", "scaleMode", "algorithmMode", "referencePitchHz" })
        apvts.removeParameterListener(id, this);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentMaxBlockSize = samplesPerBlock;
    transportPlaying.store(false);
    hostPlaybackActive.store(false);
    playheadSeconds.store(0.0);
    liveDelayBufferSize = 1;
    while (liveDelayBufferSize < (int) std::round(sampleRate * 0.08))
        liveDelayBufferSize <<= 1;

    liveDelayBuffer.setSize(juce::jmax(1, getTotalNumOutputChannels()), liveDelayBufferSize);
    liveDelayBuffer.clear();
    liveDelayWritePosition = 0;
    liveBaseDelaySamples = (float) juce::jlimit(192, 2048, (int) std::round(sampleRate * 0.014));
    liveGrainSizeSamples = (float) juce::jlimit(96, 1024, (int) std::round(sampleRate * 0.008));
    livePhaseA = 0.0f;
    livePhaseB = 0.5f;
    liveHeadStartA = 0.0f;
    liveHeadStartB = 0.0f;
    liveGrainsInitialised = false;
    liveAnalysisRing.fill(0.0f);
    liveAnalysisScratch.fill(0.0f);
    liveAnalysisWritePosition = 0;
    liveAnalysisSinceLastEstimate = 0;
    stablePitchFrames = 0;
    previousDetectedMidi = 0.0f;
    smoothedDetectedMidi = 0.0f;
    smoothedCorrectionSemitones = 0.0f;
    liveDetectedMidi.store(0.0f);
    liveTargetMidi.store(0.0f);
    liveCorrectionCents.store(0.0f);
    liveInputLevel.store(0.0f);
    livePitchDetected.store(false);
    setLatencySamples((int) std::round(liveBaseDelaySamples + liveGrainSizeSamples));
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && ! layouts.getMainInputChannelSet().isDisabled();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : std::optional<juce::AudioPlayHead::PositionInfo>{};
    const auto* positionInfo = position ? &*position : nullptr;
    const bool hostPlaying = isHostPlaying(positionInfo);
    hostPlaybackActive.store(hostPlaying);

    playheadSeconds.store(getPositionSeconds(positionInfo, currentSampleRate));
    {
        const juce::ScopedLock lock(sessionLock);
        session.playheadSample = getPositionSamples(positionInfo);
        session.playheadSeconds = playheadSeconds.load();
    }

    if (isLiveEnabled())
        processLivePitchCorrection(buffer);
    else if (captureActive.load())
        captureIncomingAudio(buffer, positionInfo);
    else if (hasClipData.load() && transferState.load() == TransferState::playback && hostPlaying)
        renderPlaybackIntoBuffer(buffer, positionInfo);

    if (correctionDirty.exchange(false))
        triggerAsyncUpdate();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("session", juce::JSON::toString(serialiseSessionToVar(getSessionCopy())), nullptr);
    state.setProperty("captureClip", juce::JSON::toString(serialiseCaptureClipToVar()), nullptr);
    state.setProperty("activeCompareSlot", activeCompareSlot.load(), nullptr);
    state.setProperty("loopEnabled", loopEnabled.load(), nullptr);
    state.setProperty("transferState", (int) transferState.load(), nullptr);
    state.setProperty("referencePitchHz", referencePitchHz.load(), nullptr);
    state.setProperty("algorithmMode", (int) selectedAlgorithm.load(), nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, (size_t) sizeInBytes);
    if (! tree.isValid())
        return;

    apvts.replaceState(tree);
    activeCompareSlot.store((int) tree.getProperty("activeCompareSlot", 0));
    loopEnabled.store((bool) tree.getProperty("loopEnabled", false));
    transferState.store((TransferState) (int) tree.getProperty("transferState", (int) TransferState::waiting));
    referencePitchHz.store((double) tree.getProperty("referencePitchHz", 440.0));
    selectedAlgorithm.store((DetectionAlgorithm) (int) tree.getProperty("algorithmMode", (int) DetectionAlgorithm::melodic));

    const auto sessionText = tree.getProperty("session").toString();
    if (sessionText.isNotEmpty())
        setSession(deserialiseSessionFromVar(juce::JSON::parse(sessionText)));

    const auto clipText = tree.getProperty("captureClip").toString();
    if (clipText.isNotEmpty())
        deserialiseCaptureClipFromVar(juce::JSON::parse(clipText));

    rebuildCorrectedNotes();
    updateSessionFromClipState();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    return createLayout();
}

const juce::Array<ssp::pitch::CorrectionPreset>& PluginProcessor::getFactoryPresets()
{
    return factoryPresets();
}

const juce::StringArray& PluginProcessor::getFactoryPresetNames()
{
    static juce::StringArray names;
    if (names.isEmpty())
        for (const auto& preset : factoryPresets())
            names.add(preset.name);
    return names;
}

const juce::StringArray& PluginProcessor::getDetectionAlgorithmNames()
{
    return detectionAlgorithmNames();
}

ssp::pitch::AnalysisSession PluginProcessor::getSessionCopy() const
{
    const juce::ScopedLock lock(sessionLock);
    return session;
}

bool PluginProcessor::hasSessionNotes() const
{
    const juce::ScopedLock lock(sessionLock);
    return ! session.notes.empty();
}

bool PluginProcessor::hasCapturedAudio() const noexcept
{
    return hasClipData.load();
}

juce::String PluginProcessor::getTransferStateLabel() const
{
    switch (transferState.load())
    {
        case TransferState::capture: return "Capture";
        case TransferState::playback: return "Playback";
        case TransferState::waiting:
        default: return "Waiting";
    }
}

void PluginProcessor::startCapture()
{
    captureActive.store(false);
    transferState.store(TransferState::waiting);
}

void PluginProcessor::stopCaptureAndAnalyze()
{
    captureActive.store(false);
    transferState.store(TransferState::waiting);
}

void PluginProcessor::analyzeAudioFile(const juce::File& file)
{
    juce::ignoreUnused(file);
}

void PluginProcessor::loadFactoryCorrectionPreset(const juce::String& presetName)
{
    for (const auto& preset : factoryPresets())
    {
        if (preset.name != presetName)
            continue;

        auto setFloat = [this](const juce::String& id, float value)
        {
            if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(value));
                param->endChangeGesture();
            }
        };

        setFloat("correctPitch", preset.correctPitch);
        setFloat("correctDrift", preset.correctDrift);
        setFloat("correctTiming", preset.correctTiming);
        setFloat("vibratoDepth", preset.vibratoDepth);
        setFloat("vibratoSpeed", preset.vibratoSpeed);
        setFloat("formantShift", preset.formantShift);
        if (auto* param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("formantPreserve")))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(preset.formantPreserve ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
        break;
    }
}

void PluginProcessor::setDetectionAlgorithm(ssp::pitch::DetectionAlgorithm algorithm)
{
    selectedAlgorithm.store(algorithm);
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("algorithmMode")))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1((float) algorithm));
        param->endChangeGesture();
    }
    updateSessionFromClipState();
}

void PluginProcessor::setReferencePitchHz(double hz)
{
    referencePitchHz.store(juce::jlimit(420.0, 460.0, hz));
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("referencePitchHz")))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1((float) referencePitchHz.load()));
        param->endChangeGesture();
    }
    updateSessionFromClipState();
}

void PluginProcessor::setActiveCompareSlot(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) compareSlots.size()))
        return;

    const juce::ScopedLock lock(sessionLock);
    compareSlots[(size_t) activeCompareSlot.load()].session = session;
    compareSlots[(size_t) activeCompareSlot.load()].valid = true;
    activeCompareSlot.store(slotIndex);

    if (compareSlots[(size_t) slotIndex].valid)
    {
        session = compareSlots[(size_t) slotIndex].session;
        sessionRevision.store(session.revision);
    }
}

void PluginProcessor::copyActiveCompareSlotToOther()
{
    const int active = activeCompareSlot.load();
    const int other = active == 0 ? 1 : 0;
    const juce::ScopedLock lock(sessionLock);
    compareSlots[(size_t) active].session = session;
    compareSlots[(size_t) active].valid = true;
    compareSlots[(size_t) other] = compareSlots[(size_t) active];
}

void PluginProcessor::setTransportPlaying(bool shouldPlay)
{
    transportPlaying.store(shouldPlay);
}

void PluginProcessor::stopTransport()
{
    transportPlaying.store(false);
    playheadSeconds.store(0.0);
}

void PluginProcessor::setLoopEnabled(bool shouldLoop)
{
    loopEnabled.store(shouldLoop);
}

void PluginProcessor::advanceTransport(double deltaSeconds)
{
    if (! transportPlaying.load())
        return;

    const auto snapshot = getSessionCopy();
    if (snapshot.durationSeconds <= 0.0)
        return;

    double playhead = playheadSeconds.load() + deltaSeconds;
    if (playhead > snapshot.durationSeconds)
    {
        if (loopEnabled.load())
            playhead = 0.0;
        else
        {
            playhead = snapshot.durationSeconds;
            transportPlaying.store(false);
        }
    }

    playheadSeconds.store(playhead);
    const juce::ScopedLock lock(sessionLock);
    session.playheadSeconds = playhead;
}

bool PluginProcessor::undoLastEdit()
{
    {
        const juce::ScopedLock lock(sessionLock);
        if (undoStack.empty())
            return false;

        redoStack.push_back(session);
        session = undoStack.back();
        undoStack.pop_back();
        session.revision += 1;
    }
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
    return true;
}

bool PluginProcessor::redoLastEdit()
{
    {
        const juce::ScopedLock lock(sessionLock);
        if (redoStack.empty())
            return false;

        undoStack.push_back(session);
        session = redoStack.back();
        redoStack.pop_back();
        session.revision += 1;
    }
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
    return true;
}

bool PluginProcessor::canUndo() const noexcept
{
    const juce::ScopedLock lock(sessionLock);
    return ! undoStack.empty();
}

bool PluginProcessor::canRedo() const noexcept
{
    const juce::ScopedLock lock(sessionLock);
    return ! redoStack.empty();
}

void PluginProcessor::moveNote(const juce::String& noteId, float semitoneDelta, double timeDeltaSeconds)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        const double clipDuration = captureClip.sampleRate > 0
                                  ? (double) captureClip.numCapturedSamples / (double) captureClip.sampleRate
                                  : std::numeric_limits<double>::max();
        const double noteLength = juce::jmax(0.03, note.endTime - note.startTime);
        const double originalStart = note.startTime;
        const double newStart = juce::jlimit(0.0, juce::jmax(0.0, clipDuration - noteLength), note.startTime + timeDeltaSeconds);
        const float clampedPitchDelta = juce::jlimit(minimumEditableMidi - note.correctedPitch,
                                                     maximumEditableMidi - note.correctedPitch,
                                                     semitoneDelta);
        note.manuallyEdited = true;
        note.startTime = newStart;
        note.endTime = juce::jmin(clipDuration, newStart + noteLength);
        note.manualPitchOffset = juce::jlimit(minimumEditableMidi - note.originalPitch,
                                              maximumEditableMidi - note.originalPitch,
                                              note.manualPitchOffset + clampedPitchDelta);
        for (auto& point : note.correctedPitchTrace)
            point.time += (float) (newStart - originalStart);
    }))
        rebuildCorrectedNotes();
}

void PluginProcessor::resizeNote(const juce::String& noteId, double startDeltaSeconds, double endDeltaSeconds)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        const double clipDuration = captureClip.sampleRate > 0
                                  ? (double) captureClip.numCapturedSamples / (double) captureClip.sampleRate
                                  : std::numeric_limits<double>::max();
        note.manuallyEdited = true;
        note.startTime = juce::jlimit(0.0, juce::jmax(0.0, note.endTime - 0.03), note.startTime + startDeltaSeconds);
        note.endTime = juce::jlimit(note.startTime + 0.03, clipDuration, note.endTime + endDeltaSeconds);
    }))
    {
        rebuildRenderedClipAudio();
        updateSessionFromClipState();
    }
}

void PluginProcessor::splitNote(const juce::String& noteId, double splitTimeSeconds)
{
    const juce::ScopedLock lock(sessionLock);
    auto noteIt = std::find_if(session.notes.begin(), session.notes.end(), [&](const auto& note) { return note.id == noteId; });
    if (noteIt == session.notes.end() || splitTimeSeconds <= noteIt->startTime || splitTimeSeconds >= noteIt->endTime)
        return;

    pushUndoState();

    DetectedNote right = *noteIt;
    right.id = juce::Uuid().toString();
    right.startTime = splitTimeSeconds;
    noteIt->endTime = splitTimeSeconds;

    auto splitTrace = [&](std::vector<PitchPoint>& left, std::vector<PitchPoint>& rightTrace)
    {
        std::vector<PitchPoint> leftPoints;
        std::vector<PitchPoint> rightPoints;
        for (const auto& point : left)
        {
            if (point.time < splitTimeSeconds)
                leftPoints.push_back(point);
            else
                rightPoints.push_back(point);
        }
        left = std::move(leftPoints);
        rightTrace = std::move(rightPoints);
    };

    splitTrace(noteIt->pitchTrace, right.pitchTrace);
    splitTrace(noteIt->correctedPitchTrace, right.correctedPitchTrace);
    right.separationType = SeparationType::hard;
    noteIt->separationType = SeparationType::hard;
    session.notes.insert(noteIt + 1, std::move(right));
    session.revision += 1;
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
}

void PluginProcessor::mergeNotes(const juce::StringArray& noteIds)
{
    if (noteIds.size() < 2)
        return;

    const juce::ScopedLock lock(sessionLock);
    std::vector<int> indexes;
    for (int i = 0; i < (int) session.notes.size(); ++i)
        if (noteIds.contains(session.notes[(size_t) i].id))
            indexes.push_back(i);

    if (indexes.size() < 2)
        return;

    std::sort(indexes.begin(), indexes.end());
    pushUndoState();

    auto& left = session.notes[(size_t) indexes.front()];
    auto& right = session.notes[(size_t) indexes.back()];
    left.endTime = right.endTime;
    left.correctedPitch = 0.5f * (left.correctedPitch + right.correctedPitch);
    left.pitchTrace.insert(left.pitchTrace.end(), right.pitchTrace.begin(), right.pitchTrace.end());
    left.correctedPitchTrace.insert(left.correctedPitchTrace.end(), right.correctedPitchTrace.begin(), right.correctedPitchTrace.end());
    left.separationType = SeparationType::soft;
    session.notes.erase(session.notes.begin() + indexes.back());
    session.revision += 1;
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
}

void PluginProcessor::addDrawnNote(double startTime, double endTime, float midiNote)
{
    const juce::ScopedLock lock(sessionLock);
    pushUndoState();

    const double clipDuration = captureClip.sampleRate > 0
                              ? (double) captureClip.numCapturedSamples / (double) captureClip.sampleRate
                              : endTime;
    startTime = juce::jlimit(0.0, clipDuration, startTime);
    endTime = juce::jlimit(startTime + 0.03, clipDuration, endTime);
    midiNote = juce::jlimit(minimumEditableMidi, maximumEditableMidi, midiNote);

    DetectedNote note;
    note.id = juce::Uuid().toString();
    note.algorithm = selectedAlgorithm.load();
    note.startTime = startTime;
    note.endTime = endTime;
    note.originalStartTime = startTime;
    note.originalEndTime = endTime;
    note.originalPitch = midiNote;
    note.correctedPitch = midiNote;
    note.pitchCenter = midiNote;
    note.amplitude = 0.72f;
    note.manuallyEdited = true;

    for (int i = 0; i <= 16; ++i)
    {
        const float alpha = (float) i / 16.0f;
        const float time = (float) juce::jmap(alpha, 0.0f, 1.0f, (float) startTime, (float) endTime);
        note.pitchTrace.push_back({ time, midiNote, 0.72f, 0.0f, false });
        note.correctedPitchTrace.push_back({ time, midiNote, 0.72f, 0.0f, false });
        note.amplitudeEnvelope.push_back(0.72f);
    }

    session.notes.push_back(std::move(note));
    std::sort(session.notes.begin(), session.notes.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
    session.revision += 1;
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
}

void PluginProcessor::setNoteMuted(const juce::String& noteId, bool shouldMute)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.muted = shouldMute;
        note.manuallyEdited = true;
    }))
    {
        rebuildRenderedClipAudio();
        updateSessionFromClipState();
    }
}

void PluginProcessor::setNoteGainDb(const juce::String& noteId, float gainDb)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.gainDb = gainDb;
        note.manuallyEdited = true;
    }))
    {
        rebuildRenderedClipAudio();
        updateSessionFromClipState();
    }
}

void PluginProcessor::setNotePitchCorrection(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.pitchCorrection = amount;
        note.manuallyEdited = true;
    }))
        rebuildCorrectedNotes();
}

void PluginProcessor::setNoteDriftCorrection(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.driftCorrection = amount;
        note.manuallyEdited = true;
    }))
        rebuildCorrectedNotes();
}

void PluginProcessor::setNoteFormantShift(const juce::String& noteId, float semitones)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.formantShift = semitones;
        note.manuallyEdited = true;
    }))
        rebuildRenderedClipAudio();
}

void PluginProcessor::setNoteTransition(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.transitionToNext = amount;
        note.manuallyEdited = true;
    }))
        updateSessionFromClipState();
}

void PluginProcessor::nudgeNotePitchTrace(const juce::String& noteId, double timeSeconds, float midiNote, float radiusSeconds)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.manuallyEdited = true;
        for (auto& point : note.correctedPitchTrace)
        {
            const float distance = std::abs(point.time - (float) timeSeconds);
            if (distance > radiusSeconds)
                continue;

            const float blend = juce::jmap(distance, 0.0f, radiusSeconds, 1.0f, 0.0f);
            point.pitch = juce::jmap(blend, point.pitch, midiNote);
        }
    }))
        rebuildRenderedClipAudio();
}

void PluginProcessor::muteNotes(const juce::StringArray& noteIds, bool shouldMute)
{
    for (const auto& id : noteIds)
        setNoteMuted(id, shouldMute);
}

void PluginProcessor::applyCorrectPitchMacro(float centerAmount, float driftAmount, bool includeEditedNotes)
{
    {
        const juce::ScopedLock lock(sessionLock);
        pushUndoState();
        for (auto& note : session.notes)
        {
            if (! includeEditedNotes && note.manuallyEdited)
                continue;
            note.pitchCorrection = centerAmount;
            note.driftCorrection = driftAmount;
        }
        session.revision += 1;
    }
    rebuildCorrectedNotes();
}

void PluginProcessor::applyCurrentScaleToAllNotes()
{
    if (auto* snapParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("snapMode")))
    {
        snapParam->beginChangeGesture();
        snapParam->setValueNotifyingHost(snapParam->convertTo0to1(1.0f));
        snapParam->endChangeGesture();
    }

    if (auto* pitchParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("correctPitch")))
    {
        pitchParam->beginChangeGesture();
        pitchParam->setValueNotifyingHost(pitchParam->convertTo0to1(100.0f));
        pitchParam->endChangeGesture();
    }

    {
        const juce::ScopedLock lock(sessionLock);
        if (session.notes.empty())
            return;

        pushUndoState();
        for (auto& note : session.notes)
            note.pitchCorrection = 100.0f;
        session.revision += 1;
        sessionRevision.store(session.revision);
    }

    rebuildCorrectedNotes();
}

void PluginProcessor::applyQuantizeTimeMacro(float amount, bool includeEditedNotes)
{
    {
        const juce::ScopedLock lock(sessionLock);
        pushUndoState();
        for (auto& note : session.notes)
        {
            if (! includeEditedNotes && note.manuallyEdited)
                continue;
            note.manuallyEdited = false;
            note.startTime = note.originalStartTime;
            note.endTime = note.originalEndTime;
        }
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("correctTiming")))
            param->setValueNotifyingHost(param->convertTo0to1(amount));
        session.revision += 1;
    }
    rebuildCorrectedNotes();
}

void PluginProcessor::applyLevelingMacro(float quietAmount, float loudAmount, bool includeEditedNotes)
{
    {
        const juce::ScopedLock lock(sessionLock);
        if (session.notes.empty())
            return;

        pushUndoState();
        float averageAmplitude = 0.0f;
        for (const auto& note : session.notes)
            averageAmplitude += note.amplitude;
        averageAmplitude /= (float) session.notes.size();

        for (auto& note : session.notes)
        {
            if (! includeEditedNotes && note.manuallyEdited)
                continue;

            const float delta = averageAmplitude - note.amplitude;
            if (delta > 0.0f)
                note.gainDb += juce::jmap(quietAmount * 0.01f, 0.0f, 1.0f, 0.0f, delta * 12.0f);
            else
                note.gainDb -= juce::jmap(loudAmount * 0.01f, 0.0f, 1.0f, 0.0f, std::abs(delta) * 12.0f);
        }
        session.revision += 1;
    }
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
}

void PluginProcessor::resetNotes(const juce::StringArray& noteIds)
{
    {
        const juce::ScopedLock lock(sessionLock);
        pushUndoState();
        for (auto& note : session.notes)
        {
            if (! noteIds.isEmpty() && ! noteIds.contains(note.id))
                continue;

            note.startTime = note.originalStartTime;
            note.endTime = note.originalEndTime;
            note.correctedPitch = note.originalPitch;
            note.correctedPitchTrace = note.pitchTrace;
            note.manualPitchOffset = 0.0f;
            note.pitchCorrection = -1.0f;
            note.driftCorrection = -1.0f;
            note.formantShift = 0.0f;
            note.gainDb = 0.0f;
            note.transitionToNext = 0.0f;
            note.fadeIn = 0.0f;
            note.fadeOut = 0.0f;
            note.sibilantBalance = 0.0f;
            note.pitchModulation = 100.0f;
            note.pitchDrift = 100.0f;
            note.attackSpeed = 100.0f;
            note.muted = false;
            note.manuallyEdited = false;
        }
        session.revision += 1;
    }
    rebuildCorrectedNotes();
}

void PluginProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "algorithmMode")
        selectedAlgorithm.store((DetectionAlgorithm) juce::roundToInt(newValue));
    else if (parameterID == "referencePitchHz")
        referencePitchHz.store(newValue);

    if (hasClipData.load())
        correctionDirty.store(true);
}

void PluginProcessor::handleAsyncUpdate()
{
    rebuildCorrectedNotes();
}

void PluginProcessor::generateDemoSession()
{
}

void PluginProcessor::setSession(const AnalysisSession& newSession)
{
    const juce::ScopedLock lock(sessionLock);
    session = newSession;
    session.referencePitchHz = referencePitchHz.load();
    session.algorithm = selectedAlgorithm.load();
    session.transferState = transferState.load();
    session.revision += 1;
    sessionRevision.store(session.revision);
}

void PluginProcessor::pushUndoState()
{
    undoStack.push_back(session);
    if (undoStack.size() > 40)
        undoStack.erase(undoStack.begin());
    clearRedoState();
}

void PluginProcessor::clearRedoState()
{
    redoStack.clear();
}

void PluginProcessor::rebuildCorrectedNotes()
{
    {
        const juce::ScopedLock lock(sessionLock);
        if (! session.notes.empty())
        {
            const auto snapModeIndex = (int) getParam(apvts, "snapMode");
            const auto scaleKey = (int) getParam(apvts, "scaleKey");
            const auto scaleMode = getScaleModeNames()[juce::jlimit(0, getScaleModeNames().size() - 1, (int) getParam(apvts, "scaleMode"))];
            const auto snapMode = snapModeIndex == 0 ? SnapMode::chromatic : (snapModeIndex == 1 ? SnapMode::scale : SnapMode::free);
            const float globalPitch = getParam(apvts, "correctPitch");
            const float globalDrift = getParam(apvts, "correctDrift");
            const float globalTiming = getParam(apvts, "correctTiming");
            const float vibratoDepthScale = getParam(apvts, "vibratoDepth") * 0.01f;

            for (auto& note : session.notes)
            {
                note.pitchCenter = note.originalPitch;
                note.pitchOffsetCents = (note.originalPitch - std::round(note.originalPitch)) * 100.0f;
                const float notePitchAmount = note.pitchCorrection >= 0.0f ? note.pitchCorrection : globalPitch;
                const float noteDriftAmount = note.driftCorrection >= 0.0f ? note.driftCorrection : globalDrift;
                const float snapped = snapMidiNote(note.originalPitch, scaleKey, scaleMode, snapMode);
                const float autoTargetPitch = juce::jmap(notePitchAmount * 0.01f, note.originalPitch, snapped);
                note.correctedPitch = juce::jlimit(minimumEditableMidi, maximumEditableMidi, autoTargetPitch + note.manualPitchOffset);
                note.pitchModulation = getParam(apvts, "vibratoDepth");
                note.pitchDrift = 100.0f - noteDriftAmount;

                const double beatGrid = 0.125;
                const double quantisedStart = std::round(note.originalStartTime / beatGrid) * beatGrid;
                const double timeBlend = globalTiming * 0.01;
                const double startOffset = juce::jmap(timeBlend, note.originalStartTime, quantisedStart) - note.originalStartTime;
                if (! note.manuallyEdited)
                {
                    note.startTime = note.originalStartTime + startOffset;
                    note.endTime = note.startTime + (note.originalEndTime - note.originalStartTime);
                }

                note.correctedPitchTrace.clear();
                for (const auto& point : note.pitchTrace)
                {
                    const float centred = point.pitch - note.originalPitch;
                    const float shiftedOriginal = juce::jlimit(minimumEditableMidi, maximumEditableMidi, point.pitch + note.manualPitchOffset);
                    const float flattened = juce::jlimit(minimumEditableMidi, maximumEditableMidi,
                                                         note.correctedPitch + centred * (1.0f - noteDriftAmount * 0.01f) * vibratoDepthScale);
                    const float moved = point.sibilant ? point.pitch : juce::jmap(notePitchAmount * 0.01f, shiftedOriginal, flattened);
                    note.correctedPitchTrace.push_back({
                        point.time + (float) startOffset,
                        moved,
                        point.amplitude,
                        (moved - std::round(moved)) * 100.0f,
                        point.sibilant
                    });
                }
            }
        }
    }
    rebuildRenderedClipAudio();
    updateSessionFromClipState();
}

void PluginProcessor::rebuildRenderedClipAudio()
{
    if (! hasClipData.load() || captureClip.numCapturedSamples <= 0)
        return;

    AnalysisSession snapshot;
    {
        const juce::ScopedLock lock(sessionLock);
        snapshot = session;
    }

    const int renderSamples = captureClip.numCapturedSamples;
    juce::AudioBuffer<float> rebuiltAudio;
    rebuiltAudio.makeCopyOf(captureClip.originalAudio);

    constexpr int maxEdgeBlendSamples = 128;

    for (const auto& note : snapshot.notes)
    {
        if (! noteHasAudibleEdit(note))
            continue;

        const int originalStart = juce::jlimit(0, renderSamples - 1, (int) std::floor(note.originalStartTime * captureClip.sampleRate));
        const int originalEnd = juce::jlimit(originalStart + 1, renderSamples, (int) std::ceil(note.originalEndTime * captureClip.sampleRate));
        const int outputStart = juce::jlimit(0, renderSamples - 1, (int) std::floor(note.startTime * captureClip.sampleRate));
        const int outputEnd = juce::jlimit(outputStart + 1, renderSamples, (int) std::ceil(note.endTime * captureClip.sampleRate));
        const int originalLength = juce::jmax(1, originalEnd - originalStart);
        const int outputLength = juce::jmax(1, outputEnd - outputStart);
        const float gainLinear = note.muted ? 0.0f : juce::Decibels::decibelsToGain(note.gainDb);
        const int edgeBlendSamples = juce::jmin(maxEdgeBlendSamples, juce::jmax(1, outputLength / 4));
        const bool needsPitchShift = std::abs(note.correctedPitch - note.originalPitch) > neutralPitchThreshold;
        juce::AudioBuffer<float> renderedNote(captureClip.numChannels, outputLength);
        renderedNote.clear();

        if (! needsPitchShift && outputLength == originalLength)
        {
            for (int channel = 0; channel < captureClip.numChannels; ++channel)
                renderedNote.copyFrom(channel, 0, captureClip.originalAudio, channel, originalStart, outputLength);
        }
        else
        {
            renderPsolaLikeNote(captureClip.originalAudio, renderedNote, note, originalStart, originalLength, outputLength, captureClip.sampleRate);
        }

        for (int channel = 0; channel < captureClip.numChannels; ++channel)
        {
            for (int i = 0; i < outputLength; ++i)
            {
                const float alpha = (float) i / (float) outputLength;
                float sample = renderedNote.getSample(channel, i);

                const float fadeInGain = note.fadeIn > 0.0f ? juce::jlimit(0.0f, 1.0f, alpha / juce::jmax(0.001f, note.fadeIn)) : 1.0f;
                const float fadeOutGain = note.fadeOut > 0.0f ? juce::jlimit(0.0f, 1.0f, (1.0f - alpha) / juce::jmax(0.001f, note.fadeOut)) : 1.0f;
                const float userFadeGain = juce::jmin(fadeInGain, fadeOutGain);

                float edgeBlend = 1.0f;
                if (i < edgeBlendSamples)
                    edgeBlend = std::sin(juce::MathConstants<float>::halfPi * ((float) i / (float) edgeBlendSamples));
                else if (i > outputLength - edgeBlendSamples - 1)
                    edgeBlend = std::sin(juce::MathConstants<float>::halfPi * ((float) (outputLength - 1 - i) / (float) edgeBlendSamples));

                const float blend = juce::jlimit(0.0f, 1.0f, edgeBlend);
                const int destinationSample = outputStart + i;
                const float existing = rebuiltAudio.getSample(channel, destinationSample);
                const float edited = sample * gainLinear * userFadeGain;
                rebuiltAudio.setSample(channel, destinationSample, juce::jmap(blend, existing, edited));
            }
        }
    }

    const juce::SpinLock::ScopedLockType renderLock(renderedAudioLock);
    std::swap(captureClip.renderedAudio, rebuiltAudio);
}

void PluginProcessor::updateSessionFromClipState()
{
    const juce::ScopedLock lock(sessionLock);
    session.transferState = transferState.load();
    session.hasCapturedAudio = hasClipData.load();
    session.algorithm = selectedAlgorithm.load();
    session.referencePitchHz = referencePitchHz.load();
    session.playheadSeconds = playheadSeconds.load();
    session.sourceName = captureClip.sourceName.isNotEmpty() ? captureClip.sourceName : session.sourceName;

    session.clips.clear();
    if (hasClipData.load())
    {
        CaptureClipSummary summary;
        summary.id = captureClip.id;
        summary.sourceName = captureClip.sourceName;
        summary.algorithm = captureClip.algorithm;
        summary.dawStartSample = captureClip.dawStartSample;
        summary.dawEndSample = captureClip.dawEndSample;
        summary.startTimeSeconds = (double) captureClip.dawStartSample / juce::jmax(1, captureClip.sampleRate);
        summary.endTimeSeconds = (double) captureClip.dawEndSample / juce::jmax(1, captureClip.sampleRate);
        summary.numChannels = captureClip.numChannels;
        summary.numSamples = captureClip.numCapturedSamples;
        session.clips.push_back(summary);
        session.transportMapPreview.assign(captureClip.transportMap.begin(),
                                           captureClip.transportMap.begin() + juce::jmin((int) captureClip.transportMap.size(), 256));
        session.durationSeconds = (double) captureClip.numCapturedSamples / juce::jmax(1, captureClip.sampleRate);
    }
    else
    {
        session.transportMapPreview.clear();
    }

    session.revision += 1;
    sessionRevision.store(session.revision);
}

void PluginProcessor::captureIncomingAudio(const juce::AudioBuffer<float>& buffer, const juce::AudioPlayHead::PositionInfo* positionInfo)
{
    if (buffer.getNumSamples() <= 0 || captureClip.originalAudio.getNumSamples() <= 0)
        return;

    if (captureClip.numCapturedSamples == 0)
    {
        captureClip.dawStartSample = getPositionSamples(positionInfo);
        captureClip.bpmAtStart = getBpm(positionInfo);
        captureClip.timeSigNumerator = getTimeSigNumerator(positionInfo);
        captureClip.timeSigDenominator = getTimeSigDenominator(positionInfo);
    }

    const int remaining = captureClip.maxSamples - captureClip.numCapturedSamples;
    const int copySamples = juce::jmin(remaining, buffer.getNumSamples());
    if (copySamples <= 0)
    {
        stopCaptureAndAnalyze();
        return;
    }

    for (int channel = 0; channel < captureClip.numChannels; ++channel)
    {
        const int inputChannel = juce::jmin(channel, juce::jmax(0, buffer.getNumChannels() - 1));
        captureClip.originalAudio.copyFrom(channel, captureClip.numCapturedSamples, buffer, inputChannel, 0, copySamples);
        captureClip.renderedAudio.copyFrom(channel, captureClip.numCapturedSamples, buffer, inputChannel, 0, copySamples);
    }

    if ((int) captureClip.transportMap.size() < maxTransportPoints)
    {
        captureClip.transportMap.push_back({
            captureClip.numCapturedSamples,
            getPositionSamples(positionInfo),
            getBpm(positionInfo),
            getTimeSigNumerator(positionInfo),
            getTimeSigDenominator(positionInfo)
        });
    }

    captureClip.numCapturedSamples += copySamples;
    captureClip.dawEndSample = getPositionSamples(positionInfo) + copySamples;
    const int uiSyncIntervalSamples = juce::jmax(256, captureClip.sampleRate / 15);
    if (captureClip.numCapturedSamples - lastCaptureUiSyncSamples >= uiSyncIntervalSamples
        || captureClip.numCapturedSamples == copySamples
        || captureClip.numCapturedSamples >= captureClip.maxSamples)
    {
        lastCaptureUiSyncSamples = captureClip.numCapturedSamples;
        updateSessionFromClipState();
    }
}

void PluginProcessor::renderPlaybackIntoBuffer(juce::AudioBuffer<float>& buffer, const juce::AudioPlayHead::PositionInfo* positionInfo)
{
    if (captureClip.numCapturedSamples <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType renderLock(renderedAudioLock);
    if (! renderLock.isLocked())
        return;

    const std::int64_t hostStartSample = getPositionSamples(positionInfo);
    const std::int64_t clipStartSample = captureClip.dawStartSample;
    const std::int64_t clipEndSample = captureClip.dawStartSample + captureClip.numCapturedSamples;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const std::int64_t hostSample = hostStartSample + sample;
        if (hostSample < clipStartSample || hostSample >= clipEndSample)
            continue;

        const int clipSample = (int) (hostSample - clipStartSample);
        if (! juce::isPositiveAndBelow(clipSample, captureClip.numCapturedSamples))
            continue;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const int clipChannel = juce::jmin(channel, captureClip.renderedAudio.getNumChannels() - 1);
            buffer.setSample(channel, sample, captureClip.renderedAudio.getSample(clipChannel, clipSample));
        }
    }
}

void PluginProcessor::updateLivePitchEstimate(const juce::AudioBuffer<float>& buffer)
{
    const int inputRangeIndex = (int) getParam(apvts, "inputRange");
    const auto inputRangeHz = getInputRangeHz(inputRangeIndex);
    const float referenceHz = (float) referencePitchHz.load();
    const float amount = getParam(apvts, "correctionAmount") * 0.01f;
    const float humanize = getParam(apvts, "humanize") * 0.01f;
    const float retuneNorm = getParam(apvts, "retuneSpeed") * 0.01f;
    const int scaleKey = (int) getParam(apvts, "scaleKey");
    const auto scaleMode = getScaleModeNames()[juce::jlimit(0, getScaleModeNames().size() - 1, (int) getParam(apvts, "scaleMode"))];
    const float retuneMs = juce::jmap(retuneNorm, 0.0f, 1.0f, 8.0f, 180.0f);
    const float hopAlpha = std::exp(-(float) liveAnalysisHopSize / juce::jmax(1.0f, retuneMs * 0.001f * (float) currentSampleRate));

    float blockLevel = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            mono += buffer.getSample(channel, sample);
        mono /= (float) juce::jmax(1, buffer.getNumChannels());

        blockLevel += mono * mono;
        liveAnalysisRing[(size_t) liveAnalysisWritePosition] = mono;
        liveAnalysisWritePosition = (liveAnalysisWritePosition + 1) % liveAnalysisRingSize;
        ++liveAnalysisSinceLastEstimate;

        if (liveAnalysisSinceLastEstimate < liveAnalysisHopSize)
            continue;

        liveAnalysisSinceLastEstimate = 0;
        for (int i = 0; i < liveAnalysisWindowSize; ++i)
        {
            const int ringIndex = (liveAnalysisWritePosition - liveAnalysisWindowSize + i + liveAnalysisRingSize) % liveAnalysisRingSize;
            liveAnalysisScratch[(size_t) i] = liveAnalysisRing[(size_t) ringIndex];
        }

        float confidence = 0.0f;
        const float hz = estimateYinPitchHz(liveAnalysisScratch.data(), liveAnalysisWindowSize, currentSampleRate,
                                            inputRangeHz.first, inputRangeHz.second, confidence);
        const bool voiced = hz >= inputRangeHz.first && hz <= inputRangeHz.second && confidence >= liveMinimumConfidence;

        if (! voiced)
        {
            smoothedCorrectionSemitones *= 0.88f;
            livePitchDetected.store(false);
            continue;
        }

        const float detectedMidi = hzToMidi(hz, referenceHz);
        if (previousDetectedMidi > 1.0f && std::abs(detectedMidi - previousDetectedMidi) < 0.22f)
            ++stablePitchFrames;
        else
            stablePitchFrames = 0;

        previousDetectedMidi = detectedMidi;
        smoothedDetectedMidi = smoothedDetectedMidi <= 0.0f ? detectedMidi
                                                            : juce::jmap(0.22f, smoothedDetectedMidi, detectedMidi);

        const float snappedMidi = snapMidiNote(smoothedDetectedMidi, scaleKey, scaleMode, SnapMode::scale);
        const float sustainedFactor = juce::jlimit(0.0f, 1.0f, (float) stablePitchFrames / 26.0f);
        const float effectiveAmount = amount * (1.0f - 0.82f * humanize * sustainedFactor);
        const float desiredSemitones = (snappedMidi - smoothedDetectedMidi) * effectiveAmount;
        smoothedCorrectionSemitones = hopAlpha * smoothedCorrectionSemitones + (1.0f - hopAlpha) * desiredSemitones;

        liveDetectedMidi.store(smoothedDetectedMidi);
        liveTargetMidi.store(smoothedDetectedMidi + smoothedCorrectionSemitones);
        liveCorrectionCents.store(smoothedCorrectionSemitones * 100.0f);
        livePitchDetected.store(true);
    }

    if (buffer.getNumSamples() > 0)
        liveInputLevel.store(std::sqrt(blockLevel / (float) buffer.getNumSamples()));
}

void PluginProcessor::processLivePitchCorrection(juce::AudioBuffer<float>& buffer)
{
    updateLivePitchEstimate(buffer);

    const float mix = getParam(apvts, "mix") * 0.01f;
    const bool pitched = livePitchDetected.load();
    const float correctionSemitones = smoothedCorrectionSemitones;
    const bool shouldCorrect = pitched && std::abs(correctionSemitones) > 0.025f && mix > 0.001f;
    const float ratio = juce::jlimit(0.75f, 1.35f, std::pow(2.0f, correctionSemitones / 12.0f));
    const float phaseIncrement = 1.0f / juce::jmax(64.0f, liveGrainSizeSamples);

    auto wrapPosition = [this](float position)
    {
        while (position < 0.0f)
            position += (float) liveDelayBufferSize;
        while (position >= (float) liveDelayBufferSize)
            position -= (float) liveDelayBufferSize;
        return position;
    };

    if (! liveGrainsInitialised)
    {
        liveHeadStartA = wrapPosition((float) liveDelayWritePosition - liveBaseDelaySamples - liveGrainSizeSamples);
        liveHeadStartB = wrapPosition((float) liveDelayWritePosition - liveBaseDelaySamples - liveGrainSizeSamples * 1.5f);
        liveGrainsInitialised = true;
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            liveDelayBuffer.setSample(channel, liveDelayWritePosition, buffer.getSample(channel, sample));

        const float windowA = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * livePhaseA);
        const float windowB = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * livePhaseB);
        const float dryRead = wrapPosition((float) liveDelayWritePosition - liveBaseDelaySamples);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float dryAligned = readHermiteInterpolated(liveDelayBuffer, juce::jmin(channel, liveDelayBuffer.getNumChannels() - 1), dryRead);
            if (! shouldCorrect)
            {
                buffer.setSample(channel, sample, buffer.getSample(channel, sample));
                continue;
            }

            const float srcA = wrapPosition(liveHeadStartA + livePhaseA * liveGrainSizeSamples * ratio);
            const float srcB = wrapPosition(liveHeadStartB + livePhaseB * liveGrainSizeSamples * ratio);
            const float wetA = readHermiteInterpolated(liveDelayBuffer, juce::jmin(channel, liveDelayBuffer.getNumChannels() - 1), srcA) * windowA;
            const float wetB = readHermiteInterpolated(liveDelayBuffer, juce::jmin(channel, liveDelayBuffer.getNumChannels() - 1), srcB) * windowB;
            const float wet = (wetA + wetB) / juce::jmax(0.0001f, windowA + windowB);
            buffer.setSample(channel, sample, juce::jmap(mix, dryAligned, wet));
        }

        livePhaseA += phaseIncrement;
        livePhaseB += phaseIncrement;
        if (livePhaseA >= 1.0f)
        {
            livePhaseA -= 1.0f;
            liveHeadStartA = wrapPosition((float) liveDelayWritePosition - liveBaseDelaySamples - liveGrainSizeSamples);
        }
        if (livePhaseB >= 1.0f)
        {
            livePhaseB -= 1.0f;
            liveHeadStartB = wrapPosition((float) liveDelayWritePosition - liveBaseDelaySamples - liveGrainSizeSamples);
        }

        liveDelayWritePosition = (liveDelayWritePosition + 1) % juce::jmax(1, liveDelayBufferSize);
    }
}

bool PluginProcessor::updateNote(const juce::String& noteId, const std::function<void(DetectedNote&)>& updater)
{
    const juce::ScopedLock lock(sessionLock);
    auto it = std::find_if(session.notes.begin(), session.notes.end(), [&](const auto& note) { return note.id == noteId; });
    if (it == session.notes.end())
        return false;

    pushUndoState();
    updater(*it);
    it->manuallyEdited = true;
    session.revision += 1;
    sessionRevision.store(session.revision);
    return true;
}

juce::var PluginProcessor::serialiseSessionToVar(const AnalysisSession& currentSession) const
{
    juce::Array<juce::var> notes;
    for (const auto& note : currentSession.notes)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("id", note.id);
        object->setProperty("algorithm", (int) note.algorithm);
        object->setProperty("separationType", (int) note.separationType);
        object->setProperty("startTime", note.startTime);
        object->setProperty("endTime", note.endTime);
        object->setProperty("originalStartTime", note.originalStartTime);
        object->setProperty("originalEndTime", note.originalEndTime);
        object->setProperty("originalPitch", note.originalPitch);
        object->setProperty("correctedPitch", note.correctedPitch);
        object->setProperty("pitchCenter", note.pitchCenter);
        object->setProperty("pitchOffsetCents", note.pitchOffsetCents);
        object->setProperty("amplitude", note.amplitude);
        object->setProperty("vibratoRate", note.vibratoRate);
        object->setProperty("vibratoDepth", note.vibratoDepth);
        object->setProperty("formantShift", note.formantShift);
        object->setProperty("muted", note.muted);
        object->setProperty("gainDb", note.gainDb);
        object->setProperty("pitchCorrection", note.pitchCorrection);
        object->setProperty("driftCorrection", note.driftCorrection);
        object->setProperty("transitionToNext", note.transitionToNext);
        object->setProperty("sibilantFlag", note.sibilantFlag);
        object->setProperty("sibilantBalance", note.sibilantBalance);
        object->setProperty("fadeIn", note.fadeIn);
        object->setProperty("fadeOut", note.fadeOut);
        object->setProperty("pitchModulation", note.pitchModulation);
        object->setProperty("pitchDrift", note.pitchDrift);
        object->setProperty("attackSpeed", note.attackSpeed);
        object->setProperty("manuallyEdited", note.manuallyEdited);
        object->setProperty("manualPitchOffset", note.manualPitchOffset);

        juce::Array<juce::var> trace;
        for (const auto& point : note.pitchTrace)
        {
            auto* traceObject = new juce::DynamicObject();
            traceObject->setProperty("time", point.time);
            traceObject->setProperty("pitch", point.pitch);
            traceObject->setProperty("amplitude", point.amplitude);
            traceObject->setProperty("cents", point.cents);
            traceObject->setProperty("sibilant", point.sibilant);
            trace.add(juce::var(traceObject));
        }

        object->setProperty("trace", trace);
        notes.add(juce::var(object));
    }

    auto* sessionObject = new juce::DynamicObject();
    sessionObject->setProperty("sourceName", currentSession.sourceName);
    sessionObject->setProperty("durationSeconds", currentSession.durationSeconds);
    sessionObject->setProperty("polyphonicWarning", currentSession.polyphonicWarning);
    sessionObject->setProperty("referencePitchHz", currentSession.referencePitchHz);
    sessionObject->setProperty("transferState", (int) currentSession.transferState);
    sessionObject->setProperty("algorithm", (int) currentSession.algorithm);
    sessionObject->setProperty("notes", notes);
    return juce::var(sessionObject);
}

ssp::pitch::AnalysisSession PluginProcessor::deserialiseSessionFromVar(const juce::var& value) const
{
    AnalysisSession restored;
    if (auto* object = value.getDynamicObject())
    {
        restored.sourceName = object->getProperty("sourceName").toString();
        restored.durationSeconds = (double) object->getProperty("durationSeconds");
        restored.polyphonicWarning = (bool) object->getProperty("polyphonicWarning");
        restored.referencePitchHz = object->hasProperty("referencePitchHz") ? (double) object->getProperty("referencePitchHz") : 440.0;
        restored.transferState = object->hasProperty("transferState") ? (TransferState) (int) object->getProperty("transferState") : TransferState::waiting;
        restored.algorithm = object->hasProperty("algorithm") ? (DetectionAlgorithm) (int) object->getProperty("algorithm") : DetectionAlgorithm::melodic;

        if (auto* noteArray = object->getProperty("notes").getArray())
        {
            for (const auto& noteVar : *noteArray)
            {
                if (auto* noteObject = noteVar.getDynamicObject())
                {
                    DetectedNote note;
                    note.id = noteObject->getProperty("id").toString();
                    note.algorithm = noteObject->hasProperty("algorithm") ? (DetectionAlgorithm) (int) noteObject->getProperty("algorithm") : DetectionAlgorithm::melodic;
                    note.separationType = noteObject->hasProperty("separationType") ? (SeparationType) (int) noteObject->getProperty("separationType") : SeparationType::soft;
                    note.startTime = (double) noteObject->getProperty("startTime");
                    note.endTime = (double) noteObject->getProperty("endTime");
                    note.originalStartTime = (double) noteObject->getProperty("originalStartTime");
                    note.originalEndTime = (double) noteObject->getProperty("originalEndTime");
                    note.originalPitch = (float) noteObject->getProperty("originalPitch");
                    note.correctedPitch = (float) noteObject->getProperty("correctedPitch");
                    note.pitchCenter = (float) noteObject->getProperty("pitchCenter");
                    note.pitchOffsetCents = (float) noteObject->getProperty("pitchOffsetCents");
                    note.amplitude = (float) noteObject->getProperty("amplitude");
                    note.vibratoRate = (float) noteObject->getProperty("vibratoRate");
                    note.vibratoDepth = (float) noteObject->getProperty("vibratoDepth");
                    note.formantShift = (float) noteObject->getProperty("formantShift");
                    note.muted = (bool) noteObject->getProperty("muted");
                    note.gainDb = (float) noteObject->getProperty("gainDb");
                    note.pitchCorrection = noteObject->hasProperty("pitchCorrection") ? (float) noteObject->getProperty("pitchCorrection") : -1.0f;
                    note.driftCorrection = noteObject->hasProperty("driftCorrection") ? (float) noteObject->getProperty("driftCorrection") : -1.0f;
                    note.transitionToNext = (float) noteObject->getProperty("transitionToNext");
                    note.sibilantFlag = (bool) noteObject->getProperty("sibilantFlag");
                    note.sibilantBalance = (float) noteObject->getProperty("sibilantBalance");
                    note.fadeIn = (float) noteObject->getProperty("fadeIn");
                    note.fadeOut = (float) noteObject->getProperty("fadeOut");
                    note.pitchModulation = noteObject->hasProperty("pitchModulation") ? (float) noteObject->getProperty("pitchModulation") : 100.0f;
                    note.pitchDrift = noteObject->hasProperty("pitchDrift") ? (float) noteObject->getProperty("pitchDrift") : 100.0f;
                    note.attackSpeed = noteObject->hasProperty("attackSpeed") ? (float) noteObject->getProperty("attackSpeed") : 100.0f;
                    note.manuallyEdited = (bool) noteObject->getProperty("manuallyEdited");
                    note.manualPitchOffset = noteObject->hasProperty("manualPitchOffset") ? (float) noteObject->getProperty("manualPitchOffset") : 0.0f;

                    if (auto* traceArray = noteObject->getProperty("trace").getArray())
                    {
                        for (const auto& pointVar : *traceArray)
                        {
                            if (auto* pointObject = pointVar.getDynamicObject())
                            {
                                note.pitchTrace.push_back({
                                    (float) pointObject->getProperty("time"),
                                    (float) pointObject->getProperty("pitch"),
                                    (float) pointObject->getProperty("amplitude"),
                                    (float) pointObject->getProperty("cents"),
                                    (bool) pointObject->getProperty("sibilant")
                                });
                            }
                        }
                    }

                    note.correctedPitchTrace = note.pitchTrace;
                    restored.notes.push_back(std::move(note));
                }
            }
        }
    }

    return restored;
}

juce::var PluginProcessor::serialiseCaptureClipToVar() const
{
    if (! hasClipData.load() || captureClip.numCapturedSamples <= 0)
        return {};

    juce::MemoryOutputStream stream;
    stream.writeString(captureClip.id);
    stream.writeString(captureClip.sourceName);
    stream.writeInt(captureClip.sampleRate);
    stream.writeInt(captureClip.numChannels);
    stream.writeInt(captureClip.numCapturedSamples);
    stream.writeInt64(captureClip.dawStartSample);
    stream.writeInt64(captureClip.dawEndSample);
    stream.writeDouble(captureClip.bpmAtStart);
    stream.writeInt(captureClip.timeSigNumerator);
    stream.writeInt(captureClip.timeSigDenominator);
    stream.writeInt((int) captureClip.algorithm);
    stream.writeInt((int) captureClip.transportMap.size());
    for (const auto& point : captureClip.transportMap)
    {
        stream.writeInt64(point.captureSampleIndex);
        stream.writeInt64(point.dawSamplePosition);
        stream.writeDouble(point.bpm);
        stream.writeInt(point.numerator);
        stream.writeInt(point.denominator);
    }

    for (int channel = 0; channel < captureClip.numChannels; ++channel)
        stream.write(captureClip.originalAudio.getReadPointer(channel), (size_t) captureClip.numCapturedSamples * sizeof(float));

    auto* object = new juce::DynamicObject();
    object->setProperty("blob", stream.getMemoryBlock().toBase64Encoding());
    return juce::var(object);
}

void PluginProcessor::deserialiseCaptureClipFromVar(const juce::var& value)
{
    if (auto* object = value.getDynamicObject())
    {
        const auto encoded = object->getProperty("blob").toString();
        if (encoded.isEmpty())
            return;

        juce::MemoryBlock block;
        if (! block.fromBase64Encoding(encoded))
            return;

        juce::MemoryInputStream stream(block, false);
        captureClip = {};
        captureClip.id = stream.readString();
        captureClip.sourceName = stream.readString();
        captureClip.sampleRate = stream.readInt();
        captureClip.numChannels = stream.readInt();
        captureClip.numCapturedSamples = stream.readInt();
        captureClip.maxSamples = captureClip.numCapturedSamples;
        captureClip.dawStartSample = stream.readInt64();
        captureClip.dawEndSample = stream.readInt64();
        captureClip.bpmAtStart = stream.readDouble();
        captureClip.timeSigNumerator = stream.readInt();
        captureClip.timeSigDenominator = stream.readInt();
        captureClip.algorithm = (DetectionAlgorithm) stream.readInt();
        const int mapSize = stream.readInt();
        captureClip.transportMap.clear();
        captureClip.transportMap.reserve((size_t) mapSize);
        for (int i = 0; i < mapSize; ++i)
        {
            captureClip.transportMap.push_back({
                stream.readInt64(),
                stream.readInt64(),
                stream.readDouble(),
                stream.readInt(),
                stream.readInt()
            });
        }

        captureClip.originalAudio.setSize(captureClip.numChannels, captureClip.numCapturedSamples);
        captureClip.renderedAudio.setSize(captureClip.numChannels, captureClip.numCapturedSamples);
        const int bytesPerChannel = captureClip.numCapturedSamples * (int) sizeof(float);
        for (int channel = 0; channel < captureClip.numChannels; ++channel)
            stream.read(captureClip.originalAudio.getWritePointer(channel), bytesPerChannel);
        captureClip.renderedAudio.makeCopyOf(captureClip.originalAudio);
        hasClipData.store(captureClip.numCapturedSamples > 0);
        if (hasClipData.load())
            transferState.store(TransferState::playback);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
