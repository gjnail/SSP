#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float minHz = 20.0f;
constexpr float maxHz = 20000.0f;

inline float attackCoeff(float ms, double sr)
{
    const auto t = juce::jmax(0.1f, ms) * 0.001f;
    return std::exp(-1.0f / static_cast<float>(t * sr));
}

inline float releaseCoeff(float ms, double sr)
{
    const auto t = juce::jmax(1.0f, ms) * 0.001f;
    return std::exp(-1.0f / static_cast<float>(t * sr));
}

static const char* bandIds[PluginProcessor::numSCBands] = {"low", "mid", "high"};

enum TriggerMode
{
    triggerAudio = 0,
    triggerBpmSync,
    triggerMidi
};

const juce::StringArray& getTriggerRateOptions()
{
    static const juce::StringArray options{"1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/32"};
    return options;
}

float getRateInQuarterNotes(int rateIndex)
{
    static constexpr std::array<float, 12> rates{{4.0f, 3.0f, 2.0f, 4.0f / 3.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f, 0.125f}};
    return rates[(size_t)juce::jlimit(0, (int)rates.size() - 1, rateIndex)];
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        return param->getIndex();

    return 0;
}

std::vector<CurvePoint> makeDefaultPumpCurve()
{
    return {{0.0f, 1.0f, 0.0f},
            {0.012f, 0.0f, 0.78f},
            {0.21f, 0.0f, 0.56f},
            {0.27f, 0.08f, 0.22f},
            {0.33f, 0.96f, -0.18f},
            {0.38f, 1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f}};
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lowCrossHz",
        "Low split",
        juce::NormalisableRange<float>(50.0f, 2000.0f, 1.0f, 0.3f),
        200.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "highCrossHz",
        "High split",
        juce::NormalisableRange<float>(500.0f, 12000.0f, 1.0f, 0.3f),
        2500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "scDepthDb",
        "Max duck depth",
        juce::NormalisableRange<float>(6.0f, 60.0f, 0.1f, 0.45f),
        36.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sidechainGainDb",
        "Sidechain gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "triggerMode",
        "Trigger mode",
        juce::StringArray{"Audio", "BPM Sync", "MIDI"},
        1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "triggerRate",
        "Trigger rate",
        getTriggerRateOptions(),
        5));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "linkBands",
        "Link bands",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attackMs",
        "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.4f),
        5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "releaseMs",
        "Release",
        juce::NormalisableRange<float>(5.0f, 800.0f, 0.1f, 0.4f),
        120.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWetBandLow",
        "Low band wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWetBandMid",
        "Mid band wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWetBandHigh",
        "High band wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWetOverall",
        "Overall wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    return {params.begin(), params.end()};
}

void PluginProcessor::initDefaultCurves()
{
    for (int b = 0; b < numSCBands; ++b)
        bandCurves[static_cast<size_t>(b)] = makeDefaultPumpCurve();
}

void PluginProcessor::syncCurveToValueTree()
{
    juce::ValueTree cd("curveData");
    {
        const juce::ScopedLock sl(curveLock);
        for (int b = 0; b < numSCBands; ++b)
        {
            juce::ValueTree band(bandIds[b]);
            for (const auto& pt : bandCurves[static_cast<size_t>(b)])
            {
                juce::ValueTree p("p");
                p.setProperty("x", pt.x, nullptr);
                p.setProperty("y", pt.y, nullptr);
                p.setProperty("c", pt.curve, nullptr);
                band.appendChild(p, nullptr);
            }
            cd.appendChild(band, nullptr);
        }
    }

    auto existing = apvts.state.getChildWithName("curveData");
    if (existing.isValid())
        apvts.state.removeChild(existing, nullptr);

    apvts.state.appendChild(cd, nullptr);
}

void PluginProcessor::loadCurvesFromValueTree()
{
    const juce::ScopedLock sl(curveLock);
    auto cd = apvts.state.getChildWithName("curveData");
    if (!cd.isValid())
    {
        initDefaultCurves();
        return;
    }

    for (int b = 0; b < numSCBands; ++b)
    {
        bandCurves[static_cast<size_t>(b)].clear();
        auto band = cd.getChildWithName(bandIds[b]);
        if (band.isValid())
        {
            for (int i = 0; i < band.getNumChildren(); ++i)
            {
                auto p = band.getChild(i);
                if (p.getType() == juce::Identifier("p"))
                {
                    const float x = (float)p.getProperty("x");
                    const float y = (float)p.getProperty("y");
                    const float c = p.hasProperty("c") ? (float)p.getProperty("c") : 0.0f;
                    bandCurves[static_cast<size_t>(b)].push_back({x, y, c});
                }
            }
        }

        CurveInterp::sortAndClamp(bandCurves[static_cast<size_t>(b)]);
        if (bandCurves[static_cast<size_t>(b)].size() < 2)
            bandCurves[static_cast<size_t>(b)] = makeDefaultPumpCurve();
    }
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto& meter : sidechainMeters)
        meter.store(0.0f);

    for (auto& meter : gainReductionMeters)
        meter.store(0.0f);

    initDefaultCurves();
    if (!apvts.state.getChildWithName("curveData").isValid())
        syncCurveToValueTree();
    else
        loadCurvesFromValueTree();
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::setBandCurve(int band, std::vector<CurvePoint> points)
{
    if (band < 0 || band >= numSCBands)
        return;

    CurveInterp::sortAndClamp(points);
    if (points.size() < 2)
        return;

    {
        const juce::ScopedLock sl(curveLock);
        if (getLinkBandsEnabled())
        {
            for (auto& curve : bandCurves)
                curve = points;
        }
        else
        {
            bandCurves[static_cast<size_t>(band)] = std::move(points);
        }
    }
    syncCurveToValueTree();
}

void PluginProcessor::getBandCurve(int band, std::vector<CurvePoint>& out) const
{
    const juce::ScopedLock sl(curveLock);
    if (band >= 0 && band < numSCBands)
        out = bandCurves[static_cast<size_t>(band)];
}

float PluginProcessor::getBandSidechainLevel(int band) const noexcept
{
    if (band < 0 || band >= numSCBands)
        return 0.0f;

    return sidechainMeters[static_cast<size_t>(band)].load();
}

float PluginProcessor::getBandGainReduction(int band) const noexcept
{
    if (band < 0 || band >= numSCBands)
        return 0.0f;

    return gainReductionMeters[static_cast<size_t>(band)].load();
}

float PluginProcessor::getTriggerActivity() const noexcept
{
    return triggerActivity.load();
}

void PluginProcessor::setLinkBandsEnabled(bool shouldLink, int sourceBand)
{
    sourceBand = juce::jlimit(0, numSCBands - 1, sourceBand);

    if (auto* param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("linkBands")))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(shouldLink ? 1.0f : 0.0f);
        param->endChangeGesture();
    }

    if (!shouldLink)
        return;

    std::vector<CurvePoint> sourceCurve;
    {
        const juce::ScopedLock sl(curveLock);
        sourceCurve = bandCurves[static_cast<size_t>(sourceBand)];
        for (auto& curve : bandCurves)
            curve = sourceCurve;
    }
    syncCurveToValueTree();
}

bool PluginProcessor::getLinkBandsEnabled() const noexcept
{
    if (auto* param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("linkBands")))
        return param->get();

    return false;
}

void PluginProcessor::setSoloBand(int band) noexcept
{
    soloBand.store((band >= 0 && band < numSCBands) ? band : -1);
}

int PluginProcessor::getSoloBand() const noexcept
{
    return soloBand.load();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    mainLowBandSplit.prepare(spec);
    mainHighBandSplit.prepare(spec);
    scLowBandSplit.prepare(spec);
    scHighBandSplit.prepare(spec);

    updateCrossoverFrequencies();

    for (auto& ch : envSc)
        for (auto& e : ch)
            e.follower = 0.0f;

    fallbackSyncPhase = 0.0;
    midiTriggerPhase = 1.0;
    midiTriggerActive = false;
    triggerActivity.store(0.0f);
    audioTriggerPhase.fill(1.0);
    audioTriggerDetector.fill(0.0f);
    audioTriggerActive.fill(false);
    audioTriggerArmed.fill(true);
    smoothedBandGain.fill(1.0f);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != mainOut)
        return false;

    if (layouts.inputBuses.size() < 2)
        return false;

    const auto sidechain = layouts.getChannelSet(true, 1);
    if (sidechain.isDisabled())
        return true;

    return sidechain == juce::AudioChannelSet::stereo();
}

void PluginProcessor::updateCrossoverFrequencies()
{
    const float lowHz = apvts.getRawParameterValue("lowCrossHz")->load();
    const float highHz = apvts.getRawParameterValue("highCrossHz")->load();

    const float safeLow = juce::jlimit(minHz, maxHz * 0.45f, lowHz);
    const float safeHigh = juce::jlimit(safeLow * 1.1f, maxHz, highHz);

    mainLowBandSplit.setCutoffFrequency(safeLow);
    scLowBandSplit.setCutoffFrequency(safeLow);
    mainHighBandSplit.setCutoffFrequency(safeHigh);
    scHighBandSplit.setCutoffFrequency(safeHigh);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    updateCrossoverFrequencies();

    const float sidechainGainDb = apvts.getRawParameterValue("sidechainGainDb")->load();
    const int triggerMode = getChoiceIndex(apvts, "triggerMode");
    const int triggerRateIndex = getChoiceIndex(apvts, "triggerRate");
    const float wetLow = apvts.getRawParameterValue("dryWetBandLow")->load();
    const float wetMid = apvts.getRawParameterValue("dryWetBandMid")->load();
    const float wetHigh = apvts.getRawParameterValue("dryWetBandHigh")->load();
    const float wetOverall = apvts.getRawParameterValue("dryWetOverall")->load();
    const int activeSoloBand = soloBand.load();

    const float sidechainGain = juce::Decibels::decibelsToGain(sidechainGainDb);
    const float rateQuarterNotes = getRateInQuarterNotes(triggerRateIndex);
    const float gainSmoothingAlpha = 1.0f - std::exp(-1.0f / (0.00075f * (float) juce::jmax(1.0, currentSampleRate)));

    const std::array<float, 3> bandWet{{wetLow, wetMid, wetHigh}};
    std::array<float, 3> sidechainMeterAccum{{0.0f, 0.0f, 0.0f}};
    std::array<float, 3> gainReductionAccum{{0.0f, 0.0f, 0.0f}};

    std::array<std::vector<CurvePoint>, numSCBands> localCurves;
    {
        const juce::ScopedLock sl(curveLock);
        for (int i = 0; i < numSCBands; ++i)
            localCurves[static_cast<size_t>(i)] = bandCurves[static_cast<size_t>(i)];
    }

    auto mainIn = getBusBuffer(buffer, true, 0);
    auto scIn = getBusBuffer(buffer, true, 1);
    auto out = getBusBuffer(buffer, false, 0);

    const int numCh = juce::jmin(2, mainIn.getNumChannels(), out.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    std::vector<uint8_t> noteOnAtSample((size_t)numSamples, 0);
    if (triggerMode == triggerMidi)
    {
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                const int samplePosition = juce::jlimit(0, juce::jmax(0, numSamples - 1), metadata.samplePosition);
                noteOnAtSample[(size_t)samplePosition] = 1;
            }
        }
    }

    double hostBpm = 120.0;
    double hostPpq = 0.0;
    bool hasHostPpq = false;
    if (auto* currentPlayHead = getPlayHead())
    {
        if (auto position = currentPlayHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;
            if (auto ppq = position->getPpqPosition())
            {
                hostPpq = *ppq;
                hasHostPpq = true;
            }
        }
    }

    const double quarterNotesPerSample = (hostBpm / 60.0) / currentSampleRate;
    const double triggerPhaseIncrement = quarterNotesPerSample / juce::jmax(0.125, (double)rateQuarterNotes);
    bool triggerEventInBlock = false;

    for (int n = 0; n < numSamples; ++n)
    {
        float globalTriggerPhase = 1.0f;

        if (triggerMode == triggerBpmSync)
        {
            if (hasHostPpq)
            {
                const double quarterNotePosition = hostPpq + (double)n * quarterNotesPerSample;
                globalTriggerPhase = (float)std::fmod(quarterNotePosition / juce::jmax(0.125, (double)rateQuarterNotes), 1.0);
                if (globalTriggerPhase < 0.0f)
                    globalTriggerPhase += 1.0f;
                if (globalTriggerPhase <= (float)(triggerPhaseIncrement * 2.0))
                    triggerEventInBlock = true;
            }
            else
            {
                globalTriggerPhase = (float)fallbackSyncPhase;
                fallbackSyncPhase += quarterNotesPerSample / juce::jmax(0.125, (double)rateQuarterNotes);
                if (fallbackSyncPhase >= 1.0)
                    fallbackSyncPhase -= std::floor(fallbackSyncPhase);
                if (globalTriggerPhase <= (float)(triggerPhaseIncrement * 2.0))
                    triggerEventInBlock = true;
            }
        }
        else if (triggerMode == triggerMidi)
        {
            if (noteOnAtSample[(size_t)n] != 0)
            {
                midiTriggerPhase = 0.0;
                midiTriggerActive = true;
                triggerEventInBlock = true;
            }

            globalTriggerPhase = midiTriggerActive ? (float)midiTriggerPhase : 1.0f;
            if (midiTriggerActive)
            {
                midiTriggerPhase += triggerPhaseIncrement;
                if (midiTriggerPhase >= 1.0)
                {
                    midiTriggerPhase = 1.0;
                    midiTriggerActive = false;
                }
            }
        }

        std::array<float, 2> mainSamples{{0.0f, 0.0f}};
        std::array<std::array<float, 3>, 2> mainBandsByChannel{};
        std::array<float, 3> audioBandInput{{0.0f, 0.0f, 0.0f}};

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float m = mainIn.getSample(ch, n);
            const float sc = (scIn.getNumChannels() > ch ? scIn.getSample(ch, n) : 0.0f) * sidechainGain;
            mainSamples[(size_t)ch] = m;

            float mLow, mMidHigh, mMid, mHigh;
            float sLow, sMidHigh, sMid, sHigh;

            mainLowBandSplit.processSample(ch, m, mLow, mMidHigh);
            scLowBandSplit.processSample(ch, sc, sLow, sMidHigh);

            mainHighBandSplit.processSample(ch, mMidHigh, mMid, mHigh);
            scHighBandSplit.processSample(ch, sMidHigh, sMid, sHigh);

            const std::array<float, 3> mainBands{{mLow, mMid, mHigh}};
            const std::array<float, 3> scBands{{sLow, sMid, sHigh}};
            mainBandsByChannel[(size_t)ch] = mainBands;

            if (triggerMode == triggerAudio)
                for (int b = 0; b < 3; ++b)
                    audioBandInput[(size_t)b] = juce::jmax(audioBandInput[(size_t)b], std::abs(scBands[(size_t)b]));
        }

        std::array<float, 3> bandTriggerPhase{{globalTriggerPhase, globalTriggerPhase, globalTriggerPhase}};
        if (triggerMode == triggerAudio)
        {
            constexpr float triggerThreshold = 0.06f;
            constexpr float rearmThreshold = 0.015f;
            constexpr float detectorAttack = 0.35f;
            constexpr float detectorRelease = 0.03f;

            for (int b = 0; b < 3; ++b)
            {
                float& detector = audioTriggerDetector[(size_t)b];
                const float input = audioBandInput[(size_t)b];
                const float coeff = input > detector ? detectorAttack : detectorRelease;
                detector += (input - detector) * coeff;

                if (audioTriggerArmed[(size_t)b] && detector >= triggerThreshold)
                {
                    audioTriggerPhase[(size_t)b] = 0.0;
                    audioTriggerActive[(size_t)b] = true;
                    audioTriggerArmed[(size_t)b] = false;
                    triggerEventInBlock = true;
                }
                else if (detector <= rearmThreshold)
                {
                    audioTriggerArmed[(size_t)b] = true;
                }

                bandTriggerPhase[(size_t)b] = audioTriggerActive[(size_t)b] ? (float)audioTriggerPhase[(size_t)b] : 1.0f;

                if (audioTriggerActive[(size_t)b])
                {
                    audioTriggerPhase[(size_t)b] += triggerPhaseIncrement;
                    if (audioTriggerPhase[(size_t)b] >= 1.0)
                    {
                        audioTriggerPhase[(size_t)b] = 1.0;
                        audioTriggerActive[(size_t)b] = false;
                    }
                }
            }
        }

        std::array<float, 3> currentBandGain{{1.0f, 1.0f, 1.0f}};
        std::array<float, 3> currentBandDuck{{0.0f, 0.0f, 0.0f}};
        for (int b = 0; b < 3; ++b)
        {
            const float phase = bandTriggerPhase[(size_t)b];
            const float targetGain = juce::jlimit(0.0f, 1.0f, CurveInterp::eval(phase, localCurves[(size_t)b]));
            smoothedBandGain[(size_t)b] += (targetGain - smoothedBandGain[(size_t)b]) * gainSmoothingAlpha;
            currentBandGain[(size_t)b] = juce::jlimit(0.0f, 1.0f, smoothedBandGain[(size_t)b]);
            currentBandDuck[(size_t)b] = 1.0f - currentBandGain[(size_t)b];
            sidechainMeterAccum[(size_t)b] += phase;
            gainReductionAccum[(size_t)b] += currentBandDuck[(size_t)b];
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float wetSum = 0.0f;
            float soloOutput = 0.0f;

            for (int b = 0; b < 3; ++b)
            {
                const float bandGain = currentBandGain[(size_t)b];
                const float sourceBand = mainBandsByChannel[(size_t)ch][(size_t)b];
                const float processed = sourceBand * bandGain;
                const float w = bandWet[(size_t)b];
                const float bandOut = sourceBand * (1.0f - w) + processed * w;
                wetSum += bandOut;
                if (activeSoloBand == b)
                    soloOutput = bandOut;
            }

            const float outS = activeSoloBand >= 0
                ? soloOutput
                : mainSamples[(size_t)ch] * (1.0f - wetOverall) + wetSum * wetOverall;
            out.setSample(ch, n, outS);
        }
    }

    for (int ch = numCh; ch < out.getNumChannels(); ++ch)
        out.clear(ch, 0, numSamples);

    const float meterSamples = static_cast<float>(juce::jmax(1, numCh * numSamples));
    constexpr float meterSmoothing = 0.25f;
    for (int b = 0; b < numSCBands; ++b)
    {
        const float levelTarget = juce::jlimit(0.0f, 1.0f, sidechainMeterAccum[static_cast<size_t>(b)] / meterSamples);
        const float grTarget = juce::jlimit(0.0f, 1.0f, gainReductionAccum[static_cast<size_t>(b)] / meterSamples);

        const float prevLevel = sidechainMeters[static_cast<size_t>(b)].load();
        const float prevGr = gainReductionMeters[static_cast<size_t>(b)].load();

        sidechainMeters[static_cast<size_t>(b)].store(prevLevel + (levelTarget - prevLevel) * meterSmoothing);
        gainReductionMeters[static_cast<size_t>(b)].store(prevGr + (grTarget - prevGr) * meterSmoothing);
    }

    const float previousTriggerActivity = triggerActivity.load();
    const float triggerTarget = triggerEventInBlock ? 1.0f : 0.0f;
    const float release = triggerEventInBlock ? 0.55f : 0.14f;
    triggerActivity.store(previousTriggerActivity + (triggerTarget - previousTriggerActivity) * release);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    syncCurveToValueTree();
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, static_cast<int>(sizeInBytes)))
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    loadCurvesFromValueTree();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
