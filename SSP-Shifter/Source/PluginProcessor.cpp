#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
constexpr double maxGrainMs = 180.0;
constexpr double minGrainMs = 20.0;
constexpr double maxFrequencyShiftHz = 1200.0;

float getParameterValue(const juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    if (auto* raw = state.getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

float linearInterpolate(const std::vector<float>& buffer, float index) noexcept
{
    if (buffer.empty())
        return 0.0f;

    const int size = (int) buffer.size();
    while (index < 0.0f)
        index += (float) size;

    while (index >= (float) size)
        index -= (float) size;

    const int indexA = (int) index;
    const int indexB = (indexA + 1) % size;
    const float frac = index - (float) indexA;
    return juce::jmap(frac, buffer[(size_t) indexA], buffer[(size_t) indexB]);
}

float mapToneToCutoff(float tonePercent)
{
    const float norm = juce::jlimit(0.0f, 1.0f, tonePercent / 100.0f);
    return juce::jmap(norm * norm, 1200.0f, 18000.0f);
}

float pitchRatioFromSemitones(float semitones)
{
    return std::pow(2.0f, semitones / 12.0f);
}

juce::StringArray makeModeNames()
{
    return { "Pitch", "Frequency" };
}
} // namespace

const juce::StringArray& PluginProcessor::getModeNames()
{
    static const auto names = makeModeNames();
    return names;
}

void PluginProcessor::PitchShifterChannel::prepare(int bufferSize)
{
    delayBuffer.assign((size_t) juce::jmax(256, bufferSize), 0.0f);
    reset();
}

void PluginProcessor::PitchShifterChannel::reset()
{
    std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writePosition = 0;
    phaseA = 0.0f;
    phaseB = 0.5f;
}

float PluginProcessor::PitchShifterChannel::processSample(float inputSample, float pitchRatio, float grainSizeSamples)
{
    if (delayBuffer.empty())
        return inputSample;

    delayBuffer[(size_t) writePosition] = inputSample;

    const float clampedRatio = juce::jlimit(0.25f, 4.0f, pitchRatio);
    const float range = juce::jlimit(48.0f, (float) delayBuffer.size() - 8.0f, grainSizeSamples);
    const float slope = 1.0f - clampedRatio;
    const float phaseIncrement = std::abs(slope) < 0.0001f ? (1.0f / range) : std::abs(slope) / range;
    const float minDelay = 8.0f;

    auto processHead = [this, range, minDelay, slope, phaseIncrement](float& phase)
    {
        const float delaySamples = slope >= 0.0f
            ? (minDelay + phase * range)
            : (minDelay + (1.0f - phase) * range);
        const float readIndex = (float) writePosition - delaySamples;
        const float envelope = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
        const float sample = linearInterpolate(delayBuffer, readIndex);
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;
        return sample * envelope;
    };

    const float wet = processHead(phaseA) + processHead(phaseB);
    writePosition = (writePosition + 1) % (int) delayBuffer.size();
    return wet;
}

void PluginProcessor::FrequencyShifterChannel::reset()
{
    history.fill(0.0f);
    writePosition = 0;
    oscillatorPhase = 0.0;
}

float PluginProcessor::FrequencyShifterChannel::processSample(float inputSample,
                                                              double shiftHz,
                                                              double sampleRate,
                                                              const std::array<float, hilbertTapCount>& hilbertCoefficients)
{
    history[(size_t) writePosition] = inputSample;

    constexpr int delaySamples = hilbertTapCount / 2;
    const int delayedIndex = (writePosition - delaySamples + hilbertTapCount) % hilbertTapCount;
    const float realPart = history[(size_t) delayedIndex];

    float imagPart = 0.0f;
    for (int tap = 0; tap < hilbertTapCount; ++tap)
    {
        const int index = (writePosition - tap + hilbertTapCount) % hilbertTapCount;
        imagPart += history[(size_t) index] * hilbertCoefficients[(size_t) tap];
    }

    const double phaseIncrement = juce::MathConstants<double>::twoPi * shiftHz / juce::jmax(1.0, sampleRate);
    oscillatorPhase = std::fmod(oscillatorPhase + phaseIncrement, juce::MathConstants<double>::twoPi);
    if (oscillatorPhase < 0.0)
        oscillatorPhase += juce::MathConstants<double>::twoPi;

    const float sine = (float) std::sin(oscillatorPhase);
    const float cosine = (float) std::cos(oscillatorPhase);
    const float shifted = realPart * cosine - imagPart * sine;

    writePosition = (writePosition + 1) % hilbertTapCount;
    return shifted;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.addParameterListener("mode", this);

    rebuildHilbertCoefficients();
    resetProcessingState();
}

PluginProcessor::~PluginProcessor()
{
    apvts.removeParameterListener("mode", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", getModeNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pitchSemitones", "Pitch Shift",
                                                                 juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fineTuneCents", "Fine Tune",
                                                                 juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("grainMs", "Texture",
                                                                 juce::NormalisableRange<float>((float) minGrainMs, (float) maxGrainMs, 0.1f), 70.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("freqShiftHz", "Frequency Shift",
                                                                 juce::NormalisableRange<float>((float) -maxFrequencyShiftHz, (float) maxFrequencyShiftHz, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone", "Tone",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 62.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 45.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    const double rampSeconds = 0.04;
    for (auto* smoother : { &pitchSemitoneSmoothed, &fineTuneSmoothed, &grainSizeSmoothed, &frequencyShiftSmoothed,
                            &toneSmoothed, &mixSmoothed, &outputGainSmoothed, &modeBlendSmoothed })
    {
        smoother->reset(sampleRate, rampSeconds);
    }

    const int bufferSize = (int) std::ceil(sampleRate * 0.55) + samplesPerBlock * 2;
    for (auto& shifter : pitchShifters)
        shifter.prepare(bufferSize);

    resetProcessingState();
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    return mainIn == mainOut && (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    pitchSemitoneSmoothed.setTargetValue(getParameterValue(apvts, "pitchSemitones"));
    fineTuneSmoothed.setTargetValue(getParameterValue(apvts, "fineTuneCents"));
    grainSizeSmoothed.setTargetValue(getParameterValue(apvts, "grainMs"));
    frequencyShiftSmoothed.setTargetValue(getParameterValue(apvts, "freqShiftHz"));
    toneSmoothed.setTargetValue(getParameterValue(apvts, "tone"));
    mixSmoothed.setTargetValue(getParameterValue(apvts, "mix") / 100.0f);
    outputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(getParameterValue(apvts, "outputGain")));
    modeBlendSmoothed.setTargetValue((float) getCurrentModeIndex());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float pitchSemitones = pitchSemitoneSmoothed.getNextValue();
        const float fineTuneCents = fineTuneSmoothed.getNextValue();
        const float grainMs = grainSizeSmoothed.getNextValue();
        const float frequencyShiftHz = frequencyShiftSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float outputGain = outputGainSmoothed.getNextValue();
        const float modeBlend = modeBlendSmoothed.getNextValue();
        const float grainSizeSamples = (float) currentSampleRate * juce::jlimit((float) minGrainMs, (float) maxGrainMs, grainMs) * 0.001f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            const float dry = channelData[sample];
            const float pitchShiftForChannel = pitchSemitones + fineTuneCents / 100.0f;
            const float frequencyShiftForChannel = frequencyShiftHz;
            const float pitchWet = std::abs(pitchShiftForChannel) < 0.0001f
                ? dry
                : pitchShifters[(size_t) channel].processSample(dry,
                                                                pitchRatioFromSemitones(pitchShiftForChannel),
                                                                grainSizeSamples);
            const float frequencyWet = std::abs(frequencyShiftForChannel) < 0.01f
                ? dry
                : frequencyShifters[(size_t) channel].processSample(dry,
                                                                    frequencyShiftForChannel,
                                                                    currentSampleRate,
                                                                    hilbertCoefficients);

            const float wet = juce::jmap(modeBlend, pitchWet, frequencyWet);
            const float filteredWet = applyToneFilter(channel, wet, mapToneToCutoff(tone));
            channelData[sample] = (dry * (1.0f - mix) + filteredWet * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

int PluginProcessor::getCurrentModeIndex() const noexcept
{
    return juce::jlimit(0, getModeNames().size() - 1, (int) std::round(getParameterValue(apvts, "mode")));
}

void PluginProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "mode")
        modeBlendSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, newValue));
}

void PluginProcessor::resetProcessingState()
{
    for (auto& shifter : pitchShifters)
        shifter.reset();

    for (auto& shifter : frequencyShifters)
        shifter.reset();

    toneFilterStates = { 0.0f, 0.0f };

    pitchSemitoneSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "pitchSemitones"));
    fineTuneSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "fineTuneCents"));
    grainSizeSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "grainMs"));
    frequencyShiftSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "freqShiftHz"));
    toneSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "tone"));
    mixSmoothed.setCurrentAndTargetValue(getParameterValue(apvts, "mix") / 100.0f);
    outputGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getParameterValue(apvts, "outputGain")));
    modeBlendSmoothed.setCurrentAndTargetValue((float) getCurrentModeIndex());
}

void PluginProcessor::rebuildHilbertCoefficients()
{
    constexpr int tapCount = FrequencyShifterChannel::hilbertTapCount;
    constexpr int centerTap = tapCount / 2;

    for (int tap = 0; tap < tapCount; ++tap)
    {
        const int n = tap - centerTap;
        if (n == 0 || (n % 2) == 0)
        {
            hilbertCoefficients[(size_t) tap] = 0.0f;
            continue;
        }

        const float ideal = 2.0f / (juce::MathConstants<float>::pi * (float) n);
        const float index = (float) tap / (float) (tapCount - 1);
        const float blackman = 0.42f
                             - 0.5f * std::cos(juce::MathConstants<float>::twoPi * index)
                             + 0.08f * std::cos(2.0f * juce::MathConstants<float>::twoPi * index);
        hilbertCoefficients[(size_t) tap] = ideal * blackman;
    }
}

float PluginProcessor::applyToneFilter(int channel, float inputSample, float cutoffHz)
{
    const float alpha = std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) juce::jmax(1.0, currentSampleRate));
    auto& state = toneFilterStates[(size_t) channel];
    state = inputSample + alpha * (state - inputSample);
    return state;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
