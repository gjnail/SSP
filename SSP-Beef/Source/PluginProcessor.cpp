#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float saturate(float sample, float drive)
{
    const float actualDrive = juce::jmax(1.0f, drive);
    return std::tanh(sample * actualDrive) / std::tanh(actualDrive);
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount",
        "Beef",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
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

juce::String PluginProcessor::getDescription() const
{
    return "One-knob fattening with full-band squash, forward saturation, and aggressive automatic makeup gain.";
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    amountSmoothed.reset(sampleRate, 0.04);
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());
    compEnvelope = 0.0f;
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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numChannels == 0 || numSamples == 0)
        return;

    const float amountTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("amount")->load());
    amountSmoothed.setTargetValue(amountTarget);

    if (amountTarget <= 0.0001f)
        return;

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float amount = amountSmoothed.getNextValue();
        const float shaped = std::pow(amount, 0.68f);

        const float inputDrive = 1.02f + 1.95f * shaped;
        const float parallelBlend = 0.26f + 0.88f * shaped;
        const float thresholdDb = -18.0f - 18.0f * shaped;
        const float ratio = 2.0f + 8.5f * shaped;
        const float attackMs = 16.0f - 12.0f * shaped;
        const float releaseMs = 170.0f - 120.0f * shaped;
        const float finalDrive = 1.08f + 1.10f * shaped;
        const float densityDrive = 1.0f + 0.55f * shaped;

        const float attackCoeff = std::exp(-1.0f / juce::jmax(0.000001f, attackMs * 0.001f * (float) currentSampleRate));
        const float releaseCoeff = std::exp(-1.0f / juce::jmax(0.000001f, releaseMs * 0.001f * (float) currentSampleRate));

        float dry[2] {};
        float enhanced[2] {};
        float detector = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float input = dryBuffer.getReadPointer(channel)[sample];
            dry[channel] = input;
            enhanced[channel] = saturate(input * inputDrive, densityDrive);
            detector = juce::jmax(detector, std::abs(enhanced[channel]));
        }

        if (detector > compEnvelope)
            compEnvelope = attackCoeff * compEnvelope + (1.0f - attackCoeff) * detector;
        else
            compEnvelope = releaseCoeff * compEnvelope + (1.0f - releaseCoeff) * detector;

        const float detectorDb = juce::Decibels::gainToDecibels(compEnvelope + 1.0e-6f, -60.0f);
        const float overshootDb = juce::jmax(0.0f, detectorDb - thresholdDb);
        const float gainReductionDb = overshootDb - (overshootDb / ratio);
        const float compGain = juce::Decibels::decibelsToGain(-gainReductionDb);
        const float makeUpGain = juce::Decibels::decibelsToGain(1.8f + shaped * 4.2f + gainReductionDb * 0.95f);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float compressed = enhanced[channel] * compGain;
            const float squashed = saturate(compressed * makeUpGain, finalDrive);
            const float thickened = enhanced[channel] + parallelBlend * (squashed - enhanced[channel]);
            const float result = saturate(thickened * (1.0f + 0.15f * shaped), 1.0f + 0.24f * shaped);
            buffer.getWritePointer(channel)[sample] = result;
        }
    }

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
