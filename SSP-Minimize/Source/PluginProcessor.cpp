#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float getValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* value = apvts.getRawParameterValue(id))
        return value->load();

    return 0.0f;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("depth", "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.58f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sensitivity", "Sensitivity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.52f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sharpness", "Sharpness", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.48f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("focus", "Focus", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.52f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.85f));

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

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    spec.numChannels = 1;

    for (auto& band : bands)
    {
        band.envelopes = { 0.0f, 0.0f };
        band.attenuationForUi = 0.0f;

        for (auto& filter : band.filters)
        {
            filter.reset();
            filter.prepare(spec);
            filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        }
    }

    mixSmoothed.reset(sampleRate, 0.04);
    mixSmoothed.setCurrentAndTargetValue(getValue(apvts, "mix"));
    updateBandConfiguration();
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::updateBandConfiguration()
{
    const auto focus = juce::jlimit(0.0f, 1.0f, getValue(apvts, "focus"));
    const auto sharpness = juce::jlimit(0.0f, 1.0f, getValue(apvts, "sharpness"));
    const auto lowHz = juce::jmap(focus, 90.0f, 240.0f);
    const auto highHz = juce::jmap(focus, 8200.0f, 16000.0f);
    const auto lowLog = std::log(lowHz);
    const auto highLog = std::log(highHz);

    for (size_t index = 0; index < bands.size(); ++index)
    {
        auto& band = bands[index];
        const auto norm = (float) index / (float) juce::jmax(1, (int) bands.size() - 1);
        band.centreHz = std::exp(juce::jmap(norm, lowLog, highLog));
        band.q = juce::jmap(sharpness, 0.0f, 1.0f, 1.4f, 8.2f);

        for (auto& filter : band.filters)
        {
            filter.setCutoffFrequency(band.centreHz);
            filter.setResonance(band.q);
        }
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    updateBandConfiguration();

    const auto depth = juce::jlimit(0.0f, 1.0f, getValue(apvts, "depth"));
    const auto sensitivity = juce::jlimit(0.0f, 1.0f, getValue(apvts, "sensitivity"));
    const auto sharpness = juce::jlimit(0.0f, 1.0f, getValue(apvts, "sharpness"));
    const auto attack = juce::jmap(getValue(apvts, "attack"), 0.001f, 0.120f);
    const auto release = juce::jmap(getValue(apvts, "release"), 0.030f, 0.800f);
    const auto threshold = juce::jmap(sensitivity, 0.010f, 0.220f);
    const auto curve = juce::jmap(sharpness, 1.0f, 3.8f);
    const auto maxCut = juce::jmap(depth, 0.0f, 0.88f);
    const auto attackSeconds = juce::jmax(0.001f, attack);
    const auto releaseSeconds = juce::jmax(0.001f, release);
    const auto attackCoeff = std::exp(-1.0 / ((double) attackSeconds * currentSampleRate));
    const auto releaseCoeff = std::exp(-1.0 / ((double) releaseSeconds * currentSampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, getValue(apvts, "mix")));

    for (auto& band : bands)
        band.attenuationForUi = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* out = buffer.getWritePointer(channel);
        const auto* dryIn = dry.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto input = dryIn[sample];
            auto processed = input;

            for (auto& band : bands)
            {
                const auto filtered = band.filters[(size_t) channel].processSample(0, input);
                const auto detector = std::abs(filtered);
                auto& envelope = band.envelopes[(size_t) channel];
                const auto coeff = detector > envelope ? attackCoeff : releaseCoeff;
                envelope = (float) (detector + coeff * (envelope - detector));

                const auto over = juce::jmax(0.0f, envelope - threshold);
                const auto normalised = juce::jlimit(0.0f, 1.0f, over / juce::jmax(0.0001f, 1.0f - threshold));
                const auto attenuation = juce::jlimit(0.0f, maxCut, std::pow(normalised, curve) * maxCut);
                processed -= filtered * attenuation;
                band.attenuationForUi = juce::jmax(band.attenuationForUi, attenuation);
            }

            const auto output = juce::jlimit(-1.0f, 1.0f, processed);
            const auto mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());
            out[sample] = input * (1.0f - mix) + output * mix;
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

std::array<PluginProcessor::BandVisual, PluginProcessor::numBands> PluginProcessor::getBandVisuals() const
{
    std::array<BandVisual, numBands> result;

    for (size_t i = 0; i < bands.size(); ++i)
    {
        result[i].centreHz = bands[i].centreHz;
        result[i].attenuation = bands[i].attenuationForUi;
    }

    return result;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
