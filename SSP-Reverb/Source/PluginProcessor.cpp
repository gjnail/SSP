#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct ReverbMode
{
    const char* name;
    const char* description;
    float roomScale;
    float roomBias;
    float dampingBias;
    float wetScale;
    float widthBias;
    float lowCutHz;
    float highToneBias;
};

const std::array<ReverbMode, 6>& getReverbModes()
{
    static const std::array<ReverbMode, 6> modes{{
        {"Hall", "Large and polished with a long cinematic tail that stays musical on synths and vocals.", 0.95f, 0.14f, 0.16f, 1.00f, 0.10f, 130.0f, 0.02f},
        {"Plate", "Bright dense reflections with a classic vocal-forward plate feel and a tighter low end.", 0.68f, 0.08f, 0.08f, 0.92f, 0.04f, 180.0f, 0.12f},
        {"Room", "Compact and realistic for drums, keys, and glue without washing everything out.", 0.50f, 0.06f, 0.28f, 0.76f, 0.00f, 220.0f, -0.04f},
        {"Chamber", "Smooth, warm, and slightly vintage with a rounded midrange bloom.", 0.74f, 0.10f, 0.22f, 0.88f, 0.06f, 170.0f, -0.02f},
        {"Bloom", "Wide ambient reverb with an exaggerated spacious tail for lush pads and transitions.", 1.00f, 0.20f, 0.12f, 1.08f, 0.18f, 120.0f, 0.08f},
        {"Air", "Cleaner and brighter with more top-end openness for modern pop and electronic textures.", 0.82f, 0.10f, 0.06f, 0.84f, 0.14f, 160.0f, 0.18f},
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
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.28f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "decay",
        "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.48f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tone",
        "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.58f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "width",
        "Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.72f));

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
        for (const auto& mode : getReverbModes())
            result.add(mode.name);
        return result;
    }();

    return names;
}

juce::String PluginProcessor::getCurrentModeDescription() const
{
    const auto& modes = getReverbModes();
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

    for (auto& filter : lowPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(18000.0f);
    }

    for (auto& filter : highPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(120.0f);
    }

    reverb.reset();
    mixSmoothed.reset(sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
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

    const auto mixTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    mixSmoothed.setTargetValue(mixTarget);
    if (mixTarget <= 0.0001f)
    {
        mixSmoothed.setCurrentAndTargetValue(mixTarget);
        reverb.reset();
        return;
    }

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);
    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf(buffer, true);

    const auto& modes = getReverbModes();
    const int modeIndex = juce::jlimit(0, (int) modes.size() - 1, getChoiceIndex(apvts, "mode"));
    const auto& mode = modes[(size_t) modeIndex];

    const float decay = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("decay")->load());
    const float tone = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("tone")->load());
    const float width = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("width")->load());

    juce::Reverb::Parameters params;
    params.roomSize = juce::jlimit(0.05f, 1.0f, mode.roomBias + decay * mode.roomScale);
    params.damping = juce::jlimit(0.08f, 0.95f, 0.72f - tone * 0.52f + mode.dampingBias);
    params.wetLevel = juce::jlimit(0.05f, 1.0f, (0.18f + decay * 0.30f) * mode.wetScale);
    params.dryLevel = 0.0f;
    params.width = juce::jlimit(0.0f, 1.0f, 0.18f + width * 0.72f + mode.widthBias);
    params.freezeMode = 0.0f;
    reverb.setParameters(params);

    const float lowPassHz = juce::jlimit(1800.0f, 20000.0f, 2800.0f + tone * 15200.0f + mode.highToneBias * 3200.0f);
    const float highPassHz = juce::jlimit(20.0f, 1800.0f, mode.lowCutHz + decay * 80.0f);

    for (auto& filter : lowPassFilters)
        filter.setCutoffFrequency(lowPassHz);

    for (auto& filter : highPassFilters)
        filter.setCutoffFrequency(highPassHz);

    if (numChannels >= 2)
        reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), numSamples);
    else if (numChannels == 1)
        reverb.processMono(wetBuffer.getWritePointer(0), numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* wet = wetBuffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto filtered = highPassFilters[(size_t) channel].processSample(0, wet[sample]);
            filtered = lowPassFilters[(size_t) channel].processSample(0, filtered);
            wet[sample] = filtered;
        }
    }

    if (numChannels >= 2)
    {
        const float widthScale = 0.72f + width * 0.85f;
        auto* left = wetBuffer.getWritePointer(0);
        auto* right = wetBuffer.getWritePointer(1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float mid = 0.5f * (left[sample] + right[sample]);
            const float side = 0.5f * (left[sample] - right[sample]) * widthScale;
            left[sample] = mid + side;
            right[sample] = mid - side;
        }
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* out = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            const auto* wet = wetBuffer.getReadPointer(channel);
            out[sample] = dry[sample] * (1.0f - mix) + wet[sample] * mix;
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
