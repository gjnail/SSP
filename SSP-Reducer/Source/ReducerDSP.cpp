#include "ReducerDSP.h"

namespace
{
constexpr double minReducerRateHz = 200.0;

float clampNormalised(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float tpdfNoise(juce::Random& random)
{
    return (random.nextFloat() - random.nextFloat());
}

float qualityToCutoff(double reducedSampleRateHz, float quality, double sampleRate)
{
    const auto clampedQuality = std::pow((double) clampNormalised(quality), 0.55);
    const auto reducedNyquist = juce::jlimit(40.0, sampleRate * 0.49, reducedSampleRateHz * 0.495);
    const auto nearBypassCutoff = juce::jlimit(200.0, sampleRate * 0.49, sampleRate * 0.49);
    return (float) juce::jmap(clampedQuality, nearBypassCutoff, reducedNyquist);
}

float quantiseLinear(float input, int bitDepth)
{
    if (bitDepth <= 1)
        return input >= 0.0f ? 1.0f : -1.0f;

    const auto maxCode = (float) ((1 << juce::jmax(1, bitDepth - 1)) - 1);

    if (maxCode <= 0.0f)
        return input >= 0.0f ? 1.0f : -1.0f;

    return juce::jlimit(-1.0f, 1.0f, std::round(input * maxCode) / maxCode);
}

float quantise(float input, int bitDepth, bool dcShiftEnabled)
{
    const auto maxCode = (float) juce::jmax(1, (1 << juce::jmax(1, bitDepth - 1)) - 1);
    const float shift = dcShiftEnabled ? 0.5f / maxCode : 0.0f;
    const float shifted = juce::jlimit(-1.0f, 1.0f, input + shift);
    return juce::jlimit(-1.0f, 1.0f, quantiseLinear(shifted, bitDepth) - shift);
}
}

namespace reducerdsp
{
int bitDepthFromNormalised(float bitsNormalised)
{
    return juce::jlimit(1, 16, juce::roundToInt(1.0f + clampNormalised(bitsNormalised) * 15.0f));
}

double reducedSampleRateFromNormalised(float rateNormalised, double sampleRate)
{
    const auto safeSampleRate = juce::jmax(8000.0, sampleRate);
    const auto ratio = safeSampleRate / minReducerRateHz;
    return minReducerRateHz * std::pow(ratio, clampNormalised(rateNormalised));
}

double adcCutoffFromQuality(double reducedSampleRateHz, float quality, double sampleRate)
{
    return qualityToCutoff(reducedSampleRateHz, quality, sampleRate);
}

double dacCutoffFromQuality(double reducedSampleRateHz, float quality, double sampleRate)
{
    return qualityToCutoff(reducedSampleRateHz, quality, sampleRate);
}

void prepareState(State& state, double sampleRate, int maximumBlockSize)
{
    state.sampleRate = juce::jmax(1.0, sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = state.sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, maximumBlockSize);
    spec.numChannels = 1;

    for (auto& filter : state.preFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency((float) juce::jmin(18000.0, state.sampleRate * 0.45));
    }

    for (auto& filter : state.postFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency((float) juce::jmin(18000.0, state.sampleRate * 0.45));
    }

    resetState(state);
}

void resetState(State& state)
{
    for (auto& channel : state.channels)
    {
        channel.heldSample = 0.0f;
        channel.holdCounter = 0;
        channel.holdSamplesTarget = 1;
    }

    for (auto& filter : state.preFilters)
        filter.reset();

    for (auto& filter : state.postFilters)
        filter.reset();
}

void processWetBuffer(juce::AudioBuffer<float>& buffer, const Parameters& parameters, State& state)
{
    const auto safeSampleRate = juce::jmax(1.0, parameters.sampleRate > 0.0 ? parameters.sampleRate : state.sampleRate);
    const auto reducedRateHz = reducedSampleRateFromNormalised(parameters.rate, safeSampleRate);
    const auto baseHoldSamples = juce::jmax(1, juce::roundToInt((float) (safeSampleRate / reducedRateHz)));
    const auto bitDepth = bitDepthFromNormalised(parameters.bits);
    const auto adcCutoffHz = (float) adcCutoffFromQuality(reducedRateHz, parameters.adcQuality, safeSampleRate);
    const auto dacCutoffHz = (float) dacCutoffFromQuality(reducedRateHz, parameters.dacQuality, safeSampleRate);
    const auto maxCode = (float) juce::jmax(1, (1 << juce::jmax(1, bitDepth - 1)) - 1);
    const auto ditherAmount = clampNormalised(parameters.dither) / juce::jmax(1.0f, maxCode);

    for (auto& filter : state.preFilters)
        filter.setCutoffFrequency(adcCutoffHz);

    for (auto& filter : state.postFilters)
        filter.setCutoffFrequency(dacCutoffHz);

    const auto numChannels = juce::jmin(2, buffer.getNumChannels());
    const auto numSamples = buffer.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& channelState = state.channels[(size_t) channel];
        auto& preFilter = state.preFilters[(size_t) channel];
        auto& postFilter = state.postFilters[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto source = data[sample];

            if (parameters.preFilterEnabled)
                source = preFilter.processSample(0, source);

            if (channelState.holdCounter <= 0)
            {
                const auto dither = tpdfNoise(state.random) * ditherAmount;
                const auto sampleForQuantiser = juce::jlimit(-1.0f, 1.0f, source + dither);
                channelState.heldSample = quantise(sampleForQuantiser, bitDepth, parameters.dcShiftEnabled);
                channelState.holdSamplesTarget = baseHoldSamples;
                channelState.holdCounter = channelState.holdSamplesTarget;
            }

            auto wet = channelState.heldSample;

            if (parameters.postFilterEnabled)
                wet = postFilter.processSample(0, wet);

            data[sample] = wet;
            --channelState.holdCounter;
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
}
}
