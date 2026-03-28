#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct ReducerMode
{
    const char* name;
    const char* description;
    float bitBias;
    float rateBias;
    float jitterBias;
    float toneBias;
    float outputTrimDb;
};

const std::array<ReducerMode, 5>& getReducerModes()
{
    static const std::array<ReducerMode, 5> modes{{
        {"Smooth", "Cleaner digital reduction with softer aliasing and more mix-friendly texture.", 0.12f, 0.10f, 0.04f, 0.18f, -0.5f},
        {"Crunch", "The classic crunchy bitcrusher lane with obvious grit but still enough body to keep musical.", 0.28f, 0.24f, 0.08f, -0.02f, -1.0f},
        {"Retro", "Low-fi game-console style reduction with bigger sample stepping and lighter top end.", 0.40f, 0.42f, 0.14f, -0.14f, -1.5f},
        {"Digital", "Sharper modern reduction with bright edges and more aggressive transient chewing.", 0.18f, 0.54f, 0.10f, 0.12f, -1.2f},
        {"Destroy", "Full-on smashed reduction for extreme aliasing, zippery rate crush, and broken textures.", 0.60f, 0.72f, 0.22f, -0.24f, -2.0f},
    }};

    return modes;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        return param->getIndex();

    return 0;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode",
        "Mode",
        getModeNames(),
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bits",
        "Bits",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.52f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rate",
        "Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.36f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tone",
        "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.62f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "jitter",
        "Jitter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.12f));

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

const juce::StringArray& PluginProcessor::getModeNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& mode : getReducerModes())
            result.add(mode.name);
        return result;
    }();

    return names;
}

juce::String PluginProcessor::getCurrentModeDescription() const
{
    const auto& modes = getReducerModes();
    const int index = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "mode");
    return modes[(size_t) juce::jlimit(0, (int) modes.size() - 1, index)].description;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    spec.numChannels = 1;

    for (auto& filter : toneFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(18000.0f);
    }

    mixSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    heldSample = {0.0f, 0.0f};
    holdCounter = {0, 0};
    holdSamplesTarget = {1, 1};
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

    const float mixTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    mixSmoothed.setTargetValue(mixTarget);
    if (mixTarget <= 0.0001f)
    {
        mixSmoothed.setCurrentAndTargetValue(mixTarget);
        return;
    }

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    const auto& modes = getReducerModes();
    const int modeIndex = juce::jlimit(0, (int) modes.size() - 1, getChoiceIndex(apvts, "mode"));
    const auto& mode = modes[(size_t) modeIndex];

    const float bitsControl = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("bits")->load());
    const float rateControl = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("rate")->load());
    const float toneControl = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("tone")->load());
    const float jitterControl = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("jitter")->load());

    const float effectiveBitsNorm = juce::jlimit(0.0f, 1.0f, bitsControl + mode.bitBias);
    const float effectiveRateNorm = juce::jlimit(0.0f, 1.0f, rateControl + mode.rateBias);
    const float effectiveJitterNorm = juce::jlimit(0.0f, 1.0f, jitterControl + mode.jitterBias);
    const float effectiveToneNorm = juce::jlimit(0.0f, 1.0f, toneControl + mode.toneBias);

    const int bitDepth = juce::jlimit(2, 16, juce::roundToInt(16.0f - effectiveBitsNorm * 13.0f));
    const float quantisationSteps = (float) (1 << bitDepth);
    const int baseHoldSamples = juce::jlimit(1, 256, juce::roundToInt(1.0f + std::pow(effectiveRateNorm, 1.55f) * 220.0f));
    const float jitterAmount = effectiveJitterNorm * 0.65f;
    const float toneHz = juce::jlimit(1200.0f, 20000.0f, 2200.0f + std::pow(effectiveToneNorm, 1.2f) * 16800.0f);

    for (auto& filter : toneFilters)
        filter.setCutoffFrequency(toneHz);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (holdCounter[(size_t) channel] <= 0)
            {
                const float source = data[sample];
                const float quantised = std::round(source * quantisationSteps) / quantisationSteps;
                heldSample[(size_t) channel] = juce::jlimit(-1.0f, 1.0f, quantised);

                const float jitterSpread = (random.nextFloat() * 2.0f - 1.0f) * jitterAmount;
                const float holdScale = juce::jlimit(0.35f, 1.8f, 1.0f + jitterSpread);
                holdSamplesTarget[(size_t) channel] = juce::jmax(1, juce::roundToInt((float) baseHoldSamples * holdScale));
                holdCounter[(size_t) channel] = holdSamplesTarget[(size_t) channel];
            }

            float crushed = heldSample[(size_t) channel];
            crushed = toneFilters[(size_t) channel].processSample(0, crushed);
            data[sample] = crushed;
            --holdCounter[(size_t) channel];
        }
    }

    const float outputTrim = juce::Decibels::decibelsToGain(mode.outputTrimDb);
    buffer.applyGain(outputTrim);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* out = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            out[sample] = dry[sample] * (1.0f - mix) + out[sample] * mix;
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
