#include "PluginProcessor.h"
#include "MusicNoteUtils.h"
#include "PluginEditor.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float analyzerDecay = 0.86f;
constexpr float transientFastMs = 1.8f;
constexpr float transientSlowMs = 42.0f;
constexpr float maxBandFrequency = 14000.0f;

float clampFrequency(float frequency)
{
    return juce::jlimit(minFrequency, maxFrequency, frequency);
}

double midiToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, ((double) midiNote - 69.0) / 12.0);
}

float frequencyToOnePoleCoefficient(float frequency, double sampleRate)
{
    const float clamped = juce::jlimit(20.0f, 20000.0f, frequency);
    return std::exp(-juce::MathConstants<float>::twoPi * clamped / (float) juce::jmax(1.0, sampleRate));
}

float msToCoefficient(float milliseconds, double sampleRate)
{
    return std::exp(-1.0f / juce::jmax(1.0f, 0.001f * milliseconds * (float) sampleRate));
}

float smoothOnePole(float input, float coefficient, float& state)
{
    state = coefficient * state + (1.0f - coefficient) * input;
    return state;
}

float computeDecayCoefficient(float milliseconds, double sampleRate)
{
    return std::exp(-1.0f / juce::jmax(1.0f, 0.001f * milliseconds * (float) sampleRate));
}

void makeBandPassCoefficients(float frequency, float q, double sampleRate,
                              float& b0, float& b1, float& b2, float& a1, float& a2)
{
    const float clampedFrequency = clampFrequency(frequency);
    const float omega = juce::MathConstants<float>::twoPi * clampedFrequency / (float) juce::jmax(1.0, sampleRate);
    const float alpha = std::sin(omega) / (2.0f * juce::jmax(0.35f, q));
    const float cosOmega = std::cos(omega);
    const float a0 = 1.0f + alpha;

    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cosOmega) / a0;
    a2 = (1.0f - alpha) / a0;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return param->getIndex();

    return juce::roundToInt(apvts.getRawParameterValue(id)->load());
}

float getRawParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = apvts.getRawParameterValue(id))
        return param->load();

    return 0.0f;
}

bool getBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return getRawParam(apvts, id) >= 0.5f;
}

void setChoiceParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) index));
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(value ? 1.0f : 0.0f);
}

void setFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(value));
}

std::vector<int> getChordIntervals(int chordIndex)
{
    switch (chordIndex)
    {
        case 1: return {0, 3, 7};
        case 2: return {0, 2, 7};
        case 3: return {0, 5, 7};
        case 4: return {0, 4, 7, 11};
        case 5: return {0, 3, 7, 10};
        case 6: return {0, 4, 7, 10};
        case 7: return {0, 3, 6, 9};
        case 8: return {0, 4, 8};
        case 9: return {0, 7, 12, 19};
        case 0:
        default: return {0, 4, 7};
    }
}

struct VoiceRecipe
{
    int semitoneOffset = 0;
    float level = 1.0f;
};

std::vector<VoiceRecipe> applyVoicing(const std::vector<int>& chordIntervals, int voicingIndex)
{
    std::vector<VoiceRecipe> voices;
    voices.reserve(PluginProcessor::maxResonatorVoices);

    auto push = [&](int semitoneOffset, float level)
    {
        if ((int) voices.size() < PluginProcessor::maxResonatorVoices)
            voices.push_back({ semitoneOffset, level });
    };

    switch (voicingIndex)
    {
        case 1:
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + (i == 1 ? 12 : (i >= 2 ? 7 : 0)), 1.0f - 0.08f * (float) i);
            push(24, 0.38f);
            break;

        case 2:
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + 12 * i, 1.0f - 0.12f * (float) i);
            push(24, 0.48f);
            push(31, 0.32f);
            break;

        case 3:
            push(0, 1.0f);
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + 12, 0.92f - 0.1f * (float) i);
            push(24, 0.56f);
            push(chordIntervals.back() + 24, 0.42f);
            break;

        case 0:
        default:
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i], 1.0f - 0.06f * (float) i);
            push(12, 0.44f);
            break;
    }

    std::sort(voices.begin(), voices.end(), [](const VoiceRecipe& lhs, const VoiceRecipe& rhs)
    {
        return lhs.semitoneOffset < rhs.semitoneOffset;
    });

    return voices;
}

struct FormantSpec
{
    float f1 = 700.0f;
    float f2 = 1200.0f;
    float q1 = 1.2f;
    float q2 = 1.0f;
    float gain1 = 0.95f;
    float gain2 = 0.72f;
};

FormantSpec getFormantSpec(int vowelIndex, float brightness)
{
    FormantSpec spec;

    switch (vowelIndex)
    {
        case 1: spec = {430.0f, 1850.0f, 1.25f, 1.5f, 0.95f, 0.84f}; break;
        case 2: spec = {320.0f, 2480.0f, 1.6f, 1.8f, 0.88f, 0.92f}; break;
        case 3: spec = {520.0f, 960.0f, 1.0f, 1.1f, 1.0f, 0.76f}; break;
        case 4: spec = {360.0f, 740.0f, 1.05f, 1.1f, 0.96f, 0.7f}; break;
        case 5: spec = {1040.0f, 2860.0f, 1.8f, 2.2f, 1.08f, 0.9f}; break;
        case 6: spec = {1650.0f, 4400.0f, 1.55f, 1.95f, 0.82f, 0.95f}; break;
        case 0:
        default: spec = {820.0f, 1280.0f, 1.2f, 1.35f, 1.0f, 0.75f}; break;
    }

    const float brightnessScale = 1.0f + brightness * 0.22f;
    spec.f1 *= brightnessScale;
    spec.f2 *= 1.0f + brightness * 0.34f;
    return spec;
}

float getDivisionInBeats(int divisionIndex)
{
    static constexpr float divisions[] = {4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 1.0f / 3.0f, 1.0f / 6.0f};
    return divisions[(size_t) juce::jlimit(0, (int) std::size(divisions) - 1, divisionIndex)];
}

juce::Array<PluginProcessor::FactoryPreset> makeFactoryPresets()
{
    using P = PluginProcessor::FactoryPreset;

    return {
        P{"Laser Fifth Stack", 7, 2, 9, 2, 5, 3, false, true, 1.7f, 0.982f, 0.9f, 0.86f, 14.0f, 0.9f, 0.82f, 2.4f, 0.72f, 115.0f, 0.28f, 0.82f, -2.2f},
        P{"Glass Major Bloom", 0, 3, 0, 1, 6, 2, false, true, 1.45f, 0.978f, 0.86f, 0.9f, 11.0f, 0.86f, 0.58f, 1.8f, 0.68f, 120.0f, 0.34f, 0.78f, -1.7f},
        P{"Future Riddim Vowel", 2, 3, 4, 3, 1, 3, false, true, 1.55f, 0.98f, 0.84f, 0.94f, 9.0f, 0.84f, 0.74f, 2.1f, 0.7f, 110.0f, 0.36f, 0.8f, -1.8f},
        P{"Wide Colour Chord", 9, 2, 6, 2, 0, 1, false, true, 1.32f, 0.974f, 0.8f, 0.8f, 15.0f, 0.96f, 0.62f, 1.2f, 0.78f, 105.0f, 0.4f, 0.76f, -1.6f},
        P{"Metallic Tearout", 5, 2, 7, 2, 5, 4, false, true, 1.9f, 0.986f, 0.95f, 0.78f, 18.0f, 0.8f, 0.88f, 3.2f, 0.62f, 135.0f, 0.24f, 0.84f, -2.8f},
        P{"Sub Safe Chord Wash", 3, 2, 1, 1, 3, 2, false, true, 1.1f, 0.968f, 0.72f, 0.66f, 7.0f, 0.74f, 0.44f, 1.4f, 0.9f, 95.0f, 0.58f, 0.66f, -1.2f},
        P{"MIDI Prism Lead", 0, 3, 0, 0, 2, 3, true, true, 1.68f, 0.982f, 0.92f, 0.9f, 10.0f, 0.82f, 0.8f, 2.0f, 0.6f, 125.0f, 0.22f, 0.86f, -2.4f},
        P{"Talking Bass Spark", 10, 2, 5, 0, 4, 4, false, true, 1.82f, 0.984f, 0.92f, 0.96f, 12.0f, 0.76f, 0.9f, 3.8f, 0.64f, 130.0f, 0.3f, 0.88f, -2.6f}
    };
}
} // namespace

const juce::StringArray& PluginProcessor::getRootNoteNames()
{
    static const juce::StringArray names{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    return names;
}

const juce::StringArray& PluginProcessor::getOctaveNames()
{
    static const juce::StringArray names{"1", "2", "3", "4", "5", "6"};
    return names;
}

const juce::StringArray& PluginProcessor::getChordNames()
{
    static const juce::StringArray names{"Major", "Minor", "Sus2", "Sus4", "Maj7", "Min7", "Dom7", "Dim7", "Aug", "Fifths"};
    return names;
}

const juce::StringArray& PluginProcessor::getVoicingNames()
{
    static const juce::StringArray names{"Tight", "Open", "Wide", "Cascade"};
    return names;
}

const juce::StringArray& PluginProcessor::getVowelNames()
{
    static const juce::StringArray names{"A", "E", "I", "O", "U", "Laser", "Glass"};
    return names;
}

const juce::StringArray& PluginProcessor::getMotionDivisionNames()
{
    static const juce::StringArray names{"1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T"};
    return names;
}

const juce::Array<PluginProcessor::FactoryPreset>& PluginProcessor::getFactoryPresets()
{
    static const auto presets = makeFactoryPresets();
    return presets;
}

juce::StringArray PluginProcessor::getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : getFactoryPresets())
        names.add(preset.name);
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("rootNote", "Root Note", getRootNoteNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("rootOctave", "Root Octave", getOctaveNames(), 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("chordType", "Chord Type", getChordNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("voicing", "Voicing", getVoicingNames(), 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("vowelMode", "Vowel Mode", getVowelNames(), 5));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("motionDivision", "Motion Division", getMotionDivisionNames(), 3));

    params.push_back(std::make_unique<juce::AudioParameterBool>("midiFollow", "MIDI Root Follow", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("motionSync", "Motion Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("differenceSolo", "Difference Solo", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Excite", juce::NormalisableRange<float>(0.35f, 3.2f, 0.001f, 0.45f), 1.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance", "Bloom", juce::NormalisableRange<float>(0.82f, 0.9986f, 0.0001f, 0.32f), 0.968f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("brightness", "Sparkle", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("formantMix", "Prism", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.68f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("detune", "Detune", juce::NormalisableRange<float>(0.0f, 28.0f, 0.01f), 9.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("stereoWidth", "Width", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.76f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionDepth", "Motion", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.58f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("motionRateHz", "Motion Rate", juce::NormalisableRange<float>(0.08f, 12.0f, 0.001f, 0.42f), 2.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lowProtect", "Sub Protect", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.68f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("crossoverHz", "Protect Crossover", juce::NormalisableRange<float>(40.0f, 320.0f, 0.01f, 0.38f), 110.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("transientPreserve", "Attack Keep", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.34f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.74f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputDb", "Output", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), -1.0f));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    loadFactoryPreset(0);
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    resetResonatorStates();
    rebuildResonatorCache();

    const juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1};
    for (auto& channelFilters : formantFilters)
    {
        for (auto& filter : channelFilters)
        {
            filter.reset();
            filter.prepare(spec);
            filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setCutoffFrequency(1200.0f);
            filter.setResonance(1.1f);
        }
    }

    inputLowState = {0.0f, 0.0f};
    wetLowState = {0.0f, 0.0f};
    transientFastEnv = {0.0f, 0.0f};
    transientSlowEnv = {0.0f, 0.0f};
    preSpectrumFifo.fill(0.0f);
    postSpectrumFifo.fill(0.0f);
    fftData.fill(0.0f);
    analyzerFrame.pre.fill(-96.0f);
    analyzerFrame.post.fill(-96.0f);
    preSpectrumFifoIndex = 0;
    postSpectrumFifoIndex = 0;
    motionPhase = 0.0;
    visualMotionPhase.store(0.0f);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();
    return mainOut == juce::AudioChannelSet::stereo() && mainIn == mainOut;
}

void PluginProcessor::handleMidiInput(const juce::MidiBuffer& midiMessages)
{
    const juce::SpinLock::ScopedLockType lock(stateLock);

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();
        if (message.isNoteOn())
        {
            const int note = message.getNoteNumber();
            if (std::find(heldMidiNotes.begin(), heldMidiNotes.end(), note) == heldMidiNotes.end())
                heldMidiNotes.push_back(note);
        }
        else if (message.isNoteOff() || message.isAllNotesOff() || message.isAllSoundOff())
        {
            if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                heldMidiNotes.clear();
            }
            else
            {
                const int note = message.getNoteNumber();
                heldMidiNotes.erase(std::remove(heldMidiNotes.begin(), heldMidiNotes.end(), note), heldMidiNotes.end());
            }
        }
    }

    std::sort(heldMidiNotes.begin(), heldMidiNotes.end());
}

int PluginProcessor::getDefaultRootMidi() const
{
    const int rootNote = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "rootNote");
    const int rootOctave = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "rootOctave") + 1;
    return (rootOctave + 1) * 12 + rootNote;
}

int PluginProcessor::getEffectiveRootMidi(bool* cameFromMidi) const
{
    const bool midiFollow = getBoolParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "midiFollow");
    bool usingMidi = false;
    int effectiveRoot = getDefaultRootMidi();

    if (midiFollow)
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        if (! heldMidiNotes.empty())
        {
            effectiveRoot = heldMidiNotes.front();
            usingMidi = true;
        }
    }

    if (cameFromMidi != nullptr)
        *cameFromMidi = usingMidi;

    return effectiveRoot;
}

float PluginProcessor::getMotionRateHz(bool syncEnabled, int divisionIndex) const
{
    if (! syncEnabled)
        return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "motionRateHz");

    const float bpm = juce::jmax(40.0f, currentHostBpm.load());
    const float beatsPerSecond = bpm / 60.0f;
    return beatsPerSecond / juce::jmax(0.0625f, getDivisionInBeats(divisionIndex));
}

void PluginProcessor::updateHostBpmFromPlayHead() noexcept
{
    if (auto* hostPlayHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (hostPlayHead->getCurrentPosition(info) && info.bpm > 0.0)
            currentHostBpm.store((float) info.bpm);
    }
}

void PluginProcessor::rebuildResonatorCache()
{
    VoiceArray voices{};
    std::array<ResonatorBand, maxResonatorBands> bands{};

    bool usingMidiRoot = false;
    const int effectiveRootMidi = getEffectiveRootMidi(&usingMidiRoot);
    const int chordType = getChoiceIndex(apvts, "chordType");
    const int voicing = getChoiceIndex(apvts, "voicing");
    const float resonance = juce::jlimit(0.82f, 0.9986f, getRawParam(apvts, "resonance"));
    const float brightness = juce::jlimit(0.0f, 1.0f, getRawParam(apvts, "brightness"));
    const float detune = juce::jlimit(0.0f, 40.0f, getRawParam(apvts, "detune"));
    const float width = juce::jlimit(0.0f, 1.0f, getRawParam(apvts, "stereoWidth"));
    const float drive = juce::jlimit(0.35f, 3.2f, getRawParam(apvts, "drive"));
    const float formantMix = juce::jlimit(0.0f, 1.0f, getRawParam(apvts, "formantMix"));

    currentEffectiveRootMidi.store(effectiveRootMidi);
    currentRootFromMidi.store(usingMidiRoot);

    const double rootFrequency = midiToFrequency(effectiveRootMidi);
    auto recipes = applyVoicing(getChordIntervals(chordType), voicing);
    const float resonanceNormalised = juce::jmap(resonance, 0.82f, 0.9986f, 0.0f, 1.0f);
    const float baseQ = 3.0f + resonanceNormalised * 24.0f;
    const float baseDecayMs = 85.0f + resonanceNormalised * 1200.0f;

    float totalLinearLevel = 0.0f;
    int bandIndex = 0;
    for (int i = 0; i < (int) recipes.size() && i < maxResonatorVoices; ++i)
    {
        const auto& recipe = recipes[(size_t) i];
        auto& voice = voices[(size_t) i];

        voice.active = true;
        voice.midiNote = effectiveRootMidi + recipe.semitoneOffset;
        voice.frequency = clampFrequency((float) (rootFrequency * std::pow(2.0, (double) recipe.semitoneOffset / 12.0)));
        while (voice.frequency > 5200.0f)
            voice.frequency *= 0.5f;
        while (voice.frequency < 40.0f)
            voice.frequency *= 2.0f;

        const float voicePan = recipes.size() <= 1
            ? 0.0f
            : juce::jmap((float) i, 0.0f, (float) juce::jmax(1, (int) recipes.size() - 1), -1.0f, 1.0f);

        voice.pan = voicePan * width;
        voice.level = recipe.level * (i == 0 ? 1.08f : 0.92f);

        const float panNorm = voice.pan * 0.5f + 0.5f;
        const float angle = panNorm * juce::MathConstants<float>::halfPi;
        const float leftLevel = std::cos(angle) * voice.level;
        const float rightLevel = std::sin(angle) * voice.level;

        static constexpr float harmonicRatios[] = {1.0f, 2.0f, 3.0f};
        const float harmonicLevels[] =
        {
            1.0f,
            0.24f + brightness * 0.40f,
            0.1f + brightness * 0.26f + formantMix * 0.08f
        };

        for (int harmonic = 0; harmonic < (int) std::size(harmonicRatios) && bandIndex < maxResonatorBands; ++harmonic)
        {
            const float harmonicFrequency = voice.frequency * harmonicRatios[(size_t) harmonic];
            if (harmonicFrequency > maxBandFrequency)
                continue;

            auto& band = bands[(size_t) bandIndex++];
            band.active = true;
            const float harmonicLevel = harmonicLevels[harmonic];
            band.leftLevel = leftLevel * harmonicLevel;
            band.rightLevel = rightLevel * harmonicLevel;
            band.excitationScale = (0.22f + drive * 0.18f + brightness * 0.16f) * (1.0f - 0.12f * (float) harmonic);
            band.decayCoefficient = computeDecayCoefficient(baseDecayMs * (1.0f - 0.09f * (float) harmonic), currentSampleRate);
            band.textureAmount = (0.06f + brightness * 0.14f) * (1.0f - 0.15f * (float) harmonic);

            const float stereoDetune = (0.2f + width * 0.55f) * detune;
            const float harmonicDetune = 0.55f + 0.2f * (float) harmonic + 0.08f * (float) i;
            const float q = baseQ * (1.0f + 0.15f * (float) harmonic);

            for (int channel = 0; channel < 2; ++channel)
            {
                const float detuneSign = channel == 0 ? -1.0f : 1.0f;
                const float cents = stereoDetune * harmonicDetune * detuneSign;
                const float tunedFrequency = clampFrequency(harmonicFrequency * std::pow(2.0f, cents / 1200.0f));
                band.frequency[channel] = tunedFrequency;
                band.phaseIncrement[channel] = juce::MathConstants<float>::twoPi * tunedFrequency / (float) juce::jmax(1.0, currentSampleRate);
                makeBandPassCoefficients(tunedFrequency, q, currentSampleRate,
                                         band.b0[channel], band.b1[channel], band.b2[channel], band.a1[channel], band.a2[channel]);
            }

            totalLinearLevel += harmonicLevel * voice.level;
        }
    }

    const float normalise = totalLinearLevel > 0.0f ? 0.88f / std::sqrt(totalLinearLevel) : 1.0f;
    for (auto& band : bands)
    {
        band.leftLevel *= normalise;
        band.rightLevel *= normalise;
    }

    const juce::SpinLock::ScopedLockType lock(stateLock);
    voiceCache = voices;
    bandCache = bands;
}

void PluginProcessor::resetResonatorStates()
{
    for (auto& bandStates : resonatorStates)
        for (auto& state : bandStates)
            state = {};

    for (auto& channelFilters : formantFilters)
        for (auto& filter : channelFilters)
            filter.reset();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    handleMidiInput(midiMessages);
    updateHostBpmFromPlayHead();
    rebuildResonatorCache();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numChannels < 2 || numSamples <= 0)
        return;

    std::array<ResonatorBand, maxResonatorBands> bands;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        bands = bandCache;
    }

    const float drive = juce::jlimit(0.2f, 4.0f, getDriveAmount());
    const float mix = juce::jlimit(0.0f, 1.0f, getMixAmount());
    const float outputGain = juce::Decibels::decibelsToGain(getOutputGainDb());
    const float resonanceAmount = juce::jlimit(0.82f, 0.9986f, getResonanceAmount());
    const float resonanceNormalised = juce::jmap(resonanceAmount, 0.82f, 0.9986f, 0.0f, 1.0f);
    const float lowProtect = juce::jlimit(0.0f, 1.0f, getLowProtectAmount());
    const float crossoverHz = juce::jlimit(40.0f, 320.0f, getLowProtectCrossoverHz());
    const float transientPreserve = juce::jlimit(0.0f, 1.0f, getTransientPreserveAmount());
    const float formantMix = juce::jlimit(0.0f, 1.0f, getFormantMixAmount());
    const float brightness = juce::jlimit(0.0f, 1.0f, getBrightnessAmount());
    const float motionDepth = juce::jlimit(0.0f, 1.0f, getMotionDepthAmount());
    const bool differenceSolo = isDifferenceSoloEnabled();
    const int vowelIndex = getChoiceIndex(apvts, "vowelMode");
    const bool motionSync = getBoolParam(apvts, "motionSync");
    const int motionDivision = getChoiceIndex(apvts, "motionDivision");
    const float motionRateHz = getMotionRateHz(motionSync, motionDivision);
    const float inputLowCoeff = frequencyToOnePoleCoefficient(crossoverHz, currentSampleRate);
    const float wetLowCoeff = frequencyToOnePoleCoefficient(crossoverHz * (0.9f + brightness * 0.25f), currentSampleRate);
    const float fastCoeff = msToCoefficient(transientFastMs, currentSampleRate);
    const float slowCoeff = msToCoefficient(transientSlowMs, currentSampleRate);
    const float phaseIncrement = juce::MathConstants<float>::twoPi * motionRateHz / (float) juce::jmax(1.0, currentSampleRate);
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    currentMotionRateHz.store(motionRateHz);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        motionPhase += phaseIncrement;
        if (motionPhase > juce::MathConstants<double>::twoPi)
            motionPhase -= juce::MathConstants<double>::twoPi;

        const float lfo = std::sin((float) motionPhase);
        const float sideLfo = std::sin((float) motionPhase + juce::MathConstants<float>::halfPi);
        visualMotionPhase.store((float) motionPhase / juce::MathConstants<float>::twoPi);

        const float dryLeft = left[sample];
        const float dryRight = right[sample];
        pushPreSpectrumSample(0.5f * (dryLeft + dryRight));

        const float lowLeft = smoothOnePole(dryLeft, inputLowCoeff, inputLowState[0]);
        const float lowRight = smoothOnePole(dryRight, inputLowCoeff, inputLowState[1]);
        const float highLeft = dryLeft - lowLeft;
        const float highRight = dryRight - lowRight;
        const float inputMono = 0.5f * (dryLeft + dryRight);
        const float highMono = 0.5f * (highLeft + highRight);

        transientFastEnv[0] = fastCoeff * transientFastEnv[0] + (1.0f - fastCoeff) * std::abs(highLeft);
        transientFastEnv[1] = fastCoeff * transientFastEnv[1] + (1.0f - fastCoeff) * std::abs(highRight);
        transientSlowEnv[0] = slowCoeff * transientSlowEnv[0] + (1.0f - slowCoeff) * std::abs(highLeft);
        transientSlowEnv[1] = slowCoeff * transientSlowEnv[1] + (1.0f - slowCoeff) * std::abs(highRight);

        const float transientAmount = juce::jlimit(0.0f, 1.0f,
                                                   juce::jmax(transientFastEnv[0] - transientSlowEnv[0],
                                                              transientFastEnv[1] - transientSlowEnv[1]) * 7.5f);

        const float transientPush = 1.0f + transientAmount * (0.42f + drive * 0.08f);
        const float excitationLeft = std::tanh((highLeft * (1.08f + drive * 0.45f) + inputMono * 0.24f) * transientPush);
        const float excitationRight = std::tanh((highRight * (1.08f + drive * 0.45f) + inputMono * 0.24f) * transientPush);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;

        for (int bandIndex = 0; bandIndex < maxResonatorBands; ++bandIndex)
        {
            if (! bands[(size_t) bandIndex].active)
                continue;

            for (int channel = 0; channel < 2; ++channel)
            {
                auto& state = resonatorStates[(size_t) bandIndex][(size_t) channel];
                const auto& band = bands[(size_t) bandIndex];
                const float excitation = channel == 0 ? excitationLeft : excitationRight;
                const float filtered = band.b0[channel] * excitation
                    + band.b1[channel] * state.x1
                    + band.b2[channel] * state.x2
                    - band.a1[channel] * state.y1
                    - band.a2[channel] * state.y2;

                state.x2 = state.x1;
                state.x1 = excitation;
                state.y2 = state.y1;
                state.y1 = filtered;
                state.envelope = juce::jmax(std::abs(filtered) * band.excitationScale,
                                            state.envelope * band.decayCoefficient);
                state.phase += band.phaseIncrement[channel];
                if (state.phase > juce::MathConstants<float>::twoPi)
                    state.phase -= juce::MathConstants<float>::twoPi;

                const float oscillator = std::sin(state.phase)
                    + std::sin(state.phase * 2.0f) * (0.05f + brightness * 0.08f);
                const float resonant = oscillator * state.envelope
                    + filtered * band.textureAmount
                    + highMono * 0.018f * (0.3f + brightness * 0.7f);

                if (channel == 0)
                    wetLeft += resonant * band.leftLevel;
                else
                    wetRight += resonant * band.rightLevel;
            }
        }

        wetLeft = std::tanh(wetLeft * (1.08f + drive * 0.16f + brightness * 0.08f));
        wetRight = std::tanh(wetRight * (1.08f + drive * 0.16f + brightness * 0.08f));

        const float motionShiftSemitones = motionDepth * lfo * 8.0f;
        const float motionScale = std::pow(2.0f, motionShiftSemitones / 12.0f);
        const auto formant = getFormantSpec(vowelIndex, brightness);

        for (int channel = 0; channel < 2; ++channel)
        {
            formantFilters[(size_t) channel][0].setCutoffFrequency(clampFrequency(formant.f1 * motionScale));
            formantFilters[(size_t) channel][1].setCutoffFrequency(clampFrequency(formant.f2 * motionScale * (1.0f + motionDepth * 0.12f * sideLfo)));
            formantFilters[(size_t) channel][0].setResonance(formant.q1 + motionDepth * 0.25f);
            formantFilters[(size_t) channel][1].setResonance(formant.q2 + motionDepth * 0.35f);
        }

        const float shapedLeft = formantFilters[0][0].processSample(0, wetLeft) * formant.gain1
            + formantFilters[0][1].processSample(0, wetLeft) * formant.gain2;
        const float shapedRight = formantFilters[1][0].processSample(0, wetRight) * formant.gain1
            + formantFilters[1][1].processSample(0, wetRight) * formant.gain2;
        wetLeft = juce::jmap(formantMix, wetLeft, shapedLeft * 1.18f);
        wetRight = juce::jmap(formantMix, wetRight, shapedRight * 1.18f);

        const float wetLowL = smoothOnePole(wetLeft, wetLowCoeff, wetLowState[0]);
        const float wetLowR = smoothOnePole(wetRight, wetLowCoeff, wetLowState[1]);
        wetLeft = (wetLeft - wetLowL) + wetLowL * (1.0f - lowProtect);
        wetRight = (wetRight - wetLowR) + wetLowR * (1.0f - lowProtect);

        float wetMid = 0.5f * (wetLeft + wetRight);
        float wetSide = 0.5f * (wetLeft - wetRight);
        wetSide *= 1.0f + motionDepth * 0.38f * sideLfo;
        wetLeft = wetMid + wetSide;
        wetRight = wetMid - wetSide;

        const float wetGain = 0.82f + drive * 0.16f + resonanceNormalised * 0.95f + formantMix * 0.2f;
        wetLeft *= wetGain;
        wetRight *= wetGain;

        const float motionGain = 1.0f + motionDepth * (0.24f * lfo + 0.1f * sideLfo);
        const float transientWetScale = 1.0f - transientAmount * transientPreserve * 0.16f;
        const float transientDryBoost = 1.0f + transientAmount * transientPreserve * 0.34f;
        const float resonantLayerLeft = wetLeft * mix * transientWetScale * motionGain;
        const float resonantLayerRight = wetRight * mix * transientWetScale * motionGain;
        const float mixedLeft = (dryLeft * transientDryBoost + resonantLayerLeft) * outputGain;
        const float mixedRight = (dryRight * transientDryBoost + resonantLayerRight) * outputGain;
        left[sample] = differenceSolo ? resonantLayerLeft * outputGain : mixedLeft;
        right[sample] = differenceSolo ? resonantLayerRight * outputGain : mixedRight;
        pushPostSpectrumSample(0.5f * (left[sample] + right[sample]));
    }

    for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
}

PluginProcessor::VoiceArray PluginProcessor::getVoicesSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return voiceCache;
}

PluginProcessor::AnalyzerFrame PluginProcessor::getAnalyzerFrameCopy() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return analyzerFrame;
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow(index, presets.size()))
        return false;

    const auto& preset = presets.getReference(index);
    setChoiceParam(apvts, "rootNote", preset.rootNote);
    setChoiceParam(apvts, "rootOctave", preset.rootOctave);
    setChoiceParam(apvts, "chordType", preset.chordType);
    setChoiceParam(apvts, "voicing", preset.voicing);
    setChoiceParam(apvts, "vowelMode", preset.vowelMode);
    setChoiceParam(apvts, "motionDivision", preset.motionDivision);
    setBoolParam(apvts, "midiFollow", preset.midiFollow);
    setBoolParam(apvts, "motionSync", preset.motionSync);
    setFloatParam(apvts, "drive", preset.drive);
    setFloatParam(apvts, "resonance", preset.resonance);
    setFloatParam(apvts, "brightness", preset.brightness);
    setFloatParam(apvts, "formantMix", preset.formantMix);
    setFloatParam(apvts, "detune", preset.detune);
    setFloatParam(apvts, "stereoWidth", preset.stereoWidth);
    setFloatParam(apvts, "motionDepth", preset.motionDepth);
    setFloatParam(apvts, "motionRateHz", preset.motionRateHz);
    setFloatParam(apvts, "lowProtect", preset.lowProtect);
    setFloatParam(apvts, "crossoverHz", preset.crossoverHz);
    setFloatParam(apvts, "transientPreserve", preset.transientPreserve);
    setFloatParam(apvts, "mix", preset.mix);
    setFloatParam(apvts, "outputDb", preset.outputDb);
    currentFactoryPresetIndex.store(index);
    rebuildResonatorCache();
    return true;
}

bool PluginProcessor::stepFactoryPreset(int delta)
{
    const auto& presets = getFactoryPresets();
    if (presets.isEmpty())
        return false;

    int current = currentFactoryPresetIndex.load();
    if (! juce::isPositiveAndBelow(current, presets.size()))
        current = 0;
    else
        current = (current + delta + presets.size()) % presets.size();

    return loadFactoryPreset(current);
}

juce::String PluginProcessor::getCurrentFactoryPresetName() const
{
    const int index = currentFactoryPresetIndex.load();
    const auto& presets = getFactoryPresets();
    if (juce::isPositiveAndBelow(index, presets.size()))
        return presets.getReference(index).name;
    return "Manual";
}

juce::String PluginProcessor::getCurrentChordLabel() const
{
    const int rootMidi = currentEffectiveRootMidi.load();
    const auto note = ssp::notes::frequencyToNote(midiToFrequency(rootMidi), true);
    const int chord = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "chordType");
    const int voicing = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "voicing");
    juce::String label = note.name + juce::String(note.octave) + " " + getChordNames()[chord] + " / " + getVoicingNames()[voicing];
    if (currentRootFromMidi.load())
        label = "MIDI " + label;
    return label;
}

juce::String PluginProcessor::getCurrentChordNoteSummary() const
{
    juce::StringArray tokens;
    for (const auto& voice : getVoicesSnapshot())
    {
        if (! voice.active)
            continue;

        const auto note = ssp::notes::frequencyToNote(voice.frequency, true);
        tokens.add(note.name + juce::String(note.octave));
    }

    return tokens.joinIntoString(" | ");
}

juce::String PluginProcessor::getCurrentMotionStatus() const
{
    const bool sync = getBoolParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "motionSync");
    const int division = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "motionDivision");
    if (sync)
        return "Motion " + getMotionDivisionNames()[division] + " @ " + juce::String(currentHostBpm.load(), 1) + " BPM";

    return "Motion " + juce::String(currentMotionRateHz.load(), 2) + " Hz";
}

juce::String PluginProcessor::getCurrentProtectionStatus() const
{
    return "Sub Protect " + juce::String((int) std::round(getLowProtectAmount() * 100.0f)) + "% @ "
        + juce::String((int) std::round(getLowProtectCrossoverHz())) + " Hz  |  Attack Keep "
        + juce::String((int) std::round(getTransientPreserveAmount() * 100.0f)) + "%";
}

juce::String PluginProcessor::getCurrentSourceStatus() const
{
    if (getBoolParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "midiFollow"))
    {
        if (currentRootFromMidi.load())
            return "Source: MIDI root is driving the chord";
        return "Source: MIDI armed, waiting for notes";
    }

    return "Source: Manual chord controls";
}

juce::String PluginProcessor::getCurrentVowelName() const
{
    return getVowelNames()[getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "vowelMode")];
}

juce::String PluginProcessor::getCurrentMonitorStatus() const
{
    return isDifferenceSoloEnabled() ? "Monitor: DIFF soloing the resonant layer"
                                     : "Monitor: Full mix";
}

float PluginProcessor::getDriveAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "drive");
}

float PluginProcessor::getResonanceAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "resonance");
}

float PluginProcessor::getBrightnessAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "brightness");
}

float PluginProcessor::getDetuneAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "detune");
}

float PluginProcessor::getWidthAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "stereoWidth");
}

float PluginProcessor::getMixAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "mix");
}

float PluginProcessor::getOutputGainDb() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "outputDb");
}

float PluginProcessor::getFormantMixAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "formantMix");
}

float PluginProcessor::getMotionDepthAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "motionDepth");
}

float PluginProcessor::getLowProtectAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "lowProtect");
}

float PluginProcessor::getLowProtectCrossoverHz() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "crossoverHz");
}

float PluginProcessor::getTransientPreserveAmount() const
{
    return getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "transientPreserve");
}

bool PluginProcessor::isDifferenceSoloEnabled() const
{
    return getBoolParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "differenceSolo");
}

void PluginProcessor::setDifferenceSoloEnabled(bool shouldBeEnabled)
{
    setBoolParam(apvts, "differenceSolo", shouldBeEnabled);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }

    rebuildResonatorCache();
    resetResonatorStates();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::pushPreSpectrumSample(float sample) noexcept
{
    preSpectrumFifo[(size_t) preSpectrumFifoIndex++] = sample;
    if (preSpectrumFifoIndex >= fftSize)
    {
        performSpectrumAnalysis(preSpectrumFifo, analyzerFrame.pre, analyzerDecay);
        preSpectrumFifoIndex = 0;
    }
}

void PluginProcessor::pushPostSpectrumSample(float sample) noexcept
{
    postSpectrumFifo[(size_t) postSpectrumFifoIndex++] = sample;
    if (postSpectrumFifoIndex >= fftSize)
    {
        performSpectrumAnalysis(postSpectrumFifo, analyzerFrame.post, analyzerDecay);
        postSpectrumFifoIndex = 0;
    }
}

void PluginProcessor::performSpectrumAnalysis(const std::array<float, fftSize>& source, SpectrumArray& destination, float decayAmount)
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::copy(source.begin(), source.end(), fftData.begin());

    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    SpectrumArray latestSpectrum;
    latestSpectrum.fill(-96.0f);

    for (int i = 1; i < fftSize / 2; ++i)
    {
        const float level = juce::jmax(fftData[(size_t) i] / (float) fftSize, 1.0e-5f);
        latestSpectrum[(size_t) i] = juce::Decibels::gainToDecibels(level, -96.0f);
    }

    const juce::SpinLock::ScopedLockType lock(stateLock);
    for (size_t i = 0; i < destination.size(); ++i)
        destination[i] = juce::jmax(latestSpectrum[i], juce::jmap(decayAmount, -96.0f, destination[i]));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
