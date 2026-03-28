#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float fixedThresholdDb = -18.0f;
constexpr float meterFloorDb = -60.0f;

float getRawFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* value = apvts.getRawParameterValue(id))
        return value->load();

    return 0.0f;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return choice->getIndex();

    return 0;
}

float mapAttackPosition(float value)
{
    const float normalised = juce::jlimit(0.0f, 1.0f, (value - 1.0f) / 6.0f);
    return juce::jmap(normalised, 0.8f, 0.02f);
}

float mapReleasePosition(float value)
{
    const float normalised = juce::jlimit(0.0f, 1.0f, (value - 1.0f) / 6.0f);
    return juce::jmap(normalised, 1100.0f, 50.0f);
}

float ratioForIndex(int index)
{
    static constexpr float ratios[] = {4.0f, 8.0f, 12.0f, 20.0f, 36.0f};
    return ratios[(size_t) juce::jlimit(0, 4, index)];
}

float meterSaturate(float sample, float drive)
{
    const float shaped = std::tanh(sample * drive);
    return shaped / std::tanh(juce::jmax(1.0f, drive));
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "input", "Input",
        juce::NormalisableRange<float>(0.0f, 36.0f, 0.01f),
        12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output", "Output",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(1.0f, 7.0f, 0.01f),
        3.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(1.0f, 7.0f, 0.01f),
        5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ratio", "Ratio",
        getRatioNames(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "meterMode", "Meter Mode",
        getMeterModeNames(),
        0));

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

const juce::StringArray& PluginProcessor::getRatioNames()
{
    static const juce::StringArray names{"4:1", "8:1", "12:1", "20:1", "ALL"};
    return names;
}

const juce::StringArray& PluginProcessor::getMeterModeNames()
{
    static const juce::StringArray names{"GR", "+8", "+4", "OFF"};
    return names;
}

int PluginProcessor::getCurrentRatioIndex() const
{
    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "ratio");
}

int PluginProcessor::getCurrentMeterModeIndex() const
{
    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "meterMode");
}

float PluginProcessor::getGainReductionMeterDb() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(meterLock);
    return gainReductionMeterDb;
}

float PluginProcessor::getOutputMeterDb() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(meterLock);
    return outputMeterDb;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    envelope = 0.0f;

    inputGainSmoothed.reset(sampleRate, 0.03);
    outputGainSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.reset(sampleRate, 0.03);

    inputGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "input")));
    outputGainSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "output")));
    mixSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "mix") * 0.01f);
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

    const float inputDb = getRawFloatParam(apvts, "input");
    const float outputDb = getRawFloatParam(apvts, "output");
    const float attackMs = mapAttackPosition(getRawFloatParam(apvts, "attack"));
    const float releaseMs = mapReleasePosition(getRawFloatParam(apvts, "release"));
    const float mixTarget = getRawFloatParam(apvts, "mix") * 0.01f;
    const int ratioIndex = getCurrentRatioIndex();
    const bool allButtonsMode = ratioIndex == 4;
    const float ratio = ratioForIndex(ratioIndex);

    inputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(inputDb));
    outputGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(outputDb));
    mixSmoothed.setTargetValue(mixTarget);

    const float effectiveAttackMs = allButtonsMode ? attackMs * 0.42f : attackMs;
    const float effectiveReleaseMs = allButtonsMode ? releaseMs * 0.68f : releaseMs;
    const float thresholdDb = allButtonsMode ? fixedThresholdDb - 4.0f : fixedThresholdDb;
    const float attackCoeff = std::exp(-1.0f / juce::jmax(0.000001f, effectiveAttackMs * 0.001f * (float) currentSampleRate));
    const float releaseCoeff = std::exp(-1.0f / juce::jmax(0.000001f, effectiveReleaseMs * 0.001f * (float) currentSampleRate));
    const float saturationDrive = (1.15f + inputDb * 0.02f) * (allButtonsMode ? 1.32f : 1.0f);

    float maxGainReduction = 0.0f;
    float outputEnergy = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputGain = inputGainSmoothed.getNextValue();
        const float outputGain = outputGainSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        float dry[2] {};
        float wet[2] {};

        for (int channel = 0; channel < numChannels; ++channel)
            dry[channel] = buffer.getReadPointer(channel)[sample];

        const float detector = juce::jmax(std::abs(dry[0]), numChannels > 1 ? std::abs(dry[1]) : 0.0f) * inputGain;
        if (detector > envelope)
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * detector;
        else
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * detector;

        const float levelDb = juce::Decibels::gainToDecibels(envelope + 1.0e-6f, meterFloorDb);
        const float overshootDb = juce::jmax(0.0f, levelDb - thresholdDb);
        const float compressedDb = overshootDb / ratio;
        float gainReductionDb = overshootDb - compressedDb;
        if (allButtonsMode)
            gainReductionDb = gainReductionDb * 1.18f + overshootDb * 0.08f;
        const float compressionGain = juce::Decibels::decibelsToGain(-gainReductionDb);
        maxGainReduction = juce::jmax(maxGainReduction, gainReductionDb);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float driven = dry[channel] * inputGain;
            float compressed = meterSaturate(driven * compressionGain, saturationDrive);
            if (allButtonsMode)
                compressed = meterSaturate(compressed, 1.45f);
            compressed *= outputGain;
            wet[channel] = compressed;

            const float result = dry[channel] + mix * (compressed - dry[channel]);
            buffer.getWritePointer(channel)[sample] = result;
            outputEnergy += result * result;
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(meterLock);
        gainReductionMeterDb = juce::jmax(maxGainReduction, gainReductionMeterDb * 0.9f);

        const float rmsOut = std::sqrt(outputEnergy / juce::jmax(1, numSamples * juce::jmax(1, numChannels)));
        const float outputDbNow = juce::Decibels::gainToDecibels(rmsOut + 1.0e-6f, meterFloorDb);
        if (outputDbNow > outputMeterDb)
            outputMeterDb += 0.5f * (outputDbNow - outputMeterDb);
        else
            outputMeterDb += 0.08f * (outputDbNow - outputMeterDb);
    }
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
