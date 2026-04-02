#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WaveshaperTables.h"

namespace
{
float meterDecay(float current, float target) noexcept
{
    return juce::jmax(target, current * 0.92f);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    using namespace ssp::waveshaper;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive",
                                                                 "Drive",
                                                                 juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f),
                                                                 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("table",
                                                                  "Table",
                                                                  getTableNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("frame",
                                                                 "Frame",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 32.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("phase",
                                                                 "Phase",
                                                                 juce::NormalisableRange<float>(-180.0f, 180.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("bias",
                                                                 "Bias",
                                                                 juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("smooth",
                                                                 "Smooth",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 18.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("overflow",
                                                                  "Overflow",
                                                                  getOverflowModeNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterBool>("dcfilter",
                                                                "DC Filter",
                                                                true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("output",
                                                                 "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
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

float PluginProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

float PluginProcessor::getDepthMeterLevel() const noexcept
{
    return depthMeter.load();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    dcInputState = {0.0f, 0.0f};
    dcOutputState = {0.0f, 0.0f};

    driveSmoothed.reset(sampleRate, 0.03);
    frameSmoothed.reset(sampleRate, 0.03);
    phaseSmoothed.reset(sampleRate, 0.03);
    biasSmoothed.reset(sampleRate, 0.03);
    smoothSmoothed.reset(sampleRate, 0.03);
    outputSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.reset(sampleRate, 0.03);

    driveSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("drive")->load());
    frameSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("frame")->load() / 100.0f);
    phaseSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("phase")->load() / 180.0f);
    biasSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("bias")->load() / 100.0f);
    smoothSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("smooth")->load() / 100.0f);
    outputSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("output")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load() / 100.0f);

    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    depthMeter.store(0.0f);
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

void PluginProcessor::updateMeters(float inputPeak, float outputPeak, float depthAmount) noexcept
{
    inputMeter.store(meterDecay(inputMeter.load(), inputPeak));
    outputMeter.store(meterDecay(outputMeter.load(), outputPeak));
    depthMeter.store(meterDecay(depthMeter.load(), depthAmount));
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    using namespace ssp::waveshaper;

    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numSamples == 0 || numChannels == 0)
        return;

    driveSmoothed.setTargetValue(apvts.getRawParameterValue("drive")->load());
    frameSmoothed.setTargetValue(apvts.getRawParameterValue("frame")->load() / 100.0f);
    phaseSmoothed.setTargetValue(apvts.getRawParameterValue("phase")->load() / 180.0f);
    biasSmoothed.setTargetValue(apvts.getRawParameterValue("bias")->load() / 100.0f);
    smoothSmoothed.setTargetValue(apvts.getRawParameterValue("smooth")->load() / 100.0f);
    outputSmoothed.setTargetValue(apvts.getRawParameterValue("output")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load() / 100.0f);

    const auto tableIndex = juce::jlimit(0,
                                         getTableNames().size() - 1,
                                         juce::roundToInt(apvts.getRawParameterValue("table")->load()));
    const auto overflowMode = static_cast<OverflowMode>(juce::jlimit(0,
                                                                     getOverflowModeNames().size() - 1,
                                                                     juce::roundToInt(apvts.getRawParameterValue("overflow")->load())));
    const auto dcFilterEnabled = apvts.getRawParameterValue("dcfilter")->load() >= 0.5f;
    const auto dcCoeff = std::exp(-juce::MathConstants<float>::twoPi * 18.0f / (float) currentSampleRate);

    if (!dcFilterEnabled)
    {
        dcInputState = {0.0f, 0.0f};
        dcOutputState = {0.0f, 0.0f};
    }

    auto inputPeak = 0.0f;
    auto outputPeak = 0.0f;
    auto depthPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto driveGain = juce::Decibels::decibelsToGain(driveSmoothed.getNextValue());
        const auto framePosition = frameSmoothed.getNextValue();
        const auto phaseOffset = phaseSmoothed.getNextValue();
        const auto bias = biasSmoothed.getNextValue();
        const auto smooth = smoothSmoothed.getNextValue();
        const auto outputGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());
        const auto mix = mixSmoothed.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            const auto driven = dry * driveGain;
            const auto lookup = juce::jlimit(-10.0f, 10.0f, driven + bias * 0.55f + phaseOffset);
            auto shaped = sampleTable(tableIndex, framePosition, lookup, smooth, overflowMode);

            if (dcFilterEnabled)
            {
                const auto dcBlocked = shaped - dcInputState[(size_t) channel] + dcCoeff * dcOutputState[(size_t) channel];
                dcInputState[(size_t) channel] = shaped;
                dcOutputState[(size_t) channel] = dcBlocked;
                shaped = dcBlocked;
            }

            const auto wet = shaped * outputGain;
            const auto mixed = juce::jlimit(-1.5f, 1.5f, dry * (1.0f - mix) + wet * mix);

            buffer.setSample(channel, sample, mixed);

            outputPeak = juce::jmax(outputPeak, std::abs(mixed));
            depthPeak = juce::jmax(depthPeak, juce::jlimit(0.0f, 1.0f, std::abs(wet - dry) * 0.8f));
        }
    }

    updateMeters(inputPeak, outputPeak, depthPeak);

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
