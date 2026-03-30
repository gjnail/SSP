#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MusicNoteUtils.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float analyzerDecay = 0.86f;

float clampFrequency(float frequency)
{
    return juce::jlimit(minFrequency, maxFrequency, frequency);
}

double midiToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, ((double) midiNote - 69.0) / 12.0);
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
        case 1: // Open
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + (i == 1 ? 12 : (i >= 2 ? 7 : 0)), 1.0f - 0.08f * (float) i);
            push(24, 0.38f);
            break;

        case 2: // Wide
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + 12 * i, 1.0f - 0.12f * (float) i);
            push(24, 0.48f);
            push(31, 0.32f);
            break;

        case 3: // Cascade
            push(0, 1.0f);
            for (int i = 0; i < (int) chordIntervals.size(); ++i)
                push(chordIntervals[(size_t) i] + 12, 0.92f - 0.1f * (float) i);
            push(24, 0.56f);
            push(chordIntervals.back() + 24, 0.42f);
            break;

        case 0:
        default: // Tight
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

float getRawParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = apvts.getRawParameterValue(id))
        return param->load();

    return 0.0f;
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

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("rootNote", "Root Note", getRootNoteNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("rootOctave", "Root Octave", getOctaveNames(), 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("chordType", "Chord Type", getChordNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("voicing", "Voicing", getVoicingNames(), 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive",
                                                                 "Excite",
                                                                 juce::NormalisableRange<float>(0.35f, 3.0f, 0.001f, 0.45f),
                                                                 1.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance",
                                                                 "Bloom",
                                                                 juce::NormalisableRange<float>(0.76f, 0.995f, 0.0001f, 0.4f),
                                                                 0.944f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("brightness",
                                                                 "Sparkle",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.64f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("detune",
                                                                 "Detune",
                                                                 juce::NormalisableRange<float>(0.0f, 28.0f, 0.01f),
                                                                 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("stereoWidth",
                                                                 "Width",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputDb",
                                                                 "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
                                                                 0.0f));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    resetResonatorStates();
    rebuildResonatorCache();

    preSpectrumFifo.fill(0.0f);
    postSpectrumFifo.fill(0.0f);
    fftData.fill(0.0f);
    analyzerFrame.pre.fill(-96.0f);
    analyzerFrame.post.fill(-96.0f);
    preSpectrumFifoIndex = 0;
    postSpectrumFifoIndex = 0;
    feedbackMemory = 0.0f;
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();
    return mainOut == juce::AudioChannelSet::stereo() && mainIn == mainOut;
}

void PluginProcessor::rebuildResonatorCache()
{
    VoiceArray voices{};
    std::array<ResonatorCoefficients, maxResonatorVoices> coeffs{};

    const int rootNote = juce::roundToInt(getRawParam(apvts, "rootNote"));
    const int rootOctave = juce::roundToInt(getRawParam(apvts, "rootOctave")) + 1;
    const int chordType = juce::roundToInt(getRawParam(apvts, "chordType"));
    const int voicing = juce::roundToInt(getRawParam(apvts, "voicing"));
    const float resonance = juce::jlimit(0.72f, 0.9975f, getRawParam(apvts, "resonance"));
    const float brightness = juce::jlimit(0.0f, 1.0f, getRawParam(apvts, "brightness"));
    const float detune = juce::jlimit(0.0f, 40.0f, getRawParam(apvts, "detune"));
    const float width = juce::jlimit(0.0f, 1.0f, getRawParam(apvts, "stereoWidth"));

    const int rootMidi = (rootOctave + 1) * 12 + rootNote;
    const double rootFrequency = midiToFrequency(rootMidi);
    auto recipes = applyVoicing(getChordIntervals(chordType), voicing);

    float totalLinearLevel = 0.0f;
    for (int i = 0; i < (int) recipes.size() && i < maxResonatorVoices; ++i)
    {
        const auto& recipe = recipes[(size_t) i];
        auto& voice = voices[(size_t) i];
        auto& coeff = coeffs[(size_t) i];

        voice.active = true;
        voice.midiNote = rootMidi + recipe.semitoneOffset;
        voice.frequency = clampFrequency((float) (rootFrequency * std::pow(2.0, (double) recipe.semitoneOffset / 12.0)));
        while (voice.frequency > 9000.0f)
            voice.frequency *= 0.5f;
        while (voice.frequency < 40.0f)
            voice.frequency *= 2.0f;

        const float voicePan = recipes.size() <= 1
            ? 0.0f
            : juce::jmap((float) i, 0.0f, (float) juce::jmax(1, (int) recipes.size() - 1), -1.0f, 1.0f);

        voice.pan = voicePan * width;
        voice.level = recipe.level * (i == 0 ? 1.05f : 0.92f);

        const float panNorm = voice.pan * 0.5f + 0.5f;
        const float angle = panNorm * juce::MathConstants<float>::halfPi;
        coeff.leftLevel = std::cos(angle) * voice.level;
        coeff.rightLevel = std::sin(angle) * voice.level;

        const float detunePattern = (i % 2 == 0 ? -1.0f : 1.0f) * (0.7f + 0.25f * (float) (i % 3));
        const float stereoDetune = 0.55f + width * 0.75f;
        const float r = juce::jlimit(0.72f, 0.9975f, resonance - 0.007f * (float) i);

        for (int channel = 0; channel < 2; ++channel)
        {
            const float cents = detune * (detunePattern + (channel == 0 ? -stereoDetune : stereoDetune) * 0.45f);
            const float tunedFrequency = clampFrequency(voice.frequency * std::pow(2.0f, cents / 1200.0f));
            const float omega = juce::MathConstants<float>::twoPi * tunedFrequency / (float) juce::jmax(1.0, currentSampleRate);
            coeff.a1[channel] = 2.0f * r * std::cos(omega);
            coeff.a2[channel] = -(r * r);
        }

        coeff.exciteGain = (1.0f - r) * (1.65f + brightness * 0.55f) * voice.level;
        coeff.dampingAlpha = juce::jlimit(0.018f,
                                          0.52f,
                                          juce::jmap(brightness, 0.03f, 0.42f)
                                              + juce::jmap(voice.frequency, 40.0f, 9000.0f, 0.0f, 0.08f));
        coeff.active = true;

        totalLinearLevel += voice.level;
    }

    const float normalise = totalLinearLevel > 0.0f ? 0.95f / std::sqrt(totalLinearLevel) : 1.0f;
    for (auto& coeff : coeffs)
    {
        coeff.leftLevel *= normalise;
        coeff.rightLevel *= normalise;
    }

    const juce::SpinLock::ScopedLockType lock(stateLock);
    voiceCache = voices;
    coefficientCache = coeffs;
}

void PluginProcessor::resetResonatorStates()
{
    for (auto& voiceStates : resonatorStates)
        for (auto& state : voiceStates)
            state = {};
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    rebuildResonatorCache();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numChannels < 2 || numSamples <= 0)
        return;

    VoiceArray voices;
    std::array<ResonatorCoefficients, maxResonatorVoices> coeffs;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        voices = voiceCache;
        coeffs = coefficientCache;
    }

    const float drive = juce::jlimit(0.2f, 4.0f, getDriveAmount());
    const float mix = juce::jlimit(0.0f, 1.0f, getMixAmount());
    const float outputGain = juce::Decibels::decibelsToGain(getOutputGainDb());
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryLeft = left[sample];
        const float dryRight = right[sample];
        const float inputMono = 0.5f * (dryLeft + dryRight);
        pushPreSpectrumSample(inputMono);

        const float excitation = std::tanh(inputMono * drive + feedbackMemory * 0.22f);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        float feedbackAccumulator = 0.0f;

        for (int voiceIndex = 0; voiceIndex < maxResonatorVoices; ++voiceIndex)
        {
            if (!voices[(size_t) voiceIndex].active || !coeffs[(size_t) voiceIndex].active)
                continue;

            for (int channel = 0; channel < 2; ++channel)
            {
                auto& state = resonatorStates[(size_t) voiceIndex][(size_t) channel];
                const auto& coeff = coeffs[(size_t) voiceIndex];

                float y = excitation * coeff.exciteGain
                    + coeff.a1[channel] * state.y1
                    + coeff.a2[channel] * state.y2;

                y = juce::jlimit(-6.0f, 6.0f, y);
                state.y2 = state.y1;
                state.y1 = y;
                state.damped += (y - state.damped) * coeff.dampingAlpha;

                const float resonated = state.damped;
                if (channel == 0)
                    wetLeft += resonated * coeff.leftLevel;
                else
                    wetRight += resonated * coeff.rightLevel;

                feedbackAccumulator += resonated;
            }
        }

        feedbackMemory = juce::jlimit(-1.5f, 1.5f, feedbackMemory * 0.92f + feedbackAccumulator * 0.025f);

        wetLeft = std::tanh(wetLeft * 1.4f);
        wetRight = std::tanh(wetRight * 1.4f);

        const float mixedLeft = (dryLeft + wetLeft * mix) * outputGain;
        const float mixedRight = (dryRight + wetRight * mix) * outputGain;
        left[sample] = mixedLeft;
        right[sample] = mixedRight;
        pushPostSpectrumSample(0.5f * (mixedLeft + mixedRight));
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

juce::String PluginProcessor::getCurrentChordLabel() const
{
    const int root = juce::roundToInt(getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "rootNote"));
    const int octave = juce::roundToInt(getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "rootOctave")) + 1;
    const int chord = juce::roundToInt(getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "chordType"));
    const int voicing = juce::roundToInt(getRawParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "voicing"));

    return getRootNoteNames()[root] + juce::String(octave) + " " + getChordNames()[chord] + " / " + getVoicingNames()[voicing];
}

juce::String PluginProcessor::getCurrentChordNoteSummary() const
{
    juce::StringArray tokens;
    for (const auto& voice : getVoicesSnapshot())
    {
        if (!voice.active)
            continue;

        const auto note = ssp::notes::frequencyToNote(voice.frequency, true);
        tokens.add(note.name + juce::String(note.octave));
    }

    return tokens.joinIntoString("  •  ");
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

void PluginProcessor::performSpectrumAnalysis(const std::array<float, fftSize>& source,
                                              SpectrumArray& destination,
                                              float decayAmount)
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
