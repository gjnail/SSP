#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.42f;
constexpr float meterRelease = 0.11f;
constexpr double momentaryWindowSeconds = 0.4;
constexpr double shortTermWindowSeconds = 3.0;
constexpr double historyStepSeconds = 0.1;
constexpr float silenceLufs = -100.0f;
} // namespace

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

PluginProcessor::~PluginProcessor() = default;

float PluginProcessor::getMomentaryLufs() const noexcept
{
    return momentaryLufs.load();
}

float PluginProcessor::getShortTermLufs() const noexcept
{
    return shortTermLufs.load();
}

float PluginProcessor::getIntegratedLufs() const noexcept
{
    return integratedLufs.load();
}

float PluginProcessor::getPeakDb() const noexcept
{
    return peakDb.load();
}

float PluginProcessor::getLeftMeterLevel() const noexcept
{
    return leftMeterLevel.load();
}

float PluginProcessor::getRightMeterLevel() const noexcept
{
    return rightMeterLevel.load();
}

void PluginProcessor::requestReset() noexcept
{
    resetRequested.store(true);
}

void PluginProcessor::copyHistory(std::vector<float>& destination) const
{
    destination.clear();

    const juce::SpinLock::ScopedLockType lock(historyLock);
    destination.reserve((size_t) historyCount);

    for (int i = 0; i < historyCount; ++i)
    {
        const int index = (historyWriteIndex - historyCount + i + (int) historyValues.size()) % (int) historyValues.size();
        destination.push_back(historyValues[(size_t) index]);
    }
}

void PluginProcessor::resetMeasurementState()
{
    momentaryEntries.clear();
    shortTermEntries.clear();
    momentaryTimeTotal = 0.0;
    momentaryEnergyTotal = 0.0;
    shortTermTimeTotal = 0.0;
    shortTermEnergyTotal = 0.0;
    integratedTimeTotal = 0.0;
    integratedEnergyTotal = 0.0;
    historyIntervalAccumulator = 0.0;

    {
        const juce::SpinLock::ScopedLockType lock(historyLock);
        historyValues.fill(silenceLufs);
        historyCount = 0;
        historyWriteIndex = 0;
    }

    momentaryLufs.store(silenceLufs);
    shortTermLufs.store(silenceLufs);
    integratedLufs.store(silenceLufs);
    peakDb.store(-60.0f);
    leftMeterLevel.store(0.0f);
    rightMeterLevel.store(0.0f);
}

void PluginProcessor::pushWindowEntry(std::deque<WindowEntry>& entries,
                                      double& timeTotal,
                                      double& energyTotal,
                                      WindowEntry entry,
                                      double maxWindowSeconds)
{
    entries.push_back(entry);
    timeTotal += entry.durationSeconds;
    energyTotal += entry.energyIntegral;

    while (!entries.empty() && timeTotal - entries.front().durationSeconds >= maxWindowSeconds)
    {
        timeTotal -= entries.front().durationSeconds;
        energyTotal -= entries.front().energyIntegral;
        entries.pop_front();
    }
}

void PluginProcessor::pushHistoryPoint(float value)
{
    const juce::SpinLock::ScopedTryLockType lock(historyLock);
    if (!lock.isLocked())
        return;

    historyValues[(size_t) historyWriteIndex] = value;
    historyWriteIndex = (historyWriteIndex + 1) % (int) historyValues.size();
    historyCount = juce::jmin(historyCount + 1, (int) historyValues.size());
}

float PluginProcessor::energyToLufs(double energy) const noexcept
{
    if (energy <= 1.0e-12)
        return silenceLufs;

    return (float) (-0.691 + 10.0 * std::log10(energy));
}

void PluginProcessor::updateMeters(float leftPeak, float rightPeak, float peakDbValue) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(leftMeterLevel, juce::jlimit(0.0f, 1.2f, leftPeak));
    smoothValue(rightMeterLevel, juce::jlimit(0.0f, 1.2f, rightPeak));
    peakDb.store(peakDbValue);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    for (int channel = 0; channel < 2; ++channel)
    {
        highPassFilters[(size_t) channel].reset();
        highShelfFilters[(size_t) channel].reset();
        highPassFilters[(size_t) channel].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.0f, 0.5f);
        highShelfFilters[(size_t) channel].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(4.0f));
    }

    resetMeasurementState();
    resetRequested.store(false);
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

    if (resetRequested.exchange(false))
        resetMeasurementState();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numChannels == 0 || numSamples == 0)
        return;

    double energySum = 0.0;
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    float blockPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float input = buffer.getSample(channel, sample);
            const float weighted = highShelfFilters[(size_t) channel].processSample(highPassFilters[(size_t) channel].processSample(input));

            energySum += (double) weighted * (double) weighted;

            if (channel == 0)
                leftPeak = juce::jmax(leftPeak, std::abs(input));
            else if (channel == 1)
                rightPeak = juce::jmax(rightPeak, std::abs(input));

            blockPeak = juce::jmax(blockPeak, std::abs(input));
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const double blockDuration = (double) numSamples / currentSampleRate;
    const double blockEnergy = energySum / (double) numSamples;
    const float blockLufs = energyToLufs(blockEnergy);

    const WindowEntry entry{blockDuration, blockEnergy * blockDuration};
    pushWindowEntry(momentaryEntries, momentaryTimeTotal, momentaryEnergyTotal, entry, momentaryWindowSeconds);
    pushWindowEntry(shortTermEntries, shortTermTimeTotal, shortTermEnergyTotal, entry, shortTermWindowSeconds);

    const float momentaryValue = momentaryTimeTotal > 0.0 ? energyToLufs(momentaryEnergyTotal / momentaryTimeTotal) : silenceLufs;
    const float shortTermValue = shortTermTimeTotal > 0.0 ? energyToLufs(shortTermEnergyTotal / shortTermTimeTotal) : silenceLufs;

    if (blockLufs > -70.0f)
    {
        integratedTimeTotal += blockDuration;
        integratedEnergyTotal += blockEnergy * blockDuration;
    }

    const float integratedValue = integratedTimeTotal > 0.0 ? energyToLufs(integratedEnergyTotal / integratedTimeTotal) : silenceLufs;

    momentaryLufs.store(momentaryValue);
    shortTermLufs.store(shortTermValue);
    integratedLufs.store(integratedValue);

    historyIntervalAccumulator += blockDuration;
    while (historyIntervalAccumulator >= historyStepSeconds)
    {
        pushHistoryPoint(shortTermValue);
        historyIntervalAccumulator -= historyStepSeconds;
    }

    updateMeters(leftPeak,
                 numChannels > 1 ? rightPeak : leftPeak,
                 juce::Decibels::gainToDecibels(juce::jmax(blockPeak, 0.00001f), -60.0f));
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
