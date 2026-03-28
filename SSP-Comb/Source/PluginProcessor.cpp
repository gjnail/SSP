#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.34f;
constexpr float meterRelease = 0.10f;
constexpr float maxDelaySeconds = 0.05f;

float getLowpassAlpha(double sampleRate, float cutoffHz)
{
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
}
} // namespace

const juce::StringArray& PluginProcessor::getPolarityModeNames() noexcept
{
    static const juce::StringArray names{"Positive", "Negative"};
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("polarityMode",
                                                                  "Polarity",
                                                                  getPolarityModeNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("frequencyHz",
                                                                 "Frequency",
                                                                 juce::NormalisableRange<float>(40.0f, 4000.0f, 0.01f, 0.35f),
                                                                 220.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback",
                                                                 "Feedback",
                                                                 juce::NormalisableRange<float>(0.0f, 0.97f, 0.001f),
                                                                 0.68f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("damp",
                                                                 "Damp",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("spread",
                                                                 "Spread",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.18f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.55f));

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

float PluginProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.load();
}

float PluginProcessor::getCombMeterLevel() const noexcept
{
    return combMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

float PluginProcessor::readDelaySample(const std::vector<float>& buffer, float delayInSamples) const noexcept
{
    if (buffer.empty() || delayBufferSize <= 1)
        return 0.0f;

    float readPosition = (float) writePosition - delayInSamples;
    while (readPosition < 0.0f)
        readPosition += (float) delayBufferSize;

    const int indexA = juce::jlimit(0, delayBufferSize - 1, (int) readPosition);
    const int indexB = (indexA + 1) % delayBufferSize;
    const float fraction = readPosition - (float) indexA;

    return buffer[(size_t) indexA] + (buffer[(size_t) indexB] - buffer[(size_t) indexA]) * fraction;
}

void PluginProcessor::updateMeters(float inputPeak, float combPeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(combMeter, juce::jlimit(0.0f, 1.2f, combPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    delayBufferSize = (int) std::ceil(maxDelaySeconds * sampleRate) + 4;

    for (auto& buffer : delayBuffers)
        buffer.assign((size_t) delayBufferSize, 0.0f);

    dampStates = {0.0f, 0.0f};
    writePosition = 0;

    frequencySmoothed.reset(sampleRate, 0.03);
    feedbackSmoothed.reset(sampleRate, 0.04);
    dampSmoothed.reset(sampleRate, 0.04);
    spreadSmoothed.reset(sampleRate, 0.04);
    mixSmoothed.reset(sampleRate, 0.03);

    frequencySmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("frequencyHz")->load());
    feedbackSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("feedback")->load());
    dampSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("damp")->load());
    spreadSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("spread")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());

    inputMeter.store(0.0f);
    combMeter.store(0.0f);
    outputMeter.store(0.0f);
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

    if (numChannels == 0 || numSamples == 0 || delayBufferSize <= 0)
        return;

    frequencySmoothed.setTargetValue(apvts.getRawParameterValue("frequencyHz")->load());
    feedbackSmoothed.setTargetValue(apvts.getRawParameterValue("feedback")->load());
    dampSmoothed.setTargetValue(apvts.getRawParameterValue("damp")->load());
    spreadSmoothed.setTargetValue(apvts.getRawParameterValue("spread")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());

    const int polarityMode = juce::roundToInt(apvts.getRawParameterValue("polarityMode")->load());
    const float polarity = polarityMode == 0 ? 1.0f : -1.0f;

    float inputPeak = 0.0f;
    float combPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float frequencyHz = frequencySmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float damp = dampSmoothed.getNextValue();
        const float spread = spreadSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        const float baseDelaySamples = juce::jlimit(2.0f,
                                                    (float) delayBufferSize - 2.0f,
                                                    (float) currentSampleRate / juce::jmax(40.0f, frequencyHz));
        const float stereoOffset = spread * 0.16f;
        const float leftDelay = juce::jlimit(2.0f,
                                             (float) delayBufferSize - 2.0f,
                                             baseDelaySamples * (1.0f - stereoOffset));
        const float rightDelay = juce::jlimit(2.0f,
                                              (float) delayBufferSize - 2.0f,
                                              baseDelaySamples * (1.0f + stereoOffset));
        const float dampCutoff = juce::jmap(damp, 18000.0f, 900.0f);
        const float dampAlpha = getLowpassAlpha(currentSampleRate, dampCutoff);

        const std::array<float, 2> delayedSamples{readDelaySample(delayBuffers[0], leftDelay),
                                                  readDelaySample(delayBuffers[1], rightDelay)};

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            dampStates[(size_t) channel] += (delayedSamples[(size_t) channel] - dampStates[(size_t) channel]) * dampAlpha;
            const float resonant = dampStates[(size_t) channel];
            const float combSignal = resonant * polarity;
            const float wet = dry + combSignal;
            const float out = dry * (1.0f - mix) + wet * mix;
            const float writeSample = juce::jlimit(-2.0f, 2.0f, dry + combSignal * feedback);

            delayBuffers[(size_t) channel][(size_t) writePosition] = writeSample;
            buffer.setSample(channel, sample, out);

            combPeak = juce::jmax(combPeak, std::abs(combSignal));
            outputPeak = juce::jmax(outputPeak, std::abs(out));
        }

        writePosition = (writePosition + 1) % delayBufferSize;
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, combPeak, outputPeak);
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
