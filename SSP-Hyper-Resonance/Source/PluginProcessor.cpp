#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.34f;
constexpr float meterRelease = 0.10f;
constexpr int coefficientUpdateInterval = 16;
} // namespace

void PluginProcessor::BiquadState::reset() noexcept
{
    s1 = 0.0f;
    s2 = 0.0f;
}

float PluginProcessor::BiquadState::process(float input, const BiquadCoefficients& coefficients) noexcept
{
    const float output = coefficients.b0 * input + s1;
    s1 = coefficients.b1 * input - coefficients.a1 * output + s2;
    s2 = coefficients.b2 * input - coefficients.a2 * output;
    return output;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("frequencyHz",
                                                                 "Frequency",
                                                                 juce::NormalisableRange<float>(30.0f, 8000.0f, 0.01f, 0.33f),
                                                                 180.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("amount",
                                                                 "Amount",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.58f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("pinch",
                                                                 "Pinch",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.62f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("spread",
                                                                 "Spread",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.14f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputDb",
                                                                 "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
                                                                 0.0f));

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

float PluginProcessor::getResonanceMeterLevel() const noexcept
{
    return resonanceMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeAllPassCoefficients(double sampleRate, float frequencyHz, float q) noexcept
{
    BiquadCoefficients coefficients;

    const float clampedFrequency = juce::jlimit(20.0f, (float) sampleRate * 0.45f, frequencyHz);
    const float clampedQ = juce::jlimit(0.25f, 18.0f, q);
    const float omega = juce::MathConstants<float>::twoPi * clampedFrequency / (float) sampleRate;
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0f * clampedQ);
    const float a0 = 1.0f + alpha;

    if (a0 <= 0.0f)
        return coefficients;

    coefficients.b0 = (1.0f - alpha) / a0;
    coefficients.b1 = (-2.0f * cosine) / a0;
    coefficients.b2 = (1.0f + alpha) / a0;
    coefficients.a1 = (-2.0f * cosine) / a0;
    coefficients.a2 = (1.0f - alpha) / a0;
    return coefficients;
}

void PluginProcessor::refreshCoefficients(float frequencyHz, float pinch, float amount, float spread) noexcept
{
    const int newStageCount = juce::jlimit(2, maxStages, 2 + juce::roundToInt(amount * (float) (maxStages - 2)));

    if (newStageCount != activeStageCount)
    {
        const int start = juce::jmin(newStageCount, activeStageCount);
        for (int channel = 0; channel < 2; ++channel)
            for (int stage = start; stage < maxStages; ++stage)
                stageStates[(size_t) channel][(size_t) stage].reset();
    }

    activeStageCount = newStageCount;

    const float spanInOctaves = juce::jmap(pinch, 0.0f, 1.0f, 1.85f, 0.18f);
    const float baseQ = juce::jmap(pinch, 0.0f, 1.0f, 0.45f, 11.5f) * juce::jmap(amount, 0.0f, 1.0f, 0.8f, 1.65f);

    for (int channel = 0; channel < 2; ++channel)
    {
        const float channelOffset = spread * 0.16f * (channel == 0 ? -1.0f : 1.0f);
        const float channelFrequency = juce::jlimit(30.0f,
                                                    (float) currentSampleRate * 0.45f,
                                                    frequencyHz * (1.0f + channelOffset));

        for (int stage = 0; stage < activeStageCount; ++stage)
        {
            const float normalisedIndex = activeStageCount > 1
                                              ? ((float) stage - (float) (activeStageCount - 1) * 0.5f)
                                                    / juce::jmax(1.0f, (float) (activeStageCount - 1) * 0.5f)
                                              : 0.0f;
            const float stageFrequency = channelFrequency * std::pow(2.0f, normalisedIndex * spanInOctaves);
            const float stageQ = juce::jlimit(0.25f,
                                              18.0f,
                                              baseQ * (1.0f - 0.18f * std::abs(normalisedIndex)));

            stageCoefficients[(size_t) channel][(size_t) stage] = makeAllPassCoefficients(currentSampleRate,
                                                                                           stageFrequency,
                                                                                           stageQ);
        }
    }
}

void PluginProcessor::updateMeters(float inputPeak, float resonancePeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(resonanceMeter, juce::jlimit(0.0f, 1.2f, resonancePeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    for (auto& channelStates : stageStates)
        for (auto& state : channelStates)
            state.reset();

    activeStageCount = 2;

    frequencySmoothed.reset(sampleRate, 0.03);
    amountSmoothed.reset(sampleRate, 0.03);
    pinchSmoothed.reset(sampleRate, 0.03);
    spreadSmoothed.reset(sampleRate, 0.04);
    mixSmoothed.reset(sampleRate, 0.03);
    outputSmoothed.reset(sampleRate, 0.03);

    frequencySmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("frequencyHz")->load());
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());
    pinchSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("pinch")->load());
    spreadSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("spread")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    outputSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("outputDb")->load());

    refreshCoefficients(frequencySmoothed.getCurrentValue(),
                        pinchSmoothed.getCurrentValue(),
                        amountSmoothed.getCurrentValue(),
                        spreadSmoothed.getCurrentValue());

    inputMeter.store(0.0f);
    resonanceMeter.store(0.0f);
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

    if (numChannels == 0 || numSamples == 0)
        return;

    frequencySmoothed.setTargetValue(apvts.getRawParameterValue("frequencyHz")->load());
    amountSmoothed.setTargetValue(apvts.getRawParameterValue("amount")->load());
    pinchSmoothed.setTargetValue(apvts.getRawParameterValue("pinch")->load());
    spreadSmoothed.setTargetValue(apvts.getRawParameterValue("spread")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());
    outputSmoothed.setTargetValue(apvts.getRawParameterValue("outputDb")->load());

    float inputPeak = 0.0f;
    float resonancePeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float frequencyHz = frequencySmoothed.getNextValue();
        const float amount = amountSmoothed.getNextValue();
        const float pinch = pinchSmoothed.getNextValue();
        const float spread = spreadSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float outputGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());
        const float phaseDepth = 0.2f + amount * 1.45f;

        if (sample == 0 || (sample % coefficientUpdateInterval) == 0)
            refreshCoefficients(frequencyHz, pinch, amount, spread);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            float dispersed = dry;
            for (int stage = 0; stage < activeStageCount; ++stage)
                dispersed = stageStates[(size_t) channel][(size_t) stage].process(dispersed,
                                                                                  stageCoefficients[(size_t) channel][(size_t) stage]);

            const float phaseDelta = dispersed - dry;
            const float shaped = dry + phaseDelta * phaseDepth;
            const float output = juce::jlimit(-2.0f, 2.0f, (dry * (1.0f - mix) + shaped * mix) * outputGain);

            buffer.setSample(channel, sample, output);

            resonancePeak = juce::jmax(resonancePeak, std::abs(phaseDelta * phaseDepth));
            outputPeak = juce::jmax(outputPeak, std::abs(output));
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, resonancePeak, outputPeak);
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
