#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
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
    return rates[(size_t) juce::jlimit(0, (int) rates.size() - 1, rateIndex)];
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

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    initDefaultCurve();
    if (!apvts.state.getChildWithName("curveData").isValid())
        syncCurveToValueTree();
    else
        loadCurveFromValueTree();
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::initDefaultCurve()
{
    const juce::ScopedLock sl(curveLock);
    bandCurve = makeDefaultPumpCurve();
}

void PluginProcessor::syncCurveToValueTree()
{
    juce::ValueTree curveData("curveData");
    juce::ValueTree curveNode("main");

    {
        const juce::ScopedLock sl(curveLock);
        for (const auto& pt : bandCurve)
        {
            juce::ValueTree p("p");
            p.setProperty("x", pt.x, nullptr);
            p.setProperty("y", pt.y, nullptr);
            p.setProperty("c", pt.curve, nullptr);
            curveNode.appendChild(p, nullptr);
        }
    }

    curveData.appendChild(curveNode, nullptr);

    auto existing = apvts.state.getChildWithName("curveData");
    if (existing.isValid())
        apvts.state.removeChild(existing, nullptr);

    apvts.state.appendChild(curveData, nullptr);
}

void PluginProcessor::loadCurveFromValueTree()
{
    std::vector<CurvePoint> loadedCurve;
    auto curveData = apvts.state.getChildWithName("curveData");
    auto curveNode = curveData.getChildWithName("main");

    if (curveNode.isValid())
    {
        for (int i = 0; i < curveNode.getNumChildren(); ++i)
        {
            auto p = curveNode.getChild(i);
            if (p.getType() == juce::Identifier("p"))
            {
                const float x = (float) p.getProperty("x");
                const float y = (float) p.getProperty("y");
                const float c = p.hasProperty("c") ? (float) p.getProperty("c") : 0.0f;
                loadedCurve.push_back({x, y, c});
            }
        }
    }

    CurveInterp::sortAndClamp(loadedCurve);
    if (loadedCurve.size() < 2)
        loadedCurve = makeDefaultPumpCurve();

    const juce::ScopedLock sl(curveLock);
    bandCurve = std::move(loadedCurve);
}

void PluginProcessor::setBandCurve(int band, std::vector<CurvePoint> points)
{
    if (band != 0)
        return;

    CurveInterp::sortAndClamp(points);
    if (points.size() < 2)
        return;

    {
        const juce::ScopedLock sl(curveLock);
        bandCurve = std::move(points);
    }
    syncCurveToValueTree();
}

void PluginProcessor::getBandCurve(int band, std::vector<CurvePoint>& out) const
{
    if (band != 0)
        return;

    const juce::ScopedLock sl(curveLock);
    out = bandCurve;
}

float PluginProcessor::getBandSidechainLevel(int band) const noexcept
{
    juce::ignoreUnused(band);
    return sidechainMeter.load();
}

float PluginProcessor::getBandGainReduction(int band) const noexcept
{
    juce::ignoreUnused(band);
    return gainReductionMeter.load();
}

float PluginProcessor::getTriggerActivity() const noexcept
{
    return triggerActivity.load();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    fallbackSyncPhase = 0.0;
    midiTriggerPhase = 1.0;
    audioTriggerPhase = 1.0;
    midiTriggerActive = false;
    audioTriggerActive = false;
    audioTriggerArmed = true;
    audioTriggerDetector = 0.0f;
    sidechainMeter.store(0.0f);
    gainReductionMeter.store(0.0f);
    triggerActivity.store(0.0f);
    smoothedVolumeGain = 1.0f;
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

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const float sidechainGainDb = apvts.getRawParameterValue("sidechainGainDb")->load();
    const float mix = apvts.getRawParameterValue("mix")->load();
    const int triggerMode = getChoiceIndex(apvts, "triggerMode");
    const int triggerRateIndex = getChoiceIndex(apvts, "triggerRate");

    const float sidechainGain = juce::Decibels::decibelsToGain(sidechainGainDb);
    const float rateQuarterNotes = getRateInQuarterNotes(triggerRateIndex);
    const float gainSmoothingAlpha = 1.0f - std::exp(-1.0f / (0.00075f * (float) juce::jmax(1.0, currentSampleRate)));

    std::vector<CurvePoint> localCurve;
    {
        const juce::ScopedLock sl(curveLock);
        localCurve = bandCurve;
    }

    auto mainIn = getBusBuffer(buffer, true, 0);
    auto scIn = getBusBuffer(buffer, true, 1);
    auto out = getBusBuffer(buffer, false, 0);

    const int numCh = juce::jmin(2, mainIn.getNumChannels(), out.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    std::vector<uint8_t> noteOnAtSample((size_t) numSamples, 0);
    if (triggerMode == triggerMidi)
    {
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                const int samplePosition = juce::jlimit(0, juce::jmax(0, numSamples - 1), metadata.samplePosition);
                noteOnAtSample[(size_t) samplePosition] = 1;
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
    const double triggerPhaseIncrement = quarterNotesPerSample / juce::jmax(0.125, (double) rateQuarterNotes);

    float phaseAccum = 0.0f;
    float duckAccum = 0.0f;
    bool triggerEventInBlock = false;

    for (int n = 0; n < numSamples; ++n)
    {
        float triggerPhase = 1.0f;

        if (triggerMode == triggerBpmSync)
        {
            if (hasHostPpq)
            {
                const double quarterNotePosition = hostPpq + (double) n * quarterNotesPerSample;
                triggerPhase = (float) std::fmod(quarterNotePosition / juce::jmax(0.125, (double) rateQuarterNotes), 1.0);
                if (triggerPhase < 0.0f)
                    triggerPhase += 1.0f;
            }
            else
            {
                triggerPhase = (float) fallbackSyncPhase;
                fallbackSyncPhase += triggerPhaseIncrement;
                if (fallbackSyncPhase >= 1.0)
                    fallbackSyncPhase -= std::floor(fallbackSyncPhase);
            }

            if (triggerPhase <= (float) (triggerPhaseIncrement * 2.0))
                triggerEventInBlock = true;
        }
        else if (triggerMode == triggerMidi)
        {
            if (noteOnAtSample[(size_t) n] != 0)
            {
                midiTriggerPhase = 0.0;
                midiTriggerActive = true;
                triggerEventInBlock = true;
            }

            triggerPhase = midiTriggerActive ? (float) midiTriggerPhase : 1.0f;
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
        else
        {
            constexpr float triggerThreshold = 0.06f;
            constexpr float rearmThreshold = 0.015f;
            constexpr float detectorAttack = 0.35f;
            constexpr float detectorRelease = 0.03f;

            float sidechainInput = 0.0f;
            for (int ch = 0; ch < juce::jmin(2, scIn.getNumChannels()); ++ch)
                sidechainInput = juce::jmax(sidechainInput, std::abs(scIn.getSample(ch, n) * sidechainGain));

            const float coeff = sidechainInput > audioTriggerDetector ? detectorAttack : detectorRelease;
            audioTriggerDetector += (sidechainInput - audioTriggerDetector) * coeff;

            if (audioTriggerArmed && audioTriggerDetector >= triggerThreshold)
            {
                audioTriggerPhase = 0.0;
                audioTriggerActive = true;
                audioTriggerArmed = false;
                triggerEventInBlock = true;
            }
            else if (audioTriggerDetector <= rearmThreshold)
            {
                audioTriggerArmed = true;
            }

            triggerPhase = audioTriggerActive ? (float) audioTriggerPhase : 1.0f;
            if (audioTriggerActive)
            {
                audioTriggerPhase += triggerPhaseIncrement;
                if (audioTriggerPhase >= 1.0)
                {
                    audioTriggerPhase = 1.0;
                    audioTriggerActive = false;
                }
            }
        }

        const float targetVolume = juce::jlimit(0.0f, 1.0f, CurveInterp::eval(triggerPhase, localCurve));
        smoothedVolumeGain += (targetVolume - smoothedVolumeGain) * gainSmoothingAlpha;
        const float volume = juce::jlimit(0.0f, 1.0f, smoothedVolumeGain);
        const float duckAmount = 1.0f - volume;
        phaseAccum += triggerPhase;
        duckAccum += duckAmount;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = mainIn.getSample(ch, n);
            const float wet = dry * volume;
            out.setSample(ch, n, dry * (1.0f - mix) + wet * mix);
        }
    }

    for (int ch = numCh; ch < out.getNumChannels(); ++ch)
        out.clear(ch, 0, numSamples);

    const float meterSamples = (float) juce::jmax(1, numSamples);
    constexpr float meterSmoothing = 0.25f;
    const float phaseTarget = juce::jlimit(0.0f, 1.0f, phaseAccum / meterSamples);
    const float gainReductionTarget = juce::jlimit(0.0f, 1.0f, duckAccum / meterSamples);

    sidechainMeter.store(sidechainMeter.load() + (phaseTarget - sidechainMeter.load()) * meterSmoothing);
    gainReductionMeter.store(gainReductionMeter.load() + (gainReductionTarget - gainReductionMeter.load()) * meterSmoothing);

    const float triggerTarget = triggerEventInBlock ? 1.0f : 0.0f;
    const float release = triggerEventInBlock ? 0.55f : 0.14f;
    triggerActivity.store(triggerActivity.load() + (triggerTarget - triggerActivity.load()) * release);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    syncCurveToValueTree();
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    loadCurveFromValueTree();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
