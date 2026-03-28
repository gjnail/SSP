#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int lowBandIndex = 0;
constexpr int midBandIndex = 1;
constexpr int highBandIndex = 2;
constexpr double pi = 3.14159265358979323846;

double clampFrequency(double sampleRate, double frequency)
{
    return juce::jlimit(20.0, juce::jmax(20.0, sampleRate * 0.495), frequency);
}

double computeShelfAlpha(double a, double w0, double slope)
{
    const auto sinW0 = std::sin(w0);
    const auto shelfTerm = (a + (1.0 / a)) * ((1.0 / slope) - 1.0) + 2.0;
    return sinW0 * 0.5 * std::sqrt(juce::jmax(0.0, shelfTerm));
}

PluginProcessor::BiquadCoefficients normalise(double b0, double b1, double b2, double a0, double a1, double a2)
{
    PluginProcessor::BiquadCoefficients coefficients;
    coefficients.b0 = b0 / a0;
    coefficients.b1 = b1 / a0;
    coefficients.b2 = b2 / a0;
    coefficients.a1 = a1 / a0;
    coefficients.a2 = a2 / a0;
    return coefficients;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    const auto gainRange = juce::NormalisableRange<float>(minGainDb, maxGainDb, 0.01f);

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("low", "Low", gainRange, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mid", "Mid", gainRange, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("high", "High", gainRange, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("power", "Power", true));

    return { parameters.begin(), parameters.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    lowGainSmoothed.reset(currentSampleRate, 0.045);
    midGainSmoothed.reset(currentSampleRate, 0.045);
    highGainSmoothed.reset(currentSampleRate, 0.045);
    powerMixSmoothed.reset(currentSampleRate, 0.025);

    lowGainSmoothed.setCurrentAndTargetValue(getParameterValue("low"));
    midGainSmoothed.setCurrentAndTargetValue(getParameterValue("mid"));
    highGainSmoothed.setCurrentAndTargetValue(getParameterValue("high"));
    powerMixSmoothed.setCurrentAndTargetValue(isPoweredOn() ? 1.0f : 0.0f);

    for (auto& bandStates : filterStates)
        for (auto& state : bandStates)
            state.reset();
}

void PluginProcessor::releaseResources()
{
}

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

    if (numSamples <= 0 || numChannels <= 0)
        return;

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    lowGainSmoothed.setTargetValue(getParameterValue("low"));
    midGainSmoothed.setTargetValue(getParameterValue("mid"));
    highGainSmoothed.setTargetValue(getParameterValue("high"));
    powerMixSmoothed.setTargetValue(isPoweredOn() ? 1.0f : 0.0f);

    float previousLow = std::numeric_limits<float>::quiet_NaN();
    float previousMid = std::numeric_limits<float>::quiet_NaN();
    float previousHigh = std::numeric_limits<float>::quiet_NaN();

    BiquadCoefficients lowCoefficients;
    BiquadCoefficients midCoefficients;
    BiquadCoefficients highCoefficients;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto lowGain = lowGainSmoothed.getNextValue();
        const auto midGain = midGainSmoothed.getNextValue();
        const auto highGain = highGainSmoothed.getNextValue();
        const auto powerMix = powerMixSmoothed.getNextValue();

        if (sample == 0
            || std::abs(lowGain - previousLow) > 1.0e-7f
            || std::abs(midGain - previousMid) > 1.0e-7f
            || std::abs(highGain - previousHigh) > 1.0e-7f)
        {
            lowCoefficients = makeLowShelfCoefficients(currentSampleRate, lowShelfFrequencyHz, lowGain);
            midCoefficients = makePeakCoefficients(currentSampleRate, midFrequencyHz, midQ, midGain);
            highCoefficients = makeHighShelfCoefficients(currentSampleRate, highShelfFrequencyHz, highGain);
            previousLow = lowGain;
            previousMid = midGain;
            previousHigh = highGain;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto input = dryBuffer.getReadPointer(channel)[sample];
            auto wet = filterStates[(size_t) lowBandIndex][(size_t) channel].processSample(input, lowCoefficients);
            wet = filterStates[(size_t) midBandIndex][(size_t) channel].processSample(wet, midCoefficients);
            wet = filterStates[(size_t) highBandIndex][(size_t) channel].processSample(wet, highCoefficients);

            buffer.getWritePointer(channel)[sample] = juce::jlimit(-1.0f, 1.0f, input + (wet - input) * powerMix);
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

float PluginProcessor::getParameterValue(const juce::String& parameterId) const
{
    if (auto* value = apvts.getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

bool PluginProcessor::isPoweredOn() const
{
    return getParameterValue("power") >= 0.5f;
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeLowShelfCoefficients(double sampleRate, double frequency, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return {};

    const auto a = std::pow(10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * clampFrequency(sampleRate, frequency) / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = computeShelfAlpha(a, w0, 1.0);
    const auto beta = 2.0 * std::sqrt(a) * alpha;

    const auto b0 = a * ((a + 1.0) - (a - 1.0) * cosW0 + beta);
    const auto b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosW0);
    const auto b2 = a * ((a + 1.0) - (a - 1.0) * cosW0 - beta);
    const auto a0 = (a + 1.0) + (a - 1.0) * cosW0 + beta;
    const auto a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosW0);
    const auto a2 = (a + 1.0) + (a - 1.0) * cosW0 - beta;

    return normalise(b0, b1, b2, a0, a1, a2);
}

PluginProcessor::BiquadCoefficients PluginProcessor::makePeakCoefficients(double sampleRate, double frequency, double q, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return {};

    const auto a = std::pow(10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * clampFrequency(sampleRate, frequency) / sampleRate;
    const auto alpha = std::sin(w0) / (2.0 * juce::jmax(0.0001, (double) q));
    const auto cosW0 = std::cos(w0);

    const auto b0 = 1.0 + alpha * a;
    const auto b1 = -2.0 * cosW0;
    const auto b2 = 1.0 - alpha * a;
    const auto a0 = 1.0 + alpha / a;
    const auto a1 = -2.0 * cosW0;
    const auto a2 = 1.0 - alpha / a;

    return normalise(b0, b1, b2, a0, a1, a2);
}

PluginProcessor::BiquadCoefficients PluginProcessor::makeHighShelfCoefficients(double sampleRate, double frequency, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return {};

    const auto a = std::pow(10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * clampFrequency(sampleRate, frequency) / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = computeShelfAlpha(a, w0, 1.0);
    const auto beta = 2.0 * std::sqrt(a) * alpha;

    const auto b0 = a * ((a + 1.0) + (a - 1.0) * cosW0 + beta);
    const auto b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW0);
    const auto b2 = a * ((a + 1.0) + (a - 1.0) * cosW0 - beta);
    const auto a0 = (a + 1.0) - (a - 1.0) * cosW0 + beta;
    const auto a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cosW0);
    const auto a2 = (a + 1.0) - (a - 1.0) * cosW0 - beta;

    return normalise(b0, b1, b2, a0, a1, a2);
}

double PluginProcessor::getMagnitudeForFrequency(const BiquadCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto clampedFrequency = clampFrequency(sampleRate, frequency);
    const auto omega = 2.0 * pi * clampedFrequency / sampleRate;
    const std::complex<double> z1(std::cos(omega), -std::sin(omega));
    const std::complex<double> z2(std::cos(2.0 * omega), -std::sin(2.0 * omega));

    const auto numerator = coefficients.b0 + coefficients.b1 * z1 + coefficients.b2 * z2;
    const auto denominator = 1.0 + coefficients.a1 * z1 + coefficients.a2 * z2;
    const auto magnitude = std::abs(numerator / denominator);

    return juce::jmax(0.0, magnitude);
}

double PluginProcessor::getResponseForFrequency(double frequency) const
{
    if (!isPoweredOn())
        return 0.0;

    const auto sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const auto low = makeLowShelfCoefficients(sampleRate, lowShelfFrequencyHz, getParameterValue("low"));
    const auto mid = makePeakCoefficients(sampleRate, midFrequencyHz, midQ, getParameterValue("mid"));
    const auto high = makeHighShelfCoefficients(sampleRate, highShelfFrequencyHz, getParameterValue("high"));

    const auto magnitude = getMagnitudeForFrequency(low, frequency, sampleRate)
                           * getMagnitudeForFrequency(mid, frequency, sampleRate)
                           * getMagnitudeForFrequency(high, frequency, sampleRate);

    return juce::Decibels::gainToDecibels((float) magnitude, minGainDb);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
