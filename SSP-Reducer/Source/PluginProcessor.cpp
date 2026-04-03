#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bits",
        "Bits",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.466667f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rate",
        "Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.48f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tone",
        "ADC Q",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "jitter",
        "Dither",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filter",
        "DAC Q",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "preFilter",
        "Pre Filter",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "postFilter",
        "Post Filter",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "dcShift",
        "DC Shift",
        false));

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

reducerdsp::Parameters PluginProcessor::getCurrentParameters() const
{
    reducerdsp::Parameters parameters;
    parameters.mix = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    parameters.bits = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("bits")->load());
    parameters.rate = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("rate")->load());
    parameters.adcQuality = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("tone")->load());
    parameters.dither = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("jitter")->load());
    parameters.dacQuality = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("filter")->load());
    parameters.preFilterEnabled = apvts.getRawParameterValue("preFilter")->load() >= 0.5f;
    parameters.postFilterEnabled = apvts.getRawParameterValue("postFilter")->load() >= 0.5f;
    parameters.dcShiftEnabled = apvts.getRawParameterValue("dcShift")->load() >= 0.5f;
    parameters.sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    return parameters;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    reducerdsp::prepareState(reducerState, sampleRate, samplesPerBlock);

    mixSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load()));
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

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(2, buffer.getNumChannels());
    auto parameters = getCurrentParameters();

    mixSmoothed.setTargetValue(parameters.mix);

    if (parameters.mix <= 0.0001f)
    {
        mixSmoothed.setCurrentAndTargetValue(parameters.mix);

        for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
            buffer.clear(channel, 0, numSamples);

        return;
    }

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    reducerdsp::processWetBuffer(buffer, parameters, reducerState);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* wet = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            wet[sample] = dry[sample] * (1.0f - mix) + wet[sample] * mix;
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
