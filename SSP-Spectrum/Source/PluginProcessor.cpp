#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int fftBaseOrder = 11;

int fftSizeForChoice(int choice)
{
    return 1 << (fftBaseOrder + juce::jlimit(0, 3, choice));
}

int overlapForChoice(int choice)
{
    static constexpr std::array<int, 4> overlaps{{ 1, 2, 4, 8 }};
    return overlaps[(size_t) juce::jlimit(0, 3, choice)];
}

juce::dsp::WindowingFunction<float>::WindowingMethod windowMethodForChoice(int choice)
{
    switch (juce::jlimit(0, 2, choice))
    {
        case PluginProcessor::blackmanHarris: return juce::dsp::WindowingFunction<float>::blackmanHarris;
        case PluginProcessor::flatTop:        return juce::dsp::WindowingFunction<float>::flatTop;
        case PluginProcessor::hann:
        default:                              return juce::dsp::WindowingFunction<float>::hann;
    }
}

float dbRelease(float current, float target, float releasePerUpdate)
{
    return target > current ? target : juce::jmax(target, current - releasePerUpdate);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("displayMode", "Display Mode", getDisplayModeNames(), stereo));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("fftSize", "FFT Size", getFftSizeNames(), fft8192));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("overlap", "Overlap", getOverlapNames(), 2));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("average", "Average", juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f), 55.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("smooth", "Smooth", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 28.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("slope", "Slope", juce::NormalisableRange<float>(0.0f, 6.0f, 0.1f), 4.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("range", "Range", juce::NormalisableRange<float>(36.0f, 120.0f, 1.0f), 84.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("hold", "Hold", juce::NormalisableRange<float>(0.0f, 10.0f, 0.05f), 2.50f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("window", "Window", getWindowNames(), hann));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("theme", "Theme", getThemeNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("freeze", "Pause", false));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>("waterfall", "Waterfall", true));

    return { parameters.begin(), parameters.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int i = 0; i < (int) fftObjects.size(); ++i)
        fftObjects[(size_t) i] = std::make_unique<juce::dsp::FFT>(fftBaseOrder + i);

    waveformLeftHistory.fill(0.0f);
    waveformRightHistory.fill(0.0f);
    primaryTrace.averageMag.fill(0.0f);
    primaryTrace.displayDb.fill(silenceDb);
    secondaryTrace.averageMag.fill(0.0f);
    secondaryTrace.displayDb.fill(silenceDb);
    sidechainTrace.averageMag.fill(0.0f);
    sidechainTrace.displayDb.fill(silenceDb);
    peakHoldTrace.fill(silenceDb);
    peakHoldTimers.fill(0.0f);
    maxHoldTrace.fill(silenceDb);

    analyzerSnapshot.primary.fill(silenceDb);
    analyzerSnapshot.secondary.fill(silenceDb);
    analyzerSnapshot.sidechain.fill(silenceDb);
    analyzerSnapshot.peakHold.fill(silenceDb);
    analyzerSnapshot.maxHold.fill(silenceDb);
    analyzerSnapshot.waveformMin.fill(0.0f);
    analyzerSnapshot.waveformMax.fill(0.0f);
    analyzerSnapshot.vectorscope.fill({});
    analyzerSnapshot.shortLoudnessDb = silenceDb;
    analyzerSnapshot.crestDb = 0.0f;

    cacheParameterPointers();
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::cacheParameterPointers()
{
    displayModeParam = apvts.getRawParameterValue("displayMode");
    fftSizeParam = apvts.getRawParameterValue("fftSize");
    overlapParam = apvts.getRawParameterValue("overlap");
    averageParam = apvts.getRawParameterValue("average");
    smoothParam = apvts.getRawParameterValue("smooth");
    slopeParam = apvts.getRawParameterValue("slope");
    rangeParam = apvts.getRawParameterValue("range");
    holdParam = apvts.getRawParameterValue("hold");
    windowParam = apvts.getRawParameterValue("window");
    themeParam = apvts.getRawParameterValue("theme");
    freezeParam = apvts.getRawParameterValue("freeze");
    waterfallParam = apvts.getRawParameterValue("waterfall");
}

void PluginProcessor::setChoiceParameter(const juce::String& parameterId, int value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(parameterId)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1((float) value);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

void PluginProcessor::setFloatParameter(const juce::String& parameterId, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(parameterId)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1(value);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

void PluginProcessor::setBoolParameter(const juce::String& parameterId, bool value)
{
    if (auto* param = apvts.getParameter(parameterId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(value ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
}

float PluginProcessor::getRawValue(const juce::String& parameterId) const
{
    if (auto* value = apvts.getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

int PluginProcessor::getChoiceValue(const juce::String& parameterId) const
{
    return juce::roundToInt(getRawValue(parameterId));
}

bool PluginProcessor::currentSettingsMatchPreset(const FactoryPreset& preset) const
{
    const auto matchesFloat = [this](const juce::String& id, float target, float tolerance)
    {
        return std::abs(getRawValue(id) - target) <= tolerance;
    };

    return getChoiceValue("displayMode") == preset.displayMode
        && getChoiceValue("fftSize") == preset.fftSize
        && getChoiceValue("overlap") == preset.overlap
        && matchesFloat("average", preset.average, 0.15f)
        && matchesFloat("smooth", preset.smooth, 0.15f)
        && matchesFloat("slope", preset.slope, 0.11f)
        && matchesFloat("range", preset.range, 0.6f)
        && matchesFloat("hold", preset.hold, 0.06f)
        && getChoiceValue("window") == preset.window
        && ((freezeParam != nullptr && freezeParam->load() >= 0.5f) == preset.freeze)
        && ((waterfallParam != nullptr && waterfallParam->load() >= 0.5f) == preset.waterfall);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    writePosition = 0;
    waveformWritePosition = 0;
    samplesSinceLastFrame = 0;

    historyLeft.fill(0.0f);
    historyRight.fill(0.0f);
    historySidechain.fill(0.0f);
    waveformLeftHistory.fill(0.0f);
    waveformRightHistory.fill(0.0f);
    tempPrimaryInput.fill(0.0f);
    tempSecondaryInput.fill(0.0f);
    tempSidechainInput.fill(0.0f);
    fftWorkA.fill(0.0f);
    fftWorkB.fill(0.0f);

    primaryTrace.averageMag.fill(0.0f);
    primaryTrace.displayDb.fill(silenceDb);
    secondaryTrace.averageMag.fill(0.0f);
    secondaryTrace.displayDb.fill(silenceDb);
    sidechainTrace.averageMag.fill(0.0f);
    sidechainTrace.displayDb.fill(silenceDb);
    peakHoldTrace.fill(silenceDb);
    peakHoldTimers.fill(0.0f);
    maxHoldTrace.fill(silenceDb);
    cachedWindowChoice = -1;
    cachedWindowFftChoice = -1;

    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    analyzerSnapshot.primary.fill(silenceDb);
    analyzerSnapshot.secondary.fill(silenceDb);
    analyzerSnapshot.sidechain.fill(silenceDb);
    analyzerSnapshot.peakHold.fill(silenceDb);
    analyzerSnapshot.maxHold.fill(silenceDb);
    analyzerSnapshot.waveformMin.fill(0.0f);
    analyzerSnapshot.waveformMax.fill(0.0f);
    analyzerSnapshot.vectorscope.fill({});
    analyzerSnapshot.peakLeftDb = silenceDb;
    analyzerSnapshot.peakRightDb = silenceDb;
    analyzerSnapshot.rmsLeftDb = silenceDb;
    analyzerSnapshot.rmsRightDb = silenceDb;
    analyzerSnapshot.sidechainPeakDb = silenceDb;
    analyzerSnapshot.correlation = 1.0f;
    analyzerSnapshot.shortLoudnessDb = silenceDb;
    analyzerSnapshot.crestDb = 0.0f;
    analyzerSnapshot.clipping = false;
    analyzerSnapshot.sampleRate = currentSampleRate;
    analyzerSnapshot.waveformSpanSeconds = 0.0f;
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo() || mainIn != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.getChannelSet(true, 1);
        if (! sidechain.isDisabled() && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void PluginProcessor::pushInputFrame(float left, float right, float sidechain) noexcept
{
    historyLeft[(size_t) writePosition] = left;
    historyRight[(size_t) writePosition] = right;
    historySidechain[(size_t) writePosition] = sidechain;
    writePosition = (writePosition + 1) % maxFftSize;

    waveformLeftHistory[(size_t) waveformWritePosition] = left;
    waveformRightHistory[(size_t) waveformWritePosition] = right;
    waveformWritePosition = (waveformWritePosition + 1) % waveformHistorySize;
}

void PluginProcessor::updateMeters(const juce::AudioBuffer<float>& mainBuffer, const juce::AudioBuffer<float>& sidechainBuffer)
{
    const int numSamples = mainBuffer.getNumSamples();
    if (numSamples <= 0 || mainBuffer.getNumChannels() <= 0)
        return;

    const auto* left = mainBuffer.getReadPointer(0);
    const auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer(1) : left;

    double sumLeft = 0.0;
    double sumRight = 0.0;
    double sumMono = 0.0;
    double cross = 0.0;
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    bool clipping = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i];
        const float r = right[i];
        peakLeft = juce::jmax(peakLeft, std::abs(l));
        peakRight = juce::jmax(peakRight, std::abs(r));
        sumLeft += (double) l * (double) l;
        sumRight += (double) r * (double) r;
        sumMono += (double) (0.5f * (l + r)) * (double) (0.5f * (l + r));
        cross += (double) l * (double) r;
        clipping = clipping || std::abs(l) >= 0.999f || std::abs(r) >= 0.999f;
    }

    float sidechainPeak = 0.0f;
    for (int channel = 0; channel < sidechainBuffer.getNumChannels(); ++channel)
    {
        const auto* data = sidechainBuffer.getReadPointer(channel);
        for (int i = 0; i < sidechainBuffer.getNumSamples(); ++i)
            sidechainPeak = juce::jmax(sidechainPeak, std::abs(data[i]));
    }

    const auto peakLeftDb = magnitudeToDb(peakLeft);
    const auto peakRightDb = magnitudeToDb(peakRight);
    const auto rmsLeftDb = magnitudeToDb((float) std::sqrt(sumLeft / (double) juce::jmax(1, numSamples)));
    const auto rmsRightDb = magnitudeToDb((float) std::sqrt(sumRight / (double) juce::jmax(1, numSamples)));
    const auto monoRmsDb = magnitudeToDb((float) std::sqrt(sumMono / (double) juce::jmax(1, numSamples)));
    const auto sidechainPeakDb = magnitudeToDb(sidechainPeak);
    const auto denominator = std::sqrt(juce::jmax(1.0e-12, sumLeft * sumRight));
    const auto blockCorrelation = (float) juce::jlimit(-1.0, 1.0, denominator > 0.0 ? cross / denominator : 1.0);
    const auto crestDb = juce::jmax(0.0f, juce::jmax(peakLeftDb, peakRightDb) - juce::jmax(rmsLeftDb, rmsRightDb));

    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    analyzerSnapshot.peakLeftDb = dbRelease(analyzerSnapshot.peakLeftDb, peakLeftDb, 1.8f);
    analyzerSnapshot.peakRightDb = dbRelease(analyzerSnapshot.peakRightDb, peakRightDb, 1.8f);
    analyzerSnapshot.rmsLeftDb = juce::jmap(0.25f, analyzerSnapshot.rmsLeftDb, rmsLeftDb);
    analyzerSnapshot.rmsRightDb = juce::jmap(0.25f, analyzerSnapshot.rmsRightDb, rmsRightDb);
    analyzerSnapshot.sidechainPeakDb = dbRelease(analyzerSnapshot.sidechainPeakDb, sidechainPeakDb, 2.2f);
    analyzerSnapshot.correlation = juce::jmap(0.18f, analyzerSnapshot.correlation, blockCorrelation);
    analyzerSnapshot.shortLoudnessDb = juce::jmap(0.12f, analyzerSnapshot.shortLoudnessDb, monoRmsDb);
    analyzerSnapshot.crestDb = juce::jmap(0.20f, analyzerSnapshot.crestDb, crestDb);
    analyzerSnapshot.clipping = analyzerSnapshot.clipping || clipping;
    analyzerSnapshot.sidechainConnected = sidechainBuffer.getNumChannels() > 0;
}

void PluginProcessor::updateWindowTable(int fftChoice, int windowChoice)
{
    if (cachedWindowFftChoice == fftChoice && cachedWindowChoice == windowChoice)
        return;

    cachedWindowFftChoice = fftChoice;
    cachedWindowChoice = windowChoice;
    windowTable.resize((size_t) fftSizeForChoice(fftChoice));
    juce::dsp::WindowingFunction<float>::fillWindowingTables(windowTable.data(),
                                                             windowTable.size(),
                                                             windowMethodForChoice(windowChoice),
                                                             true);
}

void PluginProcessor::fillRecentSamples(std::array<float, maxFftSize>& destination,
                                        const std::array<float, maxFftSize>& source,
                                        int count) const
{
    const int start = writePosition - count;
    for (int i = 0; i < count; ++i)
        destination[(size_t) i] = source[(size_t) ((start + i + maxFftSize) % maxFftSize)];

    std::fill(destination.begin() + count, destination.end(), 0.0f);
}

void PluginProcessor::buildWaveformSummary(std::array<float, waveformPointCount>& minValues,
                                           std::array<float, waveformPointCount>& maxValues,
                                           float& spanSeconds) const
{
    minValues.fill(0.0f);
    maxValues.fill(0.0f);

    const int spanSamples = juce::jlimit(waveformPointCount,
                                         waveformHistorySize,
                                         juce::roundToInt(currentSampleRate * 2.8));
    spanSeconds = currentSampleRate > 0.0 ? (float) spanSamples / (float) currentSampleRate : 0.0f;
    const int start = waveformWritePosition - spanSamples;

    for (int i = 0; i < waveformPointCount; ++i)
    {
        const int segmentStart = start + (i * spanSamples) / waveformPointCount;
        const int segmentEnd = start + ((i + 1) * spanSamples) / waveformPointCount;

        float minValue = 1.0f;
        float maxValue = -1.0f;

        for (int sample = segmentStart; sample < segmentEnd; ++sample)
        {
            const int index = (sample + waveformHistorySize) % waveformHistorySize;
            const float mono = 0.5f * (waveformLeftHistory[(size_t) index] + waveformRightHistory[(size_t) index]);
            minValue = juce::jmin(minValue, mono);
            maxValue = juce::jmax(maxValue, mono);
        }

        if (segmentEnd <= segmentStart)
        {
            minValue = 0.0f;
            maxValue = 0.0f;
        }

        minValues[(size_t) i] = minValue;
        maxValues[(size_t) i] = maxValue;
    }
}

void PluginProcessor::buildVectorscope(std::array<ScopePoint, vectorscopePointCount>& points) const
{
    points.fill({});

    const int recentSamples = juce::jlimit(vectorscopePointCount,
                                           waveformHistorySize,
                                           juce::roundToInt(currentSampleRate * 0.22));
    const int step = juce::jmax(1, recentSamples / vectorscopePointCount);
    int samplePosition = waveformWritePosition - recentSamples;

    for (int i = 0; i < vectorscopePointCount; ++i)
    {
        const int index = (samplePosition + i * step + waveformHistorySize) % waveformHistorySize;
        const float left = waveformLeftHistory[(size_t) index];
        const float right = waveformRightHistory[(size_t) index];
        points[(size_t) i] = {
            juce::jlimit(-1.0f, 1.0f, 0.5f * (left + right)),
            juce::jlimit(-1.0f, 1.0f, 0.5f * (left - right))
        };
    }
}

float PluginProcessor::frequencyAtPosition(float normalised, double sampleRate)
{
    const float minFrequency = 20.0f;
    const float maxFrequency = (float) juce::jmin(20000.0, sampleRate * 0.48);
    const float minLog = std::log10(minFrequency);
    const float maxLog = std::log10(maxFrequency);
    return std::pow(10.0f, juce::jmap(normalised, 0.0f, 1.0f, minLog, maxLog));
}

float PluginProcessor::magnitudeToDb(float magnitude)
{
    return juce::Decibels::gainToDecibels(juce::jmax(0.0000001f, magnitude), silenceDb);
}

float PluginProcessor::noteNumberForFrequency(float frequency)
{
    if (frequency <= 0.0f)
        return -1000.0f;

    return 69.0f + 12.0f * std::log2(frequency / 440.0f);
}

void PluginProcessor::smoothAcrossBins(std::array<float, displayPointCount>& values, float amount)
{
    const int radius = juce::jlimit(0, 8, juce::roundToInt(amount * 7.0f));
    if (radius <= 0)
        return;

    auto source = values;
    for (int i = 0; i < displayPointCount; ++i)
    {
        float weighted = 0.0f;
        float weightSum = 0.0f;
        for (int delta = -radius; delta <= radius; ++delta)
        {
            const int index = juce::jlimit(0, displayPointCount - 1, i + delta);
            const float weight = (float) (radius + 1 - std::abs(delta));
            weighted += source[(size_t) index] * weight;
            weightSum += weight;
        }

        values[(size_t) i] = weightSum > 0.0f ? weighted / weightSum : source[(size_t) i];
    }
}

void PluginProcessor::performSpectrum(std::array<float, displayPointCount>& destinationDb,
                                      TraceBuffers& trace,
                                      const std::array<float, maxFftSize>& timeDomain,
                                      int fftChoice,
                                      float slopeDbPerOct,
                                      float smoothingAmount,
                                      float averagingAmount)
{
    const int fftSize = fftSizeForChoice(fftChoice);

    std::fill(fftWorkA.begin(), fftWorkA.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        fftWorkA[(size_t) i] = timeDomain[(size_t) i] * windowTable[(size_t) i];

    fftObjects[(size_t) fftChoice]->performFrequencyOnlyForwardTransform(fftWorkA.data());

    const float binResolution = (float) currentSampleRate / (float) fftSize;
    destinationDb.fill(silenceDb);

    for (int i = 0; i < displayPointCount; ++i)
    {
        const float x = (float) i / (float) (displayPointCount - 1);
        const float frequency = frequencyAtPosition(x, currentSampleRate);
        const float binPosition = juce::jlimit(1.0f,
                                               (float) ((fftSize / 2) - 2),
                                               frequency / juce::jmax(1.0f, binResolution));
        const int binIndex = (int) std::floor(binPosition);
        const float fraction = binPosition - (float) binIndex;
        const float magA = juce::jmax(0.0000001f, fftWorkA[(size_t) binIndex] * (2.0f / (float) fftSize));
        const float magB = juce::jmax(0.0000001f, fftWorkA[(size_t) (binIndex + 1)] * (2.0f / (float) fftSize));
        const float magnitude = juce::jmap(fraction, magA, magB);

        auto& averaged = trace.averageMag[(size_t) i];
        averaged = averaged <= 0.0f ? magnitude : averaged * averagingAmount + magnitude * (1.0f - averagingAmount);

        const float tiltedDb = magnitudeToDb(averaged) + slopeDbPerOct * std::log2(juce::jmax(20.0f, frequency) / 1000.0f);
        destinationDb[(size_t) i] = tiltedDb;
    }

    smoothAcrossBins(destinationDb, smoothingAmount);
    trace.displayDb = destinationDb;
}

void PluginProcessor::renderAnalyzerFrame()
{
    const int fftChoice = juce::jlimit(0, 3, fftSizeParam != nullptr ? juce::roundToInt(fftSizeParam->load()) : fft8192);
    const int displayMode = juce::jlimit(0, 3, displayModeParam != nullptr ? juce::roundToInt(displayModeParam->load()) : stereo);
    const int windowChoice = juce::jlimit(0, 2, windowParam != nullptr ? juce::roundToInt(windowParam->load()) : hann);
    const float smoothingAmount = juce::jlimit(0.0f, 1.0f, (smoothParam != nullptr ? smoothParam->load() : 0.0f) / 100.0f);
    const float averagingAmount = juce::jlimit(0.0f, 0.98f, (averageParam != nullptr ? averageParam->load() : 0.0f) / 100.0f);
    const float slopeDbPerOct = slopeParam != nullptr ? slopeParam->load() : 4.5f;
    const float rangeDb = rangeParam != nullptr ? rangeParam->load() : 84.0f;
    const float holdSeconds = holdParam != nullptr ? holdParam->load() : 2.5f;
    const bool freeze = freezeParam != nullptr && freezeParam->load() >= 0.5f;
    const bool waterfall = waterfallParam == nullptr || waterfallParam->load() >= 0.5f;
    const int theme = getThemeIndex();
    const bool sidechainConnected = getBusCount(true) > 1 && getBus(true, 1) != nullptr && getBus(true, 1)->isEnabled();

    const int fftSize = fftSizeForChoice(fftChoice);
    updateWindowTable(fftChoice, windowChoice);

    fillRecentSamples(tempPrimaryInput, historyLeft, fftSize);
    fillRecentSamples(tempSecondaryInput, historyRight, fftSize);
    fillRecentSamples(tempSidechainInput, historySidechain, fftSize);

    for (int i = 0; i < fftSize; ++i)
    {
        const float left = tempPrimaryInput[(size_t) i];
        const float right = tempSecondaryInput[(size_t) i];

        if (displayMode == stereo || displayMode == stereoSidechain)
        {
            tempPrimaryInput[(size_t) i] = 0.5f * (left + right);
            tempSecondaryInput[(size_t) i] = right;
        }
        else if (displayMode == midSide)
        {
            tempPrimaryInput[(size_t) i] = 0.5f * (left + right);
            tempSecondaryInput[(size_t) i] = 0.5f * (left - right);
        }
    }

    std::array<float, displayPointCount> primary{};
    std::array<float, displayPointCount> secondary{};
    std::array<float, displayPointCount> sidechain{};
    std::array<float, waveformPointCount> waveformMin{};
    std::array<float, waveformPointCount> waveformMax{};
    std::array<ScopePoint, vectorscopePointCount> vectorscope{};
    float waveformSpanSeconds = 0.0f;
    primary.fill(silenceDb);
    secondary.fill(silenceDb);
    sidechain.fill(silenceDb);
    waveformMin.fill(0.0f);
    waveformMax.fill(0.0f);
    vectorscope.fill({});

    if (! freeze)
    {
        performSpectrum(primary, primaryTrace, tempPrimaryInput, fftChoice, slopeDbPerOct, smoothingAmount, averagingAmount);

        if (displayMode == leftRight || displayMode == midSide)
            performSpectrum(secondary, secondaryTrace, tempSecondaryInput, fftChoice, slopeDbPerOct, smoothingAmount, averagingAmount);
        else
            secondaryTrace.displayDb.fill(silenceDb);

        if (displayMode == stereoSidechain && sidechainConnected)
            performSpectrum(sidechain, sidechainTrace, tempSidechainInput, fftChoice, slopeDbPerOct, smoothingAmount, averagingAmount);
        else
            sidechainTrace.displayDb.fill(silenceDb);

        const float frameSeconds = (float) fftSize / (float) juce::jmax(1.0, currentSampleRate) / (float) overlapForChoice(getChoiceValue("overlap"));
        for (int i = 0; i < displayPointCount; ++i)
        {
            const float db = primary[(size_t) i];
            if (db >= peakHoldTrace[(size_t) i])
            {
                peakHoldTrace[(size_t) i] = db;
                peakHoldTimers[(size_t) i] = 0.0f;
            }
            else
            {
                peakHoldTimers[(size_t) i] += frameSeconds;
                if (peakHoldTimers[(size_t) i] >= holdSeconds)
                    peakHoldTrace[(size_t) i] = juce::jmax(db, peakHoldTrace[(size_t) i] - frameSeconds * 24.0f);
            }

            maxHoldTrace[(size_t) i] = juce::jmax(maxHoldTrace[(size_t) i], db);
        }

        buildWaveformSummary(waveformMin, waveformMax, waveformSpanSeconds);
        buildVectorscope(vectorscope);
    }
    else
    {
        const juce::SpinLock::ScopedTryLockType lock(analyzerLock);
        if (lock.isLocked())
        {
            primary = analyzerSnapshot.primary;
            secondary = analyzerSnapshot.secondary;
            sidechain = analyzerSnapshot.sidechain;
            waveformMin = analyzerSnapshot.waveformMin;
            waveformMax = analyzerSnapshot.waveformMax;
            vectorscope = analyzerSnapshot.vectorscope;
            waveformSpanSeconds = analyzerSnapshot.waveformSpanSeconds;
        }
    }

    float dominantDb = silenceDb;
    float dominantFrequency = 0.0f;
    for (int i = 0; i < displayPointCount; ++i)
    {
        if (primary[(size_t) i] > dominantDb)
        {
            dominantDb = primary[(size_t) i];
            dominantFrequency = frequencyAtPosition((float) i / (float) (displayPointCount - 1), currentSampleRate);
        }
    }

    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    analyzerSnapshot.primary = primary;
    analyzerSnapshot.secondary = secondary;
    analyzerSnapshot.sidechain = sidechain;
    analyzerSnapshot.peakHold = peakHoldTrace;
    analyzerSnapshot.maxHold = maxHoldTrace;
    analyzerSnapshot.waveformMin = waveformMin;
    analyzerSnapshot.waveformMax = waveformMax;
    analyzerSnapshot.vectorscope = vectorscope;
    analyzerSnapshot.secondaryVisible = displayMode == leftRight || displayMode == midSide;
    analyzerSnapshot.sidechainVisible = displayMode == stereoSidechain && sidechainConnected;
    analyzerSnapshot.freezeActive = freeze;
    analyzerSnapshot.waterfallEnabled = waterfall;
    analyzerSnapshot.displayMode = displayMode;
    analyzerSnapshot.themeIndex = theme;
    analyzerSnapshot.rangeDb = rangeDb;
    analyzerSnapshot.dominantFrequencyHz = dominantFrequency;
    analyzerSnapshot.dominantDb = dominantDb;
    analyzerSnapshot.sampleRate = currentSampleRate;
    analyzerSnapshot.waveformSpanSeconds = waveformSpanSeconds;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto sidechainInput = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<float>();

    const int numSamples = mainInput.getNumSamples();
    if (numSamples <= 0 || mainInput.getNumChannels() <= 0)
        return;

    if (mainOutput.getNumChannels() > 0)
    {
        for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
        {
            const int sourceChannel = juce::jmin(channel, mainInput.getNumChannels() - 1);
            mainOutput.copyFrom(channel, 0, mainInput, sourceChannel, 0, numSamples);
        }
    }

    updateMeters(mainInput, sidechainInput);

    const auto* left = mainInput.getReadPointer(0);
    const auto* right = mainInput.getNumChannels() > 1 ? mainInput.getReadPointer(1) : left;
    const int sidechainChannels = sidechainInput.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float sidechain = 0.0f;
        if (sidechainChannels > 0)
        {
            for (int channel = 0; channel < sidechainChannels; ++channel)
                sidechain += sidechainInput.getReadPointer(channel)[sample];
            sidechain /= (float) sidechainChannels;
        }

        pushInputFrame(left[sample], right[sample], sidechain);
    }

    const int fftChoice = juce::jlimit(0, 3, fftSizeParam != nullptr ? juce::roundToInt(fftSizeParam->load()) : fft8192);
    const int hopSize = juce::jmax(128, fftSizeForChoice(fftChoice) / overlapForChoice(getChoiceValue("overlap")));
    samplesSinceLastFrame += numSamples;

    if (samplesSinceLastFrame >= hopSize)
    {
        samplesSinceLastFrame %= hopSize;
        renderAnalyzerFrame();
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
}

PluginProcessor::AnalyzerSnapshot PluginProcessor::getAnalyzerSnapshotCopy() const
{
    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    return analyzerSnapshot;
}

void PluginProcessor::clearAnalyzerPeaks()
{
    peakHoldTrace.fill(silenceDb);
    peakHoldTimers.fill(0.0f);
    maxHoldTrace.fill(silenceDb);

    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    analyzerSnapshot.peakHold = peakHoldTrace;
    analyzerSnapshot.maxHold = maxHoldTrace;
}

void PluginProcessor::clearClipFlag()
{
    const juce::SpinLock::ScopedLockType lock(analyzerLock);
    analyzerSnapshot.clipping = false;
}

int PluginProcessor::getThemeIndex() const
{
    return juce::jlimit(0, getThemeNames().size() - 1, themeParam != nullptr ? juce::roundToInt(themeParam->load()) : 0);
}

const juce::StringArray& PluginProcessor::getDisplayModeNames()
{
    static const juce::StringArray names{ "Stereo", "Left/Right", "Mid/Side", "Stereo+SC" };
    return names;
}

const juce::StringArray& PluginProcessor::getFftSizeNames()
{
    static const juce::StringArray names{ "2048", "4096", "8192", "16384" };
    return names;
}

const juce::StringArray& PluginProcessor::getOverlapNames()
{
    static const juce::StringArray names{ "1x", "2x", "4x", "8x" };
    return names;
}

const juce::StringArray& PluginProcessor::getWindowNames()
{
    static const juce::StringArray names{ "Hann", "B.-Harris", "Flat Top" };
    return names;
}

const juce::StringArray& PluginProcessor::getThemeNames()
{
    static const juce::StringArray names{ "Ocean", "Nova", "Mint", "Heat", "Steel" };
    return names;
}

const juce::Array<PluginProcessor::FactoryPreset>& PluginProcessor::getFactoryPresets()
{
    static const juce::Array<FactoryPreset> presets = []
    {
        juce::Array<FactoryPreset> values;
        values.add({ "Default View", "Balanced full-range spectrum and waterfall for general production work.", stereo, fft8192, 2, 55.0f, 28.0f, 4.5f, 84.0f, 2.5f, hann, 0, false, true });
        values.add({ "Kick Tuner", "Long FFT and restrained smoothing for reading kick fundamentals and tail pitch.", stereo, fft16384, 3, 72.0f, 16.0f, 2.0f, 72.0f, 3.6f, blackmanHarris, 3, false, true });
        values.add({ "808/Sub Focus", "Low-end focused view for 808 notes, sub weight, and sustain consistency.", stereo, fft16384, 3, 78.0f, 24.0f, 2.6f, 60.0f, 4.4f, blackmanHarris, 3, false, true });
        values.add({ "Bass Layer Check", "Left/right and bass-layer inspection for clashes, stereo drift, and note overlap.", leftRight, fft8192, 2, 62.0f, 18.0f, 3.0f, 72.0f, 2.0f, hann, 4, false, true });
        values.add({ "Vocal Harshness Hunt", "Sharper upper-mid view for spotting buildup around presence and harshness zones.", stereo, fft8192, 2, 46.0f, 12.0f, 5.0f, 72.0f, 1.6f, flatTop, 1, false, true });
        values.add({ "Hi-Hat / Air", "Fast top-end view for hats, air bands, and brittle transient checks.", leftRight, fft4096, 2, 34.0f, 9.0f, 5.8f, 78.0f, 1.1f, flatTop, 0, false, true });
        values.add({ "Stereo Width Check", "Mid/side focus for width, mono compatibility, and side energy management.", midSide, fft4096, 2, 48.0f, 16.0f, 4.0f, 84.0f, 1.8f, hann, 2, false, true });
        values.add({ "Mix Bus Balance", "Smoother overview for checking macro tonal balance across the full mix bus.", stereo, fft8192, 2, 68.0f, 30.0f, 4.5f, 90.0f, 2.8f, hann, 4, false, true });
        values.add({ "Mastering Inspect", "High-resolution analyzer and longer hold for final tonal, width, and peak inspection.", midSide, fft16384, 3, 84.0f, 34.0f, 4.5f, 96.0f, 4.6f, blackmanHarris, 4, false, true });
        values.add({ "Reference Match", "Use sidechain input as the reference lane for A/B tonal matching against a target.", stereoSidechain, fft8192, 2, 74.0f, 24.0f, 4.5f, 90.0f, 3.2f, hann, 0, false, true });
        return values;
    }();

    return presets;
}

int PluginProcessor::getMatchingFactoryPresetIndex() const
{
    const auto& presets = getFactoryPresets();
    for (int i = 0; i < presets.size(); ++i)
        if (currentSettingsMatchPreset(presets.getReference(i)))
            return i;

    return -1;
}

juce::String PluginProcessor::getCurrentPresetName() const
{
    const int index = getMatchingFactoryPresetIndex();
    return index >= 0 ? getFactoryPresets().getReference(index).name : "Custom";
}

juce::String PluginProcessor::getCurrentPresetDescription() const
{
    const int index = getMatchingFactoryPresetIndex();
    return index >= 0 ? getFactoryPresets().getReference(index).description
                      : "Current analyzer settings differ from the factory producer presets.";
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow(index, presets.size()))
        return false;

    const auto& preset = presets.getReference(index);
    setChoiceParameter("displayMode", preset.displayMode);
    setChoiceParameter("fftSize", preset.fftSize);
    setChoiceParameter("overlap", preset.overlap);
    setFloatParameter("average", preset.average);
    setFloatParameter("smooth", preset.smooth);
    setFloatParameter("slope", preset.slope);
    setFloatParameter("range", preset.range);
    setFloatParameter("hold", preset.hold);
    setChoiceParameter("window", preset.window);
    setBoolParameter("freeze", preset.freeze);
    setBoolParameter("waterfall", preset.waterfall);
    return true;
}

bool PluginProcessor::stepFactoryPreset(int delta)
{
    const auto& presets = getFactoryPresets();
    if (presets.isEmpty() || delta == 0)
        return false;

    int index = getMatchingFactoryPresetIndex();
    if (index < 0)
        index = delta > 0 ? 0 : presets.size() - 1;
    else
        index = (index + delta + presets.size()) % presets.size();

    return loadFactoryPreset(index);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
