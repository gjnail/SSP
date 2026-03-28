#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float silenceDb = -100.0f;
constexpr float minDisplayedFrequency = 20.0f;
constexpr float maxDisplayedFrequency = 200.0f;
} // namespace

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      forwardFFT(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann, true)
{
    scopeData.fill(silenceDb);
}

PluginProcessor::~PluginProcessor() = default;

float PluginProcessor::getSubPeakDb() const noexcept
{
    return subPeakDb.load();
}

bool PluginProcessor::isSubInRange() const noexcept
{
    return subInRange.load();
}

void PluginProcessor::copySpectrum(std::vector<float>& destination) const
{
    destination.resize(scopeData.size());
    const juce::SpinLock::ScopedLockType lock(spectrumLock);
    std::copy(scopeData.begin(), scopeData.end(), destination.begin());
}

void PluginProcessor::pushNextSampleIntoFifo(float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (!nextFFTBlockReady)
        {
            std::copy(fifo.begin(), fifo.end(), fftData.begin());
            std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
            nextFFTBlockReady = true;
        }

        fifoIndex = 0;
    }

    fifo[(size_t) fifoIndex++] = sample;
}

float PluginProcessor::normaliseMagnitude(float magnitude) noexcept
{
    // Calibrated to sit closer to the reference Ableton spectrum reading.
    return magnitude * (2.1f / (float) fftSize);
}

float PluginProcessor::frequencyAtNormalisedPosition(float position) noexcept
{
    const float minLog = std::log10(minDisplayedFrequency);
    const float maxLog = std::log10(maxDisplayedFrequency);
    return std::pow(10.0f, juce::jmap(position, 0.0f, 1.0f, minLog, maxLog));
}

void PluginProcessor::generateSpectrum()
{
    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

    std::array<float, scopePointCount> nextScope{};
    nextScope.fill(silenceDb);

    const float binResolution = (float) currentSampleRate / (float) fftSize;
    float measuredSubPeakDb = silenceDb;

    for (int i = 0; i < scopePointCount; ++i)
    {
        const float position = (float) i / (float) (scopePointCount - 1);
        const float frequency = frequencyAtNormalisedPosition(position);
        const float binPosition = juce::jlimit(1.0f,
                                               (float) ((fftSize / 2) - 2),
                                               frequency / binResolution);
        const int binIndex = (int) std::floor(binPosition);
        const float fraction = binPosition - (float) binIndex;

        const float magA = normaliseMagnitude(fftData[(size_t) binIndex]);
        const float magB = normaliseMagnitude(fftData[(size_t) (binIndex + 1)]);
        const float magnitude = juce::jmax(0.000001f, juce::jmap(fraction, magA, magB));
        const float measuredDb = juce::jlimit(-100.0f, 6.0f, juce::Decibels::gainToDecibels(magnitude, silenceDb));

        nextScope[(size_t) i] = measuredDb;
        measuredSubPeakDb = juce::jmax(measuredSubPeakDb, measuredDb);
    }

    {
        const juce::SpinLock::ScopedLockType lock(spectrumLock);
        scopeData = nextScope;
    }

    subPeakDb.store(measuredSubPeakDb);
    subInRange.store(measuredSubPeakDb >= -3.0f);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    fifo.fill(0.0f);
    fftData.fill(0.0f);
    scopeData.fill(silenceDb);
    fifoIndex = 0;
    nextFFTBlockReady = false;
    subPeakDb.store(silenceDb);
    subInRange.store(false);
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

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            mono += buffer.getSample(channel, sample);

        pushNextSampleIntoFifo(mono / (float) numChannels);
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    if (nextFFTBlockReady)
    {
        generateSpectrum();
        nextFFTBlockReady = false;
    }
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
