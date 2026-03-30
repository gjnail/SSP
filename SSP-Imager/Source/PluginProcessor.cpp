#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ImagerPresetLibrary.h"

namespace
{
constexpr float minCrossoverHz = 20.0f;
constexpr float maxCrossoverHz = 20000.0f;
constexpr float minGapRatio = 1.4f;
constexpr int vectorscopeDecimation = 4; // write every Nth sample
constexpr int learnTargetFrames = 36;
constexpr float learnMinSuggested = 40.0f;
constexpr float learnMaxSuggested = 16000.0f;

float frequencyFromLogBin(int bin, int totalBins)
{
    const auto normalised = juce::jlimit(0.0f, 1.0f, (float) bin / (float) juce::jmax(1, totalBins - 1));
    return std::pow(10.0f, std::log10(minCrossoverHz) + normalised * (std::log10(maxCrossoverHz) - std::log10(minCrossoverHz)));
}

float snapLearnedFrequency(float frequency)
{
    static constexpr float musicalAnchors[] = {
        40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f,
        630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f,
        6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f
    };

    float closest = musicalAnchors[0];
    float bestDistance = std::abs(frequency - closest);

    for (auto anchor : musicalAnchors)
    {
        const auto distance = std::abs(frequency - anchor);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            closest = anchor;
        }
    }

    return closest;
}

juce::StringArray getImagerParameterIDs()
{
    juce::StringArray ids { "crossover1", "crossover2", "crossover3", "outputGain", "bypass" };

    for (int i = 1; i <= PluginProcessor::numBands; ++i)
    {
        const auto band = juce::String(i);
        ids.add("band" + band + "Width");
        ids.add("band" + band + "Pan");
        ids.add("band" + band + "Solo");
        ids.add("band" + band + "Mute");
    }

    return ids;
}

float clampCrossover(float hz)
{
    return juce::jlimit(minCrossoverHz, maxCrossoverHz, hz);
}

juce::String slugify(const juce::String& text)
{
    juce::String slug;
    for (auto c : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            slug << c;
        else if (c == ' ' || c == '/' || c == '-' || c == '_')
            slug << '-';
    }
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    return slug.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}
} // namespace

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (const auto& parameterID : getImagerParameterIDs())
        apvts.addParameterListener(parameterID, this);

    refreshUserPresets();

    if (! getFactoryPresets().isEmpty())
        loadFactoryPreset(0);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto& parameterID : getImagerParameterIDs())
        apvts.removeParameterListener(parameterID, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "crossover1", "Crossover 1",
        juce::NormalisableRange<float>(minCrossoverHz, maxCrossoverHz, 0.1f, 0.25f),
        200.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "crossover2", "Crossover 2",
        juce::NormalisableRange<float>(minCrossoverHz, maxCrossoverHz, 0.1f, 0.25f),
        2000.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "crossover3", "Crossover 3",
        juce::NormalisableRange<float>(minCrossoverHz, maxCrossoverHz, 0.1f, 0.25f),
        8000.0f));

    const juce::String bandNames[] = { "Low", "Low Mid", "High Mid", "High" };

    for (int i = 0; i < 4; ++i)
    {
        auto si = juce::String(i + 1);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "band" + si + "Width", bandNames[i] + " Width",
            juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f),
            100.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "band" + si + "Pan", bandNames[i] + " Pan",
            juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            "band" + si + "Solo", bandNames[i] + " Solo", false));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            "band" + si + "Mute", bandNames[i] + " Mute", false));
    }

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return layout;
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    if (! suppressDirtyTracking.load())
        currentPresetDirty.store(true);
}

void PluginProcessor::startAutoLearn()
{
    const juce::SpinLock::ScopedLockType lock(learnLock);
    learnFifo.fill(0.0f);
    learnFFTData.fill(0.0f);
    learnSpectrumHistogram.fill(0.0);
    learnFifoIndex = 0;
    learnFramesCaptured = 0;
    learnPreviewCrossovers = { getCrossoverFrequency(0), getCrossoverFrequency(1), getCrossoverFrequency(2) };
    learnPreviewValid.store(true);
    autoLearnProgress.store(0.0f);
    pendingLearnResultReady.store(false);
    autoLearnActive.store(true);
}

void PluginProcessor::cancelAutoLearn()
{
    autoLearnActive.store(false);
    autoLearnProgress.store(0.0f);
    learnPreviewValid.store(false);
}

float PluginProcessor::getVisualCrossoverFrequency(int index) const
{
    if (! juce::isPositiveAndBelow(index, numCrossovers))
        return getCrossoverFrequency(index);

    if (learnPreviewValid.load())
    {
        const juce::SpinLock::ScopedLockType lock(learnLock);
        return learnPreviewCrossovers[(size_t) index];
    }

    return getCrossoverFrequency(index);
}

bool PluginProcessor::applyPendingAutoLearnResult()
{
    if (! pendingLearnResultReady.exchange(false))
        return false;

    applyLearnedCrossovers(pendingLearnedCrossovers);
    learnPreviewValid.store(false);
    return true;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    crossover1.prepare(spec);
    crossover2.prepare(spec);
    crossover3.prepare(spec);
    allpass2ForLow.prepare(spec);
    allpass3ForLow.prepare(spec);
    allpass3ForLowMid.prepare(spec);

    crossover1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover3.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    allpass2ForLow.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    allpass3ForLow.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    allpass3ForLowMid.setType(juce::dsp::LinkwitzRileyFilterType::allpass);

    updateCrossoverFilters();

    const float smoothTimeSeconds = 0.02f;
    for (int i = 0; i < numBands; ++i)
    {
        smoothedWidth[i].reset(sampleRate, smoothTimeSeconds);
        smoothedWidth[i].setCurrentAndTargetValue(100.0f);
        smoothedPan[i].reset(sampleRate, smoothTimeSeconds);
        smoothedPan[i].setCurrentAndTargetValue(0.0f);
    }

    smoothedOutputGain.reset(sampleRate, smoothTimeSeconds);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);

    vectorscopeDecimationCounter = 0;
    learnFifoIndex = 0;
    learnFramesCaptured = 0;
    autoLearnProgress.store(0.0f);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::updateCrossoverFilters()
{
    float freq1 = clampCrossover(*apvts.getRawParameterValue("crossover1"));
    float freq2 = clampCrossover(*apvts.getRawParameterValue("crossover2"));
    float freq3 = clampCrossover(*apvts.getRawParameterValue("crossover3"));

    if (freq2 < freq1 * minGapRatio)
        freq2 = freq1 * minGapRatio;
    if (freq3 < freq2 * minGapRatio)
        freq3 = freq2 * minGapRatio;

    freq1 = clampCrossover(freq1);
    freq2 = clampCrossover(freq2);
    freq3 = clampCrossover(freq3);

    crossover1.setCutoffFrequency(freq1);
    crossover2.setCutoffFrequency(freq2);
    crossover3.setCutoffFrequency(freq3);
    allpass2ForLow.setCutoffFrequency(freq2);
    allpass3ForLow.setCutoffFrequency(freq3);
    allpass3ForLowMid.setCutoffFrequency(freq3);

    lastCrossoverFreqs = { freq1, freq2, freq3 };
}

void PluginProcessor::pushLearnSample(float monoSample)
{
    if (! autoLearnActive.load())
        return;

    learnFifo[(size_t) learnFifoIndex++] = monoSample;

    if (learnFifoIndex < learnFftSize)
        return;

    learnFifoIndex = 0;

    for (int i = 0; i < learnFftSize; ++i)
        learnFFTData[(size_t) i] = learnFifo[(size_t) i];
    std::fill(learnFFTData.begin() + learnFftSize, learnFFTData.end(), 0.0f);

    learnWindow.multiplyWithWindowingTable(learnFFTData.data(), learnFftSize);
    learnFFT.performFrequencyOnlyForwardTransform(learnFFTData.data());

    {
        const juce::SpinLock::ScopedLockType lock(learnLock);
        for (int bin = 1; bin < learnFftSize / 2; ++bin)
        {
            const float frequency = (float) bin * (float) currentSampleRate / (float) learnFftSize;
            if (frequency < minCrossoverHz || frequency > maxCrossoverHz)
                continue;

            const float normalised = (std::log10(frequency) - std::log10(minCrossoverHz))
                                   / (std::log10(maxCrossoverHz) - std::log10(minCrossoverHz));
            const int histogramIndex = juce::jlimit(0, learnHistogramBins - 1,
                                                    (int) std::floor(normalised * (float) (learnHistogramBins - 1)));
            const double magnitude = (double) learnFFTData[(size_t) bin];
            learnSpectrumHistogram[(size_t) histogramIndex] += magnitude * magnitude;
        }
    }

    ++learnFramesCaptured;
    autoLearnProgress.store(juce::jlimit(0.0f, 1.0f, (float) learnFramesCaptured / (float) learnTargetFrames));
    const auto livePreview = buildLearnedCrossoverTargets();
    {
        const juce::SpinLock::ScopedLockType lock(learnLock);
        learnPreviewCrossovers = livePreview;
    }

    if (learnFramesCaptured < learnTargetFrames)
        return;

    pendingLearnedCrossovers = buildLearnedCrossoverTargets();
    pendingLearnResultReady.store(true);
    autoLearnActive.store(false);
    autoLearnProgress.store(1.0f);
}

std::array<float, PluginProcessor::numCrossovers> PluginProcessor::buildLearnedCrossoverTargets() const
{
    std::array<float, numCrossovers> learned = { 160.0f, 1800.0f, 7800.0f };
    std::array<double, learnHistogramBins> histogramCopy{};

    {
        const juce::SpinLock::ScopedLockType lock(learnLock);
        histogramCopy = learnSpectrumHistogram;
    }

    double totalEnergy = 0.0;
    for (const auto energy : histogramCopy)
        totalEnergy += energy;

    if (totalEnergy <= 1.0e-9)
        return learned;

    const double targetFractions[numCrossovers] = { 0.25, 0.50, 0.75 };
    double cumulativeEnergy = 0.0;
    int targetIndex = 0;

    for (int bin = 0; bin < learnHistogramBins && targetIndex < numCrossovers; ++bin)
    {
        cumulativeEnergy += histogramCopy[(size_t) bin];
        while (targetIndex < numCrossovers && cumulativeEnergy >= totalEnergy * targetFractions[targetIndex])
        {
            const float rawFrequency = frequencyFromLogBin(bin, learnHistogramBins);
            learned[(size_t) targetIndex] = snapLearnedFrequency(rawFrequency);
            ++targetIndex;
        }
    }

    learned[0] = juce::jlimit(learnMinSuggested, 800.0f, learned[0]);
    learned[1] = juce::jlimit(learned[0] * minGapRatio, 6000.0f, learned[1]);
    learned[2] = juce::jlimit(learned[1] * minGapRatio, learnMaxSuggested, learned[2]);

    for (int i = 1; i < numCrossovers; ++i)
        learned[(size_t) i] = juce::jmax(learned[(size_t) i], learned[(size_t) (i - 1)] * minGapRatio);

    learned[2] = juce::jmin(learned[2], learnMaxSuggested);
    return learned;
}

void PluginProcessor::applyLearnedCrossovers(const std::array<float, numCrossovers>& frequencies)
{
    suppressDirtyTracking.store(true);

    const juce::String ids[] = { "crossover1", "crossover2", "crossover3" };
    for (int i = 0; i < numCrossovers; ++i)
    {
        if (auto* parameter = apvts.getParameter(ids[i]))
            parameter->setValueNotifyingHost(parameter->getNormalisableRange().convertTo0to1(frequencies[(size_t) i]));
    }

    suppressDirtyTracking.store(false);
    currentPresetDirty.store(true);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const bool bypassed = *apvts.getRawParameterValue("bypass") > 0.5f;
    if (bypassed)
        return;

    updateCrossoverFilters();

    for (int i = 0; i < numBands; ++i)
    {
        auto si = juce::String(i + 1);
        smoothedWidth[i].setTargetValue(*apvts.getRawParameterValue("band" + si + "Width"));
        smoothedPan[i].setTargetValue(*apvts.getRawParameterValue("band" + si + "Pan"));
    }

    const float outputGainDb = *apvts.getRawParameterValue("outputGain");
    smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb));

    bool anySolo = false;
    std::array<bool, numBands> soloState{};
    std::array<bool, numBands> muteState{};
    for (int i = 0; i < numBands; ++i)
    {
        auto si = juce::String(i + 1);
        soloState[i] = *apvts.getRawParameterValue("band" + si + "Solo") > 0.5f;
        muteState[i] = *apvts.getRawParameterValue("band" + si + "Mute") > 0.5f;
        if (soloState[i])
            anySolo = true;
    }

    struct BandAccum
    {
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
        double peakL = 0.0, peakR = 0.0;
    };
    std::array<BandAccum, numBands> accum{};

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = left[s];
        const float inR = right[s];
        pushLearnSample((inL + inR) * 0.5f);

        // Band splitting
        float lowL, upperRestL;
        crossover1.processSample(0, inL, lowL, upperRestL);
        float lowR, upperRestR;
        crossover1.processSample(1, inR, lowR, upperRestR);

        float lowMidL, highRestL;
        crossover2.processSample(0, upperRestL, lowMidL, highRestL);
        float lowMidR, highRestR;
        crossover2.processSample(1, upperRestR, lowMidR, highRestR);

        float highMidL, highL;
        crossover3.processSample(0, highRestL, highMidL, highL);
        float highMidR, highR;
        crossover3.processSample(1, highRestR, highMidR, highR);

        // Phase compensation
        float apDummyL, apDummyR;
        allpass2ForLow.processSample(0, lowL, lowL, apDummyL);
        allpass2ForLow.processSample(1, lowR, lowR, apDummyR);
        allpass3ForLow.processSample(0, lowL, lowL, apDummyL);
        allpass3ForLow.processSample(1, lowR, lowR, apDummyR);
        allpass3ForLowMid.processSample(0, lowMidL, lowMidL, apDummyL);
        allpass3ForLowMid.processSample(1, lowMidR, lowMidR, apDummyR);

        // Per-band M/S width processing
        float bandL[numBands] = { lowL, lowMidL, highMidL, highL };
        float bandR[numBands] = { lowR, lowMidR, highMidR, highR };

        float outL = 0.0f, outR = 0.0f;

        // Find dominant band for vectorscope colouring
        int dominantBand = 0;
        float maxEnergy = 0.0f;

        for (int b = 0; b < numBands; ++b)
        {
            const float widthPct = smoothedWidth[b].getNextValue();
            const float widthFactor = widthPct / 100.0f;
            const float panValue = smoothedPan[b].getNextValue() / 100.0f;

            float mid  = (bandL[b] + bandR[b]) * 0.5f;
            float side = (bandL[b] - bandR[b]) * 0.5f;
            side *= widthFactor;
            float bL = mid + side;
            float bR = mid - side;

            if (std::abs(panValue) > 0.001f)
            {
                const float panAngle = (panValue + 1.0f) * 0.5f;
                const float gainL = std::cos(panAngle * juce::MathConstants<float>::halfPi);
                const float gainR = std::sin(panAngle * juce::MathConstants<float>::halfPi);
                bL *= gainL * 1.4142135f;
                bR *= gainR * 1.4142135f;
            }

            accum[b].sumLR += (double) bL * (double) bR;
            accum[b].sumLL += (double) bL * (double) bL;
            accum[b].sumRR += (double) bR * (double) bR;
            accum[b].peakL = juce::jmax(accum[b].peakL, (double) std::abs(bL));
            accum[b].peakR = juce::jmax(accum[b].peakR, (double) std::abs(bR));

            float energy = bL * bL + bR * bR;
            if (energy > maxEnergy)
            {
                maxEnergy = energy;
                dominantBand = b;
            }

            bool active = true;
            if (anySolo)
                active = soloState[b];
            else if (muteState[b])
                active = false;

            if (active)
            {
                outL += bL;
                outR += bR;
            }
        }

        const float gain = smoothedOutputGain.getNextValue();
        left[s] = outL * gain;
        right[s] = outR * gain;

        // Write to vectorscope ring buffer (decimated)
        if (++vectorscopeDecimationCounter >= vectorscopeDecimation)
        {
            vectorscopeDecimationCounter = 0;
            const float vsM = (outL + outR) * 0.5f;
            const float vsS = (outL - outR) * 0.5f;
            int writeIdx = vectorscopeWriteIndex.load(std::memory_order_relaxed);
            vectorscopeBuffer[writeIdx] = { vsM, vsS, dominantBand };
            vectorscopeWriteIndex.store((writeIdx + 1) % vectorscopeSize, std::memory_order_relaxed);
        }
    }

    // Update meters
    const float decay = 0.85f;
    for (int b = 0; b < numBands; ++b)
    {
        float corr = 0.0f;
        const double denom = std::sqrt(accum[b].sumLL * accum[b].sumRR);
        if (denom > 1e-10)
            corr = (float) (accum[b].sumLR / denom);

        const float prevCorr = bandMeters[b].correlation.load();
        bandMeters[b].correlation.store(prevCorr * decay + corr * (1.0f - decay));
        bandMeters[b].levelL.store((float) accum[b].peakL);
        bandMeters[b].levelR.store((float) accum[b].peakR);

        const float measuredWidth = 1.0f - juce::jlimit(-1.0f, 1.0f, corr);
        const float prevWidth = bandMeters[b].width.load();
        bandMeters[b].width.store(prevWidth * decay + measuredWidth * (1.0f - decay));
    }
}

void PluginProcessor::getVectorscopeSnapshot(std::array<VectorscopePoint, vectorscopeSize>& dest) const
{
    // Simple copy - slight tearing is acceptable for visualization
    std::copy(vectorscopeBuffer.begin(), vectorscopeBuffer.end(), dest.begin());
}

// ---- Accessors ----

float PluginProcessor::getCrossoverFrequency(int index) const
{
    const juce::String ids[] = { "crossover1", "crossover2", "crossover3" };
    if (index >= 0 && index < numCrossovers)
        return *apvts.getRawParameterValue(ids[index]);
    return 1000.0f;
}

float PluginProcessor::getBandWidth(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands)
        return *apvts.getRawParameterValue("band" + juce::String(bandIndex + 1) + "Width");
    return 100.0f;
}

float PluginProcessor::getBandPan(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands)
        return *apvts.getRawParameterValue("band" + juce::String(bandIndex + 1) + "Pan");
    return 0.0f;
}

bool PluginProcessor::isBandSoloed(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands)
        return *apvts.getRawParameterValue("band" + juce::String(bandIndex + 1) + "Solo") > 0.5f;
    return false;
}

bool PluginProcessor::isBandMuted(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < numBands)
        return *apvts.getRawParameterValue("band" + juce::String(bandIndex + 1) + "Mute") > 0.5f;
    return false;
}

float PluginProcessor::getOutputGainDb() const
{
    return *apvts.getRawParameterValue("outputGain");
}

// ---- Preset Management ----

const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets()
{
    return sspimager::presets::getFactoryPresetLibrary();
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
#if JUCE_MAC
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Application Support");
#elif JUCE_WINDOWS
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#else
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#endif
    return base.getChildFile("SSP").getChildFile("SSP Imager").getChildFile("Presets");
}

void PluginProcessor::refreshUserPresets()
{
    userPresets.clear();
    userPresetFiles.clear();

    auto dir = getUserPresetDirectory();
    if (! dir.isDirectory())
        return;

    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.sspimagerpreset");
    files.sort();

    for (const auto& file : files)
    {
        auto json = juce::JSON::parse(file);
        if (! json.isObject())
            continue;

        PresetRecord preset;
        preset.id = "user:" + slugify(file.getFileNameWithoutExtension());
        preset.name = json.getProperty("name", file.getFileNameWithoutExtension()).toString();
        preset.category = json.getProperty("category", "").toString();
        preset.author = json.getProperty("author", "User").toString();
        preset.isFactory = false;
        preset.crossover1 = (float) json.getProperty("crossover1", 200.0);
        preset.crossover2 = (float) json.getProperty("crossover2", 2000.0);
        preset.crossover3 = (float) json.getProperty("crossover3", 8000.0);
        preset.outputGain = (float) json.getProperty("outputGain", 0.0);

        auto bandsArray = json.getProperty("bands", juce::var());
        if (auto* arr = bandsArray.getArray())
        {
            for (int i = 0; i < juce::jmin((int) arr->size(), numBands); ++i)
            {
                auto bandObj = (*arr)[i];
                preset.bands[i].width = (float) bandObj.getProperty("width", 100.0);
                preset.bands[i].pan = (float) bandObj.getProperty("pan", 0.0);
            }
        }

        userPresets.add(preset);
        userPresetFiles.add(file);
    }
}

void PluginProcessor::applyPresetRecord(const PresetRecord& preset)
{
    suppressDirtyTracking.store(true);

    if (auto* p = apvts.getParameter("crossover1"))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.crossover1));
    if (auto* p = apvts.getParameter("crossover2"))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.crossover2));
    if (auto* p = apvts.getParameter("crossover3"))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.crossover3));

    for (int i = 0; i < numBands; ++i)
    {
        auto si = juce::String(i + 1);
        if (auto* p = apvts.getParameter("band" + si + "Width"))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.bands[i].width));
        if (auto* p = apvts.getParameter("band" + si + "Pan"))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.bands[i].pan));
        if (auto* p = apvts.getParameter("band" + si + "Solo"))
            p->setValueNotifyingHost(0.0f);
        if (auto* p = apvts.getParameter("band" + si + "Mute"))
            p->setValueNotifyingHost(0.0f);
    }

    if (auto* p = apvts.getParameter("outputGain"))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(preset.outputGain));

    suppressDirtyTracking.store(false);
}

PluginProcessor::PresetRecord PluginProcessor::captureCurrentState() const
{
    PresetRecord record;
    record.crossover1 = *apvts.getRawParameterValue("crossover1");
    record.crossover2 = *apvts.getRawParameterValue("crossover2");
    record.crossover3 = *apvts.getRawParameterValue("crossover3");
    record.outputGain = *apvts.getRawParameterValue("outputGain");

    for (int i = 0; i < numBands; ++i)
    {
        auto si = juce::String(i + 1);
        record.bands[i].width = *apvts.getRawParameterValue("band" + si + "Width");
        record.bands[i].pan = *apvts.getRawParameterValue("band" + si + "Pan");
    }

    return record;
}

void PluginProcessor::setCurrentPresetMetadata(const juce::String& key, const juce::String& name,
                                                const juce::String& category, bool isFactory)
{
    currentPresetKey = key;
    currentPresetName = name;
    currentPresetCategory = category;
    currentPresetIsFactory = isFactory;
    currentPresetDirty.store(false);
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow(index, presets.size()))
        return false;

    const auto& preset = presets.getReference(index);
    applyPresetRecord(preset);
    setCurrentPresetMetadata(preset.id, preset.name, preset.category, true);
    return true;
}

bool PluginProcessor::loadUserPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, userPresets.size()))
        return false;

    const auto& preset = userPresets.getReference(index);
    applyPresetRecord(preset);
    setCurrentPresetMetadata(preset.id, preset.name, preset.category, false);
    return true;
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName, const juce::String& category)
{
    if (presetName.trim().isEmpty())
        return false;

    auto dir = getUserPresetDirectory();
    dir.createDirectory();

    auto record = captureCurrentState();
    record.name = presetName;
    record.category = category;
    record.author = "User";

    juce::var bandsVar;
    for (int i = 0; i < numBands; ++i)
    {
        auto* bandObj = new juce::DynamicObject();
        bandObj->setProperty("width", record.bands[i].width);
        bandObj->setProperty("pan", record.bands[i].pan);
        bandsVar.append(juce::var(bandObj));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", presetName);
    obj->setProperty("category", category);
    obj->setProperty("author", "User");
    obj->setProperty("crossover1", record.crossover1);
    obj->setProperty("crossover2", record.crossover2);
    obj->setProperty("crossover3", record.crossover3);
    obj->setProperty("outputGain", record.outputGain);
    obj->setProperty("bands", bandsVar);
    obj->setProperty("version", "1.0");

    auto fileName = slugify(presetName) + ".sspimagerpreset";
    auto file = dir.getChildFile(fileName);
    file.replaceWithText(juce::JSON::toString(juce::var(obj)));

    refreshUserPresets();

    auto newId = "user:" + slugify(presetName);
    setCurrentPresetMetadata(newId, presetName, category, false);
    return true;
}

bool PluginProcessor::deleteUserPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, userPresetFiles.size()))
        return false;

    userPresetFiles[index].deleteFile();
    refreshUserPresets();
    return true;
}

bool PluginProcessor::importPresetFromFile(const juce::File& file, bool loadAfterImport)
{
    auto dir = getUserPresetDirectory();
    dir.createDirectory();
    auto dest = dir.getChildFile(file.getFileName());
    if (! file.copyFileTo(dest))
        return false;

    refreshUserPresets();

    if (loadAfterImport)
    {
        for (int i = 0; i < userPresets.size(); ++i)
        {
            if (userPresetFiles[i] == dest)
                return loadUserPreset(i);
        }
    }

    return true;
}

bool PluginProcessor::exportCurrentPresetToFile(const juce::File& file) const
{
    auto record = captureCurrentState();

    juce::var bandsVar;
    for (int i = 0; i < numBands; ++i)
    {
        auto* bandObj = new juce::DynamicObject();
        bandObj->setProperty("width", record.bands[i].width);
        bandObj->setProperty("pan", record.bands[i].pan);
        bandsVar.append(juce::var(bandObj));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", currentPresetName.isNotEmpty() ? currentPresetName : "Exported Preset");
    obj->setProperty("category", currentPresetCategory);
    obj->setProperty("author", "User");
    obj->setProperty("crossover1", record.crossover1);
    obj->setProperty("crossover2", record.crossover2);
    obj->setProperty("crossover3", record.crossover3);
    obj->setProperty("outputGain", record.outputGain);
    obj->setProperty("bands", bandsVar);
    obj->setProperty("version", "1.0");

    return file.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

bool PluginProcessor::resetUserPresetsToFactoryDefaults()
{
    auto dir = getUserPresetDirectory();
    if (dir.isDirectory())
        dir.deleteRecursively();
    refreshUserPresets();
    return true;
}

juce::StringArray PluginProcessor::getAvailablePresetCategories() const
{
    juce::StringArray categories;
    for (const auto& preset : getFactoryPresets())
        if (preset.category.isNotEmpty())
            categories.addIfNotAlreadyThere(preset.category);
    for (const auto& preset : userPresets)
        if (preset.category.isNotEmpty())
            categories.addIfNotAlreadyThere(preset.category);
    categories.sort(true);
    return categories;
}

juce::Array<PluginProcessor::PresetRecord> PluginProcessor::getSequentialPresetList() const
{
    juce::Array<PresetRecord> list;
    for (const auto& preset : getFactoryPresets())
        list.add(preset);
    for (const auto& preset : userPresets)
        list.add(preset);
    return list;
}

bool PluginProcessor::stepPreset(int delta)
{
    auto list = getSequentialPresetList();
    if (list.isEmpty())
        return false;

    int currentIndex = -1;
    for (int i = 0; i < list.size(); ++i)
    {
        if (list.getReference(i).id == currentPresetKey)
        {
            currentIndex = i;
            break;
        }
    }

    int newIndex = (currentIndex + delta + list.size()) % list.size();
    return loadPresetByKey(list.getReference(newIndex).id);
}

bool PluginProcessor::loadPresetByKey(const juce::String& presetKey)
{
    const auto& factory = getFactoryPresets();
    for (int i = 0; i < factory.size(); ++i)
        if (factory.getReference(i).id == presetKey)
            return loadFactoryPreset(i);

    for (int i = 0; i < userPresets.size(); ++i)
        if (userPresets.getReference(i).id == presetKey)
            return loadUserPreset(i);

    return false;
}

// ---- Plugin boilerplate ----

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("presetKey", currentPresetKey, nullptr);
    state.setProperty("presetName", currentPresetName, nullptr);
    state.setProperty("presetCategory", currentPresetCategory, nullptr);
    state.setProperty("presetIsFactory", currentPresetIsFactory, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml(*xml);
        currentPresetKey = newState.getProperty("presetKey", "").toString();
        currentPresetName = newState.getProperty("presetName", "").toString();
        currentPresetCategory = newState.getProperty("presetCategory", "").toString();
        currentPresetIsFactory = (bool) newState.getProperty("presetIsFactory", false);
        suppressDirtyTracking.store(true);
        apvts.replaceState(newState);
        suppressDirtyTracking.store(false);
        currentPresetDirty.store(false);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
