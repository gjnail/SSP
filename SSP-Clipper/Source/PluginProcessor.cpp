#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.38f;
constexpr float meterRelease = 0.12f;
constexpr int defaultOversamplingIndex = 2;
constexpr int defaultClipTypeIndex = 1;
constexpr float legacyHardModeThreshold = 0.40f;

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

juce::ValueTree makePresetState(float driveDb, float ceilingDb, float softness, float trimDb, float mix, int oversamplingIndex, int clipTypeIndex)
{
    juce::ValueTree state("ClipperPresetState");
    state.setProperty("driveDb", driveDb, nullptr);
    state.setProperty("ceilingDb", ceilingDb, nullptr);
    state.setProperty("softness", softness, nullptr);
    state.setProperty("trimDb", trimDb, nullptr);
    state.setProperty("mix", mix, nullptr);
    state.setProperty("oversampling", oversamplingIndex, nullptr);
    state.setProperty("clipType", clipTypeIndex, nullptr);
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

bool xmlContainsParameterId(const juce::XmlElement& xml, const juce::String& parameterID)
{
    if ((xml.hasAttribute("id") && xml.getStringAttribute("id") == parameterID) || xml.hasTagName(parameterID))
        return true;

    forEachXmlChildElement(xml, child)
        if (xmlContainsParameterId(*child, parameterID))
            return true;

    return false;
}

float applyClipCurve(float normalisedSample, int clipType, float softness)
{
    const float x = juce::jlimit(-1.0f, 1.0f, normalisedSample);
    switch (juce::jlimit(0, 5, clipType))
    {
        case 0: return x;
        case 1:
        {
            const float shapeDrive = 1.0f + softness * 6.0f;
            const float normaliser = (float) std::tanh(shapeDrive);
            const float softClip = normaliser > 0.0001f ? (float) std::tanh(x * shapeDrive) / normaliser : x;
            return juce::jlimit(-1.0f, 1.0f, juce::jmap(softness, x, softClip));
        }
        case 2:
        {
            const float normaliser = (float) std::tanh(4.8f);
            return normaliser > 0.0001f ? (float) std::tanh(x * 4.8f) / normaliser : x;
        }
        case 3: return std::sin(juce::MathConstants<float>::halfPi * x);
        case 4: return juce::jlimit(-1.0f, 1.0f, 1.5f * (x - (x * x * x) / 3.0f));
        case 5:
        default:
        {
            constexpr float drive = 2.35f;
            const float normaliser = std::atan(drive);
            return normaliser > 0.0001f ? std::atan(x * drive) / normaliser : x;
        }
    }
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
    const auto preset = [](const juce::String& name, const juce::String& category, float driveDb, float ceilingDb, float softness, float trimDb, float mix, int oversamplingIndex, int clipTypeIndex)
    {
        PresetRecord item;
        item.name = name;
        item.category = category;
        item.author = "SSP";
        item.isFactory = true;
        item.id = makePresetKey(true, category, name);
        item.state = makePresetState(driveDb, ceilingDb, softness, trimDb, mix, oversamplingIndex, clipTypeIndex);
        return item;
    };

    juce::Array<PresetRecord> presets;
    presets.add(preset("Default Setting", "Core", 0.0f, -0.2f, 0.72f, 0.0f, 1.0f, 2, 1));
    presets.add(preset("Hard Edge", "Core", 6.0f, -0.8f, 0.10f, -1.0f, 1.0f, 2, 0));
    presets.add(preset("Smooth Catch", "Core", 5.0f, -1.2f, 0.82f, -0.5f, 1.0f, 3, 2));
    presets.add(preset("Tape Weight", "Color", 7.5f, -1.4f, 0.76f, -0.8f, 1.0f, 3, 5));
    presets.add(preset("Cubic Crunch", "Color", 8.5f, -2.0f, 0.58f, -1.0f, 0.92f, 2, 4));
    presets.add(preset("Sine Rounder", "Color", 4.0f, -1.0f, 0.70f, 0.0f, 1.0f, 2, 3));
    presets.add(preset("Drum Peak Tamer", "Utility", 9.0f, -3.0f, 0.18f, -2.0f, 1.0f, 4, 0));
    presets.add(preset("Parallel Lift", "Utility", 10.0f, -2.4f, 0.72f, -1.5f, 0.68f, 3, 1));
    return presets;
}
} // namespace

juce::StringArray PluginProcessor::getOversamplingChoices() { return {"1x", "2x", "4x", "8x", "16x", "32x", "64x", "128x"}; }
juce::StringArray PluginProcessor::getClipTypeChoices() { return {"Hard", "Soft", "Smooth", "Sine", "Cubic", "Tube"}; }
const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets() { static const auto presets = buildFactoryPresetLibrary(); return presets; }

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("driveDb", "Drive", juce::NormalisableRange<float>(-18.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ceilingDb", "Ceiling", juce::NormalisableRange<float>(-18.0f, 0.0f, 0.1f), -0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("softness", "Softness", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("trimDb", "Trim", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("clipType", "Clip Type", getClipTypeChoices(), defaultClipTypeIndex));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("oversampling", "Oversampling", getOversamplingChoices(), defaultOversamplingIndex));
    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (size_t i = 1; i < oversamplingEngines.size(); ++i)
        oversamplingEngines[i] = std::make_unique<juce::dsp::Oversampling<float>>(2, i, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    for (auto* id : {"driveDb", "ceilingDb", "softness", "trimDb", "mix", "clipType", "oversampling"})
        apvts.addParameterListener(id, this);
    for (auto& sample : waveformVisualizerBody)
        sample.store(0.0f);
    for (auto& sample : waveformVisualizerPeak)
        sample.store(0.0f);
    initialisePresetTracking();
    refreshUserPresets();
    loadFactoryPreset(0);
}

PluginProcessor::~PluginProcessor()
{
    for (auto* id : {"driveDb", "ceilingDb", "softness", "trimDb", "mix", "clipType", "oversampling"})
        apvts.removeParameterListener(id, this);
}

int PluginProcessor::oversamplingIndexToFactor(int oversamplingIndex) noexcept { return 1 << juce::jlimit(0, 7, oversamplingIndex); }
float PluginProcessor::getInputMeterLevel() const noexcept { return inputMeter.load(); }
float PluginProcessor::getOutputMeterLevel() const noexcept { return outputMeter.load(); }
float PluginProcessor::getClipMeterLevel() const noexcept { return clipMeter.load(); }
int PluginProcessor::getActiveOversamplingIndex() const noexcept { return juce::jlimit(0, 7, activeOversamplingIndex.load()); }
int PluginProcessor::getActiveOversamplingFactor() const noexcept { return oversamplingIndexToFactor(getActiveOversamplingIndex()); }
int PluginProcessor::getActiveClipTypeIndex() const noexcept { return juce::jlimit(0, getClipTypeChoices().size() - 1, juce::roundToInt(apvts.getRawParameterValue("clipType")->load())); }
juce::String PluginProcessor::getActiveOversamplingLabel() const { return getOversamplingChoices()[getActiveOversamplingIndex()]; }
juce::String PluginProcessor::getActiveClipTypeLabel() const { return getClipTypeChoices()[getActiveClipTypeIndex()]; }

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

bool PluginProcessor::matchesCurrentPreset(const PresetRecord& preset) const noexcept { return preset.id.isNotEmpty() && preset.id == currentPresetKey; }

PluginProcessor::VisualizerSnapshot PluginProcessor::getVisualizerSnapshot() const noexcept
{
    VisualizerSnapshot snapshot;
    snapshot.clipAmount = visualizerClipAmount.load();
    snapshot.softness = visualizerSoftness.load();
    snapshot.oversamplingIndex = getActiveOversamplingIndex();
    snapshot.writePosition = visualizerWritePosition.load();
    for (size_t i = 0; i < snapshot.waveformBody.size(); ++i)
    {
        snapshot.waveformBody[i] = waveformVisualizerBody[i].load();
        snapshot.waveformPeak[i] = waveformVisualizerPeak[i].load();
    }
    return snapshot;
}

void PluginProcessor::pushVisualizerSample(float waveformPeak, float waveformBody) noexcept
{
    constexpr float peakDecay = 0.985f;
    constexpr float bodyDecay = 0.992f;
    heldWaveformPeak = juce::jmax(waveformPeak, heldWaveformPeak * peakDecay);
    heldWaveformBody = juce::jmax(waveformBody, heldWaveformBody * bodyDecay);
    const int writePosition = visualizerWritePosition.load();
    waveformVisualizerPeak[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldWaveformPeak));
    waveformVisualizerBody[(size_t) writePosition].store(juce::jlimit(0.0f, 1.2f, heldWaveformBody));
    visualizerWritePosition.store((writePosition + 1) % visualizerPointCount);
}

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

void PluginProcessor::updateVisualizerSnapshot(const juce::AudioBuffer<float>& drivenBuffer, int numChannels, float clipAmount, float softness) noexcept
{
    const int numSamples = drivenBuffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;
    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        float drivenSample = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            drivenSample += drivenBuffer.getSample(channel, sampleIndex);
        drivenSample /= (float) numChannels;
        const float waveformAbs = juce::jlimit(0.0f, 1.2f, std::abs(drivenSample));
        waveformPeakAccumulator = juce::jmax(waveformPeakAccumulator, waveformAbs);
        waveformEnergyAccumulator += waveformAbs * waveformAbs;
        ++visualizerSamplesAccumulated;
        if (visualizerSamplesAccumulated >= visualizerStride)
        {
            const float normaliser = 1.0f / (float) visualizerSamplesAccumulated;
            pushVisualizerSample(waveformPeakAccumulator, std::sqrt(waveformEnergyAccumulator * normaliser));
            visualizerSamplesAccumulated = 0;
            waveformPeakAccumulator = 0.0f;
            waveformEnergyAccumulator = 0.0f;
        }
    }
    const float previousClip = visualizerClipAmount.load();
    visualizerClipAmount.store(previousClip + (juce::jlimit(0.0f, 1.0f, clipAmount) - previousClip) * 0.4f);
    visualizerSoftness.store(juce::jlimit(0.0f, 1.0f, softness));
}

void PluginProcessor::prepareOversamplingEngines(int samplesPerBlock)
{
    const auto maxBlockSize = (size_t) juce::jmax(1, samplesPerBlock);
    for (auto& engine : oversamplingEngines)
    {
        if (engine == nullptr)
            continue;
        engine->reset();
        engine->initProcessing(maxBlockSize);
    }
}

void PluginProcessor::updateActiveOversampling(int newOversamplingIndex, bool resetEngine) noexcept
{
    const int clampedIndex = juce::jlimit(0, 7, newOversamplingIndex);
    activeOversamplingIndex.store(clampedIndex);
    lastOversamplingIndex = clampedIndex;
    if (auto* engine = oversamplingEngines[(size_t) clampedIndex].get())
    {
        if (resetEngine)
            engine->reset();
        const int reportedLatency = (int) std::lround(engine->getLatencyInSamples());
        if (reportedLatency != lastLatencySamples)
        {
            lastLatencySamples = reportedLatency;
            setLatencySamples(reportedLatency);
        }
        return;
    }
    if (lastLatencySamples != 0)
    {
        lastLatencySamples = 0;
        setLatencySamples(0);
    }
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

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1 };
    for (auto& filter : dcFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(12.0f);
        filter.setResonance(0.55f);
    }

    prepareOversamplingEngines(samplesPerBlock);
    updateActiveOversampling(juce::roundToInt(apvts.getRawParameterValue("oversampling")->load()), false);
    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    clipMeter.store(0.0f);
    visualizerClipAmount.store(0.0f);
    visualizerSoftness.store(apvts.getRawParameterValue("softness")->load());
    for (auto& sample : waveformVisualizerBody)
        sample.store(0.0f);
    for (auto& sample : waveformVisualizerPeak)
        sample.store(0.0f);
    visualizerWritePosition.store(0);
    visualizerSamplesAccumulated = 0;
    waveformPeakAccumulator = 0.0f;
    waveformEnergyAccumulator = 0.0f;
    heldWaveformPeak = 0.0f;
    heldWaveformBody = 0.0f;
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

    juce::AudioBuffer<float> drivenPreviewBuffer;
    drivenPreviewBuffer.makeCopyOf(buffer, true);
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
    const int clipTypeIndex = getActiveClipTypeIndex();
    const int selectedOversamplingIndex = juce::jlimit(0, 7, juce::roundToInt(apvts.getRawParameterValue("oversampling")->load()));

    if (selectedOversamplingIndex != lastOversamplingIndex)
        updateActiveOversampling(selectedOversamplingIndex, true);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* driven = drivenPreviewBuffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
            driven[sample] *= driveGain;
    }

    auto wetBlock = juce::dsp::AudioBlock<float>(buffer).getSubsetChannelBlock(0, (size_t) numChannels);
    auto* oversampling = oversamplingEngines[(size_t) selectedOversamplingIndex].get();
    auto oversampledBlock = oversampling != nullptr ? oversampling->processSamplesUp(wetBlock) : wetBlock;
    const int oversampledSamples = (int) oversampledBlock.getNumSamples();
    int clippedSamples = 0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = oversampledBlock.getChannelPointer((size_t) channel);
        for (int sample = 0; sample < oversampledSamples; ++sample)
        {
            const float normalisedInput = (channelData[sample] * driveGain) / ceilingGain;
            channelData[sample] = applyClipCurve(normalisedInput, clipTypeIndex, softness) * ceilingGain;
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
            const float trimmedWet = filteredWet * trimGain;
            wet[sample] = dry[sample] * (1.0f - mix) + trimmedWet * mix;
            outputPeak = juce::jmax(outputPeak, std::abs(wet[sample]));
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const float clipRatio = oversampledSamples > 0 ? (float) clippedSamples / (float) juce::jmax(1, numChannels * oversampledSamples) : 0.0f;
    updateMeters(inputPeak, outputPeak, clipRatio);
    updateVisualizerSnapshot(drivenPreviewBuffer, numChannels, clipRatio, softness);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root("SSPCLIPPERSTATE");
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

    juce::XmlElement* paramsXml = xmlState.get();
    const bool hasPresetMetadata = xmlState->hasTagName("SSPCLIPPERSTATE");
    if (hasPresetMetadata)
    {
        paramsXml = xmlState->getChildByName(apvts.state.getType());
        if (paramsXml == nullptr)
            return;
    }

    const bool hadClipType = xmlContainsParameterId(*paramsXml, "clipType");
    if (paramsXml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*paramsXml));

    if (! hadClipType)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter("clipType")))
        {
            const int legacyType = apvts.getRawParameterValue("softness")->load() < legacyHardModeThreshold ? 0 : 1;
            param->setValueNotifyingHost(param->convertTo0to1((float) legacyType));
        }
    }

    updateActiveOversampling(juce::roundToInt(apvts.getRawParameterValue("oversampling")->load()), false);
    visualizerSoftness.store(apvts.getRawParameterValue("softness")->load());

    if (hasPresetMetadata)
    {
        currentPresetKey = xmlState->getStringAttribute("presetKey");
        currentPresetName = xmlState->getStringAttribute("presetName");
        currentPresetCategory = xmlState->getStringAttribute("presetCategory");
        currentPresetAuthor = xmlState->getStringAttribute("presetAuthor");
        currentPresetIsFactory = xmlState->getBoolAttribute("presetFactory", true);
    }

    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    updatePresetDirtyState();
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stupid Simple Plugins")
        .getChildFile("SSP Clipper Presets");
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
    preset.state = makePresetState(apvts.getRawParameterValue("driveDb")->load(),
                                   apvts.getRawParameterValue("ceilingDb")->load(),
                                   apvts.getRawParameterValue("softness")->load(),
                                   apvts.getRawParameterValue("trimDb")->load(),
                                   apvts.getRawParameterValue("mix")->load(),
                                   juce::roundToInt(apvts.getRawParameterValue("oversampling")->load()),
                                   juce::roundToInt(apvts.getRawParameterValue("clipType")->load()));
    return preset;
}

void PluginProcessor::applyPresetRecord(const PresetRecord& preset)
{
    const auto setParam = [this](const juce::String& id, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    setParam("driveDb", getPresetFloat(preset.state, "driveDb", apvts.getRawParameterValue("driveDb")->load()));
    setParam("ceilingDb", getPresetFloat(preset.state, "ceilingDb", apvts.getRawParameterValue("ceilingDb")->load()));
    setParam("softness", getPresetFloat(preset.state, "softness", apvts.getRawParameterValue("softness")->load()));
    setParam("trimDb", getPresetFloat(preset.state, "trimDb", apvts.getRawParameterValue("trimDb")->load()));
    setParam("mix", getPresetFloat(preset.state, "mix", apvts.getRawParameterValue("mix")->load()));
    setParam("oversampling", (float) getPresetInt(preset.state, "oversampling", defaultOversamplingIndex));
    setParam("clipType", (float) getPresetInt(preset.state, "clipType", defaultClipTypeIndex));
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

    const auto files = directory.findChildFiles(juce::File::findFiles, false, "*.sspclipperpreset");
    for (const auto& file : files)
    {
        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml == nullptr || ! xml->hasTagName("SSPCLIPPERPRESET"))
            continue;
        auto* stateXml = xml->getChildByName("ClipperPresetState");
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
    juce::XmlElement root("SSPCLIPPERPRESET");
    root.setAttribute("name", preset.name);
    root.setAttribute("category", preset.category);
    root.setAttribute("author", preset.author);
    root.addChildElement(preset.state.createXml().release());

    auto file = directory.getChildFile(preset.category.replace("/", "_") + " - " + preset.name + ".sspclipperpreset");
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
    if (xml == nullptr || ! xml->hasTagName("SSPCLIPPERPRESET"))
        return false;
    auto* stateXml = xml->getChildByName("ClipperPresetState");
    if (stateXml == nullptr)
        return false;

    const auto presetName = sanitisePresetName(xml->getStringAttribute("name", file.getFileNameWithoutExtension()));
    const auto category = sanitisePresetCategory(xml->getStringAttribute("category", "Imported"));
    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    juce::XmlElement output("SSPCLIPPERPRESET");
    output.setAttribute("name", presetName);
    output.setAttribute("category", category);
    output.setAttribute("author", xml->getStringAttribute("author", "SSP User"));
    output.addChildElement(new juce::XmlElement(*stateXml));

    auto destination = directory.getChildFile(category.replace("/", "_") + " - " + presetName + ".sspclipperpreset");
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
    juce::XmlElement root("SSPCLIPPERPRESET");
    root.setAttribute("name", preset.name);
    root.setAttribute("category", preset.category);
    root.setAttribute("author", preset.author);
    root.addChildElement(preset.state.createXml().release());
    return file.replaceWithText(root.toString());
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
