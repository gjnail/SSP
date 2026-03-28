#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <complex>

namespace
{
juce::String pointParamId(int index, const juce::String& suffix)
{
    return "p" + juce::String(index) + suffix;
}

float getRawFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id);
bool getRawBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id);
int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id);
juce::IIRCoefficients makeCoefficients(const PluginProcessor::EqPoint& point, double sampleRate);

juce::IIRCoefficients makeIdentity()
{
    return {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
}

PluginProcessor::PointArray readPointsFromState(juce::AudioProcessorValueTreeState& apvts)
{
    PluginProcessor::PointArray points{};
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        auto& point = points[(size_t) i];
        point.enabled = getRawBoolParam(apvts, pointParamId(i, "Enabled"));
        point.frequency = getRawFloatParam(apvts, pointParamId(i, "Freq"));
        point.gainDb = getRawFloatParam(apvts, pointParamId(i, "Gain"));
        point.q = getRawFloatParam(apvts, pointParamId(i, "Q"));
        point.type = getChoiceIndex(apvts, pointParamId(i, "Type"));
    }

    return points;
}

std::array<juce::IIRCoefficients, PluginProcessor::maxPoints> buildCoefficientArray(const PluginProcessor::PointArray& points, double sampleRate)
{
    std::array<juce::IIRCoefficients, PluginProcessor::maxPoints> coeffs{};
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
        coeffs[(size_t) i] = points[(size_t) i].enabled ? makeCoefficients(points[(size_t) i], sampleRate) : makeIdentity();

    return coeffs;
}

double getMagnitudeForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = std::polar(1.0, -2.0 * w);

    const auto* c = coefficients.coefficients;
    const std::complex<double> numerator = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
    const std::complex<double> denominator = 1.0 + (double) c[3] * z1 + (double) c[4] * z2;

    const double magnitude = std::abs(numerator / denominator);
    return std::isfinite(magnitude) ? magnitude : 1.0;
}

juce::IIRCoefficients makeCoefficients(const PluginProcessor::EqPoint& point, double sampleRate)
{
    const int type = juce::jlimit(0, PluginProcessor::getPointTypeNames().size() - 1, point.type);
    const float frequency = juce::jlimit(20.0f, 20000.0f, point.frequency);
    const float q = juce::jlimit(0.2f, 10.0f, point.q);
    const auto gain = juce::Decibels::decibelsToGain(point.gainDb);

    switch (type)
    {
        case 1: return juce::IIRCoefficients::makeLowShelf(sampleRate, frequency, q, gain);
        case 2: return juce::IIRCoefficients::makeHighShelf(sampleRate, frequency, q, gain);
        case 3: return juce::IIRCoefficients::makeHighPass(sampleRate, frequency, q);
        case 4: return juce::IIRCoefficients::makeLowPass(sampleRate, frequency, q);
        case 0:
        default:
            return juce::IIRCoefficients::makePeakFilter(sampleRate, frequency, q, gain);
    }
}

float getRawFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = apvts.getRawParameterValue(id))
        return param->load();

    return 0.0f;
}

bool getRawBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return getRawFloatParam(apvts, id) >= 0.5f;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return param->getIndex();

    return 0;
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* param = apvts.getParameter(id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(value ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
}

void setFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1(value);
        if (auto* raw = apvts.getRawParameterValue(id); raw != nullptr && std::abs(raw->load() - normalised) < 0.0001f)
            return;

        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

void setFloatParamFast(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1(value);
        if (auto* raw = apvts.getRawParameterValue(id); raw != nullptr && std::abs(raw->load() - normalised) < 0.0001f)
            return;

        param->setValueNotifyingHost(normalised);
    }
}

void setChoiceParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1((float) index);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int i = 0; i < maxPoints; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(pointParamId(i, "Enabled"), pointParamId(i, "Enabled"), false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Freq"),
            pointParamId(i, "Freq"),
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f),
            250.0f * (float) (i + 1)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Gain"),
            pointParamId(i, "Gain"),
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
            0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Q"),
            pointParamId(i, "Q"),
            juce::NormalisableRange<float>(0.2f, 10.0f, 0.01f, 0.45f),
            1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "Type"),
            pointParamId(i, "Type"),
            getPointTypeNames(),
            0));
    }

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    spectrumData.fill(-96.0f);
    syncPointCacheFromParameters();
}

PluginProcessor::~PluginProcessor() = default;

const juce::StringArray& PluginProcessor::getPointTypeNames()
{
    static const juce::StringArray names{"Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut"};
    return names;
}

PluginProcessor::EqPoint PluginProcessor::getPoint(int index) const
{
    const auto points = getPointsSnapshot();
    return juce::isPositiveAndBelow(index, maxPoints) ? points[(size_t) index] : EqPoint{};
}

PluginProcessor::PointArray PluginProcessor::getPointsSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return pointCache;
}

void PluginProcessor::setPoint(int index, const EqPoint& point)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    setBoolParam(apvts, pointParamId(index, "Enabled"), point.enabled);
    setFloatParam(apvts, pointParamId(index, "Freq"), juce::jlimit(20.0f, 20000.0f, point.frequency));
    setFloatParam(apvts, pointParamId(index, "Gain"), juce::jlimit(-24.0f, 24.0f, point.gainDb));
    setFloatParam(apvts, pointParamId(index, "Q"), juce::jlimit(0.2f, 10.0f, point.q));
    setChoiceParam(apvts, pointParamId(index, "Type"), juce::jlimit(0, getPointTypeNames().size() - 1, point.type));
    syncPointCacheFromParameters();
}

void PluginProcessor::setPointPosition(int index, float frequency, float gainDb)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    const float clampedFrequency = juce::jlimit(20.0f, 20000.0f, frequency);
    const float clampedGain = juce::jlimit(-24.0f, 24.0f, gainDb);
    setFloatParamFast(apvts, pointParamId(index, "Freq"), clampedFrequency);
    setFloatParamFast(apvts, pointParamId(index, "Gain"), clampedGain);

    auto points = getPointsSnapshot();
    if (juce::isPositiveAndBelow(index, maxPoints))
    {
        points[(size_t) index].frequency = clampedFrequency;
        points[(size_t) index].gainDb = clampedGain;
        const auto coeffs = buildCoefficientArray(points, currentSampleRate);

        const juce::SpinLock::ScopedLockType lock(stateLock);
        pointCache = points;
        coefficientCache = coeffs;
    }
}

int PluginProcessor::addPoint(float frequency, float gainDb)
{
    for (int i = 0; i < maxPoints; ++i)
    {
        if (!getPoint(i).enabled)
        {
            EqPoint point;
            point.enabled = true;
            point.frequency = frequency;
            point.gainDb = gainDb;
            point.q = 1.0f;
            point.type = 0;
            setPoint(i, point);
            return i;
        }
    }

    return -1;
}

void PluginProcessor::removePoint(int index)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    auto point = getPoint(index);
    point.enabled = false;
    setPoint(index, point);
}

float PluginProcessor::getResponseForFrequency(float frequency) const
{
    double magnitude = 1.0;
    std::array<juce::IIRCoefficients, maxPoints> coeffs;

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        coeffs = coefficientCache;
    }

    for (int i = 0; i < maxPoints; ++i)
    {
        magnitude *= getMagnitudeForFrequency(coeffs[(size_t) i], frequency, currentSampleRate);
    }

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

int PluginProcessor::getEnabledPointCount() const
{
    int count = 0;
    const auto points = getPointsSnapshot();
    for (int i = 0; i < maxPoints; ++i)
        if (points[(size_t) i].enabled)
            ++count;
    return count;
}

PluginProcessor::SpectrumArray PluginProcessor::getSpectrumDataCopy() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return spectrumData;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    syncPointCacheFromParameters();

    for (auto& channelFilters : filters)
        for (auto& filter : channelFilters)
            filter.reset();

    spectrumFifo.fill(0.0f);
    fftData.fill(0.0f);
    spectrumData.fill(-96.0f);
    spectrumFifoIndex = 0;
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

    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    syncPointCacheFromParameters();

    {
        std::array<juce::IIRCoefficients, maxPoints> coeffs;
        {
            const juce::SpinLock::ScopedLockType lock(stateLock);
            coeffs = coefficientCache;
        }

        for (int i = 0; i < maxPoints; ++i)
        {
            for (int channel = 0; channel < numChannels; ++channel)
                filters[(size_t) channel][(size_t) i].setCoefficients(coeffs[(size_t) i]);
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float value = data[sample];
                for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
                    value = filters[(size_t) channel][(size_t) pointIndex].processSingleSampleRaw(value);
                data[sample] = value;
            }
        }
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            mono += buffer.getReadPointer(channel)[sample];
        }

        pushSpectrumSample(mono / (float) juce::jmax(1, numChannels));
    }
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

    syncPointCacheFromParameters();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

void PluginProcessor::syncPointCacheFromParameters()
{
    const auto points = readPointsFromState(apvts);
    const auto coeffs = buildCoefficientArray(points, currentSampleRate);

    const juce::SpinLock::ScopedLockType lock(stateLock);
    pointCache = points;
    coefficientCache = coeffs;
}

void PluginProcessor::pushSpectrumSample(float sample) noexcept
{
    spectrumFifo[(size_t) spectrumFifoIndex++] = sample;

    if (spectrumFifoIndex == fftSize)
    {
        performSpectrumAnalysis();
        spectrumFifoIndex = 0;
    }
}

void PluginProcessor::performSpectrumAnalysis()
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::copy(spectrumFifo.begin(), spectrumFifo.end(), fftData.begin());

    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    SpectrumArray latestSpectrum;
    latestSpectrum.fill(-96.0f);

    for (int i = 1; i < fftSize / 2; ++i)
    {
        const float level = juce::jmax(fftData[(size_t) i] / (float) fftSize, 1.0e-5f);
        latestSpectrum[(size_t) i] = juce::Decibels::gainToDecibels(level, -96.0f);
    }

    const juce::SpinLock::ScopedLockType lock(stateLock);
    spectrumData = latestSpectrum;
}
