#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.34f;
constexpr float meterRelease = 0.10f;
constexpr float maxDelaySeconds = 4.5f;

float getLowpassAlpha(double sampleRate, float cutoffHz)
{
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
}

const std::array<float, 8> noteDivisions{{
    0.125f,
    0.25f,
    0.75f,
    0.5f,
    1.5f,
    1.0f,
    2.0f,
    4.0f
}};
} // namespace

const juce::StringArray& PluginProcessor::getTimeModeNames() noexcept
{
    static const juce::StringArray names{"ms", "1/32", "1/16", "1/8D", "1/8", "1/4D", "1/4", "1/2", "1/1"};
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("timeMode",
                                                                  "Timing",
                                                                  getTimeModeNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMs",
                                                                 "Time",
                                                                 juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f),
                                                                 380.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback",
                                                                 "Feedback",
                                                                 juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
                                                                 0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.26f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone",
                                                                 "Tone",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("width",
                                                                 "Width",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.32f));

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

float PluginProcessor::getDelayMeterLevel() const noexcept
{
    return delayMeter.load();
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

float PluginProcessor::getHostBpm() const noexcept
{
    if (auto* currentPlayHead = getPlayHead(); currentPlayHead != nullptr)
        if (const auto position = currentPlayHead->getPosition())
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 1.0)
                return (float) *bpm;

    return 120.0f;
}

float PluginProcessor::getDelayTimeMs() const noexcept
{
    const int mode = juce::roundToInt(apvts.getRawParameterValue("timeMode")->load());
    if (mode <= 0)
        return apvts.getRawParameterValue("timeMs")->load();

    const int noteIndex = juce::jlimit(0, (int) noteDivisions.size() - 1, mode - 1);
    const float quarterNoteMs = 60000.0f / getHostBpm();
    return quarterNoteMs * noteDivisions[(size_t) noteIndex];
}

void PluginProcessor::updateMeters(float inputPeak, float delayPeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(delayMeter, juce::jlimit(0.0f, 1.2f, delayPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    delayBufferSize = (int) std::ceil(maxDelaySeconds * sampleRate) + 4;

    for (auto& buffer : delayBuffers)
        buffer.assign((size_t) delayBufferSize, 0.0f);

    toneStates = {0.0f, 0.0f};
    writePosition = 0;

    timeSmoothed.reset(sampleRate, 0.04);
    feedbackSmoothed.reset(sampleRate, 0.06);
    mixSmoothed.reset(sampleRate, 0.04);
    toneSmoothed.reset(sampleRate, 0.05);
    widthSmoothed.reset(sampleRate, 0.05);

    timeSmoothed.setCurrentAndTargetValue(getDelayTimeMs());
    feedbackSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("feedback")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    toneSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("tone")->load());
    widthSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("width")->load());

    inputMeter.store(0.0f);
    delayMeter.store(0.0f);
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

    timeSmoothed.setTargetValue(getDelayTimeMs());
    feedbackSmoothed.setTargetValue(apvts.getRawParameterValue("feedback")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());
    toneSmoothed.setTargetValue(apvts.getRawParameterValue("tone")->load());
    widthSmoothed.setTargetValue(apvts.getRawParameterValue("width")->load());

    float inputPeak = 0.0f;
    float delayPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float timeMs = timeSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float width = widthSmoothed.getNextValue();

        const float baseDelaySamples = juce::jlimit(8.0f,
                                                    (float) delayBufferSize - 2.0f,
                                                    timeMs * (float) currentSampleRate * 0.001f);

        const float stereoOffset = width * 0.32f;
        const float leftDelay = juce::jlimit(8.0f,
                                             (float) delayBufferSize - 2.0f,
                                             baseDelaySamples * (1.0f - stereoOffset));
        const float rightDelay = juce::jlimit(8.0f,
                                              (float) delayBufferSize - 2.0f,
                                              baseDelaySamples * (1.0f + stereoOffset));
        const float crossfeed = width * 0.22f;
        const float toneCutoff = juce::jmap(tone, 850.0f, 14000.0f);
        const float toneAlpha = getLowpassAlpha(currentSampleRate, toneCutoff);

        const float leftTap = readDelaySample(delayBuffers[0], leftDelay);
        const float rightTap = readDelaySample(delayBuffers[1], rightDelay);

        const float wetLeftRaw = leftTap * (1.0f - crossfeed) + rightTap * crossfeed;
        const float wetRightRaw = rightTap * (1.0f - crossfeed) + leftTap * crossfeed;
        const std::array<float, 2> wetRaw{{wetLeftRaw, wetRightRaw}};

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            toneStates[(size_t) channel] += (wetRaw[(size_t) channel] - toneStates[(size_t) channel]) * toneAlpha;
            const float wet = toneStates[(size_t) channel];
            const float out = dry * (1.0f - mix) + wet * mix;
            const float writeInput = dry + wet * feedback;

            delayBuffers[(size_t) channel][(size_t) writePosition] = std::tanh(writeInput);
            buffer.setSample(channel, sample, out);

            delayPeak = juce::jmax(delayPeak, std::abs(wet));
            outputPeak = juce::jmax(outputPeak, std::abs(out));
        }

        writePosition = (writePosition + 1) % delayBufferSize;
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, delayPeak, outputPeak);
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
