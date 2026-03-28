#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.34f;
constexpr float meterRelease = 0.10f;
constexpr float maxDelaySeconds = 2.5f;

float getLowpassAlpha(double sampleRate, float cutoffHz)
{
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "timeMs",
        "Time",
        juce::NormalisableRange<float>(60.0f, 900.0f, 1.0f),
        340.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "feedback",
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.92f, 0.001f),
        0.42f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.28f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "color",
        "Color",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "driveDb",
        "Drive",
        juce::NormalisableRange<float>(0.0f, 18.0f, 0.1f),
        4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "flutter",
        "Flutter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.18f));

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

float PluginProcessor::getEchoMeterLevel() const noexcept
{
    return echoMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

float PluginProcessor::readDelaySample(const std::vector<float>& buffer, float delayInSamples) const noexcept
{
    if (buffer.empty())
        return 0.0f;

    float readPosition = (float) writePosition - delayInSamples;
    while (readPosition < 0.0f)
        readPosition += (float) delayBufferSize;

    const int indexA = juce::jlimit(0, delayBufferSize - 1, (int) readPosition);
    const int indexB = (indexA + 1) % delayBufferSize;
    const float fraction = readPosition - (float) indexA;

    return buffer[(size_t) indexA] + (buffer[(size_t) indexB] - buffer[(size_t) indexA]) * fraction;
}

void PluginProcessor::updateMeters(float inputPeak, float echoPeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(echoMeter, juce::jlimit(0.0f, 1.2f, echoPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    delayBufferSize = (int) std::ceil(maxDelaySeconds * sampleRate) + 4;

    for (auto& buffer : delayBuffers)
        buffer.assign((size_t) delayBufferSize, 0.0f);

    lowpassStates = {0.0f, 0.0f};
    highpassStates = {0.0f, 0.0f};
    writePosition = 0;
    lfoPhase = 0.0;

    timeSmoothed.reset(sampleRate, 0.04);
    feedbackSmoothed.reset(sampleRate, 0.05);
    mixSmoothed.reset(sampleRate, 0.04);
    colorSmoothed.reset(sampleRate, 0.06);
    driveSmoothed.reset(sampleRate, 0.05);
    flutterSmoothed.reset(sampleRate, 0.07);

    timeSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("timeMs")->load());
    feedbackSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("feedback")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    colorSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("color")->load());
    driveSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("driveDb")->load());
    flutterSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("flutter")->load());
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

    timeSmoothed.setTargetValue(apvts.getRawParameterValue("timeMs")->load());
    feedbackSmoothed.setTargetValue(apvts.getRawParameterValue("feedback")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());
    colorSmoothed.setTargetValue(apvts.getRawParameterValue("color")->load());
    driveSmoothed.setTargetValue(apvts.getRawParameterValue("driveDb")->load());
    flutterSmoothed.setTargetValue(apvts.getRawParameterValue("flutter")->load());

    float inputPeak = 0.0f;
    float echoPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float timeMs = timeSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float color = colorSmoothed.getNextValue();
        const float drive = juce::Decibels::decibelsToGain(driveSmoothed.getNextValue());
        const float flutter = flutterSmoothed.getNextValue();

        const float baseDelaySamples = juce::jlimit(8.0f,
                                                    (float) delayBufferSize - 2.0f,
                                                    timeMs * (float) currentSampleRate * 0.001f);

        const float modulation = std::sin((float) lfoPhase) * 0.65f
                               + std::sin((float) (lfoPhase * 0.37 + 0.7)) * 0.35f;
        const float modulationDepth = 0.8f + flutter * 8.0f;
        const float leftDelay = juce::jlimit(8.0f,
                                             (float) delayBufferSize - 2.0f,
                                             baseDelaySamples + modulation * modulationDepth);
        const float rightDelay = juce::jlimit(8.0f,
                                              (float) delayBufferSize - 2.0f,
                                              baseDelaySamples * 1.08f - modulation * modulationDepth * 0.75f);

        const float lowpassAlpha = getLowpassAlpha(currentSampleRate, juce::jmap(color, 1000.0f, 8200.0f));
        const float highpassAlpha = getLowpassAlpha(currentSampleRate, juce::jmap(1.0f - color, 35.0f, 180.0f));

        const std::array<float, 2> taps{readDelaySample(delayBuffers[0], leftDelay),
                                        readDelaySample(delayBuffers[1], rightDelay)};

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            lowpassStates[(size_t) channel] += (taps[(size_t) channel] - lowpassStates[(size_t) channel]) * lowpassAlpha;
            highpassStates[(size_t) channel] += (lowpassStates[(size_t) channel] - highpassStates[(size_t) channel]) * highpassAlpha;

            const float filteredRepeat = lowpassStates[(size_t) channel] - highpassStates[(size_t) channel];
            const float saturatedRepeat = std::tanh(filteredRepeat * drive);
            const float writeSample = dry + saturatedRepeat * feedback;

            delayBuffers[(size_t) channel][(size_t) writePosition] = writeSample;

            const float out = dry * (1.0f - mix) + filteredRepeat * mix;
            buffer.setSample(channel, sample, out);

            echoPeak = juce::jmax(echoPeak, std::abs(filteredRepeat));
            outputPeak = juce::jmax(outputPeak, std::abs(out));
        }

        writePosition = (writePosition + 1) % delayBufferSize;
        lfoPhase += juce::MathConstants<double>::twoPi * (0.08 + flutter * 1.6) / currentSampleRate;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, echoPeak, outputPeak);
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
