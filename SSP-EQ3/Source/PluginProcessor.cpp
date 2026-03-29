#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int lowBandIndex = 0;
constexpr int midBandIndex = 1;
constexpr int highBandIndex = 2;

juce::IIRCoefficients makeIdentity()
{
    return { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
}

double clampFrequency(double sampleRate, double frequency)
{
    return juce::jlimit(20.0, juce::jmax(20.0, sampleRate * 0.495), frequency);
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
                return juce::jmap(clamped * 2.0f, start, 0.0f);

            return juce::jmap((clamped - 0.5f) * 2.0f, 0.0f, end);
        },
        [] (float start, float end, float value)
        {
            const auto clamped = juce::jlimit(start, end, value);
            if (clamped <= 0.0f)
                return 0.5f * (clamped - start) / (0.0f - start);

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

    for (auto& band : filters)
        for (auto& filter : band)
            filter.reset();

    currentCoefficients[(size_t) lowBandIndex] = makeIdentity();
    currentCoefficients[(size_t) midBandIndex] = makeIdentity();
    currentCoefficients[(size_t) highBandIndex] = makeIdentity();
    updateFilterCoefficients(lowGainSmoothed.getCurrentValue(),
                             midGainSmoothed.getCurrentValue(),
                             highGainSmoothed.getCurrentValue());
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

    float previousLow = std::numeric_limits<float>::quiet_NaN();
    float previousMid = std::numeric_limits<float>::quiet_NaN();
    float previousHigh = std::numeric_limits<float>::quiet_NaN();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto lowGain = lowGainSmoothed.getNextValue();
        const auto midGain = midGainSmoothed.getNextValue();
        const auto highGain = highGainSmoothed.getNextValue();

        if (sample == 0
            || std::abs(lowGain - previousLow) > 1.0e-7f
            || std::abs(midGain - previousMid) > 1.0e-7f
            || std::abs(highGain - previousHigh) > 1.0e-7f)
        {
            updateFilterCoefficients(lowGain, midGain, highGain);
            previousLow = lowGain;
            previousMid = midGain;
            previousHigh = highGain;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto input = dryBuffer.getReadPointer(channel)[sample];
            auto wet = filters[(size_t) lowBandIndex][(size_t) channel].processSingleSampleRaw(input);
            wet = filters[(size_t) midBandIndex][(size_t) channel].processSingleSampleRaw(wet);
            wet = filters[(size_t) highBandIndex][(size_t) channel].processSingleSampleRaw(wet);

            buffer.getWritePointer(channel)[sample] = wet;
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

void PluginProcessor::updateFilterCoefficients(float lowGainDb, float midGainDb, float highGainDb)
{
    currentCoefficients[(size_t) lowBandIndex] = makeLowShelfCoefficients(currentSampleRate, lowShelfFrequencyHz, lowGainDb);
    currentCoefficients[(size_t) midBandIndex] = makePeakCoefficients(currentSampleRate, midFrequencyHz, midQ, midGainDb);
    currentCoefficients[(size_t) highBandIndex] = makeHighShelfCoefficients(currentSampleRate, highShelfFrequencyHz, highGainDb);

    for (int channel = 0; channel < 2; ++channel)
    {
        filters[(size_t) lowBandIndex][(size_t) channel].setCoefficients(currentCoefficients[(size_t) lowBandIndex]);
        filters[(size_t) midBandIndex][(size_t) channel].setCoefficients(currentCoefficients[(size_t) midBandIndex]);
        filters[(size_t) highBandIndex][(size_t) channel].setCoefficients(currentCoefficients[(size_t) highBandIndex]);
    }
}

juce::IIRCoefficients PluginProcessor::makeLowShelfCoefficients(double sampleRate, double frequency, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return makeIdentity();

    return juce::IIRCoefficients::makeLowShelf(sampleRate,
                                               (float) clampFrequency(sampleRate, frequency),
                                               shelfQ,
                                               juce::Decibels::decibelsToGain((float) gainDb));
}

juce::IIRCoefficients PluginProcessor::makePeakCoefficients(double sampleRate, double frequency, double q, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return makeIdentity();

    return juce::IIRCoefficients::makePeakFilter(sampleRate,
                                                 (float) clampFrequency(sampleRate, frequency),
                                                 (float) juce::jmax(0.0001, q),
                                                 juce::Decibels::decibelsToGain((float) gainDb));
}

juce::IIRCoefficients PluginProcessor::makeHighShelfCoefficients(double sampleRate, double frequency, double gainDb)
{
    if (std::abs(gainDb) < 1.0e-8)
        return makeIdentity();

    return juce::IIRCoefficients::makeHighShelf(sampleRate,
                                                (float) clampFrequency(sampleRate, frequency),
                                                shelfQ,
                                                juce::Decibels::decibelsToGain((float) gainDb));
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
    const auto low = makeLowShelfCoefficients(sampleRate, lowShelfFrequencyHz, getParameterValue("low"));
    const auto mid = makePeakCoefficients(sampleRate, midFrequencyHz, midQ, getParameterValue("mid"));
    const auto high = makeHighShelfCoefficients(sampleRate, highShelfFrequencyHz, getParameterValue("high"));

    const auto magnitude = getMagnitudeForFrequency(low, frequency, sampleRate)
                           * getMagnitudeForFrequency(mid, frequency, sampleRate)
                           * getMagnitudeForFrequency(high, frequency, sampleRate);

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
