#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.38f;
constexpr float meterRelease = 0.12f;

float applyClipCurve(float normalisedSample, float softness)
{
    const float hardClip = juce::jlimit(-1.0f, 1.0f, normalisedSample);
    const float shapeDrive = 1.0f + softness * 6.0f;
    const float normaliser = (float) std::tanh(shapeDrive);
    const float softClip = normaliser > 0.0001f
                               ? (float) std::tanh(normalisedSample * shapeDrive) / normaliser
                               : hardClip;

    return juce::jlimit(-1.0f, 1.0f, juce::jmap(softness, hardClip, softClip));
}

float getPeakLevel(const juce::AudioBuffer<float>& buffer, int numChannels)
{
    float peak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

    return peak;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("driveDb", "Drive", juce::NormalisableRange<float>(-18.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ceilingDb", "Ceiling", juce::NormalisableRange<float>(-18.0f, 0.0f, 0.1f), -0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("softness", "Softness", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("trimDb", "Trim", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
}

PluginProcessor::~PluginProcessor() = default;

float PluginProcessor::getInputMeterLevel() const noexcept { return inputMeter.load(); }
float PluginProcessor::getOutputMeterLevel() const noexcept { return outputMeter.load(); }
float PluginProcessor::getClipMeterLevel() const noexcept { return clipMeter.load(); }

void PluginProcessor::updateMeters(float inputPeak, float outputPeak, float clipAmount) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
    smoothValue(clipMeter, juce::jlimit(0.0f, 1.0f, clipAmount));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    driveSmoothed.reset(sampleRate, 0.02);
    ceilingSmoothed.reset(sampleRate, 0.02);
    trimSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.03);

    driveSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("driveDb")->load());
    ceilingSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("ceilingDb")->load());
    trimSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("trimDb")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    spec.numChannels = 1;

    for (auto& filter : dcFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(12.0f);
        filter.setResonance(0.55f);
    }

    if (oversampling != nullptr)
    {
        oversampling->reset();
        oversampling->initProcessing((size_t) juce::jmax(1, samplesPerBlock));
        setLatencySamples((int) std::lround(oversampling->getLatencyInSamples()));
    }
    else
    {
        setLatencySamples(0);
    }

    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    clipMeter.store(0.0f);
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

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    const float inputPeak = getPeakLevel(dryBuffer, numChannels);

    driveSmoothed.setTargetValue(apvts.getRawParameterValue("driveDb")->load());
    ceilingSmoothed.setTargetValue(apvts.getRawParameterValue("ceilingDb")->load());
    trimSmoothed.setTargetValue(apvts.getRawParameterValue("trimDb")->load());
    mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load()));

    const float driveGain = juce::Decibels::decibelsToGain(driveSmoothed.skip(numSamples));
    const float ceilingGain = juce::jmax(0.001f, juce::Decibels::decibelsToGain(ceilingSmoothed.skip(numSamples)));
    const float trimGain = juce::Decibels::decibelsToGain(trimSmoothed.skip(numSamples));
    const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.skip(numSamples));
    const float softness = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("softness")->load());

    auto wetBlock = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, (size_t) numChannels);
    auto oversampledBlock = oversampling != nullptr ? oversampling->processSamplesUp(wetBlock) : wetBlock;
    const int oversampledSamples = (int) oversampledBlock.getNumSamples();

    int clippedSamples = 0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = oversampledBlock.getChannelPointer((size_t) channel);

        for (int sample = 0; sample < oversampledSamples; ++sample)
        {
            const float normalisedInput = (channelData[sample] * driveGain) / ceilingGain;
            channelData[sample] = applyClipCurve(normalisedInput, softness) * ceilingGain * trimGain;

            if (std::abs(normalisedInput) >= 1.0f)
                ++clippedSamples;
        }
    }

    if (oversampling != nullptr)
        oversampling->processSamplesDown(wetBlock);

    float outputPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* wet = buffer.getWritePointer(channel);
        const auto* dry = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float filteredWet = dcFilters[(size_t) channel].processSample(0, wet[sample]);
            wet[sample] = dry[sample] * (1.0f - mix) + filteredWet * mix;
            outputPeak = juce::jmax(outputPeak, std::abs(wet[sample]));
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const float clipRatio = oversampledSamples > 0 ? (float) clippedSamples / (float) juce::jmax(1, numChannels * oversampledSamples) : 0.0f;
    updateMeters(inputPeak, outputPeak, clipRatio);
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
