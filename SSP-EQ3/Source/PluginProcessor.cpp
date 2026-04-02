#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float negativeCutTaper = 4.0f;

double clampFrequency(double sampleRate, double frequency)
{
    return juce::jlimit(20.0, juce::jmax(20.0, sampleRate * 0.495), frequency);
}

std::complex<double> getComplexResponseForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * clampFrequency(sampleRate, frequency) / sampleRate;
    const auto* c = coefficients.coefficients;
    const std::complex<double> z1(std::cos(w), -std::sin(w));
    const std::complex<double> z2(std::cos(2.0 * w), -std::sin(2.0 * w));

    const std::complex<double> numerator = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
    const std::complex<double> denominator = 1.0 + (double) c[3] * z1 + (double) c[4] * z2;

    if (std::abs(denominator) <= 0.0)
        return { 1.0, 0.0 };

    return numerator / denominator;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    const auto snapToStep = [] (float start, float end, float value)
    {
        juce::ignoreUnused(start, end);
        return std::round(value * 100.0f) / 100.0f;
    };

    const auto gainRange = juce::NormalisableRange<float>(
        minGainDb,
        maxGainDb,
        [] (float start, float end, float normalised)
        {
            const auto clamped = juce::jlimit(0.0f, 1.0f, normalised);
            if (clamped <= 0.5f)
            {
                const auto distanceFromCentre = 1.0f - clamped * 2.0f;
                return start * std::pow(distanceFromCentre, negativeCutTaper);
            }

            return juce::jmap((clamped - 0.5f) * 2.0f, 0.0f, end);
        },
        [] (float start, float end, float value)
        {
            const auto clamped = juce::jlimit(start, end, value);
            if (clamped <= 0.0f)
            {
                const auto distanceFromCentre = std::pow(clamped / start, 1.0f / negativeCutTaper);
                return 0.5f * (1.0f - distanceFromCentre);
            }

            return 0.5f + 0.5f * (clamped / end);
        },
        snapToStep);

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("low", "Low", gainRange, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mid", "Mid", gainRange, 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("high", "High", gainRange, 0.0f));

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

    lowGainSmoothed.setCurrentAndTargetValue(getParameterValue("low"));
    midGainSmoothed.setCurrentAndTargetValue(getParameterValue("mid"));
    highGainSmoothed.setCurrentAndTargetValue(getParameterValue("high"));

    for (auto& band : lowBandFilters)
        for (auto& filter : band)
            filter.reset();

    for (auto& band : midBandFilters)
        for (auto& filter : band)
            filter.reset();

    for (auto& band : highBandFilters)
        for (auto& filter : band)
            filter.reset();

    updateFilterCoefficients();
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

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto lowGain = lowGainSmoothed.getNextValue();
        const auto midGain = midGainSmoothed.getNextValue();
        const auto highGain = highGainSmoothed.getNextValue();
        const auto lowGainLinear = juce::Decibels::decibelsToGain(lowGain);
        const auto midGainLinear = juce::Decibels::decibelsToGain(midGain);
        const auto highGainLinear = juce::Decibels::decibelsToGain(highGain);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto input = dryBuffer.getReadPointer(channel)[sample];

            auto low = input;
            for (auto& stage : lowBandFilters)
                low = stage[(size_t) channel].processSingleSampleRaw(low);

            auto mid = input;
            for (auto& stage : midBandFilters)
                mid = stage[(size_t) channel].processSingleSampleRaw(mid);

            auto high = input;
            for (auto& stage : highBandFilters)
                high = stage[(size_t) channel].processSingleSampleRaw(high);

            buffer.getWritePointer(channel)[sample] = low * lowGainLinear
                                                      + mid * midGainLinear
                                                      + high * highGainLinear;
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

void PluginProcessor::updateFilterCoefficients()
{
    lowBandCoefficients[0] = makeLowPassCoefficients(currentSampleRate, lowCrossoverHz);
    lowBandCoefficients[1] = makeLowPassCoefficients(currentSampleRate, lowCrossoverHz);

    midBandCoefficients[0] = makeHighPassCoefficients(currentSampleRate, lowCrossoverHz);
    midBandCoefficients[1] = makeHighPassCoefficients(currentSampleRate, lowCrossoverHz);
    midBandCoefficients[2] = makeLowPassCoefficients(currentSampleRate, highCrossoverHz);
    midBandCoefficients[3] = makeLowPassCoefficients(currentSampleRate, highCrossoverHz);

    highBandCoefficients[0] = makeHighPassCoefficients(currentSampleRate, highCrossoverHz);
    highBandCoefficients[1] = makeHighPassCoefficients(currentSampleRate, highCrossoverHz);

    for (int channel = 0; channel < 2; ++channel)
    {
        for (size_t stage = 0; stage < lowBandFilters.size(); ++stage)
            lowBandFilters[stage][(size_t) channel].setCoefficients(lowBandCoefficients[stage]);

        for (size_t stage = 0; stage < midBandFilters.size(); ++stage)
            midBandFilters[stage][(size_t) channel].setCoefficients(midBandCoefficients[stage]);

        for (size_t stage = 0; stage < highBandFilters.size(); ++stage)
            highBandFilters[stage][(size_t) channel].setCoefficients(highBandCoefficients[stage]);
    }
}

juce::IIRCoefficients PluginProcessor::makeLowPassCoefficients(double sampleRate, double frequency)
{
    return juce::IIRCoefficients::makeLowPass(sampleRate,
                                              (float) clampFrequency(sampleRate, frequency),
                                              crossoverQ);
}

juce::IIRCoefficients PluginProcessor::makeHighPassCoefficients(double sampleRate, double frequency)
{
    return juce::IIRCoefficients::makeHighPass(sampleRate,
                                               (float) clampFrequency(sampleRate, frequency),
                                               crossoverQ);
}

double PluginProcessor::getMagnitudeForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * clampFrequency(sampleRate, frequency) / sampleRate;
    const auto* c = coefficients.coefficients;
    const double cosW = std::cos(w);
    const double sinW = std::sin(w);
    const double cos2W = std::cos(2.0 * w);
    const double sin2W = std::sin(2.0 * w);

    const double numReal = (double) c[0] + (double) c[1] * cosW + (double) c[2] * cos2W;
    const double numImag = -((double) c[1] * sinW + (double) c[2] * sin2W);
    const double denReal = 1.0 + (double) c[3] * cosW + (double) c[4] * cos2W;
    const double denImag = -((double) c[3] * sinW + (double) c[4] * sin2W);

    const double numeratorMagnitude = std::sqrt(numReal * numReal + numImag * numImag);
    const double denominatorMagnitude = std::sqrt(denReal * denReal + denImag * denImag);
    const double magnitude = denominatorMagnitude > 0.0 ? numeratorMagnitude / denominatorMagnitude : 1.0;
    return std::isfinite(magnitude) ? magnitude : 1.0;
}

double PluginProcessor::getResponseForFrequency(double frequency) const
{
    const auto sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const auto lowGain = juce::Decibels::decibelsToGain(getParameterValue("low"));
    const auto midGain = juce::Decibels::decibelsToGain(getParameterValue("mid"));
    const auto highGain = juce::Decibels::decibelsToGain(getParameterValue("high"));

    const auto lowPass = getComplexResponseForFrequency(makeLowPassCoefficients(sampleRate, lowCrossoverHz), frequency, sampleRate);
    const auto highPassLow = getComplexResponseForFrequency(makeHighPassCoefficients(sampleRate, lowCrossoverHz), frequency, sampleRate);
    const auto lowPassHigh = getComplexResponseForFrequency(makeLowPassCoefficients(sampleRate, highCrossoverHz), frequency, sampleRate);
    const auto highPassHigh = getComplexResponseForFrequency(makeHighPassCoefficients(sampleRate, highCrossoverHz), frequency, sampleRate);

    const auto lowBand = lowPass * lowPass * (double) lowGain;
    const auto midBand = highPassLow * highPassLow * lowPassHigh * lowPassHigh * (double) midGain;
    const auto highBand = highPassHigh * highPassHigh * (double) highGain;
    const auto magnitude = std::abs(lowBand + midBand + highBand);

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
