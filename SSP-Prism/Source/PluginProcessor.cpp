#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.32f;
constexpr float meterRelease = 0.10f;
constexpr int coefficientUpdateInterval = 12;

struct ChordDefinition
{
    std::array<int, 5> intervals;
    int count = 3;
};

const std::array<ChordDefinition, 8>& getChordDefinitions()
{
    static const std::array<ChordDefinition, 8> chords {{
        { { 0, 4, 7, 0, 0 }, 3 },
        { { 0, 3, 7, 0, 0 }, 3 },
        { { 0, 2, 7, 0, 0 }, 3 },
        { { 0, 5, 7, 0, 0 }, 3 },
        { { 0, 4, 7, 11, 0 }, 4 },
        { { 0, 3, 7, 10, 0 }, 4 },
        { { 0, 7, 12, 0, 0 }, 3 },
        { { 0, 2, 4, 7, 9 }, 5 }
    }};

    return chords;
}

int getChoiceIndex(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return juce::roundToInt(raw->load());

    return 0;
}

bool getBoolValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load() >= 0.5f;

    return false;
}

float noteNumberToHz(float noteNumber)
{
    return 440.0f * std::pow(2.0f, (noteNumber - 69.0f) / 12.0f);
}

float normalisedToFreeRate(float value, float minimumHz, float maximumHz)
{
    const auto skewed = std::pow(juce::jlimit(0.0f, 1.0f, value), 1.65f);
    return juce::jmap(skewed, 0.0f, 1.0f, minimumHz, maximumHz);
}

float normalisedToSyncedRate(float value, float bpm)
{
    static const std::array<float, 8> beatDivisions { 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };

    const auto index = juce::jlimit(0, (int) beatDivisions.size() - 1,
                                    juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) (beatDivisions.size() - 1)));
    const float secondsPerBeat = 60.0f / juce::jmax(1.0f, bpm);
    const float stepSeconds = secondsPerBeat * beatDivisions[(size_t) index];
    return 1.0f / juce::jmax(0.02f, stepSeconds);
}

float makeTimeCoefficient(float milliseconds, double sampleRate)
{
    const float timeSeconds = juce::jmax(0.00025f, milliseconds * 0.001f);
    return std::exp(-1.0f / (timeSeconds * (float) sampleRate));
}

float shapeLfo(float phase, float shape)
{
    const float sine = std::sin(phase);
    const float triangle = std::asin(sine) * (2.0f / juce::MathConstants<float>::pi);
    const float square = sine >= 0.0f ? 1.0f : -1.0f;

    if (shape < 0.5f)
        return juce::jmap(shape, 0.0f, 0.5f, sine, triangle);

    return juce::jmap(shape, 0.5f, 1.0f, triangle, square);
}

float softClip(float value)
{
    return std::tanh(value);
}
} // namespace

void PluginProcessor::BiquadState::reset() noexcept
{
    s1 = 0.0f;
    s2 = 0.0f;
}

float PluginProcessor::BiquadState::process(float input, const BiquadCoefficients& coefficients) noexcept
{
    const float output = coefficients.b0 * input + s1;
    s1 = coefficients.b1 * input - coefficients.a1 * output + s2;
    s2 = coefficients.b2 * input - coefficients.a2 * output;
    return output;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("rootNote", "Root Note",
                                                                  juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("chord", "Chord",
                                                                  juce::StringArray{ "Major Triad", "Minor Triad", "Sus2", "Sus4", "Major 7", "Minor 7", "Power", "Pentatonic" },
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("octave", "Octave",
                                                                  juce::StringArray{ "0", "1", "2", "3", "4" },
                                                                  2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("spread", "Spread",
                                                                  juce::StringArray{ "Tight", "1 Oct", "2 Oct", "3 Oct" },
                                                                  3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("attackMs", "Attack",
                                                                 juce::NormalisableRange<float>(2.0f, 500.0f, 0.01f, 0.35f),
                                                                 18.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decayMs", "Decay",
                                                                 juce::NormalisableRange<float>(40.0f, 2500.0f, 0.01f, 0.35f),
                                                                 480.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("arpRate", "Arp Rate",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.46f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpPattern", "Arp Pattern",
                                                                  juce::StringArray{ "Up", "Down", "Ping Pong", "Random", "Chord" },
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpEnabled", "Arp Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpSync", "Arp Sync", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("warp", "Warp",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.08f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drift", "Drift",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.28f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("shiftHz", "Shift",
                                                                 juce::NormalisableRange<float>(-300.0f, 300.0f, 0.01f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionStrength", "Motion Strength",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.24f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionRate", "Motion Rate",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.34f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionShape", "Motion Shape",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionUnison", "Motion Unison",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("motionSync", "Motion Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterTilt", "Filter Tilt",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterQ", "Filter Q",
                                                                 juce::NormalisableRange<float>(0.35f, 8.0f, 0.001f, 0.4f),
                                                                 0.95f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lowCutHz", "Low Cut",
                                                                 juce::NormalisableRange<float>(20.0f, 4000.0f, 0.01f, 0.25f),
                                                                 110.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("highCutHz", "High Cut",
                                                                 juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.01f, 0.22f),
                                                                 12000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("depth", "Depth",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.78f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.84f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputDb", "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
                                                                 0.0f));

    return { params.begin(), params.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor() = default;

float PluginProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.load();
}

float PluginProcessor::getResonatorMeterLevel() const noexcept
{
    return resonatorMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeBandPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept
{
    BiquadCoefficients coefficients;

    const float frequency = juce::jlimit(20.0f, (float) sampleRate * 0.46f, frequencyHz);
    const float qValue = juce::jlimit(0.25f, 36.0f, q);
    const float omega = juce::MathConstants<float>::twoPi * frequency / (float) sampleRate;
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0f * qValue);
    const float a0 = 1.0f + alpha;

    coefficients.b0 = alpha / a0;
    coefficients.b1 = 0.0f;
    coefficients.b2 = -alpha / a0;
    coefficients.a1 = -2.0f * cosine / a0;
    coefficients.a2 = (1.0f - alpha) / a0;
    return coefficients;
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeHighPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept
{
    BiquadCoefficients coefficients;

    const float frequency = juce::jlimit(10.0f, (float) sampleRate * 0.46f, frequencyHz);
    const float qValue = juce::jlimit(0.25f, 12.0f, q);
    const float omega = juce::MathConstants<float>::twoPi * frequency / (float) sampleRate;
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0f * qValue);
    const float a0 = 1.0f + alpha;

    coefficients.b0 = (1.0f + cosine) * 0.5f / a0;
    coefficients.b1 = -(1.0f + cosine) / a0;
    coefficients.b2 = (1.0f + cosine) * 0.5f / a0;
    coefficients.a1 = -2.0f * cosine / a0;
    coefficients.a2 = (1.0f - alpha) / a0;
    return coefficients;
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeLowPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept
{
    BiquadCoefficients coefficients;

    const float frequency = juce::jlimit(10.0f, (float) sampleRate * 0.46f, frequencyHz);
    const float qValue = juce::jlimit(0.25f, 12.0f, q);
    const float omega = juce::MathConstants<float>::twoPi * frequency / (float) sampleRate;
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0f * qValue);
    const float a0 = 1.0f + alpha;

    coefficients.b0 = (1.0f - cosine) * 0.5f / a0;
    coefficients.b1 = (1.0f - cosine) / a0;
    coefficients.b2 = (1.0f - cosine) * 0.5f / a0;
    coefficients.a1 = -2.0f * cosine / a0;
    coefficients.a2 = (1.0f - alpha) / a0;
    return coefficients;
}

void PluginProcessor::refreshTempo() noexcept
{
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 1.0)
                hostBpm = (float) *bpm;
    }
}

void PluginProcessor::refreshDiscreteParameters() noexcept
{
    const int newRoot = juce::jlimit(0, 11, getChoiceIndex(apvts, "rootNote"));
    const int newChord = juce::jlimit(0, (int) getChordDefinitions().size() - 1, getChoiceIndex(apvts, "chord"));
    const int newOctave = juce::jlimit(0, 4, getChoiceIndex(apvts, "octave"));
    const int newSpread = juce::jlimit(0, 3, getChoiceIndex(apvts, "spread"));
    const int newArpPattern = juce::jlimit(0, 4, getChoiceIndex(apvts, "arpPattern"));
    const bool newArpEnabled = getBoolValue(apvts, "arpEnabled");
    const bool newArpSync = getBoolValue(apvts, "arpSync");
    const bool newMotionSync = getBoolValue(apvts, "motionSync");

    const bool chordShapeChanged = newRoot != rootNoteIndex
                                   || newChord != chordIndex
                                   || newOctave != octaveIndex
                                   || newSpread != spreadIndex;
    const bool arpModeChanged = newArpPattern != arpPatternIndex || newArpEnabled != arpEnabled || newArpSync != arpSync;
    const bool motionModeChanged = newMotionSync != motionSync;

    rootNoteIndex = newRoot;
    chordIndex = newChord;
    octaveIndex = newOctave;
    spreadIndex = newSpread;
    arpPatternIndex = newArpPattern;
    arpEnabled = newArpEnabled;
    arpSync = newArpSync;
    motionSync = newMotionSync;

    const auto& chord = getChordDefinitions()[(size_t) chordIndex];
    currentChordNoteCount = chord.count;
    const float spreadSemitones = (float) (spreadIndex * 12);

    for (int i = 0; i < currentChordNoteCount; ++i)
    {
        const float spreadOffset = currentChordNoteCount > 1
                                       ? spreadSemitones * ((float) i / (float) (currentChordNoteCount - 1))
                                       : 0.0f;
        const float noteNumber = 36.0f + (float) octaveIndex * 12.0f + (float) rootNoteIndex
                                 + (float) chord.intervals[(size_t) i] + spreadOffset;
        chordFrequencies[(size_t) i] = noteNumberToHz(noteNumber);
    }

    for (int i = currentChordNoteCount; i < maxChordNotes; ++i)
        chordFrequencies[(size_t) i] = 0.0f;

    if (chordShapeChanged)
    {
        arpStepIndex = juce::jlimit(0, juce::jmax(0, currentChordNoteCount - 1), arpStepIndex);
        arpDirection = 1;
        samplesUntilArpStep = 0.0;
    }

    if (chordShapeChanged || arpModeChanged || motionModeChanged)
        updateArpTargets();
}

void PluginProcessor::refreshExciterFilters(float lowCutHz, float highCutHz, float q) noexcept
{
    const float lowCut = juce::jlimit(20.0f, 4000.0f, lowCutHz);
    const float highCut = juce::jlimit(lowCut + 60.0f, juce::jmin(20000.0f, (float) currentSampleRate * 0.46f), highCutHz);

    const auto hp = makeHighPassCoefficients(currentSampleRate, lowCut, q);
    const auto lp = makeLowPassCoefficients(currentSampleRate, highCut, q);

    inputHighPassCoefficients[0] = hp;
    inputHighPassCoefficients[1] = hp;
    inputLowPassCoefficients[0] = lp;
    inputLowPassCoefficients[1] = lp;
}

void PluginProcessor::refreshVoices(float warp, float drift, float shiftHz, float tilt, float depth, float decayMs) noexcept
{
    const int previousVoiceCount = activeVoiceCount;
    const float decayNormalised = juce::jlimit(0.0f, 1.0f, (decayMs - 40.0f) / (2500.0f - 40.0f));
    const float resonatorQ = juce::jmap(decayNormalised, 0.0f, 1.0f, 8.0f, 46.0f);
    const float harmonicExponent = juce::jmap(warp, -1.0f, 1.0f, 0.82f, 1.85f);
    const float depthGain = juce::jmap(depth, 0.0f, 1.0f, 0.55f, 1.85f);

    activeVoiceCount = 0;

    for (int note = 0; note < currentChordNoteCount; ++note)
    {
        const float noteFrequency = chordFrequencies[(size_t) note];

        for (int partial = 0; partial < partialsPerNote && activeVoiceCount < maxVoices; ++partial)
        {
            auto& voice = voiceConfigs[(size_t) activeVoiceCount];
            const float partialNumber = (float) (partial + 1);
            const float partialWeight = std::pow(0.74f, (float) partial);
            const float tiltBias = std::pow(2.0f, tilt * ((float) partial - 1.5f) * 1.15f);
            const float shiftedBase = noteFrequency * std::pow(partialNumber, harmonicExponent)
                                      + shiftHz * (0.60f + 0.35f * (float) partial);

            const float leftPhase = voiceDriftPhases[0][(size_t) activeVoiceCount];
            const float rightPhase = voiceDriftPhases[1][(size_t) activeVoiceCount];
            const float stereoDrift = drift * 0.024f;
            const float leftFreq = shiftedBase * (1.0f + stereoDrift * std::sin(leftPhase));
            const float rightFreq = shiftedBase * (1.0f + stereoDrift * std::sin(rightPhase));
            const float partialQ = juce::jmax(2.2f, resonatorQ * (1.0f - 0.07f * (float) partial));

            voice.noteIndex = note;
            voice.baseGain = partialWeight * tiltBias * depthGain
                             / (juce::jmax(1.0f, std::sqrt((float) juce::jmax(1, currentChordNoteCount)) * 0.82f));
            voice.leftCoefficients = makeBandPassCoefficients(currentSampleRate, leftFreq, partialQ);
            voice.rightCoefficients = makeBandPassCoefficients(currentSampleRate, rightFreq, partialQ);
            voice.leftPhaseDelta = juce::MathConstants<float>::twoPi * leftFreq / (float) currentSampleRate;
            voice.rightPhaseDelta = juce::MathConstants<float>::twoPi * rightFreq / (float) currentSampleRate;

            const float phaseAdvance = juce::MathConstants<float>::twoPi * voiceDriftRates[(size_t) activeVoiceCount]
                                       * (float) coefficientUpdateInterval / (float) currentSampleRate;
            voiceDriftPhases[0][(size_t) activeVoiceCount] = std::fmod(leftPhase + phaseAdvance, juce::MathConstants<float>::twoPi);
            voiceDriftPhases[1][(size_t) activeVoiceCount] = std::fmod(rightPhase + phaseAdvance * 1.13f, juce::MathConstants<float>::twoPi);
            ++activeVoiceCount;
        }
    }

    for (int channel = 0; channel < 2; ++channel)
        for (int voice = activeVoiceCount; voice < previousVoiceCount; ++voice)
            voiceStates[(size_t) channel][(size_t) voice].reset();
}

void PluginProcessor::updateArpTargets() noexcept
{
    if (!arpEnabled || arpPatternIndex == 4 || currentChordNoteCount <= 1)
    {
        for (int i = 0; i < maxChordNotes; ++i)
            noteWeightSmoothers[(size_t) i].setTargetValue(i < currentChordNoteCount ? 1.0f : 0.0f);

        return;
    }

    const int focusedIndex = juce::jlimit(0, currentChordNoteCount - 1, arpStepIndex);

    for (int i = 0; i < maxChordNotes; ++i)
    {
        if (i >= currentChordNoteCount)
        {
            noteWeightSmoothers[(size_t) i].setTargetValue(0.0f);
            continue;
        }

        noteWeightSmoothers[(size_t) i].setTargetValue(i == focusedIndex ? 1.0f : 0.45f);
    }
}

void PluginProcessor::stepArpeggiator() noexcept
{
    if (!arpEnabled || currentChordNoteCount <= 1 || arpPatternIndex == 4)
    {
        updateArpTargets();
        return;
    }

    switch (arpPatternIndex)
    {
        case 0: // Up
            arpStepIndex = (arpStepIndex + 1) % currentChordNoteCount;
            break;

        case 1: // Down
            arpStepIndex = (arpStepIndex - 1 + currentChordNoteCount) % currentChordNoteCount;
            break;

        case 2: // Ping Pong
            arpStepIndex += arpDirection;
            if (arpStepIndex >= currentChordNoteCount)
            {
                arpStepIndex = juce::jmax(0, currentChordNoteCount - 2);
                arpDirection = -1;
            }
            else if (arpStepIndex < 0)
            {
                arpStepIndex = juce::jmin(currentChordNoteCount - 1, 1);
                arpDirection = 1;
            }
            break;

        case 3: // Random
            arpStepIndex = random.nextInt(currentChordNoteCount);
            break;

        default:
            break;
    }

    updateArpTargets();
}

void PluginProcessor::updateMeters(float inputPeak, float resonatorPeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.5f, inputPeak));
    smoothValue(resonatorMeter, juce::jlimit(0.0f, 1.5f, resonatorPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.5f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    hostBpm = 120.0f;
    arpDirection = 1;
    arpStepIndex = 0;
    samplesUntilArpStep = 0.0;

    for (auto& channelStates : voiceStates)
        for (auto& state : channelStates)
            state.reset();

    for (auto& state : inputHighPassStates)
        state.reset();

    for (auto& state : inputLowPassStates)
        state.reset();

    for (auto& noteWeight : noteWeightSmoothers)
    {
        noteWeight.reset(sampleRate, 0.04);
        noteWeight.setCurrentAndTargetValue(0.0f);
    }

    warpSmoothed.reset(sampleRate, 0.03);
    driftSmoothed.reset(sampleRate, 0.05);
    shiftSmoothed.reset(sampleRate, 0.04);
    tiltSmoothed.reset(sampleRate, 0.05);
    depthSmoothed.reset(sampleRate, 0.03);
    lowCutSmoothed.reset(sampleRate, 0.04);
    highCutSmoothed.reset(sampleRate, 0.04);
    filterQSmoothed.reset(sampleRate, 0.04);
    mixSmoothed.reset(sampleRate, 0.03);
    outputSmoothed.reset(sampleRate, 0.03);
    motionStrengthSmoothed.reset(sampleRate, 0.04);
    motionShapeSmoothed.reset(sampleRate, 0.05);

    warpSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("warp")->load());
    driftSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("drift")->load());
    shiftSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("shiftHz")->load());
    tiltSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("filterTilt")->load());
    depthSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("depth")->load());
    lowCutSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("lowCutHz")->load());
    highCutSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("highCutHz")->load());
    filterQSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("filterQ")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    outputSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("outputDb")->load());
    motionStrengthSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("motionStrength")->load());
    motionShapeSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("motionShape")->load());

    excitationEnvelopes = { 0.0f, 0.0f };
    motionPhases = { 0.0f, 0.0f };

    for (int voice = 0; voice < maxVoices; ++voice)
    {
        voiceDriftRates[(size_t) voice] = 0.06f + random.nextFloat() * 0.31f;
        voiceDriftPhases[0][(size_t) voice] = random.nextFloat() * juce::MathConstants<float>::twoPi;
        voiceDriftPhases[1][(size_t) voice] = random.nextFloat() * juce::MathConstants<float>::twoPi;
        oscillatorPhases[0][(size_t) voice] = random.nextFloat() * juce::MathConstants<float>::twoPi;
        oscillatorPhases[1][(size_t) voice] = random.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    refreshDiscreteParameters();
    updateArpTargets();

    for (int i = 0; i < maxChordNotes; ++i)
        noteWeightSmoothers[(size_t) i].setCurrentAndTargetValue(i < currentChordNoteCount
                                                                     ? ((!arpEnabled || arpPatternIndex == 4)
                                                                            ? 1.0f
                                                                            : (i == 0 ? 1.0f : 0.45f))
                                                                     : 0.0f);

    refreshExciterFilters(lowCutSmoothed.getCurrentValue(),
                          highCutSmoothed.getCurrentValue(),
                          filterQSmoothed.getCurrentValue());
    refreshVoices(warpSmoothed.getCurrentValue(),
                  driftSmoothed.getCurrentValue(),
                  shiftSmoothed.getCurrentValue(),
                  tiltSmoothed.getCurrentValue(),
                  depthSmoothed.getCurrentValue(),
                  apvts.getRawParameterValue("decayMs")->load());

    inputMeter.store(0.0f);
    resonatorMeter.store(0.0f);
    outputMeter.store(0.0f);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    refreshTempo();
    refreshDiscreteParameters();

    warpSmoothed.setTargetValue(apvts.getRawParameterValue("warp")->load());
    driftSmoothed.setTargetValue(apvts.getRawParameterValue("drift")->load());
    shiftSmoothed.setTargetValue(apvts.getRawParameterValue("shiftHz")->load());
    tiltSmoothed.setTargetValue(apvts.getRawParameterValue("filterTilt")->load());
    depthSmoothed.setTargetValue(apvts.getRawParameterValue("depth")->load());
    lowCutSmoothed.setTargetValue(apvts.getRawParameterValue("lowCutHz")->load());
    highCutSmoothed.setTargetValue(apvts.getRawParameterValue("highCutHz")->load());
    filterQSmoothed.setTargetValue(apvts.getRawParameterValue("filterQ")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());
    outputSmoothed.setTargetValue(apvts.getRawParameterValue("outputDb")->load());
    motionStrengthSmoothed.setTargetValue(apvts.getRawParameterValue("motionStrength")->load());
    motionShapeSmoothed.setTargetValue(apvts.getRawParameterValue("motionShape")->load());

    const float attackMs = apvts.getRawParameterValue("attackMs")->load();
    const float decayMs = apvts.getRawParameterValue("decayMs")->load();
    const float motionRateParam = apvts.getRawParameterValue("motionRate")->load();
    const float motionUnison = apvts.getRawParameterValue("motionUnison")->load();
    const float arpRateParam = apvts.getRawParameterValue("arpRate")->load();
    const float attackCoeff = makeTimeCoefficient(attackMs, currentSampleRate);
    const float releaseCoeff = makeTimeCoefficient(juce::jmax(attackMs + 12.0f, decayMs), currentSampleRate);
    const float arpRateHz = arpSync ? normalisedToSyncedRate(arpRateParam, hostBpm)
                                    : normalisedToFreeRate(arpRateParam, 0.20f, 12.0f);
    const float motionRateHz = motionSync ? normalisedToSyncedRate(motionRateParam, hostBpm)
                                          : normalisedToFreeRate(motionRateParam, 0.08f, 10.0f);

    float inputPeak = 0.0f;
    float resonatorPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float warp = warpSmoothed.getNextValue();
        const float drift = driftSmoothed.getNextValue();
        const float shift = shiftSmoothed.getNextValue();
        const float tilt = tiltSmoothed.getNextValue();
        const float depth = depthSmoothed.getNextValue();
        const float lowCut = lowCutSmoothed.getNextValue();
        const float highCut = highCutSmoothed.getNextValue();
        const float filterQ = filterQSmoothed.getNextValue();
        const float motionStrength = motionStrengthSmoothed.getNextValue();
        const float motionShape = motionShapeSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float outputGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());

        if (sample == 0 || (sample % coefficientUpdateInterval) == 0)
        {
            refreshExciterFilters(lowCut, highCut, filterQ);
            refreshVoices(warp, drift, shift, tilt, depth, decayMs);
        }

        samplesUntilArpStep -= 1.0;
        if (arpEnabled && samplesUntilArpStep <= 0.0)
        {
            stepArpeggiator();
            samplesUntilArpStep += juce::jmax(1.0, currentSampleRate / juce::jmax(0.02f, arpRateHz));
        }

        std::array<float, maxChordNotes> noteWeights{};
        for (int i = 0; i < currentChordNoteCount; ++i)
            noteWeights[(size_t) i] = noteWeightSmoothers[(size_t) i].getNextValue();

        motionPhases[0] = std::fmod(motionPhases[0] + juce::MathConstants<float>::twoPi * motionRateHz / (float) currentSampleRate,
                                    juce::MathConstants<float>::twoPi);
        motionPhases[1] = motionPhases[0] + (1.0f - motionUnison) * juce::MathConstants<float>::pi * 0.75f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            const float detector = std::abs(dry);
            const float previousEnvelope = excitationEnvelopes[(size_t) channel];
            const float envCoeff = detector > previousEnvelope ? attackCoeff : releaseCoeff;
            excitationEnvelopes[(size_t) channel] = envCoeff * previousEnvelope + (1.0f - envCoeff) * detector;
            const float transient = juce::jmax(0.0f, detector - previousEnvelope);
            const float exciterSign = std::copysign(1.0f, dry == 0.0f ? 1.0f : dry);
            const float exciter = softClip(dry * (1.05f + depth * 0.75f));
            const float excitation = dry * 0.60f
                                     + exciter * (0.24f + depth * 0.18f)
                                     + transient * exciterSign * (0.16f + depth * 0.30f);
            const float additiveDrive = juce::jlimit(0.0f, 2.5f,
                                                     excitationEnvelopes[(size_t) channel] * (0.90f + depth * 0.90f)
                                                         + transient * (1.10f + depth * 1.35f));

            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            float harmonicLayer = 0.0f;
            for (int voice = 0; voice < activeVoiceCount; ++voice)
            {
                const auto& config = voiceConfigs[(size_t) voice];
                const float noteGain = config.baseGain * noteWeights[(size_t) config.noteIndex];
                const auto& coeffs = channel == 0 ? config.leftCoefficients : config.rightCoefficients;
                const float resonant = voiceStates[(size_t) channel][(size_t) voice].process(excitation * noteGain * 0.32f, coeffs);
                auto& phase = oscillatorPhases[(size_t) channel][(size_t) voice];
                const float phaseDelta = channel == 0 ? config.leftPhaseDelta : config.rightPhaseDelta;
                phase = std::fmod(phase + phaseDelta, juce::MathConstants<float>::twoPi);
                const float oscillator = std::sin(phase) * additiveDrive * noteGain * (0.65f + depth * 0.95f);
                harmonicLayer += resonant * 0.55f + oscillator;
            }

            harmonicLayer = softClip(harmonicLayer * (0.72f + depth * 0.70f));
            const float lfo = shapeLfo(motionPhases[(size_t) channel], motionShape);
            const float motionGain = juce::jmap(0.5f + 0.5f * lfo,
                                                0.0f,
                                                1.0f,
                                                juce::jmax(0.35f, 1.0f - motionStrength * 0.60f),
                                                1.0f + motionStrength * 0.95f);
            harmonicLayer *= motionGain;
            harmonicLayer = inputHighPassStates[(size_t) channel].process(harmonicLayer, inputHighPassCoefficients[(size_t) channel]);
            harmonicLayer = inputLowPassStates[(size_t) channel].process(harmonicLayer, inputLowPassCoefficients[(size_t) channel]);
            harmonicLayer *= (0.95f + depth * 0.55f);

            resonatorPeak = juce::jmax(resonatorPeak, std::abs(harmonicLayer));

            const float sourceSupport = dry * (0.92f + depth * 0.08f);
            const float enhanced = sourceSupport + harmonicLayer;
            const float blended = dry * (1.0f - mix) + enhanced * mix;
            const float compensated = blended * (1.0f / (1.0f + mix * (0.08f + depth * 0.12f)));
            const float output = softClip(compensated * outputGain * 0.99f);
            buffer.setSample(channel, sample, output);
            outputPeak = juce::jmax(outputPeak, std::abs(output));
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, resonatorPeak, outputPeak);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
