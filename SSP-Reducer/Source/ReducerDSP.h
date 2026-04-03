#pragma once

#include <JuceHeader.h>

namespace reducerdsp
{
struct Parameters
{
    float mix = 1.0f;
    float bits = 0.466667f;
    float rate = 0.48f;
    float dither = 0.0f;
    float adcQuality = 1.0f;
    float dacQuality = 1.0f;
    bool preFilterEnabled = false;
    bool postFilterEnabled = true;
    bool dcShiftEnabled = false;
    double sampleRate = 44100.0;
};

struct ChannelState
{
    float heldSample = 0.0f;
    int holdCounter = 0;
    int holdSamplesTarget = 1;
};

struct State
{
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> preFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> postFilters;
    std::array<ChannelState, 2> channels;
    juce::Random random;
    double sampleRate = 44100.0;
};

int bitDepthFromNormalised(float bitsNormalised);
double reducedSampleRateFromNormalised(float rateNormalised, double sampleRate);
double adcCutoffFromQuality(double reducedSampleRateHz, float quality, double sampleRate);
double dacCutoffFromQuality(double reducedSampleRateHz, float quality, double sampleRate);

void prepareState(State&, double sampleRate, int maximumBlockSize);
void resetState(State&);
void processWetBuffer(juce::AudioBuffer<float>&, const Parameters&, State&);
}
