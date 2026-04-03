#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.32f;
constexpr float meterRelease = 0.10f;
constexpr float maxLookaheadMs = 20.0f;
constexpr int defaultLimitTypeIndex = 3;

struct LimiterModeSettings
{
    float releaseScale = 1.0f;
    float detectorExponent = 1.0f;
    float knee = 0.1f;
    float stereoLinkBias = 1.0f;
    float ceilingScale = 1.0f;
};

juce::StringArray getLimiterTypeChoicesInternal()
{
    return { "Transparent", "Punchy", "Dynamic", "Allround", "Aggressive", "Modern", "Bus" };
}

LimiterModeSettings getLimiterModeSettings(int index)
{
    switch (index)
    {
        case 0: return { 1.45f, 1.18f, 0.22f, 1.00f, 1.00f };
        case 1: return { 0.62f, 0.90f, 0.06f, 0.72f, 1.00f };
        case 2: return { 0.84f, 1.04f, 0.28f, 0.62f, 1.00f };
        case 3: return { 1.00f, 1.00f, 0.14f, 0.88f, 1.00f };
        case 4: return { 0.46f, 0.86f, 0.03f, 0.96f, 0.985f };
        case 5: return { 0.72f, 1.10f, 0.18f, 0.95f, 0.995f };
        case 6: return { 1.35f, 1.06f, 0.16f, 1.00f, 0.99f };
        default: return {};
    }
}

juce::String sanitisePresetText(const juce::String& text)
{
    auto cleaned = text.trim().retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-/");
    return cleaned.trim();
}

juce::String sanitisePresetName(const juce::String& text)
{
    auto name = sanitisePresetText(text);
    return name.isNotEmpty() ? name : "Untitled Preset";
}

juce::String sanitisePresetCategory(const juce::String& text)
{
    auto category = sanitisePresetText(text).replaceCharacter('\\', '/');
    while (category.contains("//"))
        category = category.replace("//", "/");
    return category.isNotEmpty() ? category : "User";
}

juce::String makePresetKey(bool isFactory, const juce::String& category, const juce::String& name)
{
    return (isFactory ? "factory:" : "user:") + sanitisePresetCategory(category) + ":" + sanitisePresetName(name);
}

juce::ValueTree makePresetState(float inputDb,
                                int limitType,
                                float thresholdDb,
                                float ceilingDb,
                                float releaseMs,
                                float lookaheadMs,
                                float stereoLink,
                                float outputDb,
                                float mix)
{
    juce::ValueTree state("LimiterPresetState");
    state.setProperty("inputDb", inputDb, nullptr);
    state.setProperty("limitType", limitType, nullptr);
    state.setProperty("thresholdDb", thresholdDb, nullptr);
    state.setProperty("ceilingDb", ceilingDb, nullptr);
    state.setProperty("releaseMs", releaseMs, nullptr);
    state.setProperty("lookaheadMs", lookaheadMs, nullptr);
    state.setProperty("stereoLink", stereoLink, nullptr);
    state.setProperty("outputDb", outputDb, nullptr);
    state.setProperty("mix", mix, nullptr);
    return state;
}

float getPresetFloat(const juce::ValueTree& state, const juce::Identifier& id, float fallback)
{
    return state.hasProperty(id) ? (float) state[id] : fallback;
}

int getPresetInt(const juce::ValueTree& state, const juce::Identifier& id, int fallback)
{
    return state.hasProperty(id) ? (int) state[id] : fallback;
}

float getPeakLevel(const juce::AudioBuffer<float>& buffer, int numChannels)
{
    float peak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    return peak;
}

juce::Array<PluginProcessor::PresetRecord> buildFactoryPresetLibrary()
{
    using PresetRecord = PluginProcessor::PresetRecord;
    const auto preset = [](const juce::String& name,
                           const juce::String& category,
                           float inputDb,
                           int limitType,
                           float thresholdDb,
                           float ceilingDb,
                           float releaseMs,
                           float lookaheadMs,
                           float stereoLink,
                           float outputDb,
                           float mix)
    {
        PresetRecord item;
        item.name = name;
        item.category = category;
        item.author = "SSP";
        item.isFactory = true;
        item.id = makePresetKey(true, category, name);
        item.state = makePresetState(inputDb, limitType, thresholdDb, ceilingDb, releaseMs, lookaheadMs, stereoLink, outputDb, mix);
        return item;
    };

    juce::Array<PresetRecord> presets;
    presets.add(preset("Default Limiter", "Core", 0.0f, 3, 0.0f, 0.0f, 80.0f, 2.0f, 1.0f, 0.0f, 1.0f));
    presets.add(preset("Transparent Catch", "Core", 1.5f, 0, 0.0f, -0.2f, 120.0f, 3.0f, 1.0f, 0.0f, 1.0f));
    presets.add(preset("Punchy Drum Stop", "Drums", 4.5f, 1, 0.0f, -0.1f, 45.0f, 1.5f, 0.85f, 0.0f, 1.0f));
    presets.add(preset("Bus Glue Limit", "Bus", 2.0f, 6, 0.0f, -0.3f, 140.0f, 2.5f, 1.0f, 0.0f, 0.9f));
    presets.add(preset("Wide Master Lift", "Master", 3.0f, 5, 0.0f, -0.2f, 180.0f, 4.0f, 1.0f, 0.0f, 1.0f));
    presets.add(preset("Parallel Control", "Creative", 6.0f, 4, 0.0f, -0.5f, 90.0f, 2.0f, 0.75f, 0.0f, 0.6f));
    return presets;
}
} // namespace

const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets()
{
    static const auto presets = buildFactoryPresetLibrary();
    return presets;
}

juce::StringArray PluginProcessor::getLimitTypeChoices()
{
    return getLimiterTypeChoicesInternal();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputDb", "Input", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("limitType", "Limiting Type", getLimitTypeChoices(), defaultLimitTypeIndex));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("thresholdDb", "Threshold", juce::NormalisableRange<float>(-18.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ceilingDb", "Ceiling", juce::NormalisableRange<float>(-3.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("releaseMs", "Release", juce::NormalisableRange<float>(5.0f, 500.0f, 0.1f), 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lookaheadMs", "Lookahead", juce::NormalisableRange<float>(0.0f, maxLookaheadMs, 0.1f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("stereoLink", "Stereo Link", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputDb", "Output", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto* id : {"inputDb", "limitType", "thresholdDb", "ceilingDb", "releaseMs", "lookaheadMs", "stereoLink", "outputDb", "mix"})
        apvts.addParameterListener(id, this);

    for (auto& value : inputVisualizerBody)
        value.store(0.0f);
    for (auto& value : inputVisualizerPeak)
        value.store(0.0f);
    for (auto& value : outputVisualizerBody)
        value.store(0.0f);
    for (auto& value : outputVisualizerPeak)
        value.store(0.0f);
    for (auto& value : gainReductionVisualizer)
        value.store(0.0f);

    initialisePresetTracking();
    refreshUserPresets();
    loadFactoryPreset(0);
}

PluginProcessor::~PluginProcessor()
{
    for (auto* id : {"inputDb", "limitType", "thresholdDb", "ceilingDb", "releaseMs", "lookaheadMs", "stereoLink", "outputDb", "mix"})
        apvts.removeParameterListener(id, this);
}

float PluginProcessor::getInputMeterLevel() const noexcept { return inputMeter.load(); }
float PluginProcessor::getOutputMeterLevel() const noexcept { return outputMeter.load(); }
float PluginProcessor::getGainReductionDb() const noexcept { return gainReductionDbAtomic.load(); }
float PluginProcessor::getGainReductionMeterLevel() const noexcept { return juce::jlimit(0.0f, 1.0f, getGainReductionDb() / 18.0f); }

juce::String PluginProcessor::getActiveLimitTypeLabel() const
{
    const auto choices = getLimitTypeChoices();
    const int index = juce::jlimit(0, choices.size() - 1, juce::roundToInt(apvts.getRawParameterValue("limitType")->load()));
    return choices[index];
}

PluginProcessor::VisualizerSnapshot PluginProcessor::getVisualizerSnapshot() const noexcept
{
    VisualizerSnapshot snapshot;
    snapshot.gainReductionDb = gainReductionDbAtomic.load();
    snapshot.writePosition = visualizerWritePosition.load();
    for (size_t i = 0; i < snapshot.inputBody.size(); ++i)
    {
        snapshot.inputBody[i] = inputVisualizerBody[i].load();
        snapshot.inputPeak[i] = inputVisualizerPeak[i].load();
        snapshot.outputBody[i] = outputVisualizerBody[i].load();
        snapshot.outputPeak[i] = outputVisualizerPeak[i].load();
        snapshot.gainReductionHistory[i] = gainReductionVisualizer[i].load();
    }
    return snapshot;
}

juce::Array<PluginProcessor::PresetRecord> PluginProcessor::getSequentialPresetList() const
{
    juce::Array<PresetRecord> presets;
    presets.addArray(getFactoryPresets());
    presets.addArray(userPresets);
    return presets;
}

juce::StringArray PluginProcessor::getAvailablePresetCategories() const
{
    juce::StringArray categories;
    for (const auto& preset : getFactoryPresets())
        categories.addIfNotAlreadyThere(preset.category);
    for (const auto& preset : userPresets)
        categories.addIfNotAlreadyThere(preset.category);
    categories.sort(true);
    return categories;
}

bool PluginProcessor::matchesCurrentPreset(const PresetRecord& preset) const noexcept
{
    return preset.id.isNotEmpty() && preset.id == currentPresetKey;
}

void PluginProcessor::updateLatencyCompensation()
{
    const auto lookaheadParameter = apvts.getRawParameterValue("lookaheadMs");
    if (lookaheadParameter == nullptr)
        return;

    const float lookaheadMs = juce::jlimit(0.0f, maxLookaheadMs, lookaheadParameter->load());
    const int computedLatencySamples = currentSampleRate > 0.0
        ? juce::jmax(0, (int) std::round(lookaheadMs * 0.001f * currentSampleRate))
        : 0;
    setLatencySamples(computedLatencySamples);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
    inputGainSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("inputDb")->load());
    outputGainSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("outputDb")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());

    maxLookaheadSamples = juce::jmax(1, (int) std::ceil(sampleRate * (maxLookaheadMs * 0.001f))) + juce::jmax(1, samplesPerBlock) + 8;
    dryBuffer.setSize(2, juce::jmax(1, samplesPerBlock));
    limitedPreviewBuffer.setSize(2, juce::jmax(1, samplesPerBlock));
    lookaheadBuffer.setSize(2, maxLookaheadSamples);
    lookaheadBuffer.clear();
    lookaheadWritePosition = 0;
    channelGain = {1.0f, 1.0f};
    updateLatencyCompensation();

    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    gainReductionDbAtomic.store(0.0f);
    visualizerWritePosition.store(0);
    visualizerSamplesAccumulated = 0;
    inputPeakAccumulator = 0.0f;
    inputEnergyAccumulator = 0.0f;
    outputPeakAccumulator = 0.0f;
    outputEnergyAccumulator = 0.0f;
    gainReductionAccumulator = 0.0f;
    heldInputPeak = 0.0f;
    heldInputBody = 0.0f;
    heldOutputPeak = 0.0f;
    heldOutputBody = 0.0f;
    heldGainReduction = 0.0f;

    for (auto& value : inputVisualizerBody)
        value.store(0.0f);
    for (auto& value : inputVisualizerPeak)
        value.store(0.0f);
    for (auto& value : outputVisualizerBody)
        value.store(0.0f);
    for (auto& value : outputVisualizerPeak)
        value.store(0.0f);
    for (auto& value : gainReductionVisualizer)
        value.store(0.0f);
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

    if (dryBuffer.getNumChannels() < numChannels || dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize(numChannels, numSamples, false, false, true);

    if (limitedPreviewBuffer.getNumChannels() < numChannels || limitedPreviewBuffer.getNumSamples() < numSamples)
        limitedPreviewBuffer.setSize(numChannels, numSamples, false, false, true);

    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);

    inputGainSmoothed.setTargetValue(apvts.getRawParameterValue("inputDb")->load());
    outputGainSmoothed.setTargetValue(apvts.getRawParameterValue("outputDb")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());

    const float inputGain = juce::Decibels::decibelsToGain(inputGainSmoothed.skip(numSamples));
    const float outputGain = juce::Decibels::decibelsToGain(outputGainSmoothed.skip(numSamples));
    const int limitTypeIndex = juce::jlimit(0, getLimitTypeChoices().size() - 1,
                                            juce::roundToInt(apvts.getRawParameterValue("limitType")->load()));
    const auto mode = getLimiterModeSettings(limitTypeIndex);
    const float thresholdGain = 1.0f;
    const float ceilingGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("ceilingDb")->load()) * mode.ceilingScale;
    const float releaseMs = juce::jlimit(5.0f, 500.0f, apvts.getRawParameterValue("releaseMs")->load());
    const float lookaheadMs = juce::jlimit(0.0f, maxLookaheadMs, apvts.getRawParameterValue("lookaheadMs")->load());
    const float stereoLink = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("stereoLink")->load());
    const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.skip(numSamples));
    const float effectiveReleaseMs = juce::jlimit(3.0f, 900.0f, releaseMs * mode.releaseScale);
    const float releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, effectiveReleaseMs * 0.001f * (float) currentSampleRate));
    const int lookaheadSamples = juce::jlimit(0, maxLookaheadSamples - 2, (int) std::round(lookaheadMs * 0.001f * currentSampleRate));
    const float kneeStart = thresholdGain * juce::jmax(0.15f, 1.0f - mode.knee);
    const float thresholdDetector = std::pow(juce::jmax(0.00001f, thresholdGain), mode.detectorExponent);
    const float effectiveStereoLink = juce::jlimit(0.0f, 1.0f,
                                                   stereoLink * mode.stereoLinkBias
                                                       + (1.0f - mode.stereoLinkBias) * 0.5f);

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float maxGainReductionDb = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float driven[2]{};
        float absSample[2]{};
        float maxAbs = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            driven[channel] = dryBuffer.getSample(channel, sample) * inputGain;
            absSample[channel] = std::abs(driven[channel]);
            maxAbs = juce::jmax(maxAbs, absSample[channel]);
            lookaheadBuffer.setSample(channel, lookaheadWritePosition, driven[channel]);
            inputPeak = juce::jmax(inputPeak, absSample[channel]);
        }

        const int readPosition = (lookaheadWritePosition + maxLookaheadSamples - lookaheadSamples) % maxLookaheadSamples;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float detectorBase = juce::jmap(effectiveStereoLink, absSample[channel], maxAbs);
            const float detector = std::pow(juce::jmax(0.0f, detectorBase), mode.detectorExponent);
            float hardGain = detector > thresholdDetector ? (thresholdDetector / detector) : 1.0f;
            hardGain = juce::jlimit(0.0f, 1.0f, hardGain);

            float desiredGain = 1.0f;
            if (detectorBase > kneeStart)
            {
                const float kneeBlend = juce::jlimit(0.0f, 1.0f,
                                                     (detectorBase - kneeStart)
                                                         / juce::jmax(0.00001f, thresholdGain - kneeStart));
                desiredGain = juce::jmap(kneeBlend, 1.0f, hardGain);
            }

            if (desiredGain < channelGain[(size_t) channel])
                channelGain[(size_t) channel] = desiredGain;
            else
                channelGain[(size_t) channel] = desiredGain + releaseCoeff * (channelGain[(size_t) channel] - desiredGain);

            const float delayed = lookaheadBuffer.getSample(channel, readPosition);
            const float limited = juce::jlimit(-ceilingGain, ceilingGain, delayed * channelGain[(size_t) channel] * outputGain);
            limitedPreviewBuffer.setSample(channel, sample, limited);
            buffer.setSample(channel, sample, dryBuffer.getSample(channel, sample) * (1.0f - mix) + limited * mix);

            outputPeak = juce::jmax(outputPeak, std::abs(buffer.getSample(channel, sample)));
            maxGainReductionDb = juce::jmax(maxGainReductionDb, juce::Decibels::gainToDecibels(juce::jmax(0.00001f, 1.0f / channelGain[(size_t) channel]), 0.0f));
        }

        lookaheadWritePosition = (lookaheadWritePosition + 1) % maxLookaheadSamples;
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, outputPeak, maxGainReductionDb);
    updateVisualizerSnapshot(dryBuffer, limitedPreviewBuffer, numChannels, maxGainReductionDb);
}

void PluginProcessor::updateMeters(float inputPeak, float outputPeak, float gainReductionDb) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
    const float currentReduction = gainReductionDbAtomic.load();
    const float coeff = gainReductionDb > currentReduction ? meterAttack : meterRelease;
    gainReductionDbAtomic.store(currentReduction + (gainReductionDb - currentReduction) * coeff);
}

void PluginProcessor::pushVisualizerSample(float inputPeak, float inputBody, float outputPeak, float outputBody, float gainReductionDb) noexcept
{
    constexpr float peakDecay = 0.985f;
    constexpr float bodyDecay = 0.992f;
    constexpr float grDecay = 0.94f;
    heldInputPeak = juce::jmax(inputPeak, heldInputPeak * peakDecay);
    heldInputBody = juce::jmax(inputBody, heldInputBody * bodyDecay);
    heldOutputPeak = juce::jmax(outputPeak, heldOutputPeak * peakDecay);
    heldOutputBody = juce::jmax(outputBody, heldOutputBody * bodyDecay);
    heldGainReduction = juce::jmax(gainReductionDb, heldGainReduction * grDecay);

    const int writePosition = visualizerWritePosition.load();
    inputVisualizerPeak[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldInputPeak));
    inputVisualizerBody[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldInputBody));
    outputVisualizerPeak[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldOutputPeak));
    outputVisualizerBody[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldOutputBody));
    gainReductionVisualizer[(size_t) writePosition].store(juce::jlimit(0.0f, 18.0f, heldGainReduction));
    visualizerWritePosition.store((writePosition + 1) % visualizerPointCount);
}

void PluginProcessor::updateVisualizerSnapshot(const juce::AudioBuffer<float>& inputBuffer,
                                               const juce::AudioBuffer<float>& outputBuffer,
                                               int numChannels,
                                               float gainReductionDb) noexcept
{
    const int numSamples = juce::jmin(inputBuffer.getNumSamples(), outputBuffer.getNumSamples());
    if (numChannels <= 0 || numSamples <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputValue = 0.0f;
        float outputValue = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            inputValue += inputBuffer.getSample(channel, sample);
            outputValue += outputBuffer.getSample(channel, sample);
        }

        inputValue /= (float) numChannels;
        outputValue /= (float) numChannels;
        const float inputAbs = juce::jlimit(0.0f, 1.2f, std::abs(inputValue));
        const float outputAbs = juce::jlimit(0.0f, 1.2f, std::abs(outputValue));

        inputPeakAccumulator = juce::jmax(inputPeakAccumulator, inputAbs);
        outputPeakAccumulator = juce::jmax(outputPeakAccumulator, outputAbs);
        inputEnergyAccumulator += inputAbs * inputAbs;
        outputEnergyAccumulator += outputAbs * outputAbs;
        gainReductionAccumulator = juce::jmax(gainReductionAccumulator, gainReductionDb);
        ++visualizerSamplesAccumulated;

        if (visualizerSamplesAccumulated >= visualizerStride)
        {
            const float normaliser = 1.0f / (float) visualizerSamplesAccumulated;
            pushVisualizerSample(inputPeakAccumulator,
                                 std::sqrt(inputEnergyAccumulator * normaliser),
                                 outputPeakAccumulator,
                                 std::sqrt(outputEnergyAccumulator * normaliser),
                                 gainReductionAccumulator);
            visualizerSamplesAccumulated = 0;
            inputPeakAccumulator = 0.0f;
            inputEnergyAccumulator = 0.0f;
            outputPeakAccumulator = 0.0f;
            outputEnergyAccumulator = 0.0f;
            gainReductionAccumulator = 0.0f;
        }
    }
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    updateLatencyCompensation();
    updatePresetDirtyState();
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stupid Simple Plugins")
        .getChildFile("SSP Limiter Presets");
}

PluginProcessor::PresetRecord PluginProcessor::buildCurrentPresetRecord(const juce::String& presetName,
                                                                        const juce::String& category,
                                                                        const juce::String& author,
                                                                        bool isFactory) const
{
    PresetRecord preset;
    preset.name = sanitisePresetName(presetName);
    preset.category = sanitisePresetCategory(category);
    preset.author = author.isNotEmpty() ? author : "SSP User";
    preset.isFactory = isFactory;
    preset.id = makePresetKey(isFactory, preset.category, preset.name);
    preset.state = makePresetState(apvts.getRawParameterValue("inputDb")->load(),
                                   juce::roundToInt(apvts.getRawParameterValue("limitType")->load()),
                                   apvts.getRawParameterValue("thresholdDb")->load(),
                                   apvts.getRawParameterValue("ceilingDb")->load(),
                                   apvts.getRawParameterValue("releaseMs")->load(),
                                   apvts.getRawParameterValue("lookaheadMs")->load(),
                                   apvts.getRawParameterValue("stereoLink")->load(),
                                   apvts.getRawParameterValue("outputDb")->load(),
                                   apvts.getRawParameterValue("mix")->load());
    return preset;
}

void PluginProcessor::applyPresetRecord(const PresetRecord& preset)
{
    const auto setParam = [this](const juce::String& id, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    setParam("inputDb", getPresetFloat(preset.state, "inputDb", 0.0f));
    setParam("limitType", (float) getPresetInt(preset.state, "limitType", defaultLimitTypeIndex));
    setParam("thresholdDb", getPresetFloat(preset.state, "thresholdDb", -6.0f));
    setParam("ceilingDb", getPresetFloat(preset.state, "ceilingDb", -0.8f));
    setParam("releaseMs", getPresetFloat(preset.state, "releaseMs", 80.0f));
    setParam("lookaheadMs", getPresetFloat(preset.state, "lookaheadMs", 2.0f));
    setParam("stereoLink", getPresetFloat(preset.state, "stereoLink", 1.0f));
    setParam("outputDb", getPresetFloat(preset.state, "outputDb", 0.0f));
    setParam("mix", getPresetFloat(preset.state, "mix", 1.0f));

    setCurrentPresetMetadata(preset);
    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
}

juce::String PluginProcessor::captureCurrentPresetSnapshot() const
{
    return buildCurrentPresetRecord("Snapshot", "Internal", "SSP", false).state.toXmlString();
}

void PluginProcessor::updatePresetDirtyState()
{
    currentPresetDirty.store(captureCurrentPresetSnapshot() != lastLoadedPresetSnapshot);
}

void PluginProcessor::setCurrentPresetMetadata(const PresetRecord& preset)
{
    currentPresetKey = preset.id;
    currentPresetName = preset.name;
    currentPresetCategory = preset.category;
    currentPresetAuthor = preset.author;
    currentPresetIsFactory = preset.isFactory;
}

void PluginProcessor::initialisePresetTracking()
{
    currentPresetKey.clear();
    currentPresetName.clear();
    currentPresetCategory.clear();
    currentPresetAuthor.clear();
    currentPresetIsFactory = true;
    lastLoadedPresetSnapshot.clear();
    currentPresetDirty.store(false);
}

void PluginProcessor::refreshUserPresets()
{
    userPresets.clearQuick();
    userPresetFiles.clearQuick();

    auto directory = getUserPresetDirectory();
    if (! directory.exists())
        return;

    const auto files = directory.findChildFiles(juce::File::findFiles, false, "*.ssplimiterpreset");
    for (const auto& file : files)
    {
        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml == nullptr || ! xml->hasTagName("SSPLIMITERPRESET"))
            continue;

        auto* stateXml = xml->getChildByName("LimiterPresetState");
        if (stateXml == nullptr)
            continue;

        PresetRecord preset;
        preset.name = sanitisePresetName(xml->getStringAttribute("name"));
        preset.category = sanitisePresetCategory(xml->getStringAttribute("category", "User"));
        preset.author = xml->getStringAttribute("author", "SSP User");
        preset.isFactory = false;
        preset.id = makePresetKey(false, preset.category, preset.name);
        preset.state = juce::ValueTree::fromXml(*stateXml);
        userPresets.add(preset);
        userPresetFiles.add(file);
    }
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, getFactoryPresets().size()))
        return false;
    applyPresetRecord(getFactoryPresets().getReference(index));
    return true;
}

bool PluginProcessor::loadUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresets.size()))
        return false;
    applyPresetRecord(userPresets.getReference(index));
    return true;
}

void PluginProcessor::stepPreset(int delta)
{
    auto presets = getSequentialPresetList();
    if (presets.isEmpty())
        return;

    int currentIndex = -1;
    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets.getReference(i).id == currentPresetKey)
        {
            currentIndex = i;
            break;
        }
    }

    const int nextIndex = juce::jlimit(0, presets.size() - 1, currentIndex < 0 ? 0 : currentIndex + delta);
    const auto& preset = presets.getReference(nextIndex);
    if (preset.isFactory)
    {
        for (int i = 0; i < getFactoryPresets().size(); ++i)
            if (getFactoryPresets().getReference(i).id == preset.id)
                loadFactoryPreset(i);
    }
    else
    {
        refreshUserPresets();
        for (int i = 0; i < userPresets.size(); ++i)
            if (userPresets.getReference(i).id == preset.id)
                loadUserPreset(i);
    }
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName, const juce::String& category)
{
    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    const auto preset = buildCurrentPresetRecord(presetName, category, "SSP User", false);
    juce::XmlElement root("SSPLIMITERPRESET");
    root.setAttribute("name", preset.name);
    root.setAttribute("category", preset.category);
    root.setAttribute("author", preset.author);
    root.addChildElement(preset.state.createXml().release());

    auto file = directory.getChildFile(preset.category.replace("/", "_") + " - " + preset.name + ".ssplimiterpreset");
    if (! file.replaceWithText(root.toString()))
        return false;

    refreshUserPresets();
    setCurrentPresetMetadata(preset);
    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
    return true;
}

bool PluginProcessor::deleteUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresetFiles.size()))
        return false;

    const auto preset = userPresets.getReference(index);
    if (! userPresetFiles.getReference(index).deleteFile())
        return false;

    refreshUserPresets();
    if (preset.id == currentPresetKey)
    {
        currentPresetKey.clear();
        currentPresetName.clear();
        currentPresetCategory.clear();
        currentPresetAuthor.clear();
        currentPresetDirty.store(true);
    }
    return true;
}

bool PluginProcessor::importPresetFromFile(const juce::File& file, bool loadAfterImport)
{
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr || ! xml->hasTagName("SSPLIMITERPRESET"))
        return false;

    auto* stateXml = xml->getChildByName("LimiterPresetState");
    if (stateXml == nullptr)
        return false;

    const auto presetName = sanitisePresetName(xml->getStringAttribute("name", file.getFileNameWithoutExtension()));
    const auto category = sanitisePresetCategory(xml->getStringAttribute("category", "Imported"));
    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    juce::XmlElement output("SSPLIMITERPRESET");
    output.setAttribute("name", presetName);
    output.setAttribute("category", category);
    output.setAttribute("author", xml->getStringAttribute("author", "SSP User"));
    output.addChildElement(new juce::XmlElement(*stateXml));

    auto destination = directory.getChildFile(category.replace("/", "_") + " - " + presetName + ".ssplimiterpreset");
    if (! destination.replaceWithText(output.toString()))
        return false;

    refreshUserPresets();
    if (loadAfterImport)
    {
        for (int i = 0; i < userPresets.size(); ++i)
            if (userPresets.getReference(i).id == makePresetKey(false, category, presetName))
                return loadUserPreset(i);
    }
    return true;
}

bool PluginProcessor::exportCurrentPresetToFile(const juce::File& file) const
{
    const auto preset = buildCurrentPresetRecord(currentPresetName.isNotEmpty() ? currentPresetName : "Current Settings",
                                                 currentPresetCategory.isNotEmpty() ? currentPresetCategory : "Exported",
                                                 currentPresetAuthor.isNotEmpty() ? currentPresetAuthor : "SSP User",
                                                 false);
    juce::XmlElement root("SSPLIMITERPRESET");
    root.setAttribute("name", preset.name);
    root.setAttribute("category", preset.category);
    root.setAttribute("author", preset.author);
    root.addChildElement(preset.state.createXml().release());
    return file.replaceWithText(root.toString());
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root("SSPLIMITERSTATE");
    root.setAttribute("presetKey", currentPresetKey);
    root.setAttribute("presetName", currentPresetName);
    root.setAttribute("presetCategory", currentPresetCategory);
    root.setAttribute("presetAuthor", currentPresetAuthor);
    root.setAttribute("presetFactory", currentPresetIsFactory);
    if (auto state = apvts.copyState().createXml())
        root.addChildElement(state.release());
    copyXmlToBinary(root, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    if (xmlState->hasTagName("SSPLIMITERSTATE"))
    {
        if (auto* paramsXml = xmlState->getChildByName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*paramsXml));

        currentPresetKey = xmlState->getStringAttribute("presetKey");
        currentPresetName = xmlState->getStringAttribute("presetName");
        currentPresetCategory = xmlState->getStringAttribute("presetCategory");
        currentPresetAuthor = xmlState->getStringAttribute("presetAuthor");
        currentPresetIsFactory = xmlState->getBoolAttribute("presetFactory", true);
    }
    else if (xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }

    updateLatencyCompensation();
    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
