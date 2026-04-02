#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct RateOption
{
    const char* label;
    double quarterNotes;
};

struct FactoryPresetInfo
{
    const char* name;
    const char* category;
    const char* tags;
    float variation;
    float volumeRangeDb;
    float panRange;
    int rateIndex;
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

constexpr std::array<FactoryPresetInfo, 30> factoryPresetInfo {{
    {"Subtle Humanize", "Natural", "tight subtle pocket", 0.22f, 1.2f, 0.12f, 4},
    {"Crisp Start", "Natural", "clean short controlled", 0.18f, 1.5f, 0.10f, 2},
    {"Soft Width", "Natural", "gentle width sheen", 0.30f, 1.8f, 0.20f, 4},
    {"Lazy Lift", "Natural", "slow open drift", 0.32f, 2.0f, 0.16f, 6},
    {"Tight Bounce", "Groove", "quick groove snap", 0.40f, 2.8f, 0.18f, 2},
    {"Pocket Triplet", "Groove", "triplet pulse human", 0.46f, 3.1f, 0.22f, 1},
    {"Shuffle Air", "Groove", "swing shimmer dance", 0.52f, 2.6f, 0.24f, 3},
    {"Quarter Nudge", "Groove", "laid back quarter", 0.38f, 2.2f, 0.18f, 5},
    {"Wide Sixteenth", "Stereo", "fast width image", 0.54f, 3.0f, 0.38f, 2},
    {"Wide Eighth", "Stereo", "open side motion", 0.58f, 2.8f, 0.44f, 4},
    {"Carousel", "Stereo", "rotating image swirl", 0.62f, 2.4f, 0.52f, 6},
    {"Halo Bar", "Stereo", "slow cinematic width", 0.66f, 2.0f, 0.58f, 8},
    {"Tape Flutter", "Texture", "nervous flutter air", 0.48f, 4.0f, 0.20f, 0},
    {"Nervous Ticks", "Texture", "restless close hats", 0.64f, 5.2f, 0.26f, 0},
    {"Metallic Drift", "Texture", "shimmer scrape sway", 0.56f, 4.8f, 0.34f, 3},
    {"Dusty Barline", "Texture", "bar line wobble", 0.45f, 3.6f, 0.28f, 8},
    {"Pumped Offbeat", "Motion", "breathing offbeat lift", 0.70f, 5.8f, 0.22f, 4},
    {"Hard Duck Hats", "Motion", "ducking percussive chop", 0.78f, 6.4f, 0.16f, 5},
    {"Swirl Pocket", "Motion", "moving width pocket", 0.68f, 4.4f, 0.42f, 3},
    {"Long Sweep", "Motion", "slow phrase movement", 0.60f, 4.0f, 0.40f, 7},
    {"Dance Lift", "Club", "festival uplift energy", 0.74f, 4.8f, 0.30f, 4},
    {"Riser Control", "Club", "half note build", 0.82f, 7.2f, 0.34f, 7},
    {"Festival Wash", "Club", "wide bar rise", 0.86f, 6.0f, 0.48f, 8},
    {"Laser Hats", "Club", "hyper bright sweep", 0.92f, 7.8f, 0.60f, 0},
    {"Glitch Scatter", "Extreme", "triplet scatter chaos", 1.00f, 9.0f, 0.55f, 1},
    {"Panic Spin", "Extreme", "wide panic motion", 0.96f, 10.5f, 0.65f, 3},
    {"Broken Carousel", "Extreme", "unstable half spin", 0.94f, 8.6f, 0.70f, 5},
    {"Wide Damage", "Extreme", "long broken width", 0.88f, 11.5f, 0.72f, 9},
    {"Cinematic Drift", "Longform", "slow phrase bloom", 0.50f, 2.4f, 0.46f, 10},
    {"Outro Melt", "Longform", "slow fade smear", 0.72f, 4.2f, 0.50f, 9}
}};

const FactoryPresetInfo& getFactoryPresetInfoForIndex(int presetIndex)
{
    return factoryPresetInfo[(size_t) juce::jlimit(0, (int) factoryPresetInfo.size() - 1, presetIndex)];
}

float getParameterValue(const juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
{
    if (auto* parameter = state.getRawParameterValue(parameterID))
        return parameter->load();

    return 0.0f;
}

bool valuesDiffer(float a, float b, float tolerance) noexcept
{
    return std::abs(a - b) > tolerance;
}

float normaliseEnvelope(float value) noexcept
{
    return std::sqrt(juce::jlimit(0.0f, 1.0f, value * 1.45f));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    const auto& defaultPreset = factoryPresetInfo.front();
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("variation",
                                                                 "Variation",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 defaultPreset.variation));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("volumeRangeDb",
                                                                 "Volume Range",
                                                                 juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f),
                                                                 defaultPreset.volumeRangeDb));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("panRange",
                                                                 "Pan Range",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 defaultPreset.panRange));

    params.push_back(std::make_unique<juce::AudioParameterInt>("rateIndex",
                                                               "Rate",
                                                               0,
                                                               (int) rateOptions.size() - 1,
                                                               defaultPreset.rateIndex));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    resetVisualizerData();
}

PluginProcessor::~PluginProcessor() = default;

int PluginProcessor::getFactoryPresetCount() noexcept
{
    return (int) factoryPresetInfo.size();
}

juce::StringArray PluginProcessor::getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : factoryPresetInfo)
        names.add(preset.name);
    return names;
}

juce::StringArray PluginProcessor::getFactoryPresetCategories()
{
    juce::StringArray categories;
    for (const auto& preset : factoryPresetInfo)
        categories.addIfNotAlreadyThere(preset.category);
    return categories;
}

juce::String PluginProcessor::getFactoryPresetName(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).name;
}

juce::String PluginProcessor::getFactoryPresetCategory(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).category;
}

juce::String PluginProcessor::getFactoryPresetTags(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).tags;
}

juce::String PluginProcessor::getRateLabel(int rateIndex)
{
    return rateOptions[(size_t) juce::jlimit(0, (int) rateOptions.size() - 1, rateIndex)].label;
}

void PluginProcessor::applyFactoryPreset(int presetIndex)
{
    currentFactoryPreset = juce::jlimit(0, getFactoryPresetCount() - 1, presetIndex);
    const auto& preset = getFactoryPresetInfoForIndex(currentFactoryPreset);

    auto applyParameter = [this](const juce::String& parameterID, float value)
    {
        if (auto* parameter = apvts.getParameter(parameterID))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };

    applyParameter("variation", preset.variation);
    applyParameter("volumeRangeDb", preset.volumeRangeDb);
    applyParameter("panRange", preset.panRange);
    applyParameter("rateIndex", (float) preset.rateIndex);
}

bool PluginProcessor::stepFactoryPreset(int delta)
{
    const int total = getFactoryPresetCount();
    if (total <= 0)
        return false;

    int nextIndex = (currentFactoryPreset + delta) % total;
    if (nextIndex < 0)
        nextIndex += total;

    applyFactoryPreset(nextIndex);
    return true;
}

int PluginProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentFactoryPreset;
}

juce::String PluginProcessor::getCurrentPresetName() const
{
    return getFactoryPresetName(currentFactoryPreset);
}

juce::String PluginProcessor::getCurrentPresetCategory() const
{
    return getFactoryPresetCategory(currentFactoryPreset);
}

juce::String PluginProcessor::getCurrentPresetTags() const
{
    return getFactoryPresetTags(currentFactoryPreset);
}

bool PluginProcessor::isCurrentPresetDirty() const noexcept
{
    const auto& preset = getFactoryPresetInfoForIndex(currentFactoryPreset);
    return valuesDiffer(getParameterValue(apvts, "variation"), preset.variation, 0.0015f)
        || valuesDiffer(getParameterValue(apvts, "volumeRangeDb"), preset.volumeRangeDb, 0.11f)
        || valuesDiffer(getParameterValue(apvts, "panRange"), preset.panRange, 0.0015f)
        || juce::roundToInt(getParameterValue(apvts, "rateIndex")) != preset.rateIndex;
}

float PluginProcessor::getCurrentGainOffsetDb() const noexcept
{
    return currentGainOffsetDb.load();
}

float PluginProcessor::getCurrentPanOffset() const noexcept
{
    return currentPanOffset.load();
}

PluginProcessor::VisualizerData PluginProcessor::getVisualizerData() const noexcept
{
    VisualizerData data;
    const int writePosition = visualizerWritePosition.load(std::memory_order_acquire);
    const int start = writePosition % visualizerPointCount;

    for (int i = 0; i < visualizerPointCount; ++i)
    {
        const int index = (start + i) % visualizerPointCount;
        data.input[(size_t) i] = inputHistory[(size_t) index].load(std::memory_order_relaxed);
        data.output[(size_t) i] = outputHistory[(size_t) index].load(std::memory_order_relaxed);
        data.difference[(size_t) i] = differenceHistory[(size_t) index].load(std::memory_order_relaxed);
        data.gainMotion[(size_t) i] = gainMotionHistory[(size_t) index].load(std::memory_order_relaxed);
        data.panMotion[(size_t) i] = panMotionHistory[(size_t) index].load(std::memory_order_relaxed);
    }

    return data;
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
    visualizerSamplesPerPoint = juce::jlimit(24, 256, juce::roundToInt((float) (sampleRate / 720.0)));
    visualizerAccumulatorCount = 0;
    visualizerInputAccumulator = 0.0f;
    visualizerOutputAccumulator = 0.0f;
    visualizerDifferenceAccumulator = 0.0f;
    visualizerGainAccumulator = 0.0f;
    visualizerPanAccumulator = 0.0f;
    resetVisualizerData();
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
        const float beforeLeft = buffer.getSample(0, sample);
        const float beforeRight = numChannels > 1 ? buffer.getSample(1, sample) : beforeLeft;
        const float beforeMono = 0.5f * (std::abs(beforeLeft) + std::abs(beforeRight));

        const float gainLfo = std::sin((float) modulationPhase);
        const float panLfo = std::sin((float) (modulationPhase + juce::MathConstants<double>::halfPi));
        const float gainDb = gainLfo * volumeRangeDb;
        const float pan = panLfo * panRange;
        currentGainOffsetDb.store(gainDb);
        currentPanOffset.store(pan);

        const float gain = juce::Decibels::decibelsToGain(gainDb);
        float processedLeft = beforeLeft * gain;
        float processedRight = beforeRight * gain;

        if (numChannels > 1)
        {
            const float leftPanGain = pan > 0.0f ? 1.0f - pan : 1.0f;
            const float rightPanGain = pan < 0.0f ? 1.0f + pan : 1.0f;
            processedLeft *= leftPanGain;
            processedRight *= rightPanGain;
        }

        buffer.setSample(0, sample, processedLeft);
        if (numChannels > 1)
            buffer.setSample(1, sample, processedRight);

        const float afterMono = 0.5f * (std::abs(processedLeft) + std::abs(numChannels > 1 ? processedRight : processedLeft));
        pushVisualizerSample(beforeMono, afterMono, gainDb, pan);

        modulationPhase += phaseIncrement;
        if (modulationPhase >= juce::MathConstants<double>::twoPi)
            modulationPhase -= juce::MathConstants<double>::twoPi;
    }

    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear(channel, 0, numSamples);
}

void PluginProcessor::pushVisualizerSample(float inputLevel, float outputLevel, float gainDb, float pan) noexcept
{
    visualizerInputAccumulator += normaliseEnvelope(inputLevel);
    visualizerOutputAccumulator += normaliseEnvelope(outputLevel);
    visualizerDifferenceAccumulator += normaliseEnvelope(std::abs(outputLevel - inputLevel) * 1.6f);
    visualizerGainAccumulator += juce::jlimit(-1.0f, 1.0f, gainDb / 12.0f);
    visualizerPanAccumulator += juce::jlimit(-1.0f, 1.0f, pan);
    ++visualizerAccumulatorCount;

    if (visualizerAccumulatorCount < visualizerSamplesPerPoint)
        return;

    const float scale = 1.0f / (float) visualizerAccumulatorCount;
    const int writePosition = visualizerWritePosition.load(std::memory_order_relaxed);
    const int index = writePosition % visualizerPointCount;

    inputHistory[(size_t) index].store(juce::jlimit(0.0f, 1.0f, visualizerInputAccumulator * scale), std::memory_order_relaxed);
    outputHistory[(size_t) index].store(juce::jlimit(0.0f, 1.0f, visualizerOutputAccumulator * scale), std::memory_order_relaxed);
    differenceHistory[(size_t) index].store(juce::jlimit(0.0f, 1.0f, visualizerDifferenceAccumulator * scale), std::memory_order_relaxed);
    gainMotionHistory[(size_t) index].store(juce::jlimit(-1.0f, 1.0f, visualizerGainAccumulator * scale), std::memory_order_relaxed);
    panMotionHistory[(size_t) index].store(juce::jlimit(-1.0f, 1.0f, visualizerPanAccumulator * scale), std::memory_order_relaxed);
    visualizerWritePosition.store(writePosition + 1, std::memory_order_release);

    visualizerAccumulatorCount = 0;
    visualizerInputAccumulator = 0.0f;
    visualizerOutputAccumulator = 0.0f;
    visualizerDifferenceAccumulator = 0.0f;
    visualizerGainAccumulator = 0.0f;
    visualizerPanAccumulator = 0.0f;
}

void PluginProcessor::resetVisualizerData() noexcept
{
    for (int i = 0; i < visualizerPointCount; ++i)
    {
        inputHistory[(size_t) i].store(0.0f, std::memory_order_relaxed);
        outputHistory[(size_t) i].store(0.0f, std::memory_order_relaxed);
        differenceHistory[(size_t) i].store(0.0f, std::memory_order_relaxed);
        gainMotionHistory[(size_t) i].store(0.0f, std::memory_order_relaxed);
        panMotionHistory[(size_t) i].store(0.0f, std::memory_order_relaxed);
    }

    visualizerWritePosition.store(0, std::memory_order_relaxed);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
    {
        state->setAttribute("currentFactoryPreset", currentFactoryPreset);
        copyXmlToBinary(*state, destData);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            const int restoredPreset = xmlState->getIntAttribute("currentFactoryPreset", currentFactoryPreset);
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            currentFactoryPreset = juce::jlimit(0, getFactoryPresetCount() - 1, restoredPreset);
        }
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
