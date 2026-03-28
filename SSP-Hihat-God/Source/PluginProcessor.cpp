#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct RateOption
{
    const char* label;
    double quarterNotes;
};

constexpr std::array<RateOption, 11> rateOptions {{
    {"1/32", 0.125},
    {"1/16T", 1.0 / 6.0},
    {"1/16", 0.25},
    {"1/8T", 1.0 / 3.0},
    {"1/8", 0.5},
    {"1/4T", 2.0 / 3.0},
    {"1/4", 1.0},
    {"1/2", 2.0},
    {"1 Bar", 4.0},
    {"2 Bar", 8.0},
    {"4 Bar", 16.0}
}};
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("variation",
                                                                 "Variation",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("volumeRangeDb",
                                                                 "Volume Range",
                                                                 juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
                                                                 3.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("panRange",
                                                                 "Pan Range",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterInt>("rateIndex",
                                                               "Rate",
                                                               0,
                                                               (int) rateOptions.size() - 1,
                                                               4));

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

float PluginProcessor::getCurrentGainOffsetDb() const noexcept
{
    return currentGainOffsetDb.load();
}

float PluginProcessor::getCurrentPanOffset() const noexcept
{
    return currentPanOffset.load();
}

double PluginProcessor::getRateSeconds() const noexcept
{
    double bpm = 120.0;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = juce::jlimit(40.0, 240.0, *hostBpm);
        }
    }

    const auto rateIndex = juce::jlimit(0,
                                        (int) rateOptions.size() - 1,
                                        juce::roundToInt(apvts.getRawParameterValue("rateIndex")->load()));

    const double secondsPerQuarter = 60.0 / bpm;
    return secondsPerQuarter * rateOptions[(size_t) rateIndex].quarterNotes;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    modulationPhase = 0.0;
    currentGainOffsetDb.store(0.0f);
    currentPanOffset.store(0.0f);
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
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    const float variation = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("variation")->load());
    const float volumeRangeDb = juce::jmax(0.0f, apvts.getRawParameterValue("volumeRangeDb")->load()) * variation;
    const float panRange = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("panRange")->load()) * variation;
    const double cycleSeconds = juce::jmax(0.001, getRateSeconds());
    const double phaseIncrement = juce::MathConstants<double>::twoPi / (juce::jmax(1.0, currentSampleRate) * cycleSeconds);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gainLfo = std::sin((float) modulationPhase);
        const float panLfo = std::sin((float) (modulationPhase + juce::MathConstants<double>::halfPi));
        const float gainDb = gainLfo * volumeRangeDb;
        const float pan = panLfo * panRange;
        currentGainOffsetDb.store(gainDb);
        currentPanOffset.store(pan);

        const float gain = juce::Decibels::decibelsToGain(gainDb);

        if (numChannels == 1)
        {
            buffer.setSample(0, sample, buffer.getSample(0, sample) * gain);
        }
        else
        {
            const float leftPanGain = pan > 0.0f ? 1.0f - pan : 1.0f;
            const float rightPanGain = pan < 0.0f ? 1.0f + pan : 1.0f;
            const float leftGain = leftPanGain * gain;
            const float rightGain = rightPanGain * gain;

            buffer.setSample(0, sample, buffer.getSample(0, sample) * leftGain);
            buffer.setSample(1, sample, buffer.getSample(1, sample) * rightGain);
        }

        modulationPhase += phaseIncrement;
        if (modulationPhase >= juce::MathConstants<double>::twoPi)
            modulationPhase -= juce::MathConstants<double>::twoPi;
    }

    for (int channel = 2; channel < numChannels; ++channel)
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
