#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float envelopeCoeff(double sampleRate, double timeMs) noexcept
{
    return std::exp(-1.0f / (float) (0.001 * timeMs * sampleRate));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack",
                                                                 "Attack",
                                                                 juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain",
                                                                 "Sustain",
                                                                 juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                 10.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("clip",
                                                                 "Clip",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("output",
                                                                 "Output",
                                                                 juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 100.0f));

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

float PluginProcessor::getCurrentAttackActivity() const noexcept
{
    return currentAttackActivity.load();
}

float PluginProcessor::getCurrentSustainActivity() const noexcept
{
    return currentSustainActivity.load();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    fastEnvelope = {0.0f, 0.0f};
    slowEnvelope = {0.0f, 0.0f};
    currentAttackActivity.store(0.0f);
    currentSustainActivity.store(0.0f);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numSamples == 0 || numChannels == 0)
        return;

    const float attackAmount = apvts.getRawParameterValue("attack")->load() / 100.0f;
    const float sustainAmount = apvts.getRawParameterValue("sustain")->load() / 100.0f;
    const float clipAmount = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("clip")->load() / 100.0f);
    const float outputGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());
    const float wetMix = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load() / 100.0f);
    const float dryMix = 1.0f - wetMix;

    const float fastCoeff = envelopeCoeff(currentSampleRate, 2.2);
    const float slowCoeff = envelopeCoeff(currentSampleRate, 42.0);
    const float clipDrive = 1.0f + clipAmount * 7.0f;
    const float clipNormalise = std::tanh(clipDrive);

    float attackPeak = 0.0f;
    float sustainPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            const float inputLevel = std::abs(dry);

            fastEnvelope[(size_t) channel] = inputLevel + fastCoeff * (fastEnvelope[(size_t) channel] - inputLevel);
            slowEnvelope[(size_t) channel] = inputLevel + slowCoeff * (slowEnvelope[(size_t) channel] - inputLevel);

            const float attackDetector = juce::jmax(0.0f, fastEnvelope[(size_t) channel] - slowEnvelope[(size_t) channel]);
            const float sustainDetector = juce::jmax(0.0f, slowEnvelope[(size_t) channel] - fastEnvelope[(size_t) channel]);

            attackPeak = juce::jmax(attackPeak, attackDetector);
            sustainPeak = juce::jmax(sustainPeak, sustainDetector);

            const float shaperDb = (attackAmount * attackDetector * 30.0f) + (sustainAmount * sustainDetector * 24.0f);
            float wet = dry * juce::Decibels::decibelsToGain(shaperDb);

            if (clipAmount > 0.0f)
            {
                const float clipped = std::tanh(wet * clipDrive) / clipNormalise;
                wet = juce::jmap(clipAmount, wet, clipped);
            }

            wet *= outputGain;
            buffer.setSample(channel, sample, dry * dryMix + wet * wetMix);
        }
    }

    currentAttackActivity.store(juce::jlimit(0.0f, 1.0f, attackPeak * 4.5f));
    currentSustainActivity.store(juce::jlimit(0.0f, 1.0f, sustainPeak * 4.5f));

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
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
