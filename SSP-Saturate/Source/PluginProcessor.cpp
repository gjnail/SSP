#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
enum class SaturationMode
{
    warm = 0,
    tube,
    tape,
    diode,
    hard
};

float meterDecay(float current, float target) noexcept
{
    return juce::jmax(target, current * 0.92f);
}

float shapeSignal(float sample, float color, float asymmetry, SaturationMode mode) noexcept
{
    switch (mode)
    {
        case SaturationMode::warm:
        {
            const float warm = std::tanh(sample + asymmetry);
            const float brightInput = juce::jlimit(-3.0f, 3.0f, sample + asymmetry * 0.8f);
            const float cubic = brightInput - 0.16f * brightInput * brightInput * brightInput;
            const float bright = juce::jlimit(-1.2f, 1.2f, cubic);
            return juce::jmap(color, warm, bright);
        }

        case SaturationMode::tube:
        {
            const float tubeInput = sample + asymmetry * 1.1f;
            const float soft = std::tanh(tubeInput * juce::jmap(color, 0.0f, 1.0f, 0.9f, 1.8f));
            const float even = tubeInput / (1.0f + std::abs(tubeInput) * juce::jmap(color, 0.0f, 1.0f, 0.65f, 1.4f));
            return juce::jlimit(-1.25f, 1.25f, juce::jmap(0.42f + color * 0.36f, even, soft));
        }

        case SaturationMode::tape:
        {
            const float tapeInput = sample + asymmetry * 0.45f;
            const float compressed = std::atan(tapeInput * juce::jmap(color, 0.0f, 1.0f, 1.0f, 3.0f)) / 1.25f;
            const float rounded = std::tanh((tapeInput - 0.06f * tapeInput * tapeInput * tapeInput) * 0.92f);
            return juce::jlimit(-1.2f, 1.2f, juce::jmap(color * 0.65f, rounded, compressed));
        }

        case SaturationMode::diode:
        {
            const float diodeInput = sample + asymmetry * 1.4f;
            const float fold = diodeInput / (1.0f + std::abs(diodeInput) * juce::jmap(color, 0.0f, 1.0f, 1.8f, 4.0f));
            const float clipped = juce::jlimit(-1.0f, 1.0f, diodeInput * juce::jmap(color, 0.0f, 1.0f, 1.1f, 2.8f));
            return juce::jlimit(-1.15f, 1.15f, juce::jmap(0.35f + color * 0.45f, fold, clipped));
        }

        case SaturationMode::hard:
        {
            const float hardInput = sample + asymmetry * 0.9f;
            const float cubic = hardInput - 0.12f * hardInput * hardInput * hardInput;
            const float clipped = juce::jlimit(-1.0f, 1.0f, hardInput * juce::jmap(color, 0.0f, 1.0f, 1.2f, 3.4f));
            return juce::jlimit(-1.1f, 1.1f, juce::jmap(0.30f + color * 0.55f, cubic, clipped));
        }
    }

    return std::tanh(sample + asymmetry);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive",
                                                                 "Drive",
                                                                 juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f),
                                                                 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("color",
                                                                 "Color",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 58.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("bias",
                                                                 "Bias",
                                                                 juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("mode",
                                                                  "Mode",
                                                                  juce::StringArray{"Warm", "Tube", "Tape", "Diode", "Hard"},
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("output",
                                                                 "Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
                                                                 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 100.0f));

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

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

float PluginProcessor::getHeatMeterLevel() const noexcept
{
    return heatMeter.load();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    toneState = {0.0f, 0.0f};
    dcInputState = {0.0f, 0.0f};
    dcOutputState = {0.0f, 0.0f};

    driveSmoothed.reset(sampleRate, 0.03);
    colorSmoothed.reset(sampleRate, 0.03);
    biasSmoothed.reset(sampleRate, 0.03);
    outputSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.reset(sampleRate, 0.03);

    driveSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("drive")->load());
    colorSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("color")->load() / 100.0f);
    biasSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("bias")->load() / 100.0f);
    outputSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("output")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load() / 100.0f);

    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    heatMeter.store(0.0f);
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

void PluginProcessor::updateMeters(float inputPeak, float outputPeak, float heatAmount) noexcept
{
    inputMeter.store(meterDecay(inputMeter.load(), inputPeak));
    outputMeter.store(meterDecay(outputMeter.load(), outputPeak));
    heatMeter.store(meterDecay(heatMeter.load(), heatAmount));
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numSamples == 0 || numChannels == 0)
        return;

    driveSmoothed.setTargetValue(apvts.getRawParameterValue("drive")->load());
    colorSmoothed.setTargetValue(apvts.getRawParameterValue("color")->load() / 100.0f);
    biasSmoothed.setTargetValue(apvts.getRawParameterValue("bias")->load() / 100.0f);
    outputSmoothed.setTargetValue(apvts.getRawParameterValue("output")->load());
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load() / 100.0f);

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float heatPeak = 0.0f;
    const auto mode = (SaturationMode) juce::jlimit(0, 4, juce::roundToInt(apvts.getRawParameterValue("mode")->load()));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float driveDb = driveSmoothed.getNextValue();
        const float color = colorSmoothed.getNextValue();
        const float bias = biasSmoothed.getNextValue();
        const float outputDb = outputSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        const float driveGain = juce::Decibels::decibelsToGain(driveDb);
        const float outputGain = juce::Decibels::decibelsToGain(outputDb);
        const float asymmetry = bias * 0.38f;
        const float cutoffHz = juce::jmap(color, 0.0f, 1.0f, 1400.0f, 15000.0f);
        const float toneCoeff = std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) currentSampleRate);
        const float highBlend = juce::jmap(color, 0.0f, 1.0f, 0.28f, 1.35f);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            const float driven = dry * driveGain;
            float saturated = shapeSignal(driven, color, asymmetry, mode);

            // Remove offset introduced by asymmetric drive so the plugin stays centered.
            const float dcBlocked = saturated - dcInputState[(size_t) channel] + 0.995f * dcOutputState[(size_t) channel];
            dcInputState[(size_t) channel] = saturated;
            dcOutputState[(size_t) channel] = dcBlocked;
            saturated = dcBlocked;

            toneState[(size_t) channel] = saturated + toneCoeff * (toneState[(size_t) channel] - saturated);
            const float low = toneState[(size_t) channel];
            const float high = saturated - low;
            const float coloured = low + high * highBlend;

            const float wet = coloured * outputGain;
            const float mixed = juce::jlimit(-1.5f, 1.5f, dry * (1.0f - mix) + wet * mix);
            buffer.setSample(channel, sample, mixed);

            outputPeak = juce::jmax(outputPeak, std::abs(mixed));
            heatPeak = juce::jmax(heatPeak, juce::jlimit(0.0f, 1.0f, std::abs(coloured - dry) * 0.85f));
        }
    }

    updateMeters(inputPeak, outputPeak, heatPeak);

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
