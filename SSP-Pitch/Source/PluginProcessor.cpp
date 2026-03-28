#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using namespace ssp::pitch;

constexpr double maxCaptureSeconds = 180.0;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto percent = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctPitch", "Correct Pitch", percent, 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctDrift", "Correct Drift", percent, 45.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("correctTiming", "Correct Timing", percent, 15.0f));
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

float getParam(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return apvts.getRawParameterValue(id)->load();
}
} // namespace

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SSPPITCHSTATE", createParameterLayout()),
      analysisEngine(*this)
{
    formatManager.registerBasicFormats();

    for (const auto& id : { "correctPitch", "correctDrift", "correctTiming", "vibratoDepth", "vibratoSpeed", "formantShift",
                            "formantPreserve", "snapMode", "scaleKey", "scaleMode" })
        apvts.addParameterListener(id, this);

    generateDemoSession();
}

PluginProcessor::~PluginProcessor()
{
    for (const auto& id : { "correctPitch", "correctDrift", "correctTiming", "vibratoDepth", "vibratoSpeed", "formantShift",
                            "formantPreserve", "snapMode", "scaleKey", "scaleMode" })
        apvts.removeParameterListener(id, this);
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
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
    if (captureActive.load())
        captureIncomingAudio(buffer);

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
    state.setProperty("activeCompareSlot", activeCompareSlot.load(), nullptr);
    state.setProperty("loopEnabled", loopEnabled.load(), nullptr);

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

    const auto sessionText = tree.getProperty("session").toString();
    if (sessionText.isNotEmpty())
        setSession(deserialiseSessionFromVar(juce::JSON::parse(sessionText)));

    triggerAsyncUpdate();
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

void PluginProcessor::startCapture()
{
    capturedMono.clear();
    captureActive.store(true);

    auto current = getSessionCopy();
    current.analyzing = true;
    current.progress = 0.0f;
    current.progressLabel = "Capturing incoming audio";
    setSession(current);
}

void PluginProcessor::stopCaptureAndAnalyze()
{
    captureActive.store(false);
    if (capturedMono.empty())
        return;

    juce::AudioBuffer<float> mono(1, (int) capturedMono.size());
    std::copy(capturedMono.begin(), capturedMono.end(), mono.getWritePointer(0));
    analysisEngine.requestAnalysis(std::move(mono), currentSampleRate, "Captured Input");
}

void PluginProcessor::analyzeAudioFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return;

    juce::AudioBuffer<float> buffer((int) juce::jmax<unsigned int>(1u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read(&buffer, 0, (int) reader->lengthInSamples, 0, true, true);

    juce::AudioBuffer<float> mono(1, buffer.getNumSamples());
    mono.clear();
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        mono.addFrom(0, 0, buffer, channel, 0, buffer.getNumSamples(), 1.0f / (float) buffer.getNumChannels());

    analysisEngine.requestAnalysis(std::move(mono), reader->sampleRate, file.getFileName());
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
                param->beginChangeGesture(), param->setValueNotifyingHost(param->convertTo0to1(value)), param->endChangeGesture();
        };

        setFloat("correctPitch", preset.correctPitch);
        setFloat("correctDrift", preset.correctDrift);
        setFloat("correctTiming", preset.correctTiming);
        setFloat("vibratoDepth", preset.vibratoDepth);
        setFloat("vibratoSpeed", preset.vibratoSpeed);
        setFloat("formantShift", preset.formantShift);
        if (auto* param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("formantPreserve")))
            param->beginChangeGesture(), param->setValueNotifyingHost(preset.formantPreserve ? 1.0f : 0.0f), param->endChangeGesture();
        break;
    }
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
        session = compareSlots[(size_t) slotIndex].session;
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
}

bool PluginProcessor::undoLastEdit()
{
    const juce::ScopedLock lock(sessionLock);
    if (undoStack.empty())
        return false;

    redoStack.push_back(session);
    session = undoStack.back();
    undoStack.pop_back();
    return true;
}

bool PluginProcessor::redoLastEdit()
{
    const juce::ScopedLock lock(sessionLock);
    if (redoStack.empty())
        return false;

    undoStack.push_back(session);
    session = redoStack.back();
    redoStack.pop_back();
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
        note.startTime = juce::jmax(0.0, note.startTime + timeDeltaSeconds);
        note.endTime = juce::jmax(note.startTime + 0.03, note.endTime + timeDeltaSeconds);
        note.correctedPitch += semitoneDelta;
        for (auto& point : note.correctedPitchTrace)
        {
            point.time += (float) timeDeltaSeconds;
            point.pitch += semitoneDelta;
        }
    }))
        rebuildCorrectedNotes();
}

void PluginProcessor::resizeNote(const juce::String& noteId, double startDeltaSeconds, double endDeltaSeconds)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        note.startTime = juce::jmax(0.0, juce::jmin(note.endTime - 0.03, note.startTime + startDeltaSeconds));
        note.endTime = juce::jmax(note.startTime + 0.03, note.endTime + endDeltaSeconds);
    }))
        rebuildCorrectedNotes();
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
    session.notes.insert(noteIt + 1, std::move(right));
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
    session.notes.erase(session.notes.begin() + indexes.back());
}

void PluginProcessor::addDrawnNote(double startTime, double endTime, float midiNote)
{
    const juce::ScopedLock lock(sessionLock);
    pushUndoState();

    DetectedNote note;
    note.id = juce::Uuid().toString();
    note.startTime = startTime;
    note.endTime = endTime;
    note.originalStartTime = startTime;
    note.originalEndTime = endTime;
    note.originalPitch = midiNote;
    note.correctedPitch = midiNote;
    note.amplitude = 0.72f;

    for (int i = 0; i <= 16; ++i)
    {
        const float alpha = (float) i / 16.0f;
        const float time = (float) juce::jmap(alpha, 0.0f, 1.0f, (float) startTime, (float) endTime);
        note.pitchTrace.push_back({ time, midiNote, 0.72f });
        note.correctedPitchTrace.push_back({ time, midiNote, 0.72f });
        note.amplitudeEnvelope.push_back(0.72f);
    }

    session.notes.push_back(std::move(note));
    std::sort(session.notes.begin(), session.notes.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
}

void PluginProcessor::setNoteMuted(const juce::String& noteId, bool shouldMute)
{
    if (updateNote(noteId, [&](DetectedNote& note) { note.muted = shouldMute; }))
        rebuildCorrectedNotes();
}

void PluginProcessor::setNoteGainDb(const juce::String& noteId, float gainDb)
{
    updateNote(noteId, [&](DetectedNote& note) { note.gainDb = gainDb; });
}

void PluginProcessor::setNotePitchCorrection(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note) { note.pitchCorrection = amount; }))
        rebuildCorrectedNotes();
}

void PluginProcessor::setNoteDriftCorrection(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note) { note.driftCorrection = amount; }))
        rebuildCorrectedNotes();
}

void PluginProcessor::setNoteFormantShift(const juce::String& noteId, float semitones)
{
    updateNote(noteId, [&](DetectedNote& note) { note.formantShift = semitones; });
}

void PluginProcessor::setNoteTransition(const juce::String& noteId, float amount)
{
    if (updateNote(noteId, [&](DetectedNote& note) { note.transitionToNext = amount; }))
        rebuildCorrectedNotes();
}

void PluginProcessor::nudgeNotePitchTrace(const juce::String& noteId, double timeSeconds, float midiNote, float radiusSeconds)
{
    if (updateNote(noteId, [&](DetectedNote& note)
    {
        for (auto& point : note.correctedPitchTrace)
        {
            const float distance = std::abs(point.time - (float) timeSeconds);
            if (distance > radiusSeconds)
                continue;

            const float blend = juce::jmap(distance, 0.0f, radiusSeconds, 1.0f, 0.0f);
            point.pitch = juce::jmap(blend, point.pitch, midiNote);
        }
    }))
        rebuildCorrectedNotes();
}

void PluginProcessor::muteNotes(const juce::StringArray& noteIds, bool shouldMute)
{
    for (const auto& id : noteIds)
        setNoteMuted(id, shouldMute);
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    correctionDirty.store(true);
}

void PluginProcessor::handleAsyncUpdate()
{
    rebuildCorrectedNotes();
}

void PluginProcessor::pitchAnalysisUpdated(const AnalysisSession& newSession)
{
    setSession(newSession);
}

void PluginProcessor::pitchAnalysisFinished(const AnalysisSession& newSession)
{
    setSession(newSession);
    const juce::ScopedLock lock(sessionLock);
    compareSlots[(size_t) activeCompareSlot.load()].session = session;
    compareSlots[(size_t) activeCompareSlot.load()].valid = true;
}

void PluginProcessor::generateDemoSession()
{
    juce::AudioBuffer<float> demo(1, (int) (currentSampleRate * 4.5));
    auto* write = demo.getWritePointer(0);
    const std::array<float, 9> melody{ 57.0f, 57.0f, 60.0f, 62.0f, 64.0f, 62.0f, 60.0f, 57.0f, 55.0f };

    for (int sample = 0; sample < demo.getNumSamples(); ++sample)
    {
        const double time = sample / currentSampleRate;
        const int noteIndex = juce::jlimit(0, (int) melody.size() - 1, (int) (time / 0.5));
        const float midi = melody[(size_t) noteIndex] + 0.12f * std::sin((float) (time * 6.0));
        const double hz = 440.0 * std::pow(2.0, (midi - 69.0f) / 12.0f);
        write[sample] = (float) (0.18 * std::sin(juce::MathConstants<double>::twoPi * hz * time));
    }

    analysisEngine.requestAnalysis(std::move(demo), currentSampleRate, "Factory Demo");
}

void PluginProcessor::setSession(const AnalysisSession& newSession)
{
    const juce::ScopedLock lock(sessionLock);
    session = newSession;
    session.revision += 1;
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
    const juce::ScopedLock lock(sessionLock);
    if (session.notes.empty())
        return;

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
        const float notePitchAmount = note.pitchCorrection >= 0.0f ? note.pitchCorrection : globalPitch;
        const float noteDriftAmount = note.driftCorrection >= 0.0f ? note.driftCorrection : globalDrift;
        const float snapped = snapMidiNote(note.originalPitch, scaleKey, scaleMode, snapMode);
        note.correctedPitch = juce::jmap(notePitchAmount * 0.01f, note.originalPitch, snapped);

        const double beatGrid = 0.125;
        const double quantisedStart = std::round(note.originalStartTime / beatGrid) * beatGrid;
        const double timeBlend = globalTiming * 0.01;
        const double startOffset = juce::jmap(timeBlend, note.originalStartTime, quantisedStart) - note.originalStartTime;
        note.startTime = note.originalStartTime + startOffset;
        note.endTime = note.startTime + (note.originalEndTime - note.originalStartTime);

        note.correctedPitchTrace.clear();
        for (const auto& point : note.pitchTrace)
        {
            const float centred = point.pitch - note.originalPitch;
            const float flattened = note.correctedPitch + centred * (1.0f - noteDriftAmount * 0.01f) * vibratoDepthScale;
            const float moved = juce::jmap(notePitchAmount * 0.01f, point.pitch, flattened);
            note.correctedPitchTrace.push_back({
                point.time + (float) startOffset,
                moved,
                point.amplitude
            });
        }
    }
}

void PluginProcessor::captureIncomingAudio(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int maxSamples = (int) std::round(currentSampleRate * maxCaptureSeconds);
    if ((int) capturedMono.size() + numSamples > maxSamples)
        return;

    const int numChannels = juce::jmax(1, buffer.getNumChannels());
    capturedMono.reserve(capturedMono.size() + (size_t) numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mono += buffer.getReadPointer(channel)[sample];
        capturedMono.push_back(mono / (float) numChannels);
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
    return true;
}

juce::var PluginProcessor::serialiseSessionToVar(const AnalysisSession& currentSession) const
{
    juce::Array<juce::var> notes;
    for (const auto& note : currentSession.notes)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("id", note.id);
        object->setProperty("startTime", note.startTime);
        object->setProperty("endTime", note.endTime);
        object->setProperty("originalStartTime", note.originalStartTime);
        object->setProperty("originalEndTime", note.originalEndTime);
        object->setProperty("originalPitch", note.originalPitch);
        object->setProperty("correctedPitch", note.correctedPitch);
        object->setProperty("amplitude", note.amplitude);
        object->setProperty("vibratoRate", note.vibratoRate);
        object->setProperty("vibratoDepth", note.vibratoDepth);
        object->setProperty("formantShift", note.formantShift);
        object->setProperty("muted", note.muted);
        object->setProperty("gainDb", note.gainDb);
        object->setProperty("pitchCorrection", note.pitchCorrection);
        object->setProperty("driftCorrection", note.driftCorrection);
        object->setProperty("transitionToNext", note.transitionToNext);

        juce::Array<juce::var> trace;
        for (const auto& point : note.pitchTrace)
        {
            auto* traceObject = new juce::DynamicObject();
            traceObject->setProperty("time", point.time);
            traceObject->setProperty("pitch", point.pitch);
            traceObject->setProperty("amplitude", point.amplitude);
            trace.add(juce::var(traceObject));
        }

        object->setProperty("trace", trace);
        notes.add(juce::var(object));
    }

    auto* sessionObject = new juce::DynamicObject();
    sessionObject->setProperty("sourceName", currentSession.sourceName);
    sessionObject->setProperty("durationSeconds", currentSession.durationSeconds);
    sessionObject->setProperty("polyphonicWarning", currentSession.polyphonicWarning);
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

        if (auto* noteArray = object->getProperty("notes").getArray())
        {
            for (const auto& noteVar : *noteArray)
            {
                if (auto* noteObject = noteVar.getDynamicObject())
                {
                    DetectedNote note;
                    note.id = noteObject->getProperty("id").toString();
                    note.startTime = (double) noteObject->getProperty("startTime");
                    note.endTime = (double) noteObject->getProperty("endTime");
                    note.originalStartTime = (double) noteObject->getProperty("originalStartTime");
                    note.originalEndTime = (double) noteObject->getProperty("originalEndTime");
                    note.originalPitch = (float) noteObject->getProperty("originalPitch");
                    note.correctedPitch = (float) noteObject->getProperty("correctedPitch");
                    note.amplitude = (float) noteObject->getProperty("amplitude");
                    note.vibratoRate = (float) noteObject->getProperty("vibratoRate");
                    note.vibratoDepth = (float) noteObject->getProperty("vibratoDepth");
                    note.formantShift = (float) noteObject->getProperty("formantShift");
                    note.muted = (bool) noteObject->getProperty("muted");
                    note.gainDb = (float) noteObject->getProperty("gainDb");
                    note.pitchCorrection = noteObject->hasProperty("pitchCorrection") ? (float) noteObject->getProperty("pitchCorrection") : -1.0f;
                    note.driftCorrection = noteObject->hasProperty("driftCorrection") ? (float) noteObject->getProperty("driftCorrection") : -1.0f;
                    note.transitionToNext = (float) noteObject->getProperty("transitionToNext");

                    if (auto* traceArray = noteObject->getProperty("trace").getArray())
                    {
                        for (const auto& pointVar : *traceArray)
                        {
                            if (auto* pointObject = pointVar.getDynamicObject())
                            {
                                note.pitchTrace.push_back({
                                    (float) pointObject->getProperty("time"),
                                    (float) pointObject->getProperty("pitch"),
                                    (float) pointObject->getProperty("amplitude")
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
