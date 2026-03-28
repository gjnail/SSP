#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct TransitionPreset
{
    const char* name;
    const char* description;
    float highPassMaxHz;
    float lowPassMinHz;
    float reverbWet;
    float roomSize;
    float damping;
    float widthBoost;
    float fadeDb;
    float responseCurve;
    bool fullKillAtMax;
};

const std::array<TransitionPreset, 5>& getTransitionPresets()
{
    static const std::array<TransitionPreset, 5> presets{{
        {"Sky Lift", "Narrow the spectrum, widen the stereo image, and wash into a glossy reverb tail.", 2600.0f, 11000.0f, 0.58f, 0.92f, 0.28f, 0.55f, 4.0f, 0.78f, false},
        {"Fade Out", "A true end-of-section fade that darkens and disappears completely by the end of the knob throw.", 900.0f, 3800.0f, 0.32f, 0.74f, 0.48f, 0.18f, 120.0f, 1.05f, true},
        {"Wash Out", "Heavy ambience with a wide stereo bloom that feels bigger the further you turn it.", 1800.0f, 7600.0f, 0.72f, 0.98f, 0.20f, 0.72f, 6.5f, 0.72f, false},
        {"Telephone Exit", "Push into a compact filtered band for DJ-style exits and tight breakdown sweeps.", 1600.0f, 2900.0f, 0.18f, 0.56f, 0.54f, 0.12f, 9.0f, 0.94f, false},
        {"Air Lift", "Keep more top end while opening the sides and lifting the transition into the next phrase.", 3200.0f, 14500.0f, 0.44f, 0.88f, 0.32f, 0.84f, 2.5f, 0.62f, false},
    }};

    return presets;
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

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount",
        "Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "preset",
        "Preset",
        getPresetNames(),
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

const juce::StringArray& PluginProcessor::getPresetNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& preset : getTransitionPresets())
            result.add(preset.name);
        return result;
    }();

    return names;
}

juce::String PluginProcessor::getCurrentPresetDescription() const
{
    const auto& presets = getTransitionPresets();
    const int index = getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "preset");
    return presets[(size_t) juce::jlimit(0, (int) presets.size() - 1, index)].description;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    spec.numChannels = 1;

    for (auto& filter : highPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(20.0f);
    }

    for (auto& filter : lowPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(20000.0f);
    }

    reverb.reset();
    amountSmoothed.reset(sampleRate, 0.08);
    mixSmoothed.reset(sampleRate, 0.06);
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());
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

    const float amountTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("amount")->load());
    const float mixTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    amountSmoothed.setTargetValue(amountTarget);
    mixSmoothed.setTargetValue(mixTarget);

    if (amountTarget <= 0.0001f || mixTarget <= 0.0001f)
    {
        amountSmoothed.setCurrentAndTargetValue(amountTarget);
        mixSmoothed.setCurrentAndTargetValue(mixTarget);
        reverb.reset();
        return;
    }

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    const auto& presets = getTransitionPresets();
    const int presetIndex = juce::jlimit(0, (int) presets.size() - 1, getChoiceIndex(apvts, "preset"));
    const auto& preset = presets[(size_t) presetIndex];

    const float amount = juce::jlimit(0.0f, 1.0f, amountSmoothed.skip(numSamples));
    const float shapedAmount = std::pow(amount, preset.responseCurve);

    const float highPassHz = juce::jlimit(20.0f, 18000.0f, 20.0f + shapedAmount * (preset.highPassMaxHz - 20.0f));
    const float lowPassHz = juce::jlimit(1200.0f, 20000.0f, 20000.0f - shapedAmount * (20000.0f - preset.lowPassMinHz));

    for (auto& filter : highPassFilters)
        filter.setCutoffFrequency(highPassHz);

    for (auto& filter : lowPassFilters)
        filter.setCutoffFrequency(lowPassHz);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto processed = highPassFilters[(size_t) channel].processSample(0, channelData[sample]);
            processed = lowPassFilters[(size_t) channel].processSample(0, processed);
            channelData[sample] = processed;
        }
    }

    juce::Reverb::Parameters reverbParams;
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, 0.12f + shapedAmount * preset.roomSize);
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, 0.20f + shapedAmount * preset.damping);
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, shapedAmount * preset.reverbWet);
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, 1.0f - shapedAmount * 0.42f);
    reverbParams.width = juce::jlimit(0.0f, 1.0f, 0.85f + shapedAmount * 0.15f);
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    if (numChannels >= 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    else if (numChannels == 1)
        reverb.processMono(buffer.getWritePointer(0), numSamples);

    if (numChannels >= 2)
    {
        const float widthScale = 1.0f + preset.widthBoost * shapedAmount;
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float mid = 0.5f * (left[sample] + right[sample]);
            const float side = 0.5f * (left[sample] - right[sample]) * widthScale;
            left[sample] = mid + side;
            right[sample] = mid - side;
        }
    }

    const float wetGain = juce::Decibels::decibelsToGain(-preset.fadeDb * shapedAmount);
    buffer.applyGain(wetGain);

    if (preset.fullKillAtMax && amount >= 0.999f)
        buffer.clear();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* wet = buffer.getWritePointer(channel);
        const auto* dry = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());
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
