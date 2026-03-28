#include "PluginProcessor.h"
#include "FactoryPresetBank.h"
#include "FactoryPresetLoader.h"
#include "PluginEditor.h"
#include "ReactorUI.h"
#include "SynthVoice.h"
#include "WavetableLibrary.h"

namespace
{
template <typename Param>
std::unique_ptr<Param> makeParam(Param* p)
{
    return std::unique_ptr<Param>(p);
}

constexpr const char* themeModePropertyID = "uiThemeMode";
constexpr const char* themeModeDark = "dark";
constexpr const char* themeModeLight = "light";

bool shouldRandomizeParameterID(const juce::String& parameterID)
{
    if (parameterID.isEmpty())
        return false;

    return ! parameterID.startsWith("modSlot");
}

reactormod::DynamicLfoData makeRandomizedLfoData(int lfoIndex, juce::Random& random)
{
    auto data = reactormod::makeDefaultLfo(lfoIndex + 1);
    data.name = "LFO " + juce::String(lfoIndex + 1);
    data.syncEnabled = random.nextBool();
    data.dotted = data.syncEnabled && random.nextBool();
    data.syncDivisionIndex = random.nextInt(juce::jmax(1, reactormod::getSyncDivisionNames().size()));
    data.rateHz = juce::jmap(random.nextFloat(), 0.05f, 20.0f);
    data.triggerMode = static_cast<reactormod::TriggerMode>(random.nextInt(reactormod::getTriggerModeNames().size()));
    data.type = static_cast<reactormod::LfoType>(random.nextInt(reactormod::getLfoTypeNames().size()));

    if (data.type == reactormod::LfoType::shape)
    {
        const int nodeCount = 4 + random.nextInt(4);
        data.nodes.clear();
        data.nodes.reserve((size_t) nodeCount);

        for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
        {
            reactormod::LfoNode node;
            node.x = nodeCount > 1 ? (float) nodeIndex / (float) (nodeCount - 1) : 0.0f;
            node.y = juce::jmap(random.nextFloat(), 0.05f, 0.95f);
            node.curveToNext = nodeIndex + 1 < nodeCount ? juce::jmap(random.nextFloat(), -0.8f, 0.8f) : 0.0f;
            data.nodes.push_back(node);
        }
    }

    return data;
}

bool setParameterPlain(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainValue)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        ranged->setValueNotifyingHost(ranged->convertTo0to1(plainValue));
        return true;
    }

    return false;
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    setParameterPlain(apvts, id, value ? 1.0f : 0.0f);
}

float readParameterPlain(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback = 0.0f)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();

    return fallback;
}

float getModulationSourceValue(int sourceIndex,
                               const std::vector<float>& lfoValues,
                               const std::array<float, reactormod::macroSourceCount>& macroValues) noexcept
{
    if (reactormod::isLfoSourceIndex(sourceIndex))
    {
        const int lfoIndex = reactormod::lfoNumberForSourceIndex(sourceIndex) - 1;
        if (juce::isPositiveAndBelow(lfoIndex, (int) lfoValues.size()))
            return lfoValues[(size_t) lfoIndex];

        return 0.0f;
    }

    if (reactormod::isMacroSourceIndex(sourceIndex))
    {
        const int macroIndex = reactormod::macroNumberForSourceIndex(sourceIndex) - 1;
        if (juce::isPositiveAndBelow(macroIndex, (int) macroValues.size()))
            return juce::jlimit(0.0f, 1.0f, macroValues[(size_t) macroIndex]);
    }

    return 0.0f;
}

float midiNoteToFrequency(int midiNote)
{
    return (float) juce::MidiMessage::getMidiNoteInHertz(juce::jlimit(0, 127, midiNote));
}

float computeWindowRms(const std::vector<float>& signal, int start, int length)
{
    const int safeLength = juce::jmax(0, juce::jmin(length, (int) signal.size() - start));
    if (safeLength <= 0)
        return 0.0f;

    double sumSquares = 0.0;
    for (int i = 0; i < safeLength; ++i)
    {
        const double sample = signal[(size_t) (start + i)];
        sumSquares += sample * sample;
    }

    return (float) std::sqrt(sumSquares / (double) safeLength);
}

bool detectRootMidiNote(const juce::AudioBuffer<float>& buffer, double sourceSampleRate, int& detectedMidiNote)
{
    constexpr int minDetectMidi = 12;   // C0
    constexpr int maxDetectMidi = 108;  // C8

    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples < 2048 || sourceSampleRate < 1000.0)
        return false;

    const int analysisSamples = juce::jmin(numSamples, (int) std::round(sourceSampleRate * 6.0));
    std::vector<float> mono;
    mono.resize((size_t) analysisSamples, 0.0f);

    double mean = 0.0;
    for (int sample = 0; sample < analysisSamples; ++sample)
    {
        float mixed = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mixed += buffer.getSample(channel, sample);

        mixed /= (float) numChannels;
        mono[(size_t) sample] = mixed;
        mean += mixed;
    }

    mean /= (double) analysisSamples;

    float peak = 0.0f;
    for (auto& sample : mono)
    {
        sample -= (float) mean;
        peak = juce::jmax(peak, std::abs(sample));
    }

    if (peak < 0.01f)
        return false;

    const float globalRms = computeWindowRms(mono, 0, analysisSamples);
    if (globalRms < 0.003f)
        return false;

    const int minLag = juce::jmax(2, (int) std::floor(sourceSampleRate / midiNoteToFrequency(maxDetectMidi)));
    const int maxLag = juce::jmin((int) std::floor(sourceSampleRate / midiNoteToFrequency(minDetectMidi)),
                                  analysisSamples / 3);
    if (maxLag <= minLag + 2)
        return false;

    const int windowSize = juce::jlimit(4096, 16384, juce::jmax(maxLag * 4, (int) std::round(sourceSampleRate * 0.16)));
    if (analysisSamples <= windowSize + maxLag)
        return false;

    const int hopSize = juce::jmax(512, windowSize / 4);
    const int firstStart = juce::jmin(analysisSamples - windowSize, (int) std::round(sourceSampleRate * 0.03));

    float bestCorrelation = 0.0f;
    float bestScore = 0.0f;
    int bestLag = 0;

    for (int start = firstStart; start + windowSize + maxLag < analysisSamples; start += hopSize)
    {
        const float rms = computeWindowRms(mono, start, windowSize);
        if (rms < globalRms * 0.28f)
            continue;

        float localBestCorrelation = 0.0f;
        int localBestLag = 0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double numerator = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;

            for (int i = 0; i < windowSize; ++i)
            {
                const double a = mono[(size_t) (start + i)];
                const double b = mono[(size_t) (start + i + lag)];
                numerator += a * b;
                energyA += a * a;
                energyB += b * b;
            }

            const double denominator = std::sqrt(energyA * energyB) + 1.0e-12;
            const float correlation = (float) (numerator / denominator);
            if (correlation > localBestCorrelation)
            {
                localBestCorrelation = correlation;
                localBestLag = lag;
            }
        }

        const float score = localBestCorrelation * (0.35f + rms * 8.0f);
        if (score > bestScore)
        {
            bestScore = score;
            bestCorrelation = localBestCorrelation;
            bestLag = localBestLag;
        }
    }

    if (bestLag <= 0 || bestCorrelation < 0.50f)
        return false;

    const double frequency = sourceSampleRate / (double) bestLag;
    if (frequency < midiNoteToFrequency(minDetectMidi) * 0.9
        || frequency > midiNoteToFrequency(maxDetectMidi) * 1.1)
        return false;

    const int midiNote = juce::jlimit(minDetectMidi, 127,
                                      juce::roundToInt(69.0 + 12.0 * std::log2(frequency / 440.0)));
    detectedMidiNote = midiNote;
    return true;
}

enum DualWarpModeIndex
{
    warpModeOff = 0,
    warpModeFM,
    warpModeSync,
    warpModeBendPlus,
    warpModeBendMinus,
    warpModePhasePlus,
    warpModePhaseMinus,
    warpModeMirror,
    warpModeWrap,
    warpModePinch
};

juce::String getWarpModeParamID(int oscIndex, int slotIndex)
{
    return "osc" + juce::String(oscIndex) + "Warp" + juce::String(slotIndex) + "Mode";
}

juce::String getWarpAmountParamID(int oscIndex, int slotIndex)
{
    return "osc" + juce::String(oscIndex) + "Warp" + juce::String(slotIndex) + "Amount";
}

struct LegacyWarpCandidate
{
    int mode = warpModeOff;
    float amount = 0.0f;
    int priority = 0;
};

void applyDualWarpFromLegacy(juce::AudioProcessorValueTreeState& apvts, int oscIndex, bool forceFromLegacy)
{
    const int slot1Mode = juce::roundToInt(readParameterPlain(apvts, getWarpModeParamID(oscIndex, 1), 0.0f));
    const float slot1Amount = readParameterPlain(apvts, getWarpAmountParamID(oscIndex, 1), 0.0f);
    const int slot2Mode = juce::roundToInt(readParameterPlain(apvts, getWarpModeParamID(oscIndex, 2), 0.0f));
    const float slot2Amount = readParameterPlain(apvts, getWarpAmountParamID(oscIndex, 2), 0.0f);
    const bool hasModernWarp = slot1Mode != warpModeOff || slot2Mode != warpModeOff
                               || slot1Amount > 0.0001f || slot2Amount > 0.0001f;

    if (!forceFromLegacy && hasModernWarp)
        return;

    std::vector<LegacyWarpCandidate> candidates;
    const auto prefix = "osc" + juce::String(oscIndex);
    const float fm = readParameterPlain(apvts, prefix + "WarpFM", 0.0f);
    const float sync = readParameterPlain(apvts, prefix + "WarpSync", 0.0f);
    const float bend = readParameterPlain(apvts, prefix + "WarpBend", 0.0f);

    if (fm > 0.0001f)
        candidates.push_back({ warpModeFM, fm, 0 });
    if (sync > 0.0001f)
        candidates.push_back({ warpModeSync, sync, 1 });
    if (bend > 0.0001f)
        candidates.push_back({ warpModeBendPlus, bend, 2 });

    std::stable_sort(candidates.begin(), candidates.end(), [] (const auto& a, const auto& b)
    {
        if (a.amount != b.amount)
            return a.amount > b.amount;

        return a.priority < b.priority;
    });

    const auto applySlot = [&apvts, oscIndex] (int slotIndex, int mode, float amount)
    {
        setParameterPlain(apvts, getWarpModeParamID(oscIndex, slotIndex), (float) mode);
        setParameterPlain(apvts, getWarpAmountParamID(oscIndex, slotIndex), amount);
    };

    applySlot(1,
              candidates.size() > 0 ? candidates[0].mode : warpModeOff,
              candidates.size() > 0 ? candidates[0].amount : 0.0f);
    applySlot(2,
              candidates.size() > 1 ? candidates[1].mode : warpModeOff,
              candidates.size() > 1 ? candidates[1].amount : 0.0f);
}

juce::String getFXOrderParamID(int slotIndex)
{
    return "fxOrder" + juce::String(slotIndex + 1);
}

juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotParallelParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Parallel";
}

juce::String getFXSlotFloatParamID(int slotIndex, int parameterIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
}

juce::String getFXSlotVariantParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Variant";
}

enum FXModuleType
{
    fxOff = 0,
    fxDrive,
    fxFilter,
    fxChorus,
    fxFlanger,
    fxPhaser,
    fxDelay,
    fxReverb,
    fxCompressor,
    fxDimension,
    fxEQ,
    fxMultiband,
    fxBitcrusher,
    fxImager,
    fxShift,
    fxTremolo,
    fxGate
};

float getFXSlotDefaultValue(int parameterIndex)
{
    switch (parameterIndex)
    {
        case 2: return 0.3f;
        case 3: return 0.0f;
        case 4: return 0.3f;
        case 5: return 0.3f;
        case 6: return 0.0f;
        case 7: return 0.0f;
        case 8: return 0.22f;
        case 9: return 0.22f;
        case 10: return 0.22f;
        case 11: return 0.22f;
        default: return 0.5f;
    }
}

constexpr int fxEQBandCount = 4;
constexpr int fxEQTypeCount = 5;
constexpr int fxMultibandBandCount = 3;
constexpr int fxImagerBandCount = 4;
constexpr int fxEQTargetCount = 3;
constexpr int fxEQVariantLimit = 1 << 20;

struct FXBitcrusherMode
{
    const char* name;
    const char* description;
    float bitBias;
    float rateBias;
    float jitterBias;
    float toneBias;
    float outputTrimDb;
};

const std::array<FXBitcrusherMode, 5>& getFXBitcrusherModes()
{
    static const std::array<FXBitcrusherMode, 5> modes{{
        { "Smooth", "Cleaner digital reduction with softer aliasing and more mix-friendly texture.", 0.12f, 0.10f, 0.04f, 0.18f, -0.5f },
        { "Crunch", "The classic crunchy bitcrusher lane with obvious grit but still enough body to keep musical.", 0.28f, 0.24f, 0.08f, -0.02f, -1.0f },
        { "Retro", "Low-fi game-console style reduction with bigger sample stepping and lighter top end.", 0.40f, 0.42f, 0.14f, -0.14f, -1.5f },
        { "Digital", "Sharper modern reduction with bright edges and more aggressive transient chewing.", 0.18f, 0.54f, 0.10f, 0.12f, -1.2f },
        { "Destroy", "Full-on smashed reduction for extreme aliasing, zippery rate crush, and broken textures.", 0.60f, 0.72f, 0.22f, -0.24f, -2.0f }
    }};

    return modes;
}

int getFXVariantLimit(int moduleType)
{
    switch (moduleType)
    {
        case fxDrive:  return PluginProcessor::getFXDriveTypeNames().size();
        case fxFilter: return PluginProcessor::getFXFilterTypeNames().size();
        case fxChorus: return 2;
        case fxFlanger:return 2;
        case fxPhaser: return 2;
        case fxDelay:  return PluginProcessor::getFXDelayTypeNames().size();
        case fxReverb: return PluginProcessor::getFXReverbTypeNames().size();
        case fxCompressor: return PluginProcessor::getFXCompressorTypeNames().size();
        case fxEQ:     return fxEQVariantLimit;
        case fxMultiband: return 1;
        case fxBitcrusher: return PluginProcessor::getFXBitcrusherTypeNames().size();
        case fxImager: return 1;
        case fxShift: return PluginProcessor::getFXShiftTypeNames().size();
        case fxTremolo: return 2;
        case fxGate: return fxEQVariantLimit;
        default:       return 1;
    }
}

float normalToFXChorusRateHz(float value)
{
    constexpr float minRate = 0.05f;
    constexpr float maxRate = 8.0f;
    return minRate * std::pow(maxRate / minRate, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXChorusDelayMs(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 3.0f, 28.0f);
}

float normalToFXChorusFeedback(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -0.7f, 0.7f);
}

float normalToFXPhaserRateHz(float value)
{
    constexpr float minRate = 0.05f;
    constexpr float maxRate = 8.0f;
    return minRate * std::pow(maxRate / minRate, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXFlangerDelayMs(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.2f, 8.0f);
}

float normalToFXFlangerFeedback(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -0.85f, 0.85f);
}

float normalToFXPhaserCentreHz(float value)
{
    constexpr float minFrequency = 120.0f;
    constexpr float maxFrequency = 4200.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXDelayFreeMs(float value)
{
    constexpr float minDelayMs = 1.0f;
    constexpr float maxDelayMs = 1800.0f;
    return minDelayMs * std::pow(maxDelayMs / minDelayMs, juce::jlimit(0.0f, 1.0f, value));
}

struct DelaySyncDivision
{
    float beats = 1.0f;
};

const std::array<DelaySyncDivision, 12>& getFXDelaySyncDivisions()
{
    static const std::array<DelaySyncDivision, 12> divisions{{
        { 0.125f },
        { 1.0f / 6.0f },
        { 0.25f },
        { 1.0f / 3.0f },
        { 0.5f },
        { 2.0f / 3.0f },
        { 1.0f },
        { 4.0f / 3.0f },
        { 2.0f },
        { 4.0f },
        { 8.0f },
        { 16.0f }
    }};
    return divisions;
}

float normalToFXDelaySyncedMs(float value, double bpm)
{
    const auto& divisions = getFXDelaySyncDivisions();
    const int maxIndex = (int) divisions.size() - 1;
    const int index = juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
    const float beatDurationMs = 60000.0f / (float) juce::jmax(1.0, bpm);
    return juce::jlimit(1.0f, 2400.0f, beatDurationMs * divisions[(size_t) index].beats);
}

float normalToFXDelayHighPassFrequency(float value)
{
    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 2400.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXDelayLowPassFrequency(float value)
{
    constexpr float minFrequency = 700.0f;
    constexpr float maxFrequency = 20000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

struct ChorusSyncDivision
{
    float beats = 1.0f;
};

const std::array<ChorusSyncDivision, 13>& getFXChorusSyncDivisions()
{
    static const std::array<ChorusSyncDivision, 13> divisions{{
        { 0.125f },
        { 1.0f / 6.0f },
        { 0.25f },
        { 1.0f / 3.0f },
        { 0.5f },
        { 2.0f / 3.0f },
        { 1.0f },
        { 4.0f / 3.0f },
        { 2.0f },
        { 4.0f },
        { 8.0f },
        { 12.0f },
        { 16.0f }
    }};
    return divisions;
}

float normalToFXChorusSyncedRateHz(float value, double bpm)
{
    const auto& divisions = getFXChorusSyncDivisions();
    const int maxIndex = (int) divisions.size() - 1;
    const int index = juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
    const float beatsPerCycle = divisions[(size_t) index].beats;
    const float beatsPerSecond = (float) juce::jmax(1.0, bpm) / 60.0f;
    return juce::jlimit(0.01f, 20.0f, beatsPerSecond / juce::jmax(0.03125f, beatsPerCycle));
}

float normalToFXFilterFrequency(float value)
{
    constexpr float minFrequency = 24.0f;
    constexpr float maxFrequency = 18000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXFilterQ(float value)
{
    return 0.55f + juce::jlimit(0.0f, 1.0f, value) * 13.5f;
}

float normalToFXFilterDriveGain(float value)
{
    return 1.0f + juce::jlimit(0.0f, 1.0f, value) * 8.0f;
}

float normalToFXFilterGainDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -18.0f, 18.0f);
}

float normalToFXCompressorInputDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.0f, 24.0f);
}

float normalToFXCompressorAttackMs(float value)
{
    return 0.02f * std::pow(300.0f, 1.0f - juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXCompressorReleaseMs(float value)
{
    return 40.0f * std::pow(27.5f, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXCompressorOutputDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -6.0f, 18.0f);
}

float normalToFXMultibandTimeMs(float value)
{
    return 5.0f * std::pow(36.0f, juce::jlimit(0.0f, 1.0f, value));
}

int normalToFXBitcrusherBitDepth(float value)
{
    return juce::jlimit(2, 16, juce::roundToInt(16.0f - juce::jlimit(0.0f, 1.0f, value) * 13.0f));
}

int normalToFXBitcrusherHoldSamples(float value)
{
    return juce::jlimit(1, 256, juce::roundToInt(1.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.55f) * 220.0f));
}

float normalToFXBitcrusherToneHz(float value)
{
    return juce::jlimit(1200.0f, 20000.0f, 2200.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.2f) * 16800.0f);
}

float normalToFXImagerFrequency(float value)
{
    constexpr float minFrequency = 40.0f;
    constexpr float maxFrequency = 16000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXImagerWidth(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), 0.0f, 2.0f);
}

float normalToFXShiftDetuneCents(float value)
{
    return 2.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.2f) * 36.0f;
}

float normalToFXShiftDelayMs(float value)
{
    return 2.5f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.15f) * 28.0f;
}

float normalToFXShiftPreDelayMs(float value)
{
    return std::pow(juce::jlimit(0.0f, 1.0f, value), 1.18f) * 120.0f;
}

float normalToFXShiftRateHz(float value)
{
    constexpr float minRate = 5.0f;
    constexpr float maxRate = 2200.0f;
    return minRate * std::pow(maxRate / minRate, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXTremoloStereoPhase(float value)
{
    return juce::jlimit(0.0f, 1.0f, value) * juce::MathConstants<float>::pi;
}

float normalToFXGateThresholdDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -60.0f, -6.0f);
}

float normalToFXGateAttackMs(float value)
{
    return 0.5f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.2f) * 70.0f;
}

float normalToFXGateHoldMs(float value)
{
    return std::pow(juce::jlimit(0.0f, 1.0f, value), 1.08f) * 180.0f;
}

float normalToFXGateReleaseMs(float value)
{
    return 4.0f + std::pow(juce::jlimit(0.0f, 1.0f, value), 1.15f) * 420.0f;
}

bool isFXGateRhythmic(int variant)
{
    return (variant & 0x1) != 0;
}

int withFXGateRhythmic(int variant, bool rhythmic)
{
    if (rhythmic)
        return variant | 0x1;

    return variant & ~0x1;
}

bool getFXGateStepEnabled(int variant, int stepIndex)
{
    return ((variant >> (stepIndex + 1)) & 0x1) != 0;
}

int withFXGateStepEnabled(int variant, int stepIndex, bool enabled)
{
    const int mask = 0x1 << (stepIndex + 1);
    if (enabled)
        return variant | mask;

    return variant & ~mask;
}

int getDefaultFXGateVariant()
{
    int variant = 0;
    for (int step = 0; step < 16; ++step)
        variant = withFXGateStepEnabled(variant, step, step % 2 == 0);
    return variant;
}

struct FXGateRateDivision
{
    float beats = 0.25f;
};

const std::array<FXGateRateDivision, 10>& getGateRateDivisions()
{
    static const std::array<FXGateRateDivision, 10> divisions{{
        { 0.125f },
        { 1.0f / 6.0f },
        { 0.25f },
        { 1.0f / 3.0f },
        { 0.5f },
        { 2.0f / 3.0f },
        { 1.0f },
        { 2.0f },
        { 4.0f },
        { 8.0f }
    }};
    return divisions;
}

int normalToGateRateIndex(float value)
{
    const auto maxIndex = (int) getGateRateDivisions().size() - 1;
    return juce::jlimit(0, maxIndex, juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * (float) maxIndex));
}

float gateRateIndexToNormal(int index)
{
    const auto maxIndex = (int) getGateRateDivisions().size() - 1;
    if (maxIndex <= 0)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, (float) juce::jlimit(0, maxIndex, index) / (float) maxIndex);
}

float compressorRatioFromType(int type)
{
    switch (type)
    {
        case 1: return 8.0f;
        case 2: return 12.0f;
        case 3: return 20.0f;
        case 4: return 50.0f;
        default: return 4.0f;
    }
}

float normalToFXEQFrequency(float value)
{
    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 20000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float normalToFXEQGainDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -18.0f, 18.0f);
}

float normalToFXEQQ(float value)
{
    return 0.35f + juce::jlimit(0.0f, 1.0f, value) * 11.65f;
}

bool isFXEQBandActive(int variant, int bandIndex)
{
    return juce::isPositiveAndBelow(bandIndex, fxEQBandCount)
        && (((variant >> bandIndex) & 0x1) != 0);
}

int getFXEQBandType(int variant, int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, fxEQBandCount))
        return 0;

    return (variant >> (4 + bandIndex * 2)) & 0x3;
}

int getFXEQBandTarget(int variant, int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, fxEQBandCount))
        return 0;

    return (variant >> (12 + bandIndex * 2)) & 0x3;
}

juce::IIRCoefficients makeFXEQCoefficients(int type, double sampleRate, float frequency, float q, float gainDb)
{
    const float safeFrequency = juce::jlimit(20.0f, (float) sampleRate * 0.45f, frequency);
    const float safeQ = juce::jlimit(0.35f, 12.0f, q);
    const float gain = juce::Decibels::decibelsToGain(gainDb);

    switch (type)
    {
        case 1: return juce::IIRCoefficients::makeLowShelf(sampleRate, safeFrequency, juce::jmax(0.4f, safeQ * 0.45f), gain);
        case 2: return juce::IIRCoefficients::makeHighShelf(sampleRate, safeFrequency, juce::jmax(0.4f, safeQ * 0.45f), gain);
        case 3: return juce::IIRCoefficients::makeHighPass(sampleRate, safeFrequency, safeQ);
        case 4: return juce::IIRCoefficients::makeLowPass(sampleRate, safeFrequency, safeQ);
        default:return juce::IIRCoefficients::makePeakFilter(sampleRate, safeFrequency, safeQ, gain);
    }
}

std::pair<float, float> getVowelFormants(int filterType, float tone, float shift)
{
    const float toneShift = juce::jmap(tone, -140.0f, 240.0f);
    const float shiftScale = 0.75f + shift * 0.75f;

    switch (filterType)
    {
        case 14: return { (700.0f + toneShift) * shiftScale, (1220.0f + toneShift * 1.6f) * shiftScale };
        case 15: return { (500.0f + toneShift * 0.8f) * shiftScale, (1900.0f + toneShift * 1.2f) * shiftScale };
        case 16: return { (340.0f + toneShift * 0.5f) * shiftScale, (2300.0f + toneShift) * shiftScale };
        case 17: return { (430.0f + toneShift * 0.7f) * shiftScale, (900.0f + toneShift * 0.9f) * shiftScale };
        case 18: return { (300.0f + toneShift * 0.5f) * shiftScale, (820.0f + toneShift * 0.7f) * shiftScale };
        default: return { (520.0f + toneShift) * shiftScale, (1380.0f + toneShift * 1.3f) * shiftScale };
    }
}

constexpr int baseFactoryPresetCount = 40;
constexpr int factoryPresetVariantCount = 5;

const std::array<juce::String, factoryPresetVariantCount>& getFactoryVariantNames()
{
    static const std::array<juce::String, factoryPresetVariantCount> names
    {
        "Core",
        "Club",
        "Wide",
        "Dark",
        "Hyper"
    };
    return names;
}

const std::array<juce::String, 10>& getVoltageNames()
{
    static const std::array<juce::String, 10> names
    {
        "Neon Vector", "Afterburn Halo", "Pulse Atlas", "Quartz Runner", "Nova District",
        "Ion Anthem", "Signal Forge", "Voltage Crest", "Chrome Horizon", "Laser Union"
    };
    return names;
}

const std::array<juce::String, 10>& getBasslineNames()
{
    static const std::array<juce::String, 10> names
    {
        "Sub District", "Velvet Current", "Coil Pressure", "Midnight Piston", "Tube Sector",
        "Low Voltage", "Graviton Floor", "Iron Current", "Night Engine", "Static Hollow"
    };
    return names;
}

const std::array<juce::String, 10>& getAtmosNames()
{
    static const std::array<juce::String, 10> names
    {
        "Glass Aurora", "Dream Circuit", "Choir Mirage", "Night Canopy", "Vowel Mist",
        "Phantom Sky", "Arc Vapor", "Blue Vacuum", "Phase Harbor", "Violet Rain"
    };
    return names;
}

const std::array<juce::String, 10>& getMotionNames()
{
    static const std::array<juce::String, 10> names
    {
        "Bell Reactor", "Hollow Transit", "Formant Pulse", "FM Lattice", "Sync Array",
        "Noise Helix", "Solar Arc", "Dust Prism", "Metal Filament", "Circuit Surge"
    };
    return names;
}

const std::array<juce::String, factoryPresetVariantCount>& getStyleWords()
{
    static const std::array<juce::String, factoryPresetVariantCount> names
    {
        "Prime",
        "Rush",
        "Bloom",
        "Shade",
        "Ignite"
    };
    return names;
}

juce::String getVariantSuffix(int variantIndex)
{
    if (variantIndex <= 0)
        return {};

    return " " + getFactoryVariantNames()[(size_t) juce::jlimit(0, factoryPresetVariantCount - 1, variantIndex)];
}

juce::String getVariantDescription(int variantIndex)
{
    switch (variantIndex)
    {
        case 1: return "Voiced for club pressure, sharper transients, and stronger impact.";
        case 2: return "Opened into a wider, more melodic frame with expanded image and air.";
        case 3: return "Tilted darker with moodier harmonics and a more cinematic balance.";
        case 4: return "Driven harder with extra motion, aggression, and Reactor energy.";
        default: return "Prime edition built as the main voiced version of the patch.";
    }
}

juce::String makeUniqueFactoryName(const juce::String& pack, int withinPackIndex)
{
    const auto variantWord = getStyleWords()[(size_t) (withinPackIndex / 10)];
    const auto stemIndex = (size_t) (withinPackIndex % 10);

    if (pack == "Festival Voltage")
        return getVoltageNames()[stemIndex] + " " + variantWord;
    if (pack == "Low Pressure District")
        return getBasslineNames()[stemIndex] + " " + variantWord;
    if (pack == "Afterglow Cinema")
        return getAtmosNames()[stemIndex] + " " + variantWord;

    return getMotionNames()[stemIndex] + " " + variantWord;
}

juce::Array<PluginProcessor::FactoryPresetInfo> buildBaseFactoryPresetLibrary()
{
    juce::Array<PluginProcessor::FactoryPresetInfo> items;
    items.add({ 0,  "Init Saw",         "Festival Voltage",    "Init",     "Core",        "Clean starting saw patch for building festival leads, plucks, and chords from scratch." });
    items.add({ 1,  "Hyper Stack",      "Festival Voltage",    "Lead",     "Anthem",      "Wide supersaw stack built for hands-in-the-air hooks and mainstage melodies." });
    items.add({ 3,  "Acid Runner",      "Festival Voltage",    "Bass",     "Acid",        "Resonant rolling acid bassline with tight movement and aggressive edge." });
    items.add({ 4,  "Neon Pluck",       "Festival Voltage",    "Pluck",    "Festival",    "Bright transient pluck for chord shots, riffs, and EDM toplines." });
    items.add({ 8,  "Mono Driver",      "Festival Voltage",    "Bass",     "Mono",        "Forward mono drive patch for punchy drops and modern electro bass work." });
    items.add({ 16, "Brass Charge",     "Festival Voltage",    "Lead",     "Brass",       "Hybrid brass-leaning lead with impact for stabs and heroic hooks." });
    items.add({ 27, "Quartz Lead",      "Festival Voltage",    "Lead",     "Supersaw",    "Sharper glassy lead tuned for trance and big-room melody work." });
    items.add({ 29, "Circuit Pluck",    "Festival Voltage",    "Pluck",    "Digital",     "Snappy digital pluck with precise attack for rhythmic hooks and arps." });
    items.add({ 32, "Glass Mono",       "Festival Voltage",    "Lead",     "Mono",        "Focused mono lead with polished top end for tight topline phrasing." });
    items.add({ 35, "Resonator Brass",  "Festival Voltage",    "Lead",     "Hybrid",      "Resonant brass hybrid that cuts through dense dance arrangements." });

    items.add({ 2,  "Reactor Bass",     "Low Pressure District","Bass",    "Analog",      "Solid analog-style bass with weight and controlled aggression." });
    items.add({ 9,  "Velvet Keys",      "Low Pressure District","Keys",    "Soft",        "Warm velvet key patch for house chords and mellow progression work." });
    items.add({ 12, "Comb String",      "Low Pressure District","String",  "Resonant",    "Tight comb-string tone suited to plucked phrases and synthetic string riffs." });
    items.add({ 17, "Organ Drift",      "Low Pressure District","Keys",    "Organ",       "Driven organ-style patch with motion for groove-based chord layers." });
    items.add({ 21, "Tube Organ",       "Low Pressure District","Keys",    "Tube",        "Tube-warmed organ tone for fuller mids and classic dance harmonies." });
    items.add({ 22, "Plasma Reed",      "Low Pressure District","Keys",    "Reed",        "Hybrid reed voice that sits between organ, brass, and synth lead." });
    items.add({ 26, "Sub Reactor",      "Low Pressure District","Bass",    "Sub",         "Low-end focused sub patch for stacking under modern EDM basses." });
    items.add({ 30, "Low Orbit",        "Low Pressure District","Bass",    "Dark",        "Dark orbiting low-end patch for moody drops and underground grooves." });
    items.add({ 34, "Tilt Keys",        "Low Pressure District","Keys",    "House",       "Punchy tilted key sound voiced for house chords and tight stabs." });
    items.add({ 39, "Cosmic Reed",      "Low Pressure District","Keys",    "Ethereal",    "Glossy reed-based key patch that stays musical in bright arrangements." });

    items.add({ 5,  "Glass Pad",        "Afterglow Cinema",    "Pad",      "Air",         "Airy glass pad designed for widescreen intros and emotional breakdowns." });
    items.add({ 6,  "Dream Bloom",      "Afterglow Cinema",    "Pad",      "Lush",        "Lush blooming pad with soft movement for melodic and future-focused builds." });
    items.add({ 18, "Flange Choir",     "Afterglow Cinema",    "Vocal",    "Choral",      "Modulated choir texture for cinematic layers and vocal-like washes." });
    items.add({ 19, "Night Drone",      "Afterglow Cinema",    "Drone",    "Dark",        "Slow dark drone built for tension, underscoring, and nocturnal atmospheres." });
    items.add({ 23, "Vowel Pad",        "Afterglow Cinema",    "Pad",      "Vocal",       "Formant-shaped pad that adds movement and human color to chord beds." });
    items.add({ 28, "Ghost Choir",      "Afterglow Cinema",    "Vocal",    "Ghost",       "Haunted choir-style layer for breakdowns, transitions, and dramatic spacing." });
    items.add({ 31, "Arc Wind",         "Afterglow Cinema",    "Texture",  "Air",         "Wind-blown texture patch for risers, intros, and ambient transitions." });
    items.add({ 33, "Blue Noise Pad",   "Afterglow Cinema",    "Pad",      "Noise",       "Noisy diffused pad for modern melodic intros and washed-out atmospheres." });
    items.add({ 36, "Phase Bloom",      "Afterglow Cinema",    "Pad",      "Phase",       "Phase-rich blooming pad that opens up wide in melodic sections." });
    items.add({ 38, "Violet Drone",     "Afterglow Cinema",    "Drone",    "Analog",      "Deep analog-style drone with color and patience for darker scenes." });

    items.add({ 7,  "Bell Matrix",      "Future Fracture",     "Bell",     "FM",          "Matrix-like FM bell for intricate riffs, arps, and sparkling top layers." });
    items.add({ 10, "Hollow Square",    "Future Fracture",     "Lead",     "Hollow",      "Hollow square hybrid for edgy hooks with an empty modern bite." });
    items.add({ 11, "Formant Vox",      "Future Fracture",     "Vocal",    "Formant",     "Expressive formant synth for vocal-ish leads and talking textures." });
    items.add({ 13, "FM Shiver",        "Future Fracture",     "Lead",     "FM",          "Cold metallic FM lead with controlled bite and futuristic motion." });
    items.add({ 14, "Sync Laser",       "Future Fracture",     "Lead",     "Sync",        "Sharp sync lead aimed at laser-like riffs and cutting toplines." });
    items.add({ 15, "Noise Sweep",      "Future Fracture",     "FX",       "Sweep",       "Noise-based riser texture for transitions and festival build ramps." });
    items.add({ 20, "Solar Sweep",      "Future Fracture",     "FX",       "Riser",       "Bright rising motion preset for tension lifts and euphoric pre-drops." });
    items.add({ 24, "Dust Bell",        "Future Fracture",     "Bell",     "Lo-Fi",       "Dusty bell patch for characterful melodic accents and softer toplines." });
    items.add({ 25, "Metal Harp",       "Future Fracture",     "Pluck",    "Metallic",    "Metal harp pluck for sharp rhythmic phrases and synthetic ornamentation." });
    items.add({ 37, "Metal Sync",       "Future Fracture",     "Lead",     "Industrial",  "Industrial sync lead with harder transient bite for aggressive EDM hooks." });
    return items;
}

void setOscillator(juce::AudioProcessorValueTreeState& apvts,
                   int osc,
                   int wave,
                   int octave,
                   float level,
                   float detune,
                   bool toFilter,
                   bool unisonOn,
                   int unisonVoices,
                   float unisonDetune,
                   float fm,
                   float sync,
                   float bend)
{
    const auto prefix = "osc" + juce::String(osc);
    setParameterPlain(apvts, prefix + "Type", 0.0f);
    setParameterPlain(apvts, prefix + "Wave", (float) wave);
    setParameterPlain(apvts, prefix + "Wavetable", 0.0f);
    setParameterPlain(apvts, prefix + "WTPos", 0.0f);
    setParameterPlain(apvts, prefix + "Octave", (float) octave);
    setParameterPlain(apvts, prefix + "Coarse", 0.0f);
    setParameterPlain(apvts, prefix + "SampleRoot", 60.0f);
    setParameterPlain(apvts, prefix + "Level", level);
    setParameterPlain(apvts, prefix + "Detune", detune);
    setParameterPlain(apvts, prefix + "Pan", 0.0f);
    setBoolParam(apvts, prefix + "ToFilter", toFilter);
    setBoolParam(apvts, prefix + "UnisonOn", unisonOn);
    setParameterPlain(apvts, prefix + "UnisonVoices", (float) unisonVoices);
    setParameterPlain(apvts, prefix + "Unison", unisonDetune);
    setParameterPlain(apvts, prefix + "WarpFM", fm);
    setParameterPlain(apvts, prefix + "WarpSync", sync);
    setParameterPlain(apvts, prefix + "WarpBend", bend);
}

void setDriveFX(juce::AudioProcessorValueTreeState& apvts, bool on, float amount, float tone, float mix)
{
    setBoolParam(apvts, "fxDriveOn", on);
    setParameterPlain(apvts, "fxDriveAmount", amount);
    setParameterPlain(apvts, "fxDriveTone", tone);
    setParameterPlain(apvts, "fxDriveMix", mix);
}

void setChorusFX(juce::AudioProcessorValueTreeState& apvts, bool on, float rate, float depth, float mix)
{
    setBoolParam(apvts, "fxChorusOn", on);
    setParameterPlain(apvts, "fxChorusRate", rate);
    setParameterPlain(apvts, "fxChorusDepth", depth);
    setParameterPlain(apvts, "fxChorusMix", mix);
}

void setDelayFX(juce::AudioProcessorValueTreeState& apvts, bool on, float timeSeconds, float feedback, float mix)
{
    setBoolParam(apvts, "fxDelayOn", on);
    setParameterPlain(apvts, "fxDelayTime", timeSeconds);
    setParameterPlain(apvts, "fxDelayFeedback", feedback);
    setParameterPlain(apvts, "fxDelayMix", mix);
}

void setReverbFX(juce::AudioProcessorValueTreeState& apvts, bool on, float size, float damp, float mix)
{
    setBoolParam(apvts, "fxReverbOn", on);
    setParameterPlain(apvts, "fxReverbSize", size);
    setParameterPlain(apvts, "fxReverbDamp", damp);
    setParameterPlain(apvts, "fxReverbMix", mix);
}

void setFilterFX(juce::AudioProcessorValueTreeState& apvts, bool on, float cutoff, float resonance, float mix)
{
    setBoolParam(apvts, "fxFilterOn", on);
    setParameterPlain(apvts, "fxFilterCutoff", cutoff);
    setParameterPlain(apvts, "fxFilterResonance", resonance);
    setParameterPlain(apvts, "fxFilterMix", mix);
}

void setPhaserFX(juce::AudioProcessorValueTreeState& apvts, bool on, float rate, float depth, float mix)
{
    setBoolParam(apvts, "fxPhaserOn", on);
    setParameterPlain(apvts, "fxPhaserRate", rate);
    setParameterPlain(apvts, "fxPhaserDepth", depth);
    setParameterPlain(apvts, "fxPhaserMix", mix);
}

void setFlangerFX(juce::AudioProcessorValueTreeState& apvts, bool on, float rate, float depth, float mix)
{
    setBoolParam(apvts, "fxFlangerOn", on);
    setParameterPlain(apvts, "fxFlangerRate", rate);
    setParameterPlain(apvts, "fxFlangerDepth", depth);
    setParameterPlain(apvts, "fxFlangerMix", mix);
}

void setCompressorFX(juce::AudioProcessorValueTreeState& apvts, bool on, float amount, float punch, float mix)
{
    setBoolParam(apvts, "fxCompressorOn", on);
    setParameterPlain(apvts, "fxCompressorAmount", amount);
    setParameterPlain(apvts, "fxCompressorPunch", punch);
    setParameterPlain(apvts, "fxCompressorMix", mix);
}

void setDimensionFX(juce::AudioProcessorValueTreeState& apvts, bool on, float size, float width, float mix)
{
    setBoolParam(apvts, "fxDimensionOn", on);
    setParameterPlain(apvts, "fxDimensionSize", size);
    setParameterPlain(apvts, "fxDimensionWidth", width);
    setParameterPlain(apvts, "fxDimensionMix", mix);
}

void setFXOrder(juce::AudioProcessorValueTreeState& apvts, std::initializer_list<int> order)
{
    int slot = 0;
    for (const auto moduleType : order)
    {
        if (slot >= PluginProcessor::maxFXSlots)
            break;

        setParameterPlain(apvts, getFXOrderParamID(slot), (float) moduleType);
        ++slot;
    }

    while (slot < PluginProcessor::maxFXSlots)
    {
        setParameterPlain(apvts, getFXOrderParamID(slot), 0.0f);
        ++slot;
    }
}

enum FilterModeIndex
{
    filterLow12 = 0,
    filterLow24,
    filterHigh12,
    filterHigh24,
    filterBand12,
    filterBand24,
    filterNotch,
    filterPeak,
    filterLowShelf,
    filterHighShelf,
    filterAllPass,
    filterCombPlus,
    filterCombMinus,
    filterFormant,
    filterFlangePlus,
    filterFlangeMinus,
    filterVowelA,
    filterVowelE,
    filterVowelI,
    filterVowelO,
    filterVowelU,
    filterTilt,
    filterResonator,
    filterLadderDrive,
    filterMetalComb,
    filterPhaseWarp
};

float scaleNormalised(float value, float minimum, float maximum)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), minimum, maximum);
}

int quantiseIndex(float value, int count)
{
    if (count <= 1)
        return 0;

    return juce::jlimit(0, count - 1, (int) std::floor(juce::jlimit(0.0f, 0.9999f, value) * (float) count));
}

template <size_t N>
int selectChoice(const std::array<int, N>& choices, float selector)
{
    static_assert(N > 0, "Choice list must not be empty");
    return choices[(size_t) quantiseIndex(selector, (int) N)];
}

void setOscillatorFMSource(juce::AudioProcessorValueTreeState& apvts, int osc, int sourceIndex)
{
    setParameterPlain(apvts, "osc" + juce::String(osc) + "WarpFMSource", (float) juce::jlimit(0, 6, sourceIndex));
}

void setFilterSection(juce::AudioProcessorValueTreeState& apvts,
                      bool enabled,
                      int mode,
                      float cutoff,
                      float resonance,
                      float drive,
                      float envAmount,
                      float attack,
                      float decay,
                      float sustain,
                      float release)
{
    setBoolParam(apvts, "filterOn", enabled);
    setParameterPlain(apvts, "filterMode", (float) juce::jlimit(0, (int) filterPhaseWarp, mode));
    setParameterPlain(apvts, "filterCutoff", juce::jlimit(20.0f, 20000.0f, cutoff));
    setParameterPlain(apvts, "filterResonance", juce::jlimit(0.2f, 18.0f, resonance));
    setParameterPlain(apvts, "filterDrive", juce::jlimit(0.0f, 1.0f, drive));
    setParameterPlain(apvts, "filterEnvAmount", juce::jlimit(0.0f, 1.0f, envAmount));
    setParameterPlain(apvts, "filterAttack", juce::jlimit(0.001f, 5.0f, attack));
    setParameterPlain(apvts, "filterDecay", juce::jlimit(0.001f, 5.0f, decay));
    setParameterPlain(apvts, "filterSustain", juce::jlimit(0.0f, 1.0f, sustain));
    setParameterPlain(apvts, "filterRelease", juce::jlimit(0.001f, 5.0f, release));
}

void setAmpSection(juce::AudioProcessorValueTreeState& apvts, float attack, float decay, float sustain, float release)
{
    setParameterPlain(apvts, "ampAttack", juce::jlimit(0.001f, 5.0f, attack));
    setParameterPlain(apvts, "ampDecay", juce::jlimit(0.001f, 5.0f, decay));
    setParameterPlain(apvts, "ampSustain", juce::jlimit(0.0f, 1.0f, sustain));
    setParameterPlain(apvts, "ampRelease", juce::jlimit(0.001f, 5.0f, release));
}

void setVoiceSection(juce::AudioProcessorValueTreeState& apvts,
                     bool mono,
                     bool legato,
                     bool portamento,
                     float glide,
                     float bendUp,
                     float bendDown)
{
    setParameterPlain(apvts, "voiceMode", mono ? 1.0f : 0.0f);
    setBoolParam(apvts, "voiceLegato", legato);
    setBoolParam(apvts, "voicePortamento", portamento);
    setParameterPlain(apvts, "voiceGlide", juce::jlimit(0.0f, 2.0f, glide));
    setParameterPlain(apvts, "pitchBendUp", juce::jlimit(0.0f, 24.0f, bendUp));
    setParameterPlain(apvts, "pitchBendDown", juce::jlimit(0.0f, 24.0f, bendDown));
}

void setModSection(juce::AudioProcessorValueTreeState& apvts,
                   float lfoRate,
                   float lfoDepth,
                   float modWheelCutoff,
                   float modWheelVibrato)
{
    setParameterPlain(apvts, "lfoRate", juce::jlimit(0.05f, 20.0f, lfoRate));
    setParameterPlain(apvts, "lfoDepth", juce::jlimit(0.0f, 1.0f, lfoDepth));
    setParameterPlain(apvts, "modWheelCutoff", juce::jlimit(0.0f, 1.0f, modWheelCutoff));
    setParameterPlain(apvts, "modWheelVibrato", juce::jlimit(0.0f, 1.0f, modWheelVibrato));
}

bool stateHasProperty(const juce::ValueTree& state, const juce::String& propertyName)
{
    return state.hasProperty(juce::Identifier(propertyName));
}

const juce::Identifier modulationStateType{ "MODULATION_STATE" };
const juce::Identifier lfoBankType{ "LFO_BANK" };
const juce::Identifier lfoNodeType{ "LFO" };
const juce::Identifier lfoPointNodeType{ "POINT" };
const juce::Identifier matrixType{ "MOD_MATRIX" };
const juce::Identifier matrixSlotType{ "SLOT" };
const juce::Identifier lfoNameProperty{ "name" };
const juce::Identifier lfoRateProperty{ "rateHz" };
const juce::Identifier lfoSyncProperty{ "sync" };
const juce::Identifier lfoDottedProperty{ "dotted" };
const juce::Identifier lfoSyncDivisionProperty{ "syncDivision" };
const juce::Identifier lfoTypeProperty{ "type" };
const juce::Identifier lfoTriggerProperty{ "triggerMode" };
const juce::Identifier lfoXProperty{ "x" };
const juce::Identifier lfoYProperty{ "y" };
const juce::Identifier lfoCurveProperty{ "curve" };
const juce::Identifier matrixSlotIndexProperty{ "slot" };
const juce::Identifier matrixSourceProperty{ "source" };
const juce::Identifier matrixEnabledProperty{ "enabled" };

void setMatrixSlot(juce::AudioProcessorValueTreeState& apvts,
                   int slotIndex,
                   int sourceIndex,
                   reactormod::Destination destination,
                   float amount)
{
    setParameterPlain(apvts, reactormod::getMatrixSourceParamID(slotIndex), (float) sourceIndex);
    setParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slotIndex),
                      (float) reactormod::destinationToChoiceIndex(destination));
    setParameterPlain(apvts, reactormod::getMatrixAmountParamID(slotIndex), juce::jlimit(-1.0f, 1.0f, amount));
}

void setWarpSection(juce::AudioProcessorValueTreeState& apvts, float saturator, bool postFilter, float mutate)
{
    setParameterPlain(apvts, "warpSaturator", juce::jlimit(0.0f, 1.0f, saturator));
    setBoolParam(apvts, "warpSaturatorPostFilter", postFilter);
    setParameterPlain(apvts, "warpMutate", juce::jlimit(0.0f, 1.0f, mutate));
}

void setNoiseSection(juce::AudioProcessorValueTreeState& apvts, float amount, int type, bool toFilter, bool toAmpEnv)
{
    setParameterPlain(apvts, "noiseAmount", juce::jlimit(0.0f, 0.4f, amount));
    setParameterPlain(apvts, "noiseType", (float) juce::jlimit(0, 11, type));
    setBoolParam(apvts, "noiseToFilter", toFilter);
    setBoolParam(apvts, "noiseToAmpEnv", toAmpEnv);
}

void resetLegacyFXParams(juce::AudioProcessorValueTreeState& apvts)
{
    setDriveFX(apvts, false, 0.28f, 0.55f, 0.25f);
    setChorusFX(apvts, false, 0.85f, 0.28f, 0.28f);
    setDelayFX(apvts, false, 0.36f, 0.36f, 0.24f);
    setReverbFX(apvts, false, 0.34f, 0.42f, 0.22f);
    setFilterFX(apvts, false, 6800.0f, 1.20f, 0.26f);
    setPhaserFX(apvts, false, 0.42f, 0.48f, 0.26f);
    setFlangerFX(apvts, false, 0.28f, 0.36f, 0.24f);
    setCompressorFX(apvts, false, 0.46f, 0.40f, 0.36f);
    setDimensionFX(apvts, false, 0.38f, 0.52f, 0.30f);
}

void clearFXChain(juce::AudioProcessorValueTreeState& apvts)
{
    for (int slot = 0; slot < PluginProcessor::maxFXSlots; ++slot)
    {
        setParameterPlain(apvts, getFXOrderParamID(slot), 0.0f);
        setBoolParam(apvts, getFXSlotOnParamID(slot), false);
        for (int parameterIndex = 0; parameterIndex < PluginProcessor::fxSlotParameterCount; ++parameterIndex)
            setParameterPlain(apvts, getFXSlotFloatParamID(slot, parameterIndex), getFXSlotDefaultValue(parameterIndex));
        setParameterPlain(apvts, getFXSlotVariantParamID(slot), 0.0f);
    }
}

void setFXSlot(juce::AudioProcessorValueTreeState& apvts,
               int slot,
               int type,
               int variant,
               float a,
               float b,
               float c,
               bool enabled = true)
{
    if (! juce::isPositiveAndBelow(slot, PluginProcessor::maxFXSlots))
        return;

    std::array<float, PluginProcessor::fxSlotParameterCount> parameters{};
    for (int parameterIndex = 0; parameterIndex < PluginProcessor::fxSlotParameterCount; ++parameterIndex)
        parameters[(size_t) parameterIndex] = getFXSlotDefaultValue(parameterIndex);

    parameters[0] = juce::jlimit(0.0f, 1.0f, a);
    parameters[1] = juce::jlimit(0.0f, 1.0f, b);
    parameters[2] = juce::jlimit(0.0f, 1.0f, c);

    if (type == fxFilter)
    {
        parameters[2] = 0.18f;
        parameters[3] = 0.40f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
    }
    else if (type == fxChorus)
    {
        parameters[2] = 0.30f;
        parameters[3] = 0.50f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
    }
    else if (type == fxFlanger)
    {
        parameters[2] = 0.22f;
        parameters[3] = 0.58f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
    }
    else if (type == fxPhaser)
    {
        parameters[2] = 0.50f;
        parameters[3] = 0.48f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
    }
    else if (type == fxDelay)
    {
        parameters[1] = juce::jlimit(0.0f, 1.0f, a);
        parameters[2] = 0.0f;
        parameters[3] = 1.0f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, b);
        parameters[5] = juce::jlimit(0.0f, 1.0f, c);
        parameters[6] = 0.0f;
        parameters[7] = 1.0f;
    }
    else if (type == fxReverb)
    {
        parameters[2] = 0.82f;
        parameters[3] = 0.08f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
    }
    else if (type == fxCompressor)
    {
        parameters[2] = 0.55f;
        parameters[3] = 0.50f;
        parameters[4] = juce::jlimit(0.0f, 1.0f, c);
        variant = variant > 0 ? 3 : 0;
    }

    setParameterPlain(apvts, getFXOrderParamID(slot), (float) juce::jlimit(0, PluginProcessor::getFXModuleNames().size() - 1, type));
    setBoolParam(apvts, getFXSlotOnParamID(slot), enabled && type != fxOff);
    for (int parameterIndex = 0; parameterIndex < PluginProcessor::fxSlotParameterCount; ++parameterIndex)
        setParameterPlain(apvts, getFXSlotFloatParamID(slot, parameterIndex), parameters[(size_t) parameterIndex]);
    setParameterPlain(apvts, getFXSlotVariantParamID(slot), (float) juce::jmax(0, variant));
}

int pickNoiseType(factorypresetbank::PackStyle style, float selector)
{
    using PackStyle = factorypresetbank::PackStyle;

    switch (style)
    {
        case PackStyle::solar:
            return selectChoice(std::array<int, 4>{ 0, 3, 7, 8 }, selector);
        case PackStyle::ground:
            return selectChoice(std::array<int, 4>{ 1, 2, 5, 10 }, selector);
        case PackStyle::halo:
            return selectChoice(std::array<int, 4>{ 1, 6, 8, 10 }, selector);
        case PackStyle::shattered:
            return selectChoice(std::array<int, 4>{ 3, 8, 9, 11 }, selector);
    }

    return 0;
}

void applyInitPreset(juce::AudioProcessorValueTreeState& apvts)
{
    setOscillator(apvts, 1, 1, 2, 0.88f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 2, 0, 2, 0.00f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 3, 3, 2, 0.00f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setParameterPlain(apvts, "osc1WarpFMSource", 1.0f);
    setParameterPlain(apvts, "osc2WarpFMSource", 2.0f);
    setParameterPlain(apvts, "osc3WarpFMSource", 0.0f);
    setParameterPlain(apvts, "osc1Width", 0.5f);
    setParameterPlain(apvts, "osc2Width", 0.5f);
    setParameterPlain(apvts, "osc3Width", 0.5f);

    setBoolParam(apvts, "filterOn", true);
    setParameterPlain(apvts, "filterMode", 0.0f);
    setParameterPlain(apvts, "filterCutoff", 18000.0f);
    setParameterPlain(apvts, "filterResonance", 0.75f);
    setParameterPlain(apvts, "filterDrive", 0.10f);
    setParameterPlain(apvts, "filterEnvAmount", 0.22f);
    setParameterPlain(apvts, "filterAttack", 0.001f);
    setParameterPlain(apvts, "filterDecay", 0.18f);
    setParameterPlain(apvts, "filterSustain", 0.0f);
    setParameterPlain(apvts, "filterRelease", 0.22f);

    setParameterPlain(apvts, "ampAttack", 0.001f);
    setParameterPlain(apvts, "ampDecay", 0.22f);
    setParameterPlain(apvts, "ampSustain", 0.80f);
    setParameterPlain(apvts, "ampRelease", 0.28f);

    setParameterPlain(apvts, "lfoRate", 2.0f);
    setParameterPlain(apvts, "lfoDepth", 0.0f);

    setParameterPlain(apvts, "voiceMode", 0.0f);
    setBoolParam(apvts, "voiceLegato", true);
    setBoolParam(apvts, "voicePortamento", false);
    setParameterPlain(apvts, "voiceGlide", 0.08f);

    setParameterPlain(apvts, "pitchBendUp", 7.0f);
    setParameterPlain(apvts, "pitchBendDown", 2.0f);
    setParameterPlain(apvts, "modWheelCutoff", 0.0f);
    setParameterPlain(apvts, "modWheelVibrato", 0.0f);
    setParameterPlain(apvts, "osc1Width", 0.5f);
    setParameterPlain(apvts, "osc2Width", 0.5f);
    setParameterPlain(apvts, "osc3Width", 0.5f);

    setParameterPlain(apvts, "warpSaturator", 0.0f);
    setBoolParam(apvts, "warpSaturatorPostFilter", false);
    setParameterPlain(apvts, "warpMutate", 0.0f);

    setParameterPlain(apvts, "noiseAmount", 0.0f);
    setParameterPlain(apvts, "noiseType", 0.0f);
    setBoolParam(apvts, "noiseToFilter", true);
    setBoolParam(apvts, "noiseToAmpEnv", true);
    setParameterPlain(apvts, "masterSpread", 1.0f);
    setParameterPlain(apvts, "masterGain", 1.0f);

    setDriveFX(apvts, false, 0.28f, 0.55f, 0.25f);
    setChorusFX(apvts, false, 0.85f, 0.28f, 0.28f);
    setDelayFX(apvts, false, 0.36f, 0.36f, 0.24f);
    setReverbFX(apvts, false, 0.34f, 0.42f, 0.22f);
    setFilterFX(apvts, false, 6800.0f, 1.20f, 0.26f);
    setPhaserFX(apvts, false, 0.42f, 0.48f, 0.26f);
    setFlangerFX(apvts, false, 0.28f, 0.36f, 0.24f);
    setCompressorFX(apvts, false, 0.46f, 0.40f, 0.36f);
    setDimensionFX(apvts, false, 0.38f, 0.52f, 0.30f);
    setFXOrder(apvts, { fxDrive, fxFilter, fxChorus, fxDelay, fxReverb, fxCompressor });
}

void applyFactoryPresetState(juce::AudioProcessorValueTreeState& apvts, int index)
{
    applyInitPreset(apvts);

    switch (index)
    {
        case 1: // Hyper Stack
            setOscillator(apvts, 1, 1, 2, 0.72f, -4.0f, true, true, 7, 0.55f, 0.0f, 0.0f, 0.06f);
            setOscillator(apvts, 2, 1, 2, 0.46f, 4.0f, true, true, 7, 0.48f, 0.0f, 0.0f, 0.02f);
            setOscillator(apvts, 3, 2, 3, 0.22f, 0.0f, true, true, 5, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 1.0f);
            setParameterPlain(apvts, "filterCutoff", 9200.0f);
            setParameterPlain(apvts, "filterResonance", 1.10f);
            setParameterPlain(apvts, "filterDrive", 0.16f);
            setParameterPlain(apvts, "ampDecay", 0.28f);
            setParameterPlain(apvts, "ampSustain", 0.72f);
            setParameterPlain(apvts, "ampRelease", 0.22f);
            setParameterPlain(apvts, "masterSpread", 0.88f);
            break;

        case 2: // Reactor Bass
            setOscillator(apvts, 1, 1, 1, 0.84f, -2.0f, true, false, 3, 0.20f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 0, 0, 0.46f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 2, 1, 0.26f, 3.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 1.0f);
            setParameterPlain(apvts, "filterCutoff", 420.0f);
            setParameterPlain(apvts, "filterResonance", 1.20f);
            setParameterPlain(apvts, "filterDrive", 0.42f);
            setParameterPlain(apvts, "filterEnvAmount", 0.56f);
            setParameterPlain(apvts, "filterDecay", 0.22f);
            setParameterPlain(apvts, "ampDecay", 0.16f);
            setParameterPlain(apvts, "ampSustain", 0.60f);
            setParameterPlain(apvts, "ampRelease", 0.14f);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            setBoolParam(apvts, "voicePortamento", true);
            setParameterPlain(apvts, "voiceGlide", 0.06f);
            break;

        case 3: // Acid Runner
            setOscillator(apvts, 1, 2, 2, 0.76f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 1, 2, 0.42f, 7.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 1.0f);
            setParameterPlain(apvts, "filterCutoff", 880.0f);
            setParameterPlain(apvts, "filterResonance", 8.60f);
            setParameterPlain(apvts, "filterDrive", 0.48f);
            setParameterPlain(apvts, "filterEnvAmount", 0.92f);
            setParameterPlain(apvts, "filterDecay", 0.20f);
            setParameterPlain(apvts, "ampDecay", 0.16f);
            setParameterPlain(apvts, "ampSustain", 0.24f);
            setParameterPlain(apvts, "ampRelease", 0.10f);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            setBoolParam(apvts, "voicePortamento", true);
            setParameterPlain(apvts, "voiceGlide", 0.08f);
            break;

        case 4: // Neon Pluck
            setOscillator(apvts, 1, 1, 2, 0.64f, 0.0f, true, true, 5, 0.26f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 3, 0.24f, 12.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 0.0f);
            setParameterPlain(apvts, "filterCutoff", 2800.0f);
            setParameterPlain(apvts, "filterResonance", 3.80f);
            setParameterPlain(apvts, "filterEnvAmount", 0.86f);
            setParameterPlain(apvts, "ampDecay", 0.18f);
            setParameterPlain(apvts, "ampSustain", 0.08f);
            setParameterPlain(apvts, "ampRelease", 0.12f);
            setParameterPlain(apvts, "masterSpread", 0.72f);
            break;

        case 5: // Glass Pad
            setOscillator(apvts, 1, 1, 2, 0.54f, 0.0f, true, true, 5, 0.28f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 2, 0.34f, 5.0f, true, true, 5, 0.20f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 0, 3, 0.12f, 12.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterCutoff", 5200.0f);
            setParameterPlain(apvts, "filterResonance", 1.05f);
            setParameterPlain(apvts, "ampAttack", 0.18f);
            setParameterPlain(apvts, "ampDecay", 1.40f);
            setParameterPlain(apvts, "ampSustain", 0.62f);
            setParameterPlain(apvts, "ampRelease", 1.90f);
            setParameterPlain(apvts, "masterSpread", 0.84f);
            break;

        case 6: // Dream Bloom
            setOscillator(apvts, 1, 3, 2, 0.46f, 0.0f, true, true, 7, 0.18f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 2, 0.26f, 8.0f, true, true, 5, 0.16f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 0, 3, 0.16f, -12.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 6.0f);
            setParameterPlain(apvts, "filterCutoff", 6400.0f);
            setParameterPlain(apvts, "filterResonance", 1.80f);
            setParameterPlain(apvts, "ampAttack", 0.12f);
            setParameterPlain(apvts, "ampDecay", 2.00f);
            setParameterPlain(apvts, "ampSustain", 0.76f);
            setParameterPlain(apvts, "ampRelease", 2.40f);
            setParameterPlain(apvts, "lfoRate", 0.30f);
            setParameterPlain(apvts, "lfoDepth", 0.12f);
            setParameterPlain(apvts, "noiseAmount", 0.02f);
            break;

        case 7: // Bell Matrix
            setOscillator(apvts, 1, 0, 3, 0.62f, 0.0f, true, false, 3, 0.22f, 0.44f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 4, 0.28f, 12.0f, true, false, 3, 0.22f, 0.18f, 0.0f, 0.0f);
            setParameterPlain(apvts, "osc1WarpFMSource", 4.0f);
            setParameterPlain(apvts, "osc2WarpFMSource", 0.0f);
            setParameterPlain(apvts, "filterMode", 4.0f);
            setParameterPlain(apvts, "filterCutoff", 4200.0f);
            setParameterPlain(apvts, "filterResonance", 2.20f);
            setParameterPlain(apvts, "ampDecay", 2.60f);
            setParameterPlain(apvts, "ampSustain", 0.0f);
            setParameterPlain(apvts, "ampRelease", 2.20f);
            setParameterPlain(apvts, "masterSpread", 0.70f);
            break;

        case 8: // Mono Driver
            setOscillator(apvts, 1, 1, 2, 0.78f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 2, 0.24f, -7.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.12f);
            setParameterPlain(apvts, "filterMode", 1.0f);
            setParameterPlain(apvts, "filterCutoff", 760.0f);
            setParameterPlain(apvts, "filterResonance", 1.45f);
            setParameterPlain(apvts, "filterDrive", 0.58f);
            setParameterPlain(apvts, "filterEnvAmount", 0.46f);
            setParameterPlain(apvts, "warpSaturator", 0.34f);
            setBoolParam(apvts, "warpSaturatorPostFilter", false);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            setBoolParam(apvts, "voicePortamento", true);
            setParameterPlain(apvts, "voiceGlide", 0.05f);
            break;

        case 9: // Velvet Keys
            setOscillator(apvts, 1, 2, 2, 0.52f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 2, 0.28f, 4.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterCutoff", 3400.0f);
            setParameterPlain(apvts, "filterResonance", 1.70f);
            setParameterPlain(apvts, "ampAttack", 0.01f);
            setParameterPlain(apvts, "ampDecay", 0.55f);
            setParameterPlain(apvts, "ampSustain", 0.48f);
            setParameterPlain(apvts, "ampRelease", 0.66f);
            setParameterPlain(apvts, "masterSpread", 0.38f);
            break;

        case 10: // Hollow Square
            setOscillator(apvts, 1, 2, 2, 0.78f, 0.0f, true, true, 5, 0.16f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 3, 0.18f, 12.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 13.0f);
            setParameterPlain(apvts, "filterCutoff", 1600.0f);
            setParameterPlain(apvts, "filterResonance", 5.20f);
            setParameterPlain(apvts, "ampDecay", 0.34f);
            setParameterPlain(apvts, "ampSustain", 0.56f);
            break;

        case 11: // Formant Vox
            setOscillator(apvts, 1, 1, 2, 0.54f, 0.0f, true, true, 5, 0.18f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 3, 0.26f, 7.0f, true, false, 3, 0.22f, 0.10f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 5.0f);
            setParameterPlain(apvts, "filterMode", 13.0f);
            setParameterPlain(apvts, "filterCutoff", 1800.0f);
            setParameterPlain(apvts, "filterResonance", 5.80f);
            setParameterPlain(apvts, "filterEnvAmount", 0.52f);
            setParameterPlain(apvts, "lfoRate", 0.90f);
            setParameterPlain(apvts, "lfoDepth", 0.16f);
            setParameterPlain(apvts, "noiseAmount", 0.03f);
            break;

        case 12: // Comb String
            setOscillator(apvts, 1, 1, 2, 0.58f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 2, 0.34f, 2.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 11.0f);
            setParameterPlain(apvts, "filterCutoff", 330.0f);
            setParameterPlain(apvts, "filterResonance", 8.80f);
            setParameterPlain(apvts, "filterDrive", 0.18f);
            setParameterPlain(apvts, "ampAttack", 0.02f);
            setParameterPlain(apvts, "ampDecay", 0.80f);
            setParameterPlain(apvts, "ampSustain", 0.48f);
            setParameterPlain(apvts, "ampRelease", 0.92f);
            break;

        case 13: // FM Shiver
            setOscillator(apvts, 1, 0, 3, 0.66f, 0.0f, true, false, 3, 0.22f, 0.58f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 0, 4, 0.18f, 12.0f, false, false, 3, 0.22f, 0.22f, 0.0f, 0.0f);
            setParameterPlain(apvts, "osc1WarpFMSource", 6.0f);
            setParameterPlain(apvts, "osc2WarpFMSource", 3.0f);
            setParameterPlain(apvts, "filterMode", 7.0f);
            setParameterPlain(apvts, "filterCutoff", 2600.0f);
            setParameterPlain(apvts, "filterResonance", 2.80f);
            setParameterPlain(apvts, "ampDecay", 0.42f);
            setParameterPlain(apvts, "ampSustain", 0.26f);
            setParameterPlain(apvts, "ampRelease", 0.62f);
            break;

        case 14: // Sync Laser
            setOscillator(apvts, 1, 1, 2, 0.72f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.74f, 0.12f);
            setOscillator(apvts, 2, 1, 3, 0.26f, 7.0f, true, false, 3, 0.22f, 0.0f, 0.42f, 0.0f);
            setParameterPlain(apvts, "filterMode", 7.0f);
            setParameterPlain(apvts, "filterCutoff", 2400.0f);
            setParameterPlain(apvts, "filterResonance", 6.80f);
            setParameterPlain(apvts, "filterDrive", 0.28f);
            setParameterPlain(apvts, "ampDecay", 0.24f);
            setParameterPlain(apvts, "ampSustain", 0.20f);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            break;

        case 15: // Noise Sweep
            setOscillator(apvts, 1, 1, 2, 0.36f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 4.0f);
            setParameterPlain(apvts, "filterMode", 2.0f);
            setParameterPlain(apvts, "filterCutoff", 1800.0f);
            setParameterPlain(apvts, "filterResonance", 2.40f);
            setParameterPlain(apvts, "filterEnvAmount", 0.70f);
            setParameterPlain(apvts, "ampAttack", 0.001f);
            setParameterPlain(apvts, "ampDecay", 1.60f);
            setParameterPlain(apvts, "ampSustain", 0.0f);
            setParameterPlain(apvts, "ampRelease", 1.20f);
            setParameterPlain(apvts, "noiseAmount", 0.18f);
            setBoolParam(apvts, "noiseToFilter", true);
            break;

        case 16: // Brass Charge
            setOscillator(apvts, 1, 1, 2, 0.62f, 0.0f, true, true, 5, 0.18f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 2, 0.28f, 3.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterCutoff", 1600.0f);
            setParameterPlain(apvts, "filterResonance", 1.60f);
            setParameterPlain(apvts, "filterDrive", 0.24f);
            setParameterPlain(apvts, "ampAttack", 0.03f);
            setParameterPlain(apvts, "ampDecay", 0.36f);
            setParameterPlain(apvts, "ampSustain", 0.74f);
            setParameterPlain(apvts, "ampRelease", 0.26f);
            break;

        case 17: // Organ Drift
            setOscillator(apvts, 1, 2, 2, 0.48f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 3, 0.32f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 0, 1, 0.22f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setBoolParam(apvts, "filterOn", false);
            setParameterPlain(apvts, "ampDecay", 0.24f);
            setParameterPlain(apvts, "ampSustain", 1.0f);
            setParameterPlain(apvts, "ampRelease", 0.22f);
            setParameterPlain(apvts, "lfoRate", 0.28f);
            setParameterPlain(apvts, "lfoDepth", 0.04f);
            break;

        case 18: // Flange Choir
            setOscillator(apvts, 1, 3, 2, 0.46f, 0.0f, true, true, 5, 0.14f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 1, 2, 0.24f, 7.0f, true, true, 5, 0.14f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 1.0f);
            setParameterPlain(apvts, "filterMode", 14.0f);
            setParameterPlain(apvts, "filterCutoff", 880.0f);
            setParameterPlain(apvts, "filterResonance", 8.60f);
            setParameterPlain(apvts, "ampAttack", 0.08f);
            setParameterPlain(apvts, "ampDecay", 1.20f);
            setParameterPlain(apvts, "ampSustain", 0.66f);
            setParameterPlain(apvts, "ampRelease", 1.60f);
            setParameterPlain(apvts, "masterSpread", 0.90f);
            break;

        case 19: // Night Drone
            setOscillator(apvts, 1, 3, 1, 0.56f, 0.0f, true, true, 7, 0.18f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 0, 0, 0.26f, 0.0f, false, false, 3, 0.22f, 0.12f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 1, 2, 0.16f, -12.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.08f);
            setParameterPlain(apvts, "noiseType", 6.0f);
            setParameterPlain(apvts, "filterCutoff", 560.0f);
            setParameterPlain(apvts, "filterResonance", 2.20f);
            setParameterPlain(apvts, "ampAttack", 0.50f);
            setParameterPlain(apvts, "ampDecay", 2.60f);
            setParameterPlain(apvts, "ampSustain", 0.82f);
            setParameterPlain(apvts, "ampRelease", 3.20f);
            setParameterPlain(apvts, "lfoRate", 0.22f);
            setParameterPlain(apvts, "lfoDepth", 0.10f);
            setParameterPlain(apvts, "noiseAmount", 0.05f);
            break;

        case 20: // Solar Sweep
            setOscillator(apvts, 1, 1, 2, 0.68f, 0.0f, true, true, 5, 0.22f, 0.0f, 0.18f, 0.12f);
            setOscillator(apvts, 2, 1, 3, 0.28f, 6.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.08f);
            setParameterPlain(apvts, "filterMode", 21.0f);
            setParameterPlain(apvts, "filterCutoff", 3200.0f);
            setParameterPlain(apvts, "filterResonance", 2.60f);
            setParameterPlain(apvts, "filterEnvAmount", 0.66f);
            setParameterPlain(apvts, "ampDecay", 0.40f);
            setParameterPlain(apvts, "ampRelease", 0.48f);
            setDelayFX(apvts, true, 0.30f, 0.28f, 0.20f);
            break;

        case 21: // Tube Organ
            setOscillator(apvts, 1, 2, 2, 0.46f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 3, 0.34f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 0, 1, 0.18f, 2.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setBoolParam(apvts, "filterOn", false);
            setParameterPlain(apvts, "ampSustain", 1.0f);
            setParameterPlain(apvts, "ampRelease", 0.18f);
            setDriveFX(apvts, true, 0.40f, 0.46f, 0.34f);
            break;

        case 22: // Plasma Reed
            setOscillator(apvts, 1, 1, 2, 0.62f, 0.0f, true, false, 3, 0.22f, 0.18f, 0.0f, 0.22f);
            setOscillator(apvts, 2, 0, 3, 0.22f, 12.0f, true, false, 3, 0.22f, 0.36f, 0.0f, 0.0f);
            setParameterPlain(apvts, "osc2WarpFMSource", 6.0f);
            setParameterPlain(apvts, "noiseType", 11.0f);
            setParameterPlain(apvts, "noiseAmount", 0.08f);
            setParameterPlain(apvts, "filterMode", 17.0f);
            setParameterPlain(apvts, "filterCutoff", 1400.0f);
            setParameterPlain(apvts, "filterResonance", 4.20f);
            setDriveFX(apvts, true, 0.32f, 0.62f, 0.24f);
            break;

        case 23: // Vowel Pad
            setOscillator(apvts, 1, 3, 2, 0.50f, 0.0f, true, true, 7, 0.16f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 1, 2, 0.24f, 7.0f, true, true, 5, 0.14f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 18.0f);
            setParameterPlain(apvts, "filterCutoff", 1700.0f);
            setParameterPlain(apvts, "filterResonance", 5.80f);
            setParameterPlain(apvts, "ampAttack", 0.12f);
            setParameterPlain(apvts, "ampDecay", 1.30f);
            setParameterPlain(apvts, "ampSustain", 0.72f);
            setParameterPlain(apvts, "ampRelease", 1.90f);
            setReverbFX(apvts, true, 0.52f, 0.36f, 0.30f);
            break;

        case 24: // Dust Bell
            setOscillator(apvts, 1, 0, 4, 0.48f, 0.0f, true, false, 3, 0.22f, 0.42f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 3, 0.18f, 7.0f, true, false, 3, 0.22f, 0.12f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 9.0f);
            setParameterPlain(apvts, "noiseAmount", 0.10f);
            setParameterPlain(apvts, "filterMode", 22.0f);
            setParameterPlain(apvts, "filterCutoff", 2800.0f);
            setParameterPlain(apvts, "filterResonance", 4.80f);
            setDelayFX(apvts, true, 0.42f, 0.34f, 0.24f);
            break;

        case 25: // Metal Harp
            setOscillator(apvts, 1, 0, 3, 0.62f, 0.0f, true, false, 3, 0.22f, 0.20f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 24.0f);
            setParameterPlain(apvts, "filterCutoff", 1200.0f);
            setParameterPlain(apvts, "filterResonance", 8.20f);
            setParameterPlain(apvts, "ampDecay", 1.20f);
            setParameterPlain(apvts, "ampSustain", 0.0f);
            setParameterPlain(apvts, "ampRelease", 1.10f);
            setReverbFX(apvts, true, 0.40f, 0.28f, 0.22f);
            break;

        case 26: // Sub Reactor
            setOscillator(apvts, 1, 1, 0, 0.80f, -3.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 0, 0, 0.34f, 0.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 23.0f);
            setParameterPlain(apvts, "filterCutoff", 240.0f);
            setParameterPlain(apvts, "filterResonance", 1.80f);
            setParameterPlain(apvts, "filterDrive", 0.62f);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            setBoolParam(apvts, "voicePortamento", true);
            setDriveFX(apvts, true, 0.48f, 0.42f, 0.36f);
            break;

        case 27: // Quartz Lead
            setOscillator(apvts, 1, 1, 2, 0.70f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.44f, 0.10f);
            setOscillator(apvts, 2, 1, 3, 0.24f, 12.0f, true, false, 3, 0.22f, 0.0f, 0.22f, 0.0f);
            setParameterPlain(apvts, "filterMode", 25.0f);
            setParameterPlain(apvts, "filterCutoff", 2300.0f);
            setParameterPlain(apvts, "filterResonance", 6.20f);
            setParameterPlain(apvts, "ampDecay", 0.18f);
            setParameterPlain(apvts, "ampSustain", 0.24f);
            setChorusFX(apvts, true, 1.60f, 0.24f, 0.18f);
            break;

        case 28: // Ghost Choir
            setOscillator(apvts, 1, 3, 2, 0.42f, 0.0f, true, true, 7, 0.14f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 3, 0.22f, 12.0f, true, true, 5, 0.10f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 19.0f);
            setParameterPlain(apvts, "filterCutoff", 1100.0f);
            setParameterPlain(apvts, "filterResonance", 4.40f);
            setParameterPlain(apvts, "ampAttack", 0.24f);
            setParameterPlain(apvts, "ampRelease", 2.30f);
            setChorusFX(apvts, true, 0.52f, 0.42f, 0.28f);
            setReverbFX(apvts, true, 0.62f, 0.34f, 0.34f);
            break;

        case 29: // Circuit Pluck
            setOscillator(apvts, 1, 2, 2, 0.66f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.16f);
            setParameterPlain(apvts, "filterMode", 10.0f);
            setParameterPlain(apvts, "filterCutoff", 3600.0f);
            setParameterPlain(apvts, "filterResonance", 3.40f);
            setParameterPlain(apvts, "ampDecay", 0.12f);
            setParameterPlain(apvts, "ampSustain", 0.04f);
            setDelayFX(apvts, true, 0.18f, 0.22f, 0.18f);
            break;

        case 30: // Low Orbit
            setOscillator(apvts, 1, 3, 0, 0.72f, 0.0f, true, true, 5, 0.12f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 1, 1, 0.24f, -7.0f, false, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 10.0f);
            setParameterPlain(apvts, "noiseAmount", 0.04f);
            setParameterPlain(apvts, "filterCutoff", 300.0f);
            setParameterPlain(apvts, "filterResonance", 1.60f);
            setParameterPlain(apvts, "lfoRate", 0.18f);
            setParameterPlain(apvts, "lfoDepth", 0.08f);
            setReverbFX(apvts, true, 0.48f, 0.54f, 0.18f);
            break;

        case 31: // Arc Wind
            setOscillator(apvts, 1, 1, 2, 0.36f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 11.0f);
            setParameterPlain(apvts, "noiseAmount", 0.16f);
            setParameterPlain(apvts, "filterMode", 2.0f);
            setParameterPlain(apvts, "filterCutoff", 2200.0f);
            setParameterPlain(apvts, "filterResonance", 3.80f);
            setDelayFX(apvts, true, 0.48f, 0.42f, 0.22f);
            break;

        case 32: // Glass Mono
            setOscillator(apvts, 1, 0, 3, 0.58f, 0.0f, true, false, 3, 0.22f, 0.36f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 3, 0.18f, 12.0f, true, false, 3, 0.22f, 0.10f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 22.0f);
            setParameterPlain(apvts, "filterCutoff", 3400.0f);
            setParameterPlain(apvts, "filterResonance", 3.80f);
            setParameterPlain(apvts, "voiceMode", 1.0f);
            setDelayFX(apvts, true, 0.26f, 0.18f, 0.16f);
            break;

        case 33: // Blue Noise Pad
            setOscillator(apvts, 1, 3, 2, 0.42f, 0.0f, true, true, 5, 0.14f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 7.0f);
            setParameterPlain(apvts, "noiseAmount", 0.12f);
            setParameterPlain(apvts, "filterMode", 20.0f);
            setParameterPlain(apvts, "filterCutoff", 960.0f);
            setParameterPlain(apvts, "filterResonance", 4.20f);
            setParameterPlain(apvts, "ampAttack", 0.10f);
            setParameterPlain(apvts, "ampRelease", 1.80f);
            setChorusFX(apvts, true, 0.44f, 0.36f, 0.26f);
            break;

        case 34: // Tilt Keys
            setOscillator(apvts, 1, 2, 2, 0.54f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 3, 2, 0.24f, 5.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 21.0f);
            setParameterPlain(apvts, "filterCutoff", 2100.0f);
            setParameterPlain(apvts, "filterResonance", 2.20f);
            setParameterPlain(apvts, "ampDecay", 0.48f);
            setParameterPlain(apvts, "ampRelease", 0.54f);
            setDriveFX(apvts, true, 0.24f, 0.66f, 0.18f);
            break;

        case 35: // Resonator Brass
            setOscillator(apvts, 1, 1, 2, 0.62f, 0.0f, true, true, 5, 0.16f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 2, 2, 0.26f, 4.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 22.0f);
            setParameterPlain(apvts, "filterCutoff", 1200.0f);
            setParameterPlain(apvts, "filterResonance", 5.60f);
            setParameterPlain(apvts, "ampAttack", 0.03f);
            setParameterPlain(apvts, "ampDecay", 0.36f);
            setParameterPlain(apvts, "ampSustain", 0.70f);
            setChorusFX(apvts, true, 0.80f, 0.20f, 0.18f);
            break;

        case 36: // Phase Bloom
            setOscillator(apvts, 1, 3, 2, 0.50f, 0.0f, true, true, 7, 0.16f, 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 2, 1, 3, 0.18f, 12.0f, true, true, 5, 0.10f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 25.0f);
            setParameterPlain(apvts, "filterCutoff", 1800.0f);
            setParameterPlain(apvts, "filterResonance", 6.00f);
            setParameterPlain(apvts, "ampAttack", 0.12f);
            setParameterPlain(apvts, "ampRelease", 1.40f);
            setReverbFX(apvts, true, 0.58f, 0.24f, 0.24f);
            break;

        case 37: // Metal Sync
            setOscillator(apvts, 1, 1, 2, 0.68f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.66f, 0.12f);
            setOscillator(apvts, 2, 0, 3, 0.22f, 12.0f, true, false, 3, 0.22f, 0.24f, 0.22f, 0.0f);
            setParameterPlain(apvts, "filterMode", 24.0f);
            setParameterPlain(apvts, "filterCutoff", 900.0f);
            setParameterPlain(apvts, "filterResonance", 7.60f);
            setDriveFX(apvts, true, 0.40f, 0.38f, 0.24f);
            setDelayFX(apvts, true, 0.22f, 0.18f, 0.14f);
            break;

        case 38: // Violet Drone
            setOscillator(apvts, 1, 3, 1, 0.54f, 0.0f, true, true, 7, 0.18f, 0.0f, 0.0f, 0.0f);
            setParameterPlain(apvts, "noiseType", 8.0f);
            setParameterPlain(apvts, "noiseAmount", 0.10f);
            setParameterPlain(apvts, "filterMode", 16.0f);
            setParameterPlain(apvts, "filterCutoff", 760.0f);
            setParameterPlain(apvts, "filterResonance", 4.80f);
            setParameterPlain(apvts, "ampAttack", 0.40f);
            setParameterPlain(apvts, "ampRelease", 2.60f);
            setReverbFX(apvts, true, 0.66f, 0.48f, 0.34f);
            break;

        case 39: // Cosmic Reed
            setOscillator(apvts, 1, 1, 2, 0.58f, 0.0f, true, false, 3, 0.22f, 0.20f, 0.0f, 0.18f);
            setOscillator(apvts, 2, 0, 3, 0.22f, 7.0f, true, false, 3, 0.22f, 0.24f, 0.0f, 0.0f);
            setParameterPlain(apvts, "filterMode", 17.0f);
            setParameterPlain(apvts, "filterCutoff", 1550.0f);
            setParameterPlain(apvts, "filterResonance", 5.10f);
            setChorusFX(apvts, true, 1.20f, 0.18f, 0.18f);
            setDelayFX(apvts, true, 0.34f, 0.26f, 0.20f);
            break;

        default:
            break;
    }
}

void applyFactoryPresetVariant(juce::AudioProcessorValueTreeState& apvts, int variantIndex)
{
    if (variantIndex <= 0)
        return;

    auto readValue = [&apvts](const juce::String& id, float fallback)
    {
        if (auto* value = apvts.getRawParameterValue(id))
            return value->load();
        return fallback;
    };

    const float cutoff = readValue("filterCutoff", 2200.0f);
    const float resonance = readValue("filterResonance", 1.0f);
    const float ampAttack = readValue("ampAttack", 0.001f);
    const float ampRelease = readValue("ampRelease", 0.22f);
    const float spread = readValue("masterSpread", 0.5f);
    const float gain = readValue("masterGain", 0.6f);
    const float saturator = readValue("warpSaturator", 0.0f);
    const float mutate = readValue("warpMutate", 0.0f);
    const float lfoDepth = readValue("lfoDepth", 0.0f);

    switch (variantIndex)
    {
        case 1: // Club
            setParameterPlain(apvts, "filterDrive", juce::jlimit(0.0f, 1.0f, readValue("filterDrive", 0.12f) + 0.10f));
            setParameterPlain(apvts, "warpSaturator", juce::jlimit(0.0f, 1.0f, saturator + 0.18f));
            setParameterPlain(apvts, "masterGain", juce::jlimit(0.0f, 1.0f, gain + 0.06f));
            setParameterPlain(apvts, "ampAttack", juce::jlimit(0.001f, 6.0f, ampAttack * 0.75f));
            setParameterPlain(apvts, "ampRelease", juce::jlimit(0.001f, 8.0f, ampRelease * 0.85f));
            setParameterPlain(apvts, "masterSpread", juce::jlimit(0.0f, 1.0f, spread * 0.92f));
            break;

        case 2: // Wide
            setBoolParam(apvts, "osc1UnisonOn", true);
            setBoolParam(apvts, "osc2UnisonOn", true);
            setParameterPlain(apvts, "osc1UnisonVoices", juce::jlimit(3.0f, 7.0f, readValue("osc1UnisonVoices", 3.0f) + 2.0f));
            setParameterPlain(apvts, "osc2UnisonVoices", juce::jlimit(3.0f, 7.0f, readValue("osc2UnisonVoices", 3.0f) + 2.0f));
            setParameterPlain(apvts, "osc1Unison", juce::jlimit(0.0f, 1.0f, readValue("osc1Unison", 0.22f) + 0.12f));
            setParameterPlain(apvts, "osc2Unison", juce::jlimit(0.0f, 1.0f, readValue("osc2Unison", 0.22f) + 0.10f));
            setParameterPlain(apvts, "masterSpread", juce::jlimit(0.0f, 1.0f, spread + 0.20f));
            setParameterPlain(apvts, "ampRelease", juce::jlimit(0.001f, 8.0f, ampRelease * 1.35f));
            setParameterPlain(apvts, "fxChorusOn", 1.0f);
            setParameterPlain(apvts, "fxReverbOn", 1.0f);
            break;

        case 3: // Dark
            setParameterPlain(apvts, "filterCutoff", juce::jlimit(20.0f, 22000.0f, cutoff * 0.48f));
            setParameterPlain(apvts, "filterResonance", juce::jlimit(0.2f, 18.0f, resonance * 1.08f));
            setParameterPlain(apvts, "masterGain", juce::jlimit(0.0f, 1.0f, gain * 0.95f));
            setParameterPlain(apvts, "warpSaturator", juce::jlimit(0.0f, 1.0f, saturator + 0.08f));
            setParameterPlain(apvts, "noiseAmount", juce::jlimit(0.0f, 1.0f, readValue("noiseAmount", 0.0f) + 0.015f));
            break;

        case 4: // Hyper
            setParameterPlain(apvts, "warpMutate", juce::jlimit(0.0f, 1.0f, mutate + 0.30f));
            setParameterPlain(apvts, "warpSaturator", juce::jlimit(0.0f, 1.0f, saturator + 0.14f));
            setParameterPlain(apvts, "lfoDepth", juce::jlimit(0.0f, 1.0f, lfoDepth + 0.14f));
            setParameterPlain(apvts, "masterSpread", juce::jlimit(0.0f, 1.0f, spread + 0.10f));
            setParameterPlain(apvts, "filterCutoff", juce::jlimit(20.0f, 22000.0f, cutoff * 1.22f));
            setParameterPlain(apvts, "osc1WarpSync", juce::jlimit(0.0f, 1.0f, readValue("osc1WarpSync", 0.0f) + 0.18f));
            setParameterPlain(apvts, "osc2WarpFM", juce::jlimit(0.0f, 1.0f, readValue("osc2WarpFM", 0.0f) + 0.12f));
            setParameterPlain(apvts, "fxDelayOn", 1.0f);
            setParameterPlain(apvts, "fxDelayMix", juce::jlimit(0.0f, 1.0f, readValue("fxDelayMix", 0.24f) + 0.10f));
            break;

        default:
            break;
    }
}

void applyFactoryPresetIndexPolish(juce::AudioProcessorValueTreeState& apvts, int expandedIndex)
{
    auto readValue = [&apvts](const juce::String& id, float fallback)
    {
        if (auto* value = apvts.getRawParameterValue(id))
            return value->load();
        return fallback;
    };

    const float seedA = std::fmod(std::sin((float) expandedIndex * 12.9898f) * 43758.5453f, 1.0f);
    const float seedB = std::fmod(std::sin((float) expandedIndex * 5.3983f + 2.7f) * 24617.2148f, 1.0f);
    const float seedC = std::fmod(std::sin((float) expandedIndex * 8.1234f + 5.1f) * 12345.6789f, 1.0f);
    const float a = std::abs(seedA);
    const float b = std::abs(seedB);
    const float c = std::abs(seedC);

    setParameterPlain(apvts, "filterCutoff",
                      juce::jlimit(20.0f, 22000.0f, readValue("filterCutoff", 2400.0f) * juce::jmap(a, 0.82f, 1.18f)));
    setParameterPlain(apvts, "filterResonance",
                      juce::jlimit(0.2f, 18.0f, readValue("filterResonance", 1.0f) * juce::jmap(b, 0.90f, 1.18f)));
    setParameterPlain(apvts, "filterDrive",
                      juce::jlimit(0.0f, 1.0f, readValue("filterDrive", 0.12f) + juce::jmap(c, -0.03f, 0.10f)));
    setParameterPlain(apvts, "warpMutate",
                      juce::jlimit(0.0f, 1.0f, readValue("warpMutate", 0.0f) + juce::jmap(a, 0.0f, 0.18f)));
    setParameterPlain(apvts, "warpSaturator",
                      juce::jlimit(0.0f, 1.0f, readValue("warpSaturator", 0.0f) + juce::jmap(b, 0.0f, 0.16f)));
    setParameterPlain(apvts, "lfoRate",
                      juce::jlimit(0.05f, 16.0f, readValue("lfoRate", 2.0f) * juce::jmap(c, 0.85f, 1.35f)));
    setParameterPlain(apvts, "lfoDepth",
                      juce::jlimit(0.0f, 1.0f, readValue("lfoDepth", 0.0f) + juce::jmap(a, 0.0f, 0.12f)));
    setParameterPlain(apvts, "masterSpread",
                      juce::jlimit(0.0f, 1.0f, readValue("masterSpread", 0.5f) + juce::jmap(b, -0.08f, 0.14f)));
    setParameterPlain(apvts, "masterGain",
                      juce::jlimit(0.0f, 1.0f, readValue("masterGain", 0.6f) + juce::jmap(c, -0.05f, 0.08f)));
    setParameterPlain(apvts, "noiseType",
                      (float) (expandedIndex % PluginProcessor::getNoiseTypeNames().size()));
    setParameterPlain(apvts, "noiseAmount",
                      juce::jlimit(0.0f, 1.0f, readValue("noiseAmount", 0.0f) + juce::jmap(a * b, 0.0f, 0.04f)));

    setParameterPlain(apvts, "osc1Detune", juce::jlimit(-24.0f, 24.0f, readValue("osc1Detune", 0.0f) + juce::jmap(a, -2.2f, 2.2f)));
    setParameterPlain(apvts, "osc2Detune", juce::jlimit(-24.0f, 24.0f, readValue("osc2Detune", 0.0f) + juce::jmap(b, -3.0f, 3.0f)));
    setParameterPlain(apvts, "osc3Detune", juce::jlimit(-24.0f, 24.0f, readValue("osc3Detune", 0.0f) + juce::jmap(c, -4.0f, 4.0f)));
    setParameterPlain(apvts, "osc1WarpBend", juce::jlimit(0.0f, 1.0f, readValue("osc1WarpBend", 0.0f) + juce::jmap(a, 0.0f, 0.16f)));
    setParameterPlain(apvts, "osc2WarpFM", juce::jlimit(0.0f, 1.0f, readValue("osc2WarpFM", 0.0f) + juce::jmap(b, 0.0f, 0.18f)));
    setParameterPlain(apvts, "osc3WarpSync", juce::jlimit(0.0f, 1.0f, readValue("osc3WarpSync", 0.0f) + juce::jmap(c, 0.0f, 0.22f)));
}

juce::String sanitisePresetName(juce::String name)
{
    name = name.trim();
    name = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-()[]");
    while (name.contains("  "))
        name = name.replace("  ", " ");

    return name.trim();
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int osc = 1; osc <= 3; ++osc)
    {
        const auto prefix = "osc" + juce::String(osc);

        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Type", prefix + " Type", getOscillatorTypeNames(), 0)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Wave", prefix + " Wave", getWaveNames(), osc == 1 ? 1 : osc == 2 ? 2 : 3)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Wavetable", prefix + " Wavetable", getWavetableNames(), (osc - 1) % juce::jmax(1, getWavetableNames().size()))));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "WTPos", prefix + " Wavetable Position",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Level", prefix + " Level",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 osc == 1 ? 0.65f : osc == 2 ? 0.35f : 0.25f)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Octave", prefix + " Octave", getOctaveNames(), osc == 3 ? 1 : 2)));
        params.push_back(makeParam(new juce::AudioParameterInt(prefix + "Coarse", prefix + " Coarse", -24, 24, 0)));
        params.push_back(makeParam(new juce::AudioParameterInt(prefix + "SampleRoot", prefix + " Sample Root", 12, 127, 60)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Detune", prefix + " Detune",
                                                                 juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
                                                                 osc == 2 ? 7.0f : osc == 3 ? -5.0f : 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Pan", prefix + " Pan",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Width", prefix + " Width",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.5f)));
        params.push_back(makeParam(new juce::AudioParameterBool(prefix + "ToFilter", prefix + " To Filter", true)));
        params.push_back(makeParam(new juce::AudioParameterBool(prefix + "UnisonOn", prefix + " Unison On", false)));
        params.push_back(makeParam(new juce::AudioParameterInt(prefix + "UnisonVoices", prefix + " Unison Voices", 2, 7, 3)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Unison", prefix + " Unison Detune",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.22f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "WarpFM", prefix + " Warp FM",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "WarpFMSource", prefix + " Warp FM Source",
                                                                  getWarpFMSourceNames(),
                                                                  osc % 3)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Warp1Mode", prefix + " Warp 1 Mode",
                                                                  getWarpModeNames(),
                                                                  warpModeOff)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Warp1Amount", prefix + " Warp 1 Amount",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterChoice(prefix + "Warp2Mode", prefix + " Warp 2 Mode",
                                                                  getWarpModeNames(),
                                                                  warpModeOff)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "Warp2Amount", prefix + " Warp 2 Amount",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "WarpSync", prefix + " Warp Sync",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
        params.push_back(makeParam(new juce::AudioParameterFloat(prefix + "WarpBend", prefix + " Warp Bend",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
    }

    params.push_back(makeParam(new juce::AudioParameterBool("filterOn", "Filter On", true)));
    params.push_back(makeParam(new juce::AudioParameterChoice("filterMode", "Filter Mode", getFilterModeNames(), 0)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterCutoff", "Filter Cutoff",
                                                             juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.28f),
                                                             2200.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterResonance", "Filter Resonance",
                                                             juce::NormalisableRange<float>(0.2f, 18.0f, 0.001f, 0.35f),
                                                             0.75f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterDrive", "Filter Drive",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.12f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterEnvAmount", "Filter Env Amount",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.45f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterAttack", "Filter Attack",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.01f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterDecay", "Filter Decay",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.18f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterSustain", "Filter Sustain",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("filterRelease", "Filter Release",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.22f)));

    params.push_back(makeParam(new juce::AudioParameterFloat("ampAttack", "Amp Attack",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.01f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("ampDecay", "Amp Decay",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.22f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("ampSustain", "Amp Sustain",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.78f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("ampRelease", "Amp Release",
                                                             juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f),
                                                             0.35f)));

    params.push_back(makeParam(new juce::AudioParameterFloat("lfoRate", "LFO Rate",
                                                             juce::NormalisableRange<float>(0.05f, 20.0f, 0.001f, 0.35f),
                                                             2.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("lfoDepth", "LFO Depth",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));

    for (int macro = 1; macro <= reactormod::macroSourceCount; ++macro)
    {
        params.push_back(makeParam(new juce::AudioParameterFloat(reactormod::getMacroParamID(macro),
                                                                 "Macro " + juce::String(macro),
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.0f)));
    }

    for (int lfo = 2; lfo <= reactormod::lfoCount; ++lfo)
    {
        params.push_back(makeParam(new juce::AudioParameterFloat(reactormod::getLfoRateParamID(lfo),
                                                                 "LFO " + juce::String(lfo) + " Rate",
                                                                 juce::NormalisableRange<float>(0.05f, 20.0f, 0.001f, 0.35f),
                                                                 1.0f + (float) lfo * 0.35f)));
    }

    for (int lfo = 1; lfo <= reactormod::lfoCount; ++lfo)
    {
        const auto defaults = reactormod::defaultShapeForIndex(lfo);
        for (int point = 1; point <= reactormod::lfoPointCount; ++point)
        {
            params.push_back(makeParam(new juce::AudioParameterFloat(reactormod::getLfoPointParamID(lfo, point),
                                                                     "LFO " + juce::String(lfo) + " Point " + juce::String(point),
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                     defaults[(size_t) (point - 1)])));
        }
    }

    for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
    {
        params.push_back(makeParam(new juce::AudioParameterChoice(reactormod::getMatrixSourceParamID(slot),
                                                                  "Mod Slot " + juce::String(slot) + " Source",
                                                                  reactormod::getSourceNames(),
                                                                  0)));
        params.push_back(makeParam(new juce::AudioParameterChoice(reactormod::getMatrixDestinationParamID(slot),
                                                                  "Mod Slot " + juce::String(slot) + " Destination",
                                                                  reactormod::getDestinationNames(),
                                                                  0)));
        params.push_back(makeParam(new juce::AudioParameterFloat(reactormod::getMatrixAmountParamID(slot),
                                                                 "Mod Slot " + juce::String(slot) + " Amount",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.0f)));
    }

    params.push_back(makeParam(new juce::AudioParameterChoice("voiceMode", "Voice Mode", juce::StringArray{"Poly", "Mono"}, 0)));
    params.push_back(makeParam(new juce::AudioParameterBool("voiceLegato", "Legato", true)));
    params.push_back(makeParam(new juce::AudioParameterBool("voicePortamento", "Portamento", true)));
    params.push_back(makeParam(new juce::AudioParameterFloat("voiceGlide", "Glide",
                                                             juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.35f),
                                                             0.08f)));

    params.push_back(makeParam(new juce::AudioParameterFloat("pitchBendUp", "Pitch Bend Up",
                                                             juce::NormalisableRange<float>(0.0f, 24.0f, 1.0f),
                                                             7.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("pitchBendDown", "Pitch Bend Down",
                                                             juce::NormalisableRange<float>(0.0f, 24.0f, 1.0f),
                                                             2.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("modWheelCutoff", "Mod Wheel Cutoff",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("modWheelVibrato", "Mod Wheel Vibrato",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));

    params.push_back(makeParam(new juce::AudioParameterFloat("warpSaturator", "Warp Saturator",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));
    params.push_back(makeParam(new juce::AudioParameterBool("warpSaturatorPostFilter", "Warp Saturator Post Filter", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("warpMutate", "Warp Mutate",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));

    params.push_back(makeParam(new juce::AudioParameterFloat("noiseAmount", "Noise",
                                                             juce::NormalisableRange<float>(0.0f, 0.4f, 0.001f),
                                                             0.02f)));
    params.push_back(makeParam(new juce::AudioParameterChoice("noiseType", "Noise Type", getNoiseTypeNames(), 0)));
    params.push_back(makeParam(new juce::AudioParameterBool("noiseToFilter", "Noise To Filter", true)));
    params.push_back(makeParam(new juce::AudioParameterBool("noiseToAmpEnv", "Noise To Amp Envelope", true)));
    params.push_back(makeParam(new juce::AudioParameterChoice("subWave", "Sub Wave", getSubWaveNames(), 0)));
    params.push_back(makeParam(new juce::AudioParameterFloat("subLevel", "Sub Level",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.0f)));
    params.push_back(makeParam(new juce::AudioParameterChoice("subOctave", "Sub Octave", getOctaveNames(), 1)));
    params.push_back(makeParam(new juce::AudioParameterBool("subDirectOut", "Sub Direct Out", false)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxDriveOn", "FX Drive On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDriveAmount", "FX Drive Amount",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.28f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDriveTone", "FX Drive Tone",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.55f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDriveMix", "FX Drive Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.25f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxChorusOn", "FX Chorus On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxChorusRate", "FX Chorus Rate",
                                                             juce::NormalisableRange<float>(0.05f, 8.0f, 0.001f, 0.35f),
                                                             0.85f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxChorusDepth", "FX Chorus Depth",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.28f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxChorusMix", "FX Chorus Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.28f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxDelayOn", "FX Delay On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDelayTime", "FX Delay Time",
                                                             juce::NormalisableRange<float>(0.03f, 1.20f, 0.001f, 0.35f),
                                                             0.36f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDelayFeedback", "FX Delay Feedback",
                                                             juce::NormalisableRange<float>(0.0f, 0.92f, 0.001f),
                                                             0.36f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDelayMix", "FX Delay Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.24f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxReverbOn", "FX Reverb On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxReverbSize", "FX Reverb Size",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.34f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxReverbDamp", "FX Reverb Damp",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.42f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxReverbMix", "FX Reverb Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.22f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxFilterOn", "FX Filter On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFilterCutoff", "FX Filter Cutoff",
                                                             juce::NormalisableRange<float>(50.0f, 20000.0f, 0.01f, 0.32f),
                                                             6800.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFilterResonance", "FX Filter Resonance",
                                                             juce::NormalisableRange<float>(0.35f, 10.0f, 0.001f, 0.28f),
                                                             1.20f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFilterMix", "FX Filter Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.26f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxPhaserOn", "FX Phaser On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxPhaserRate", "FX Phaser Rate",
                                                             juce::NormalisableRange<float>(0.05f, 6.0f, 0.001f, 0.35f),
                                                             0.42f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxPhaserDepth", "FX Phaser Depth",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.48f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxPhaserMix", "FX Phaser Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.26f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxFlangerOn", "FX Flanger On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFlangerRate", "FX Flanger Rate",
                                                             juce::NormalisableRange<float>(0.05f, 5.0f, 0.001f, 0.35f),
                                                             0.28f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFlangerDepth", "FX Flanger Depth",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.36f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxFlangerMix", "FX Flanger Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.24f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxCompressorOn", "FX Compressor On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxCompressorAmount", "FX Compressor Amount",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.46f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxCompressorPunch", "FX Compressor Punch",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.40f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxCompressorMix", "FX Compressor Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.36f)));

    params.push_back(makeParam(new juce::AudioParameterBool("fxDimensionOn", "FX Dimension On", false)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDimensionSize", "FX Dimension Size",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.38f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDimensionWidth", "FX Dimension Width",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.52f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("fxDimensionMix", "FX Dimension Mix",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             0.30f)));

    for (int slot = 0; slot < PluginProcessor::maxFXSlots; ++slot)
    {
        params.push_back(makeParam(new juce::AudioParameterChoice(getFXOrderParamID(slot),
                                                                  "FX Order " + juce::String(slot + 1),
                                                                  getFXModuleNames(),
                                                                  fxOff)));

        params.push_back(makeParam(new juce::AudioParameterBool(getFXSlotOnParamID(slot),
                                                                "FX Slot " + juce::String(slot + 1) + " On",
                                                                false)));
        params.push_back(makeParam(new juce::AudioParameterBool(getFXSlotParallelParamID(slot),
                                                                "FX Slot " + juce::String(slot + 1) + " Parallel",
                                                                false)));
        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        {
            params.push_back(makeParam(new juce::AudioParameterFloat(getFXSlotFloatParamID(slot, parameterIndex),
                                                                     "FX Slot " + juce::String(slot + 1) + " Param " + juce::String(parameterIndex + 1),
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                     getFXSlotDefaultValue(parameterIndex))));
        }
        params.push_back(makeParam(new juce::AudioParameterInt(getFXSlotVariantParamID(slot),
                                                               "FX Slot " + juce::String(slot + 1) + " Variant",
                                                               0,
                                                               fxEQVariantLimit - 1,
                                                               0)));
    }

    params.push_back(makeParam(new juce::AudioParameterFloat("masterSpread", "Width",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             1.0f)));
    params.push_back(makeParam(new juce::AudioParameterFloat("masterGain", "Gain",
                                                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                             1.0f)));

    return { params.begin(), params.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    if (! apvts.state.hasProperty(themeModePropertyID))
        apvts.state.setProperty(themeModePropertyID, themeModeDark, nullptr);

    applyStoredThemeMode();
    audioFormatManager.registerBasicFormats();
    refreshUserPresets();
    loadBasicInitPatch();
    updateVoiceConfiguration();
    synth.addSound(new SynthSound());
}

PluginProcessor::~PluginProcessor() = default;

const juce::StringArray& PluginProcessor::getWaveNames()
{
    static const juce::StringArray names{ "Sine", "Saw", "Square", "Triangle" };
    return names;
}

const juce::StringArray& PluginProcessor::getOscillatorTypeNames()
{
    static const juce::StringArray names{ "Waveform", "Sampler", "Wavetable" };
    return names;
}

const juce::StringArray& PluginProcessor::getWavetableNames()
{
    static const auto names = wavetable::getTableNames();
    return names;
}

const juce::StringArray& PluginProcessor::getOctaveNames()
{
    static const juce::StringArray names{ "-2", "-1", "0", "+1", "+2" };
    return names;
}

const juce::StringArray& PluginProcessor::getSubWaveNames()
{
    return getWaveNames();
}

const juce::StringArray& PluginProcessor::getFilterModeNames()
{
    static const juce::StringArray names{
        "MG Low 12",
        "MG Low 24",
        "MG High 12",
        "MG High 24",
        "Band 12",
        "Band 24",
        "Notch",
        "Peak",
        "Low Shelf",
        "High Shelf",
        "All Pass",
        "Comb +",
        "Comb -",
        "Formant",
        "Flange +",
        "Flange -",
        "Vowel A",
        "Vowel E",
        "Vowel I",
        "Vowel O",
        "Vowel U",
        "Tilt",
        "Resonator",
        "Ladder Drive",
        "Metal Comb",
        "Phase Warp"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFactoryPresetNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray items;
        for (const auto& preset : getFactoryPresetLibrary())
            items.add(preset.name);
        return items;
    }();
    return names;
}

const juce::Array<PluginProcessor::FactoryPresetInfo>& PluginProcessor::getFactoryPresetLibrary()
{
    return factorypresetbank::getPresetLibrary();
}

const juce::StringArray& PluginProcessor::getWarpFMSourceNames()
{
    static const juce::StringArray names{
        "Osc 1",
        "Osc 2",
        "Osc 3",
        "Noise",
        "Sub Harmonic",
        "LFO Whine",
        "Mutant Warp"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getWarpModeNames()
{
    static const juce::StringArray names{
        "Off",
        "FM",
        "Sync",
        "Bend +",
        "Bend -",
        "Phase +",
        "Phase -",
        "Mirror",
        "Wrap",
        "Pinch"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getNoiseTypeNames()
{
    static const juce::StringArray names{
        "White",
        "Pink",
        "Brown",
        "Digital",
        "Dust",
        "Radio",
        "Space Vent",
        "Blue",
        "Violet",
        "Crackle",
        "Wind Tunnel",
        "Arc Plasma"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXModuleNames()
{
    static const juce::StringArray names{
        "Off",
        "Distortion",
        "Filter",
        "Chorus",
        "Flanger",
        "Phaser",
        "Delay",
        "Reverb",
        "Compressor",
        "Dimension",
        "EQ",
        "Multiband Comp",
        "Bitcrusher",
        "Imager",
        "Pitch / Shift",
        "Tremolo",
        "Gate"
    };
    return names;
}

bool PluginProcessor::isSupportedFXModuleType(int moduleType)
{
    return moduleType == fxOff || moduleType == fxDrive || moduleType == fxFilter || moduleType == fxChorus || moduleType == fxFlanger || moduleType == fxPhaser || moduleType == fxDelay || moduleType == fxReverb || moduleType == fxCompressor || moduleType == fxEQ || moduleType == fxMultiband || moduleType == fxBitcrusher || moduleType == fxImager || moduleType == fxShift || moduleType == fxTremolo || moduleType == fxGate;
}

const juce::StringArray& PluginProcessor::getFXFilterTypeNames()
{
    static const juce::StringArray names{
        "Low 12",
        "Low 24",
        "High 12",
        "High 24",
        "Band 12",
        "Band 24",
        "Notch",
        "Peak",
        "Low Shelf",
        "High Shelf",
        "All Pass",
        "Comb +",
        "Comb -",
        "Formant",
        "Vowel A",
        "Vowel E",
        "Vowel I",
        "Vowel O",
        "Vowel U",
        "Ladder Drive"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXDriveTypeNames()
{
    static const juce::StringArray names{
        "Soft Clip",
        "Hard Clip",
        "Tube",
        "Foldback",
        "Fuzz",
        "Diode",
        "Sine Drive",
        "Speaker Crush"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXEQTypeNames()
{
    static const juce::StringArray names{
        "Bell",
        "Low Shelf",
        "High Shelf",
        "Low Cut",
        "High Cut"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXReverbTypeNames()
{
    static const juce::StringArray names{
        "Hall",
        "Plate",
        "Room",
        "Shimmer",
        "Lo-Fi"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXDelayTypeNames()
{
    static const juce::StringArray names{
        "Stereo",
        "Ping Pong",
        "Tape",
        "Diffused"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXCompressorTypeNames()
{
    static const juce::StringArray names{
        "4:1",
        "8:1",
        "12:1",
        "20:1",
        "All"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXBitcrusherTypeNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& mode : getFXBitcrusherModes())
            result.add(mode.name);
        return result;
    }();

    return names;
}

const juce::StringArray& PluginProcessor::getFXShiftTypeNames()
{
    static const juce::StringArray names{
        "Microshift",
        "Dual Detune",
        "Ring Mod",
        "Freq Shift"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXGateTypeNames()
{
    static const juce::StringArray names{
        "Gate",
        "Rhythmic Gate"
    };
    return names;
}

juce::String PluginProcessor::getOscillatorSamplePropertyID(int oscillatorIndex)
{
    return "osc" + juce::String(oscillatorIndex) + "SampleFile";
}

bool PluginProcessor::loadOscillatorSample(int oscillatorIndex, const juce::File& file)
{
    if (!juce::isPositiveAndBelow(oscillatorIndex - 1, 3) || !file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    auto data = new OscillatorSampleData();
    const int numChannels = juce::jlimit(1, 2, (int) reader->numChannels);
    const int numSamples = juce::jmax(1, (int) reader->lengthInSamples);
    data->buffer.setSize(numChannels, numSamples);
    reader->read(&data->buffer, 0, numSamples, 0, true, true);
    data->sourceSampleRate = reader->sampleRate;

    int detectedRootNote = 60;
    if (detectRootMidiNote(data->buffer, data->sourceSampleRate, detectedRootNote))
        setParameterPlain(apvts, "osc" + juce::String(oscillatorIndex) + "SampleRoot", (float) detectedRootNote);

    {
        const juce::SpinLock::ScopedLockType lock(oscillatorSampleLock);
        oscillatorSamples[(size_t) (oscillatorIndex - 1)] = data;
        oscillatorSamplePaths[(size_t) (oscillatorIndex - 1)] = file.getFullPathName();
    }

    apvts.state.setProperty(getOscillatorSamplePropertyID(oscillatorIndex), file.getFullPathName(), nullptr);
    return true;
}

void PluginProcessor::clearOscillatorSample(int oscillatorIndex)
{
    if (!juce::isPositiveAndBelow(oscillatorIndex - 1, 3))
        return;

    {
        const juce::SpinLock::ScopedLockType lock(oscillatorSampleLock);
        oscillatorSamples[(size_t) (oscillatorIndex - 1)] = nullptr;
        oscillatorSamplePaths[(size_t) (oscillatorIndex - 1)].clear();
    }

    apvts.state.removeProperty(getOscillatorSamplePropertyID(oscillatorIndex), nullptr);
}

void PluginProcessor::clearAllOscillatorSamples()
{
    for (int osc = 1; osc <= 3; ++osc)
        clearOscillatorSample(osc);
}

juce::String PluginProcessor::getOscillatorSamplePath(int oscillatorIndex) const
{
    if (!juce::isPositiveAndBelow(oscillatorIndex - 1, 3))
        return {};

    const juce::SpinLock::ScopedLockType lock(oscillatorSampleLock);
    return oscillatorSamplePaths[(size_t) (oscillatorIndex - 1)];
}

juce::String PluginProcessor::getOscillatorSampleDisplayName(int oscillatorIndex) const
{
    const auto path = getOscillatorSamplePath(oscillatorIndex);
    if (path.isEmpty())
        return "No sample loaded";

    const auto file = juce::File(path);
    if (getOscillatorSampleData(oscillatorIndex) != nullptr)
        return file.getFileName();

    return "Missing: " + file.getFileName();
}

PluginProcessor::OscillatorSampleData::Ptr PluginProcessor::getOscillatorSampleData(int oscillatorIndex) const
{
    if (!juce::isPositiveAndBelow(oscillatorIndex - 1, 3))
        return nullptr;

    const juce::SpinLock::ScopedLockType lock(oscillatorSampleLock);
    return oscillatorSamples[(size_t) (oscillatorIndex - 1)];
}

void PluginProcessor::restoreOscillatorSampleState()
{
    for (int osc = 1; osc <= 3; ++osc)
    {
        const auto propertyId = getOscillatorSamplePropertyID(osc);
        const auto path = apvts.state.getProperty(propertyId).toString();

        OscillatorSampleData::Ptr data;
        if (path.isNotEmpty())
        {
            const auto file = juce::File(path);
            if (file.existsAsFile())
            {
                std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager.createReaderFor(file));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    data = new OscillatorSampleData();
                    const int numChannels = juce::jlimit(1, 2, (int) reader->numChannels);
                    const int numSamples = juce::jmax(1, (int) reader->lengthInSamples);
                    data->buffer.setSize(numChannels, numSamples);
                    reader->read(&data->buffer, 0, numSamples, 0, true, true);
                    data->sourceSampleRate = reader->sampleRate;
                }
            }
        }

        const juce::SpinLock::ScopedLockType lock(oscillatorSampleLock);
        oscillatorSamples[(size_t) (osc - 1)] = data;
        oscillatorSamplePaths[(size_t) (osc - 1)] = path;
    }
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    updateVoiceConfiguration();
    synth.setCurrentPlaybackSampleRate(sampleRate);
    prepareEffectProcessors(sampleRate, samplesPerBlock);
    modulationHeldNotes.fill(false);
    modulationHeldNoteCount = 0;
    modulationDisplayOneShotFinished.fill(false);
    for (auto& phase : modulationDisplayPhases)
        phase.store(0.0f);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.inputBuses.isEmpty();
}

void PluginProcessor::updateDisplayedLfoState(const juce::MidiBuffer& midiMessages, int numSamples)
{
    if (numSamples <= 0 || currentSampleRate <= 0.0)
        return;

    const auto snapshot = getModulationSnapshot();
    const int lfoCount = juce::jmin((int) snapshot.lfos.size(), (int) modulationDisplayPhases.size());
    bool sawTrigger = false;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            const int note = juce::jlimit(0, 127, message.getNoteNumber());
            if (! modulationHeldNotes[(size_t) note])
            {
                modulationHeldNotes[(size_t) note] = true;
                ++modulationHeldNoteCount;
            }
            sawTrigger = true;
        }
        else if (message.isNoteOff())
        {
            const int note = juce::jlimit(0, 127, message.getNoteNumber());
            if (modulationHeldNotes[(size_t) note])
            {
                modulationHeldNotes[(size_t) note] = false;
                modulationHeldNoteCount = juce::jmax(0, modulationHeldNoteCount - 1);
            }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            modulationHeldNotes.fill(false);
            modulationHeldNoteCount = 0;
        }
    }

    if (sawTrigger)
    {
        for (int i = 0; i < lfoCount; ++i)
        {
            if (snapshot.lfos[(size_t) i].triggerMode != reactormod::TriggerMode::free)
            {
                modulationDisplayPhases[(size_t) i].store(0.0f);
                modulationDisplayOneShotFinished[(size_t) i] = false;
            }
        }
    }

    for (int i = 0; i < lfoCount; ++i)
    {
        const auto& lfo = snapshot.lfos[(size_t) i];
        float phase = juce::jlimit(0.0f, 1.0f, modulationDisplayPhases[(size_t) i].load());

        bool shouldAdvance = false;
        switch (lfo.triggerMode)
        {
            case reactormod::TriggerMode::free:
                shouldAdvance = true;
                modulationDisplayOneShotFinished[(size_t) i] = false;
                break;

            case reactormod::TriggerMode::retrigger:
                shouldAdvance = modulationHeldNoteCount > 0;
                modulationDisplayOneShotFinished[(size_t) i] = false;
                break;

            case reactormod::TriggerMode::oneShot:
                shouldAdvance = ! modulationDisplayOneShotFinished[(size_t) i]
                    && (modulationHeldNoteCount > 0 || phase > 0.0f);
                break;
        }

        if (! shouldAdvance)
            continue;

        phase += reactormod::computeLfoRateHz(lfo, currentHostBpm) * (float) numSamples / (float) currentSampleRate;

        if (lfo.triggerMode == reactormod::TriggerMode::oneShot)
        {
            if (phase >= 1.0f)
            {
                phase = 1.0f;
                modulationDisplayOneShotFinished[(size_t) i] = true;
            }
        }
        else if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
        }

        modulationDisplayPhases[(size_t) i].store(phase);
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                currentHostBpm = *bpm;
        }
    }

    updateVoiceConfiguration();
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    updateDisplayedLfoState(midiMessages, buffer.getNumSamples());
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
    applyEffects(buffer);
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

    if (! apvts.state.hasProperty(themeModePropertyID))
        apvts.state.setProperty(themeModePropertyID, themeModeDark, nullptr);

    applyStoredThemeMode();
    restoreOscillatorSampleState();
    initialiseModulationState(false);
    migrateLegacyWarpModes(false);
    sanitiseFXChain();
    resetEffectState();
    updateVoiceConfiguration();
    refreshUserPresets();
    setCurrentPreset("Project State", false);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

bool PluginProcessor::isLightThemeEnabled() const
{
    return apvts.state.getProperty(themeModePropertyID, themeModeDark).toString() == themeModeLight;
}

void PluginProcessor::setLightThemeEnabled(bool enabled)
{
    apvts.state.setProperty(themeModePropertyID, enabled ? themeModeLight : themeModeDark, nullptr);
    applyStoredThemeMode();
}

void PluginProcessor::randomizeAllParameters()
{
    juce::Random random((juce::int64) juce::Time::currentTimeMillis());

    for (auto* parameter : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        auto* parameterWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        if (ranged == nullptr || parameterWithID == nullptr)
            continue;

        if (ranged->isMetaParameter() || ! shouldRandomizeParameterID(parameterWithID->paramID))
            continue;

        float normalizedValue = random.nextFloat();
        if (ranged->isBoolean())
        {
            normalizedValue = random.nextBool() ? 1.0f : 0.0f;
        }
        else if (ranged->isDiscrete())
        {
            const int steps = juce::jmax(2, ranged->getNumSteps());
            normalizedValue = (float) random.nextInt(steps) / (float) (steps - 1);
        }

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(normalizedValue);
        ranged->endChangeGesture();
    }

    const int lfoCount = getModulationLfoCount();
    for (int i = 0; i < lfoCount; ++i)
        updateModulationLfo(i, makeRandomizedLfoData(i, random));
}

void PluginProcessor::applyStoredThemeMode()
{
    reactorui::setThemeMode(isLightThemeEnabled() ? reactorui::ThemeMode::light
                                                  : reactorui::ThemeMode::dark);
}

void PluginProcessor::migrateLegacyWarpModes(bool forceFromLegacy)
{
    for (int osc = 1; osc <= 3; ++osc)
        applyDualWarpFromLegacy(apvts, osc, forceFromLegacy);
}

void PluginProcessor::initialiseModulationState(bool forceLegacyMatrix)
{
    const bool hasCustomState = apvts.state.getChildWithName(modulationStateType).isValid();

    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        modulationLfos.clear();
        matrixSourceIndices.fill(0);
        matrixSlotEnableds.fill(false);
    }

    if (! forceLegacyMatrix && hasCustomState)
    {
        auto lfoBank = getOrCreateModulationLfoBankTree();
        {
            const juce::SpinLock::ScopedLockType lock(modulationStateLock);
            for (int i = 0; i < lfoBank.getNumChildren(); ++i)
            {
                auto lfoNode = lfoBank.getChild(i);
                reactormod::DynamicLfoData data;
                const auto fallback = reactormod::makeDefaultLfo(i + 1);
                data.name = lfoNode.getProperty(lfoNameProperty, fallback.name).toString();
                data.rateHz = (float) lfoNode.getProperty(lfoRateProperty, fallback.rateHz);
                data.syncEnabled = (bool) lfoNode.getProperty(lfoSyncProperty, fallback.syncEnabled);
                data.dotted = (bool) lfoNode.getProperty(lfoDottedProperty, fallback.dotted);
                data.syncDivisionIndex = juce::jlimit(0, reactormod::getSyncDivisionNames().size() - 1,
                                                      (int) lfoNode.getProperty(lfoSyncDivisionProperty, fallback.syncDivisionIndex));
                data.type = (reactormod::LfoType) juce::jlimit(0,
                                                               reactormod::getLfoTypeNames().size() - 1,
                                                               (int) lfoNode.getProperty(lfoTypeProperty, (int) fallback.type));
                data.triggerMode = (reactormod::TriggerMode) juce::jlimit(0,
                                                                          reactormod::getTriggerModeNames().size() - 1,
                                                                          (int) lfoNode.getProperty(lfoTriggerProperty, (int) fallback.triggerMode));
                data.nodes.clear();
                for (int pointIndex = 0; pointIndex < lfoNode.getNumChildren(); ++pointIndex)
                {
                    auto pointNode = lfoNode.getChild(pointIndex);
                    if (! pointNode.hasType(lfoPointNodeType))
                        continue;

                    reactormod::LfoNode node;
                    node.x = juce::jlimit(0.0f, 1.0f, (float) pointNode.getProperty(lfoXProperty, 0.0f));
                    node.y = juce::jlimit(0.0f, 1.0f, (float) pointNode.getProperty(lfoYProperty, 0.5f));
                    node.curveToNext = juce::jlimit(-0.95f, 0.95f, (float) pointNode.getProperty(lfoCurveProperty, 0.0f));
                    data.nodes.push_back(node);
                }

                if (data.nodes.size() < 2)
                    data = fallback;

                modulationLfos.push_back(data);
            }
        }

        auto matrixTree = getOrCreateModMatrixTree();
        {
            const juce::SpinLock::ScopedLockType lock(modulationStateLock);
            for (int i = 0; i < matrixTree.getNumChildren(); ++i)
            {
                auto slotNode = matrixTree.getChild(i);
                const int slotIndex = juce::jlimit(1, reactormod::matrixSlotCount,
                                                   (int) slotNode.getProperty(matrixSlotIndexProperty, i + 1));
                matrixSourceIndices[(size_t) (slotIndex - 1)] =
                    juce::jlimit(0, reactormod::maxModulationSourceCount, (int) slotNode.getProperty(matrixSourceProperty, 0));
                matrixSlotEnableds[(size_t) (slotIndex - 1)] =
                    (bool) slotNode.getProperty(matrixEnabledProperty, matrixSourceIndices[(size_t) (slotIndex - 1)] > 0);
            }
        }
    }

    bool needsLegacyPopulate = forceLegacyMatrix;
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        needsLegacyPopulate = needsLegacyPopulate || modulationLfos.empty();
    }

    if (needsLegacyPopulate)
        {
            const juce::SpinLock::ScopedLockType lock(modulationStateLock);
            modulationLfos.clear();
            for (int lfo = 1; lfo <= reactormod::lfoCount; ++lfo)
            {
                auto data = reactormod::makeDefaultLfo(lfo);
                data.rateHz = readParameterPlain(apvts, reactormod::getLfoRateParamID(lfo), data.rateHz);
                modulationLfos.push_back(data);
            }

        matrixSourceIndices.fill(0);
        matrixSlotEnableds.fill(false);
        for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
        {
            setMatrixSlot(apvts, slot, 0, reactormod::Destination::none, 0.0f);
            matrixSourceIndices[(size_t) (slot - 1)] =
                juce::jlimit(0,
                             reactormod::maxModulationSourceCount,
                             juce::roundToInt(readParameterPlain(apvts, reactormod::getMatrixSourceParamID(slot), 0.0f)));
            matrixSlotEnableds[(size_t) (slot - 1)] = false;
        }

        const float legacyAmount = readParameterPlain(apvts, "lfoDepth", 0.0f);
        matrixSourceIndices[0] = juce::jmax(1, matrixSourceIndices[0]);
        matrixSlotEnableds[0] = true;
        setMatrixSlot(apvts, 1, 0, reactormod::Destination::filterCutoff, legacyAmount);
    }

    for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
    {
        if (! stateHasProperty(apvts.state, reactormod::getMatrixDestinationParamID(slot)))
            setParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slot), 0.0f);
        if (! stateHasProperty(apvts.state, reactormod::getMatrixAmountParamID(slot)))
            setParameterPlain(apvts, reactormod::getMatrixAmountParamID(slot), 0.0f);
    }

    writeModulationStateToTree();
}

juce::ValueTree PluginProcessor::getOrCreateModulationStateTree()
{
    auto tree = apvts.state.getChildWithName(modulationStateType);
    if (! tree.isValid())
    {
        tree = juce::ValueTree(modulationStateType);
        apvts.state.appendChild(tree, nullptr);
    }
    return tree;
}

juce::ValueTree PluginProcessor::getOrCreateModulationLfoBankTree()
{
    auto stateTree = getOrCreateModulationStateTree();
    auto bank = stateTree.getChildWithName(lfoBankType);
    if (! bank.isValid())
    {
        bank = juce::ValueTree(lfoBankType);
        stateTree.appendChild(bank, nullptr);
    }
    return bank;
}

juce::ValueTree PluginProcessor::getOrCreateModMatrixTree()
{
    auto stateTree = getOrCreateModulationStateTree();
    auto matrixTree = stateTree.getChildWithName(matrixType);
    if (! matrixTree.isValid())
    {
        matrixTree = juce::ValueTree(matrixType);
        stateTree.appendChild(matrixTree, nullptr);
    }
    return matrixTree;
}

void PluginProcessor::writeModulationStateToTree()
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);

    auto lfoBank = getOrCreateModulationLfoBankTree();
    while (lfoBank.getNumChildren() > 0)
        lfoBank.removeChild(0, nullptr);

    for (int i = 0; i < (int) modulationLfos.size(); ++i)
    {
        const auto& data = modulationLfos[(size_t) i];
        juce::ValueTree lfoNode(lfoNodeType);
        lfoNode.setProperty(lfoNameProperty, data.name, nullptr);
        lfoNode.setProperty(lfoRateProperty, data.rateHz, nullptr);
        lfoNode.setProperty(lfoSyncProperty, data.syncEnabled, nullptr);
        lfoNode.setProperty(lfoDottedProperty, data.dotted, nullptr);
        lfoNode.setProperty(lfoSyncDivisionProperty, data.syncDivisionIndex, nullptr);
        lfoNode.setProperty(lfoTypeProperty, (int) data.type, nullptr);
        lfoNode.setProperty(lfoTriggerProperty, (int) data.triggerMode, nullptr);

        for (const auto& point : data.nodes)
        {
            juce::ValueTree pointNode(lfoPointNodeType);
            pointNode.setProperty(lfoXProperty, point.x, nullptr);
            pointNode.setProperty(lfoYProperty, point.y, nullptr);
            pointNode.setProperty(lfoCurveProperty, point.curveToNext, nullptr);
            lfoNode.appendChild(pointNode, nullptr);
        }

        lfoBank.appendChild(lfoNode, nullptr);
    }

    auto matrixTree = getOrCreateModMatrixTree();
    while (matrixTree.getNumChildren() > 0)
        matrixTree.removeChild(0, nullptr);

    for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
    {
        juce::ValueTree slotNode(matrixSlotType);
        slotNode.setProperty(matrixSlotIndexProperty, slot, nullptr);
        slotNode.setProperty(matrixSourceProperty, matrixSourceIndices[(size_t) (slot - 1)], nullptr);
        slotNode.setProperty(matrixEnabledProperty, matrixSlotEnableds[(size_t) (slot - 1)], nullptr);
        matrixTree.appendChild(slotNode, nullptr);
    }
}

PluginProcessor::ModulationSnapshot PluginProcessor::getModulationSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    ModulationSnapshot snapshot;
    snapshot.lfos = modulationLfos;
    for (int macro = 1; macro <= reactormod::macroSourceCount; ++macro)
        snapshot.macroValues[(size_t) (macro - 1)] = readParameterPlain(apvts, reactormod::getMacroParamID(macro), 0.0f);
    snapshot.matrixSources = matrixSourceIndices;
    snapshot.matrixEnabled = matrixSlotEnableds;
    return snapshot;
}

int PluginProcessor::getModulationLfoCount() const
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    return (int) modulationLfos.size();
}

reactormod::DynamicLfoData PluginProcessor::getModulationLfo(int index) const
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    if (! juce::isPositiveAndBelow(index, modulationLfos.size()))
        return reactormod::makeDefaultLfo(1);

    return modulationLfos[(size_t) index];
}

std::vector<PluginProcessor::ModulationSourceInfo> PluginProcessor::getAvailableModulationSources() const
{
    std::vector<ModulationSourceInfo> sources;

    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    const size_t lfoSourceCount = juce::jmin(modulationLfos.size(), (size_t) reactormod::maxLfoSourceCount);
    sources.reserve(lfoSourceCount + (size_t) reactormod::macroSourceCount);

    for (size_t i = 0; i < lfoSourceCount; ++i)
        sources.push_back({ reactormod::sourceIndexForLfo((int) i + 1), modulationLfos[i].name });

    for (int macro = 1; macro <= reactormod::macroSourceCount; ++macro)
        sources.push_back({ reactormod::sourceIndexForMacro(macro), "MOD " + juce::String(macro) });

    return sources;
}

juce::StringArray PluginProcessor::getModulationLfoNames() const
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    juce::StringArray names;
    names.add("Off");
    for (const auto& lfo : modulationLfos)
        names.add(lfo.name);
    return names;
}

float PluginProcessor::getModulationLfoDisplayPhase(int index) const
{
    if (! juce::isPositiveAndBelow(index, (int) modulationDisplayPhases.size()))
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, modulationDisplayPhases[(size_t) index].load());
}

int PluginProcessor::addModulationLfo()
{
    int newIndex = 0;
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        newIndex = (int) modulationLfos.size() + 1;
        auto data = reactormod::makeDefaultLfo(newIndex);
        data.name = "LFO " + juce::String(newIndex);
        modulationLfos.push_back(data);
    }
    writeModulationStateToTree();
    return newIndex - 1;
}

void PluginProcessor::updateModulationLfo(int index, const reactormod::DynamicLfoData& incomingData)
{
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        if (! juce::isPositiveAndBelow(index, modulationLfos.size()))
            return;

        auto data = incomingData;
        data.rateHz = juce::jlimit(0.05f, 20.0f, data.rateHz);
        data.syncDivisionIndex = juce::jlimit(0, reactormod::getSyncDivisionNames().size() - 1, data.syncDivisionIndex);
        data.type = (reactormod::LfoType) juce::jlimit(0, reactormod::getLfoTypeNames().size() - 1, (int) data.type);
        if (data.nodes.size() < 2)
            data.nodes = reactormod::makeDefaultLfo(index + 1).nodes;

        std::sort(data.nodes.begin(), data.nodes.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
        data.nodes.front().x = 0.0f;
        data.nodes.back().x = 1.0f;

        for (size_t nodeIndex = 0; nodeIndex < data.nodes.size(); ++nodeIndex)
        {
            auto& node = data.nodes[nodeIndex];
            node.y = juce::jlimit(0.0f, 1.0f, node.y);
            node.curveToNext = juce::jlimit(-0.95f, 0.95f, node.curveToNext);

            if (nodeIndex > 0 && nodeIndex + 1 < data.nodes.size())
                node.x = juce::jlimit(data.nodes[nodeIndex - 1].x + 0.001f, data.nodes[nodeIndex + 1].x - 0.001f, node.x);
        }

        modulationLfos[(size_t) index] = data;
    }
    writeModulationStateToTree();
}

int PluginProcessor::getMatrixSourceForSlot(int slotIndex) const
{
    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    if (! juce::isPositiveAndBelow(slotIndex, reactormod::matrixSlotCount))
        return 0;

    return matrixSourceIndices[(size_t) slotIndex];
}

void PluginProcessor::setMatrixSourceForSlot(int slotIndex, int sourceIndex)
{
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        if (! juce::isPositiveAndBelow(slotIndex, reactormod::matrixSlotCount))
            return;

        const int clampedSourceIndex = juce::jlimit(0, reactormod::maxModulationSourceCount, sourceIndex);
        matrixSourceIndices[(size_t) slotIndex] = clampedSourceIndex;
        matrixSlotEnableds[(size_t) slotIndex] = clampedSourceIndex > 0;
    }
    writeModulationStateToTree();
}

int PluginProcessor::getSelectedModulationSourceIndex() const noexcept
{
    return juce::jlimit(0, reactormod::maxModulationSourceCount, selectedModulationSourceIndex.load());
}

void PluginProcessor::setSelectedModulationSourceIndex(int sourceIndex) noexcept
{
    selectedModulationSourceIndex.store(juce::jlimit(0, reactormod::maxModulationSourceCount, sourceIndex));
}

PluginProcessor::DestinationModulationInfo PluginProcessor::getDestinationModulationInfo(reactormod::Destination destination,
                                                                                          int preferredSourceIndex) const
{
    DestinationModulationInfo info;
    if (destination == reactormod::Destination::none || destination == reactormod::Destination::count)
        return info;

    const int selectedSourceIndex = juce::jlimit(0,
                                                 reactormod::maxModulationSourceCount,
                                                 preferredSourceIndex > 0 ? preferredSourceIndex
                                                                          : getSelectedModulationSourceIndex());

    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
    {
        const auto slotDestination = reactormod::destinationFromChoiceIndex(
            juce::roundToInt(readParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slot + 1), 0.0f)));
        const float amount = readParameterPlain(apvts, reactormod::getMatrixAmountParamID(slot + 1), 0.0f);
        const int sourceIndex = matrixSourceIndices[(size_t) slot];
        const bool enabled = matrixSlotEnableds[(size_t) slot];

        if (slotDestination != destination || ! enabled || sourceIndex <= 0 || std::abs(amount) <= 0.0001f)
            continue;

        ++info.routeCount;
        if (selectedSourceIndex > 0 && sourceIndex == selectedSourceIndex)
        {
            info.sourceIndex = sourceIndex;
            info.amount = amount;
            info.slotIndex = slot;
            return info;
        }

        if (selectedSourceIndex <= 0 && (info.slotIndex < 0 || std::abs(amount) > std::abs(info.amount)))
        {
            info.sourceIndex = sourceIndex;
            info.amount = amount;
            info.slotIndex = slot;
        }
    }

    return info;
}

std::vector<PluginProcessor::ModulationRouteInfo> PluginProcessor::getRoutesForSource(int sourceIndex) const
{
    std::vector<ModulationRouteInfo> routes;
    if (sourceIndex <= 0)
        return routes;

    const int clampedSourceIndex = juce::jlimit(1, reactormod::maxModulationSourceCount, sourceIndex);

    const juce::SpinLock::ScopedLockType lock(modulationStateLock);
    for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
    {
        if (matrixSourceIndices[(size_t) slot] != clampedSourceIndex)
            continue;

        ModulationRouteInfo route;
        route.slotIndex = slot;
        route.sourceIndex = clampedSourceIndex;
        route.destination = reactormod::destinationFromChoiceIndex(
            juce::roundToInt(readParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slot + 1), 0.0f)));
        route.amount = readParameterPlain(apvts, reactormod::getMatrixAmountParamID(slot + 1), 0.0f);
        route.enabled = matrixSlotEnableds[(size_t) slot];
        route.destinationName = reactormod::getDestinationNames()[reactormod::destinationToChoiceIndex(route.destination)];
        route.parameterID = reactormod::getParameterIDForDestination(route.destination);

        if (route.destination != reactormod::Destination::none)
            routes.push_back(route);
    }

    return routes;
}

std::vector<PluginProcessor::ModulationRouteInfo> PluginProcessor::getRoutesForLfo(int lfoIndex) const
{
    return getRoutesForSource(reactormod::sourceIndexForLfo(lfoIndex + 1));
}

void PluginProcessor::setMatrixSlotEnabled(int slotIndex, bool enabled)
{
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        if (! juce::isPositiveAndBelow(slotIndex, reactormod::matrixSlotCount))
            return;

        matrixSlotEnableds[(size_t) slotIndex] = enabled && matrixSourceIndices[(size_t) slotIndex] > 0;
    }
    writeModulationStateToTree();
}

void PluginProcessor::setMatrixAmountForSlot(int slotIndex, float amount)
{
    if (! juce::isPositiveAndBelow(slotIndex, reactormod::matrixSlotCount))
        return;

    setParameterPlain(apvts, reactormod::getMatrixAmountParamID(slotIndex + 1), juce::jlimit(-1.0f, 1.0f, amount));
}

void PluginProcessor::removeMatrixSlot(int slotIndex)
{
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        if (! juce::isPositiveAndBelow(slotIndex, reactormod::matrixSlotCount))
            return;

        matrixSourceIndices[(size_t) slotIndex] = 0;
        matrixSlotEnableds[(size_t) slotIndex] = false;
    }

    setParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slotIndex + 1), 0.0f);
    setParameterPlain(apvts, reactormod::getMatrixAmountParamID(slotIndex + 1), 0.0f);
    writeModulationStateToTree();
}

int PluginProcessor::assignSourceToDestination(int sourceIndex, reactormod::Destination destination, float defaultAmount)
{
    if (destination == reactormod::Destination::none || destination == reactormod::Destination::count)
        return -1;

    const int clampedSourceIndex = juce::jlimit(1, reactormod::maxModulationSourceCount, sourceIndex);
    const float clampedDefaultAmount = juce::jlimit(-1.0f, 1.0f, defaultAmount);
    int chosenSlot = -1;
    float chosenAmount = clampedDefaultAmount;

    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        int firstSameDestination = -1;
        int firstEmptySlot = -1;

        for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
        {
            const auto slotDestination = reactormod::destinationFromChoiceIndex(
                juce::roundToInt(readParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slot + 1), 0.0f)));
            const float slotAmount = readParameterPlain(apvts, reactormod::getMatrixAmountParamID(slot + 1), 0.0f);
            const int slotSource = matrixSourceIndices[(size_t) slot];

            if (slotDestination == destination && slotSource == clampedSourceIndex)
            {
                chosenSlot = slot;
                chosenAmount = std::abs(slotAmount) > 0.0001f ? slotAmount : clampedDefaultAmount;
                matrixSlotEnableds[(size_t) slot] = true;
                break;
            }

            if (firstSameDestination < 0 && slotDestination == destination)
            {
                firstSameDestination = slot;
                if (std::abs(slotAmount) > 0.0001f)
                    chosenAmount = slotAmount;
            }

            if (firstEmptySlot < 0 && (slotDestination == reactormod::Destination::none || slotSource <= 0))
                firstEmptySlot = slot;
        }

        if (chosenSlot < 0)
        {
            // Allow multiple modulators per destination by preferring a free row
            // over recycling an existing route from a different source.
            if (firstEmptySlot >= 0)
                chosenSlot = firstEmptySlot;
            else if (firstSameDestination >= 0)
                chosenSlot = firstSameDestination;
            else
                chosenSlot = 0;
        }

        matrixSourceIndices[(size_t) chosenSlot] = clampedSourceIndex;
        matrixSlotEnableds[(size_t) chosenSlot] = true;
    }

    setParameterPlain(apvts, reactormod::getMatrixDestinationParamID(chosenSlot + 1),
                      (float) reactormod::destinationToChoiceIndex(destination));
    setParameterPlain(apvts, reactormod::getMatrixAmountParamID(chosenSlot + 1), chosenAmount);
    writeModulationStateToTree();
    return chosenSlot;
}

int PluginProcessor::assignLfoToDestination(int sourceIndex, reactormod::Destination destination, float defaultAmount)
{
    return assignSourceToDestination(sourceIndex, destination, defaultAmount);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

void PluginProcessor::updateVoiceConfiguration()
{
    const bool monoMode = apvts.getRawParameterValue("voiceMode")->load() >= 0.5f;
    if (synth.getNumVoices() > 0 && monoMode == lastMonoMode)
        return;

    synth.allNotesOff(0, false);
    synth.clearVoices();

    const int voiceCount = monoMode ? 1 : 12;
    for (int i = 0; i < voiceCount; ++i)
        synth.addVoice(new SynthVoice(*this));

    synth.setNoteStealingEnabled(monoMode);
    lastMonoMode = monoMode;
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("SSP")
        .getChildFile("Reactor Presets");
}

void PluginProcessor::refreshUserPresets()
{
    userPresetFiles.clearQuick();
    userPresetNames.clearQuick();

    auto directory = getUserPresetDirectory();
    directory.createDirectory();

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.sspreset");
    std::sort(files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });

    for (const auto& file : files)
    {
        userPresetFiles.add(file);
        userPresetNames.add(file.getFileNameWithoutExtension());
    }
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    const auto& presets = factorypresetbank::getPresetSeeds();
    if (! juce::isPositiveAndBelow(index, presets.size()))
        return false;

    if (index == 0)
    {
        loadBasicInitPatch();
        setCurrentPreset("Init Saw", true);
        return true;
    }

    clearAllOscillatorSamples();
    factorypresetloader::applyPreset(apvts, presets.getReference(index));
    initialiseModulationState(true);
    migrateLegacyWarpModes(true);
    sanitiseFXChain();
    resetEffectState();
    updateVoiceConfiguration();
    setCurrentPreset(presets.getReference(index).info.name, true);
    return true;
}

void PluginProcessor::loadBasicInitPatch()
{
    clearAllOscillatorSamples();
    setOscillator(apvts, 1, 1, 2, 1.0f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 2, 0, 2, 0.0f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 3, 0, 2, 0.0f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setParameterPlain(apvts, "osc1WarpFMSource", 1.0f);
    setParameterPlain(apvts, "osc2WarpFMSource", 2.0f);
    setParameterPlain(apvts, "osc3WarpFMSource", 0.0f);

    setFilterSection(apvts, false, filterLow12, 20000.0f, 0.75f, 0.0f, 0.0f, 0.001f, 0.001f, 1.0f, 0.20f);
    setAmpSection(apvts, 0.001f, 0.001f, 1.0f, 0.25f);
    setModSection(apvts, 2.0f, 0.0f, 0.0f, 0.0f);
    setVoiceSection(apvts, false, true, false, 0.08f, 7.0f, 2.0f);
    setWarpSection(apvts, 0.0f, false, 0.0f);
    setNoiseSection(apvts, 0.0f, 0, true, true);
    setParameterPlain(apvts, "subWave", 0.0f);
    setParameterPlain(apvts, "subLevel", 0.0f);
    setParameterPlain(apvts, "subOctave", 1.0f);
    setBoolParam(apvts, "subDirectOut", false);
    setParameterPlain(apvts, "masterSpread", 1.0f);
    setParameterPlain(apvts, "masterGain", 1.0f);

    initialiseModulationState(true);
    migrateLegacyWarpModes(true);
    resetLegacyFXParams(apvts);
    clearFXChain(apvts);
    resetEffectState();
    compactFXOrder();
    if (! apvts.state.hasProperty(themeModePropertyID))
        apvts.state.setProperty(themeModePropertyID, themeModeDark, nullptr);

    applyStoredThemeMode();
    updateVoiceConfiguration();
    setCurrentPreset("Init Saw", false);
}

bool PluginProcessor::loadUserPreset(int index)
{
    refreshUserPresets();

    if (! juce::isPositiveAndBelow(index, userPresetFiles.size()))
        return false;

    auto file = userPresetFiles.getReference(index);
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
        return false;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    if (! apvts.state.hasProperty(themeModePropertyID))
        apvts.state.setProperty(themeModePropertyID, themeModeDark, nullptr);

    applyStoredThemeMode();
    restoreOscillatorSampleState();
    initialiseModulationState(false);
    migrateLegacyWarpModes(false);
    sanitiseFXChain();
    resetEffectState();
    updateVoiceConfiguration();
    setCurrentPreset(userPresetNames[index], false);
    return true;
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName)
{
    auto cleanName = sanitisePresetName(presetName);
    if (cleanName.isEmpty())
        return false;

    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    auto file = directory.getChildFile(cleanName + ".sspreset");
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute("presetName", cleanName);
        xml->setAttribute("plugin", "SSP Reactor");
        if (! file.replaceWithText(xml->toString()))
            return false;
    }
    else
    {
        return false;
    }

    refreshUserPresets();
    setCurrentPreset(cleanName, false);
    return true;
}

void PluginProcessor::setCurrentPreset(juce::String presetName, bool isFactory)
{
    currentPresetName = std::move(presetName);
    currentPresetIsFactory = isFactory;
}

juce::Array<int> PluginProcessor::getFXOrder() const
{
    juce::Array<int> order;
    for (int slot = 0; slot < maxFXSlots; ++slot)
        order.add(getFXOrderSlot(slot));

    return order;
}

bool PluginProcessor::addFXModule(int typeIndex)
{
    const int clampedType = juce::jlimit(1, getFXModuleNames().size() - 1, typeIndex);
    if (! isSupportedFXModuleType(clampedType) || clampedType == fxOff)
        return false;

    const auto order = getFXOrder();
    const int slot = order.indexOf(fxOff);
    if (slot < 0)
        return false;

    setFXOrderSlot(slot, clampedType);
    initialiseFXSlotDefaults(slot, clampedType);
    return true;
}

void PluginProcessor::removeFXModuleFromOrder(int orderIndex)
{
    if (! juce::isPositiveAndBelow(orderIndex, maxFXSlots))
        return;

    clearFXSlotState(orderIndex);
    compactFXOrder();
}

void PluginProcessor::moveFXModule(int fromIndex, int toIndex)
{
    if (! juce::isPositiveAndBelow(fromIndex, maxFXSlots)
        || ! juce::isPositiveAndBelow(toIndex, maxFXSlots)
        || fromIndex == toIndex)
        return;

    if (getFXOrderSlot(fromIndex) == fxOff)
        return;

    struct SlotSnapshot
    {
        int type = fxOff;
        bool enabled = false;
        bool parallel = false;
        std::array<float, fxSlotParameterCount> parameters{};
        int variant = 0;
    };

    auto readSlot = [this](int slotIndex)
    {
        SlotSnapshot snapshot;
        snapshot.type = getFXOrderSlot(slotIndex);
        snapshot.enabled = getFXSlotOnValue(slotIndex);
        snapshot.parallel = getFXSlotParallelValue(slotIndex);
        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
            snapshot.parameters[(size_t) parameterIndex] = getFXSlotFloatValue(slotIndex, parameterIndex);
        snapshot.variant = getFXSlotVariant(slotIndex);
        return snapshot;
    };

    auto writeSlot = [this](int slotIndex, const SlotSnapshot& snapshot)
    {
        setFXOrderSlot(slotIndex, snapshot.type);
        setBoolParam(apvts, getFXSlotOnParamID(slotIndex), snapshot.enabled);
        setBoolParam(apvts, getFXSlotParallelParamID(slotIndex), snapshot.parallel);
        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
            setParameterPlain(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), snapshot.parameters[(size_t) parameterIndex]);
        setParameterPlain(apvts, getFXSlotVariantParamID(slotIndex), (float) snapshot.variant);
    };

    const auto moving = readSlot(fromIndex);
    if (fromIndex < toIndex)
    {
        for (int slot = fromIndex; slot < toIndex; ++slot)
            writeSlot(slot, readSlot(slot + 1));
    }
    else
    {
        for (int slot = fromIndex; slot > toIndex; --slot)
            writeSlot(slot, readSlot(slot - 1));
    }

    writeSlot(toIndex, moving);
}

int PluginProcessor::getFXSlotType(int slotIndex) const
{
    return getFXOrderSlot(slotIndex);
}

bool PluginProcessor::isFXSlotActive(int slotIndex) const
{
    return getFXSlotType(slotIndex) != fxOff;
}

bool PluginProcessor::isFXSlotParallel(int slotIndex) const
{
    return getFXSlotParallelValue(slotIndex);
}

int PluginProcessor::getFXSlotVariant(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return 0;

    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jmax(0, (int) std::round(raw->load()));

    return 0;
}

void PluginProcessor::setFXSlotVariant(int slotIndex, int variantIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setParameterPlain(apvts, getFXSlotVariantParamID(slotIndex), (float) juce::jmax(0, variantIndex));
}

float PluginProcessor::getFXCompressorGainReduction(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, compressorGainReductionMeters[(size_t) slotIndex].load());
}

void PluginProcessor::getFXMultibandMeter(int slotIndex, std::array<float, 3>& upward, std::array<float, 3>& downward) const
{
    upward.fill(0.0f);
    downward.fill(0.0f);

    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int band = 0; band < fxMultibandBandCount; ++band)
    {
        upward[(size_t) band] = juce::jlimit(0.0f, 1.0f, multibandUpwardMeters[(size_t) slotIndex][(size_t) band].load());
        downward[(size_t) band] = juce::jlimit(0.0f, 1.0f, multibandDownwardMeters[(size_t) slotIndex][(size_t) band].load());
    }
}

void PluginProcessor::getFXEQSpectrum(int slotIndex, std::array<float, fxEQSpectrumBinCount>& spectrum) const
{
    spectrum.fill(0.0f);

    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int i = 0; i < fxEQSpectrumBinCount; ++i)
        spectrum[(size_t) i] = juce::jlimit(0.0f, 1.0f, eqSpectrumBins[(size_t) slotIndex][(size_t) i].load());
}

void PluginProcessor::getFXImagerScope(int slotIndex,
                                       std::array<float, fxImagerScopePointCount>& xs,
                                       std::array<float, fxImagerScopePointCount>& ys) const
{
    xs.fill(0.0f);
    ys.fill(0.0f);

    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int i = 0; i < fxImagerScopePointCount; ++i)
    {
        xs[(size_t) i] = juce::jlimit(-1.0f, 1.0f, imagerScopeX[(size_t) slotIndex][(size_t) i].load());
        ys[(size_t) i] = juce::jlimit(-1.0f, 1.0f, imagerScopeY[(size_t) slotIndex][(size_t) i].load());
    }
}

int PluginProcessor::getFXOrderSlot(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return fxOff;

    if (auto* raw = apvts.getRawParameterValue(getFXOrderParamID(slotIndex)))
        return juce::jlimit(0, getFXModuleNames().size() - 1, (int) std::round(raw->load()));

    return fxOff;
}

void PluginProcessor::setFXOrderSlot(int slotIndex, int moduleTypeIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setParameterPlain(apvts, getFXOrderParamID(slotIndex),
                      (float) juce::jlimit(0, getFXModuleNames().size() - 1, moduleTypeIndex));
}

void PluginProcessor::compactFXOrder()
{
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < maxFXSlots; ++readIndex)
    {
        if (getFXOrderSlot(readIndex) == fxOff)
            continue;

        if (writeIndex != readIndex)
        {
            copyFXSlotState(readIndex, writeIndex);
            clearFXSlotState(readIndex);
        }

        ++writeIndex;
    }

    while (writeIndex < maxFXSlots)
        clearFXSlotState(writeIndex++);
}

void PluginProcessor::initialiseFXSlotDefaults(int slotIndex, int moduleTypeIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setBoolParam(apvts, getFXSlotOnParamID(slotIndex), true);
    setBoolParam(apvts, getFXSlotParallelParamID(slotIndex), false);
    int initialVariant = 0;

    std::array<float, fxSlotParameterCount> parameters{};
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        parameters[(size_t) parameterIndex] = getFXSlotDefaultValue(parameterIndex);

    switch (moduleTypeIndex)
    {
        case fxDrive:
            parameters[0] = 0.34f; parameters[1] = 0.56f; parameters[2] = 1.0f;
            break;
        case fxFilter:
            parameters[0] = 0.72f; parameters[1] = 0.22f; parameters[2] = 0.18f; parameters[3] = 0.40f; parameters[4] = 1.0f;
            break;
        case fxChorus:
            parameters[0] = 0.18f; parameters[1] = 0.46f; parameters[2] = 0.30f; parameters[3] = 0.50f; parameters[4] = 1.0f;
            break;
        case fxFlanger:
            parameters[0] = 0.22f; parameters[1] = 0.48f; parameters[2] = 0.22f; parameters[3] = 0.58f; parameters[4] = 1.0f;
            break;
        case fxPhaser:
            parameters[0] = 0.24f; parameters[1] = 0.52f; parameters[2] = 0.50f; parameters[3] = 0.48f; parameters[4] = 1.0f;
            break;
        case fxDelay:
            parameters[0] = 0.42f; parameters[1] = 0.42f; parameters[2] = 0.0f; parameters[3] = 1.0f; parameters[4] = 0.38f; parameters[5] = 1.0f; parameters[6] = 1.0f; parameters[7] = 1.0f;
            break;
        case fxReverb:
            parameters[0] = 0.34f; parameters[1] = 0.40f; parameters[2] = 0.82f; parameters[3] = 0.08f; parameters[4] = 1.0f;
            break;
        case fxCompressor:
            parameters[0] = 0.42f; parameters[1] = 0.76f; parameters[2] = 0.52f; parameters[3] = 0.46f; parameters[4] = 1.0f;
            break;
        case fxEQ:
            parameters[0] = 0.14f; parameters[1] = 0.50f;
            parameters[2] = 0.34f; parameters[3] = 0.50f;
            parameters[4] = 0.58f; parameters[5] = 0.50f;
            parameters[6] = 0.82f; parameters[7] = 0.50f;
            parameters[8] = 0.22f; parameters[9] = 0.22f; parameters[10] = 0.22f; parameters[11] = 0.22f;
            break;
        case fxMultiband:
            parameters[0] = 0.42f; parameters[1] = 0.30f; parameters[2] = 0.22f;
            parameters[3] = 0.58f; parameters[4] = 0.44f; parameters[5] = 0.30f;
            parameters[6] = 0.72f; parameters[7] = 0.34f; parameters[8] = 0.50f; parameters[9] = 1.0f;
            break;
        case fxBitcrusher:
            parameters[0] = 0.52f; parameters[1] = 0.36f; parameters[2] = 0.12f; parameters[3] = 0.62f; parameters[4] = 1.0f;
            break;
        case fxImager:
            parameters[0] = 0.50f; parameters[1] = 0.50f; parameters[2] = 0.50f; parameters[3] = 0.50f;
            parameters[4] = 0.28f; parameters[5] = 0.53f; parameters[6] = 0.78f;
            break;
        case fxShift:
            parameters[0] = 0.24f; parameters[1] = 0.62f; parameters[2] = 0.18f; parameters[3] = 0.14f; parameters[4] = 1.0f;
            break;
        case fxTremolo:
            parameters[0] = 0.18f; parameters[1] = 0.70f; parameters[2] = 0.28f; parameters[3] = 0.52f; parameters[4] = 1.0f;
            break;
        case fxGate:
            parameters[0] = 0.46f; parameters[1] = 0.08f; parameters[2] = 0.18f; parameters[3] = 0.20f; parameters[4] = gateRateIndexToNormal(2); parameters[5] = 1.0f;
            initialVariant = getDefaultFXGateVariant();
            break;
        case fxDimension:
            parameters[0] = 0.40f; parameters[1] = 0.54f; parameters[2] = 0.30f;
            break;
        default: break;
    }

    setParameterPlain(apvts, getFXSlotVariantParamID(slotIndex), (float) initialVariant);
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setParameterPlain(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), parameters[(size_t) parameterIndex]);
    driveToneStates[(size_t) slotIndex] = {};
    compressorStates[(size_t) slotIndex] = {};
    bitcrusherStates[(size_t) slotIndex] = {};
    imagerStates[(size_t) slotIndex] = {};
    compressorGainReductionMeters[(size_t) slotIndex].store(0.0f);
    clearFXEQSpectrum(slotIndex);
    clearFXImagerScope(slotIndex);
    resetShiftState(slotIndex);
    tremoloPhases[(size_t) slotIndex] = 0.0f;
    gateStates[(size_t) slotIndex] = {};
    for (int band = 0; band < fxMultibandBandCount; ++band)
    {
        multibandUpwardMeters[(size_t) slotIndex][(size_t) band].store(0.0f);
        multibandDownwardMeters[(size_t) slotIndex][(size_t) band].store(0.0f);
    }
    resetPhaserState(slotIndex);
    fxFilterSlotsL[(size_t) slotIndex].reset();
    fxFilterSlotsR[(size_t) slotIndex].reset();
    fxFilterStage2SlotsL[(size_t) slotIndex].reset();
    fxFilterStage2SlotsR[(size_t) slotIndex].reset();
    for (int band = 0; band < fxEQBandCount; ++band)
    {
        fxEQSlotsL[(size_t) slotIndex][(size_t) band].reset();
        fxEQSlotsR[(size_t) slotIndex][(size_t) band].reset();
    }
    chorusSlots[(size_t) slotIndex].reset();
    flangerSlots[(size_t) slotIndex].reset();
    reverbSlots[(size_t) slotIndex].reset();
    std::fill(delayStates[(size_t) slotIndex].left.begin(), delayStates[(size_t) slotIndex].left.end(), 0.0f);
    std::fill(delayStates[(size_t) slotIndex].right.begin(), delayStates[(size_t) slotIndex].right.end(), 0.0f);
    delayStates[(size_t) slotIndex].lowpassState = {};
    delayStates[(size_t) slotIndex].highpassState = {};
    delayStates[(size_t) slotIndex].previousHighpassInput = {};
    delayStates[(size_t) slotIndex].writePosition = 0;
}

void PluginProcessor::copyFXSlotState(int sourceIndex, int destinationIndex)
{
    if (! juce::isPositiveAndBelow(sourceIndex, maxFXSlots) || ! juce::isPositiveAndBelow(destinationIndex, maxFXSlots))
        return;

    setFXOrderSlot(destinationIndex, getFXOrderSlot(sourceIndex));
    setBoolParam(apvts, getFXSlotOnParamID(destinationIndex), getFXSlotOnValue(sourceIndex));
    setBoolParam(apvts, getFXSlotParallelParamID(destinationIndex), getFXSlotParallelValue(sourceIndex));
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setParameterPlain(apvts, getFXSlotFloatParamID(destinationIndex, parameterIndex), getFXSlotFloatValue(sourceIndex, parameterIndex));
    setParameterPlain(apvts, getFXSlotVariantParamID(destinationIndex), (float) getFXSlotVariant(sourceIndex));
}

void PluginProcessor::clearFXSlotState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setFXOrderSlot(slotIndex, fxOff);
    setBoolParam(apvts, getFXSlotOnParamID(slotIndex), false);
    setBoolParam(apvts, getFXSlotParallelParamID(slotIndex), false);
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setParameterPlain(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), getFXSlotDefaultValue(parameterIndex));
    setParameterPlain(apvts, getFXSlotVariantParamID(slotIndex), 0.0f);
    driveToneStates[(size_t) slotIndex] = {};
    compressorStates[(size_t) slotIndex] = {};
    bitcrusherStates[(size_t) slotIndex] = {};
    imagerStates[(size_t) slotIndex] = {};
    compressorGainReductionMeters[(size_t) slotIndex].store(0.0f);
    clearFXEQSpectrum(slotIndex);
    clearFXImagerScope(slotIndex);
    resetShiftState(slotIndex);
    tremoloPhases[(size_t) slotIndex] = 0.0f;
    gateStates[(size_t) slotIndex] = {};
    for (int band = 0; band < fxMultibandBandCount; ++band)
    {
        multibandUpwardMeters[(size_t) slotIndex][(size_t) band].store(0.0f);
        multibandDownwardMeters[(size_t) slotIndex][(size_t) band].store(0.0f);
    }
    resetPhaserState(slotIndex);
    fxFilterSlotsL[(size_t) slotIndex].reset();
    fxFilterSlotsR[(size_t) slotIndex].reset();
    fxFilterStage2SlotsL[(size_t) slotIndex].reset();
    fxFilterStage2SlotsR[(size_t) slotIndex].reset();
    for (int band = 0; band < fxEQBandCount; ++band)
    {
        fxEQSlotsL[(size_t) slotIndex][(size_t) band].reset();
        fxEQSlotsR[(size_t) slotIndex][(size_t) band].reset();
    }
    chorusSlots[(size_t) slotIndex].reset();
    flangerSlots[(size_t) slotIndex].reset();
    reverbSlots[(size_t) slotIndex].reset();
    std::fill(delayStates[(size_t) slotIndex].left.begin(), delayStates[(size_t) slotIndex].left.end(), 0.0f);
    std::fill(delayStates[(size_t) slotIndex].right.begin(), delayStates[(size_t) slotIndex].right.end(), 0.0f);
    delayStates[(size_t) slotIndex].lowpassState = {};
    delayStates[(size_t) slotIndex].highpassState = {};
    delayStates[(size_t) slotIndex].previousHighpassInput = {};
    delayStates[(size_t) slotIndex].writePosition = 0;
}

void PluginProcessor::resetPhaserState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    phaserStates[(size_t) slotIndex] = {};
}

void PluginProcessor::resetShiftState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    auto& state = shiftStates[(size_t) slotIndex];
    std::fill(state.left.begin(), state.left.end(), 0.0f);
    std::fill(state.right.begin(), state.right.end(), 0.0f);
    state.writePosition = 0;
    state.modPhaseL = 0.0f;
    state.modPhaseR = 1.7f;
    state.carrierPhaseL = 0.0f;
    state.carrierPhaseR = 1.57f;
    state.feedbackState = {};
    state.allpassX1 = {};
    state.allpassY1 = {};
    state.allpassX2 = {};
    state.allpassY2 = {};
}

void PluginProcessor::clearFXEQSpectrum(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int i = 0; i < fxEQSpectrumBinCount; ++i)
        eqSpectrumBins[(size_t) slotIndex][(size_t) i].store(0.0f);
}

void PluginProcessor::clearFXImagerScope(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int i = 0; i < fxImagerScopePointCount; ++i)
    {
        imagerScopeX[(size_t) slotIndex][(size_t) i].store(0.0f);
        imagerScopeY[(size_t) slotIndex][(size_t) i].store(0.0f);
    }
}

void PluginProcessor::resetEffectState()
{
    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        driveToneStates[(size_t) slot] = {};
        compressorStates[(size_t) slot] = {};
        bitcrusherStates[(size_t) slot] = {};
        imagerStates[(size_t) slot] = {};
        compressorGainReductionMeters[(size_t) slot].store(0.0f);
        for (int band = 0; band < fxMultibandBandCount; ++band)
        {
            multibandUpwardMeters[(size_t) slot][(size_t) band].store(0.0f);
            multibandDownwardMeters[(size_t) slot][(size_t) band].store(0.0f);
        }
        clearFXEQSpectrum(slot);
        clearFXImagerScope(slot);
        tremoloPhases[(size_t) slot] = 0.0f;
        gateStates[(size_t) slot] = {};
        resetShiftState(slot);
        resetPhaserState(slot);
        fxFilterSlotsL[(size_t) slot].reset();
        fxFilterSlotsR[(size_t) slot].reset();
        fxFilterStage2SlotsL[(size_t) slot].reset();
        fxFilterStage2SlotsR[(size_t) slot].reset();
        for (int band = 0; band < fxEQBandCount; ++band)
        {
            fxEQSlotsL[(size_t) slot][(size_t) band].reset();
            fxEQSlotsR[(size_t) slot][(size_t) band].reset();
        }
        chorusSlots[(size_t) slot].reset();
        flangerSlots[(size_t) slot].reset();
        reverbSlots[(size_t) slot].reset();
        std::fill(delayStates[(size_t) slot].left.begin(), delayStates[(size_t) slot].left.end(), 0.0f);
        std::fill(delayStates[(size_t) slot].right.begin(), delayStates[(size_t) slot].right.end(), 0.0f);
        delayStates[(size_t) slot].lowpassState = {};
        delayStates[(size_t) slot].highpassState = {};
        delayStates[(size_t) slot].previousHighpassInput = {};
        delayStates[(size_t) slot].writePosition = 0;
        std::fill(dimensionStates[(size_t) slot].left.begin(), dimensionStates[(size_t) slot].left.end(), 0.0f);
        std::fill(dimensionStates[(size_t) slot].right.begin(), dimensionStates[(size_t) slot].right.end(), 0.0f);
        dimensionStates[(size_t) slot].writePosition = 0;
    }
}

void PluginProcessor::updateFXEQSpectrum(int slotIndex, const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    constexpr int fftOrder = 11;
    constexpr int fftSize = 1 << fftOrder;
    if (numSamples <= 0 || numChannels <= 0)
    {
        clearFXEQSpectrum(slotIndex);
        return;
    }

    std::fill(eqSpectrumFFTData.begin(), eqSpectrumFFTData.end(), 0.0f);
    const int copySamples = juce::jmin(fftSize, numSamples);
    const int sourceStart = juce::jmax(0, numSamples - copySamples);

    for (int i = 0; i < copySamples; ++i)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mono += buffer.getSample(channel, sourceStart + i);

        eqSpectrumFFTData[(size_t) i] = mono / (float) numChannels;
    }

    eqSpectrumWindow.multiplyWithWindowingTable(eqSpectrumFFTData.data(), fftSize);
    eqSpectrumFFT.performRealOnlyForwardTransform(eqSpectrumFFTData.data());

    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 20000.0f;
    const float nyquist = (float) currentSampleRate * 0.5f;

    for (int i = 0; i < fxEQSpectrumBinCount; ++i)
    {
        const float norm = (float) i / juce::jmax(1.0f, (float) fxEQSpectrumBinCount - 1.0f);
        const float frequency = minFrequency * std::pow(maxFrequency / minFrequency, norm);
        const float fractionalBin = juce::jlimit(1.0f, (float) fftSize * 0.5f - 2.0f,
                                                 (frequency / juce::jmax(20.0f, nyquist)) * (float) (fftSize * 0.5f));
        const int lowerBin = juce::jlimit(1, fftSize / 2 - 2, (int) std::floor(fractionalBin));
        const int upperBin = lowerBin + 1;
        const float alpha = fractionalBin - (float) lowerBin;

        const auto magnitudeForBin = [this, fftSize] (int bin)
        {
            const float real = eqSpectrumFFTData[(size_t) bin * 2];
            const float imag = eqSpectrumFFTData[(size_t) bin * 2 + 1];
            return std::sqrt(real * real + imag * imag) / (float) fftSize;
        };

        const float magnitude = juce::jmap(alpha, magnitudeForBin(lowerBin), magnitudeForBin(upperBin));
        const float magnitudeDb = juce::Decibels::gainToDecibels(magnitude * 7.0f, -78.0f);
        const float mapped = std::pow(juce::jlimit(0.0f, 1.0f, juce::jmap(magnitudeDb, -78.0f, -6.0f, 0.0f, 1.0f)), 1.45f);
        const float previous = eqSpectrumBins[(size_t) slotIndex][(size_t) i].load();
        const float smoothed = mapped > previous
                             ? previous + (mapped - previous) * 0.72f
                             : previous + (mapped - previous) * 0.42f;
        eqSpectrumBins[(size_t) slotIndex][(size_t) i].store(smoothed);
    }
}

void PluginProcessor::sanitiseFXChain()
{
    bool needsCompacting = false;

    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        const int moduleType = getFXOrderSlot(slot);
        if (! isSupportedFXModuleType(moduleType))
        {
            clearFXSlotState(slot);
            needsCompacting = true;
            continue;
        }

        if (moduleType == fxOff)
        {
            setBoolParam(apvts, getFXSlotOnParamID(slot), false);
            setBoolParam(apvts, getFXSlotParallelParamID(slot), false);
            continue;
        }

        if (moduleType == fxDelay)
        {
            const float leftTime = getFXSlotFloatValue(slot, 0);
            const float feedback = getFXSlotFloatValue(slot, 1);
            const float mix = getFXSlotFloatValue(slot, 2);
            const float legacyMarkerA = getFXSlotFloatValue(slot, 3);
            const float legacyMarkerB = getFXSlotFloatValue(slot, 4);
            const float mixParam = getFXSlotFloatValue(slot, 5);
            const float syncParam = getFXSlotFloatValue(slot, 6);
            const float linkParam = getFXSlotFloatValue(slot, 7);

            const bool looksLegacyDelay = legacyMarkerA <= 0.0001f
                                       && legacyMarkerB <= 0.3001f
                                       && mixParam <= 0.3001f
                                       && syncParam <= 0.0001f
                                       && linkParam <= 0.0001f;

            if (looksLegacyDelay)
            {
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 0), leftTime);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 1), leftTime);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 2), 0.0f);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 3), 1.0f);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 4), feedback);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 5), mix);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 6), 0.0f);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 7), 1.0f);
            }
        }
        else if (moduleType == fxCompressor)
        {
            const float amount = getFXSlotFloatValue(slot, 0);
            const float punch = getFXSlotFloatValue(slot, 1);
            const float legacyMix = getFXSlotFloatValue(slot, 2);
            const float output = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const bool looksLegacyCompressor = output <= 0.0001f
                                            && mix <= 0.3001f
                                            && getFXSlotFloatValue(slot, 5) <= 0.3001f;

            if (looksLegacyCompressor)
            {
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 0), amount);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 1), punch);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 2), 0.52f);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 3), 0.46f);
                setParameterPlain(apvts, getFXSlotFloatParamID(slot, 4), legacyMix);
            }
        }

        const int variantLimit = getFXVariantLimit(moduleType);
        setParameterPlain(apvts,
                          getFXSlotVariantParamID(slot),
                          (float) juce::jlimit(0, juce::jmax(0, variantLimit - 1), getFXSlotVariant(slot)));
    }

    if (needsCompacting)
        compactFXOrder();
}

float PluginProcessor::getFXSlotFloatValue(int slotIndex, int parameterIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots) || ! juce::isPositiveAndBelow(parameterIndex, fxSlotParameterCount))
        return getFXSlotDefaultValue(parameterIndex);

    float value = getFXSlotDefaultValue(parameterIndex);

    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, parameterIndex)))
        value = raw->load();

    if (effectModulationActive)
    {
        const int flatIndex = slotIndex * reactormod::fxParameterCount + parameterIndex;
        if (juce::isPositiveAndBelow(flatIndex, (int) effectModulationAmounts.size()))
            value = juce::jlimit(0.0f, 1.0f, value + effectModulationAmounts[(size_t) flatIndex]);
    }

    return value;
}

bool PluginProcessor::getFXSlotOnValue(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return false;

    if (auto* raw = apvts.getRawParameterValue(getFXSlotOnParamID(slotIndex)))
        return raw->load() >= 0.5f;

    return false;
}

bool PluginProcessor::getFXSlotParallelValue(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return false;

    if (getFXOrderSlot(slotIndex) == fxOff)
        return false;

    if (auto* raw = apvts.getRawParameterValue(getFXSlotParallelParamID(slotIndex)))
        return raw->load() >= 0.5f;

    return false;
}

float PluginProcessor::processPhaserSample(float input, std::array<PhaserStage, 4>& stages, float coeff, float& feedbackState)
{
    float sample = input + feedbackState * 0.32f;

    for (auto& stage : stages)
    {
        const float output = -coeff * sample + stage.x1 + coeff * stage.y1;
        stage.x1 = sample;
        stage.y1 = output;
        sample = output;
    }

    feedbackState = sample;
    return sample;
}

void PluginProcessor::prepareEffectProcessors(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), (juce::uint32) juce::jmax(1, getTotalNumOutputChannels()) };
    effectScratch.setSize(juce::jmax(2, getTotalNumOutputChannels()), juce::jmax(1, samplesPerBlock), false, true, true);
    effectParallelSerialTap.setSize(juce::jmax(2, getTotalNumOutputChannels()), juce::jmax(1, samplesPerBlock), false, true, true);
    effectParallelDryMix.setSize(juce::jmax(2, getTotalNumOutputChannels()), juce::jmax(1, samplesPerBlock), false, true, true);
    std::fill(effectModulationAmounts.begin(), effectModulationAmounts.end(), 0.0f);
    effectModulationActive = false;
    effectModulationLfoPhases.resize(modulationLfos.size(), 0.0f);
    effectModulationLfoHeldValues.resize(modulationLfos.size(), 0.0f);
    effectModulationOneShotFinished.resize(modulationLfos.size(), false);
    const int delaySize = juce::jmax(2048, (int) std::ceil(sampleRate * 2.5));
    const int dimensionSize = juce::jmax(1024, (int) std::ceil(sampleRate * 0.08));
    const int shiftSize = juce::jmax(4096, (int) std::ceil(sampleRate * 0.25));

    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        chorusSlots[(size_t) slot].reset();
        chorusSlots[(size_t) slot].prepare(spec);
        chorusSlots[(size_t) slot].setCentreDelay(8.0f);
        chorusSlots[(size_t) slot].setFeedback(0.10f);
        chorusSlots[(size_t) slot].setMix(0.35f);

        flangerSlots[(size_t) slot].reset();
        flangerSlots[(size_t) slot].prepare(spec);
        flangerSlots[(size_t) slot].setCentreDelay(2.2f);
        flangerSlots[(size_t) slot].setFeedback(0.18f);
        flangerSlots[(size_t) slot].setMix(0.5f);

        reverbSlots[(size_t) slot].reset();
        reverbSlots[(size_t) slot].prepare(spec);

        fxFilterSlotsL[(size_t) slot].reset();
        fxFilterSlotsR[(size_t) slot].reset();
        fxFilterStage2SlotsL[(size_t) slot].reset();
        fxFilterStage2SlotsR[(size_t) slot].reset();
        for (int band = 0; band < fxEQBandCount; ++band)
        {
            fxEQSlotsL[(size_t) slot][(size_t) band].reset();
            fxEQSlotsR[(size_t) slot][(size_t) band].reset();
        }
        driveToneStates[(size_t) slot] = { 0.0f, 0.0f };
        delayStates[(size_t) slot].left.assign((size_t) delaySize, 0.0f);
        delayStates[(size_t) slot].right.assign((size_t) delaySize, 0.0f);
        delayStates[(size_t) slot].lowpassState = {};
        delayStates[(size_t) slot].highpassState = {};
        delayStates[(size_t) slot].previousHighpassInput = {};
        delayStates[(size_t) slot].writePosition = 0;
        dimensionStates[(size_t) slot].left.assign((size_t) dimensionSize, 0.0f);
        dimensionStates[(size_t) slot].right.assign((size_t) dimensionSize, 0.0f);
        dimensionStates[(size_t) slot].writePosition = 0;
        resetPhaserState(slot);
        compressorStates[(size_t) slot] = {};
        bitcrusherStates[(size_t) slot] = {};
        imagerStates[(size_t) slot] = {};
        shiftStates[(size_t) slot].left.assign((size_t) shiftSize, 0.0f);
        shiftStates[(size_t) slot].right.assign((size_t) shiftSize, 0.0f);
        resetShiftState(slot);
        tremoloPhases[(size_t) slot] = 0.0f;
        gateStates[(size_t) slot] = {};
        compressorGainReductionMeters[(size_t) slot].store(0.0f);
        for (int band = 0; band < fxMultibandBandCount; ++band)
        {
            multibandUpwardMeters[(size_t) slot][(size_t) band].store(0.0f);
            multibandDownwardMeters[(size_t) slot][(size_t) band].store(0.0f);
        }
        clearFXEQSpectrum(slot);
        clearFXImagerScope(slot);
    }
}

void PluginProcessor::updateEffectModulationState(int numSamples)
{
    std::fill(effectModulationAmounts.begin(), effectModulationAmounts.end(), 0.0f);
    effectModulationActive = false;

    if (numSamples <= 0 || currentSampleRate <= 0.0)
        return;

    ModulationSnapshot snapshot;
    {
        const juce::SpinLock::ScopedLockType lock(modulationStateLock);
        snapshot.lfos = modulationLfos;
        snapshot.matrixSources = matrixSourceIndices;
        snapshot.matrixEnabled = matrixSlotEnableds;
    }

    if (snapshot.lfos.empty())
    {
        effectModulationLfoPhases.clear();
        effectModulationLfoHeldValues.clear();
        effectModulationOneShotFinished.clear();
        return;
    }

    effectModulationLfoPhases.resize(snapshot.lfos.size(), 0.0f);
    effectModulationLfoHeldValues.resize(snapshot.lfos.size(), 0.0f);
    effectModulationOneShotFinished.resize(snapshot.lfos.size(), false);

    std::vector<float> lfoValues(snapshot.lfos.size(), 0.0f);

    for (size_t i = 0; i < snapshot.lfos.size(); ++i)
    {
        const auto& lfo = snapshot.lfos[i];
        const float phase = juce::jlimit(0.0f, 1.0f, effectModulationLfoPhases[i]);

        if (lfo.triggerMode == reactormod::TriggerMode::oneShot
            && effectModulationOneShotFinished[i])
        {
            lfoValues[i] = effectModulationLfoHeldValues[i];
            continue;
        }

        const float value = reactormod::evaluateLfoValue(lfo, phase, (int) i + 1);
        lfoValues[i] = value;

        const float rateHz = reactormod::computeLfoRateHz(lfo, currentHostBpm);
        const float phaseAdvance = rateHz * (float) numSamples / (float) currentSampleRate;

        if (lfo.triggerMode == reactormod::TriggerMode::oneShot)
        {
            const float advancedPhase = phase + phaseAdvance;
            if (advancedPhase >= 1.0f)
            {
                effectModulationLfoPhases[i] = 1.0f;
                effectModulationOneShotFinished[i] = true;
                effectModulationLfoHeldValues[i] = reactormod::evaluateLfoValue(lfo, 0.9999f, (int) i + 1);
                lfoValues[i] = effectModulationLfoHeldValues[i];
            }
            else
            {
                effectModulationLfoPhases[i] = advancedPhase;
                effectModulationOneShotFinished[i] = false;
                effectModulationLfoHeldValues[i] = value;
            }
        }
        else
        {
            effectModulationLfoPhases[i] = phase + phaseAdvance;
            effectModulationLfoPhases[i] -= std::floor(effectModulationLfoPhases[i]);
            effectModulationOneShotFinished[i] = false;
            effectModulationLfoHeldValues[i] = value;
        }
    }

    for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
    {
        if (! snapshot.matrixEnabled[(size_t) slot])
            continue;

        const int sourceIndex = snapshot.matrixSources[(size_t) slot];
        const auto destination = reactormod::destinationFromChoiceIndex(
            juce::roundToInt(readParameterPlain(apvts, reactormod::getMatrixDestinationParamID(slot + 1), 0.0f)));
        if (! reactormod::isFXDestination(destination))
            continue;

        const float amount = readParameterPlain(apvts, reactormod::getMatrixAmountParamID(slot + 1), 0.0f);
        if (std::abs(amount) <= 0.0001f)
            continue;

        const int destinationSlot = reactormod::getFXSlotIndexForDestination(destination);
        const int destinationParameter = reactormod::getFXParameterIndexForDestination(destination);
        const int flatIndex = destinationSlot * reactormod::fxParameterCount + destinationParameter;
        if (! juce::isPositiveAndBelow(flatIndex, (int) effectModulationAmounts.size()))
            continue;

        const float sourceValue = getModulationSourceValue(sourceIndex, lfoValues, snapshot.macroValues);
        if (std::abs(sourceValue) <= 0.0001f)
            continue;

        effectModulationAmounts[(size_t) flatIndex] = juce::jlimit(-1.0f,
                                                                   1.0f,
                                                                   effectModulationAmounts[(size_t) flatIndex]
                                                                       + sourceValue * amount);
        effectModulationActive = true;
    }
}

void PluginProcessor::applyEffects(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0)
        return;

    updateEffectModulationState(numSamples);

    effectScratch.setSize(juce::jmax(2, numChannels), numSamples, false, false, true);
    effectParallelSerialTap.setSize(juce::jmax(2, numChannels), numSamples, false, false, true);
    effectParallelDryMix.setSize(juce::jmax(2, numChannels), numSamples, false, false, true);

    auto copyBufferToScratch = [this, &buffer, numChannels, numSamples]()
    {
        for (int channel = 0; channel < numChannels; ++channel)
            effectScratch.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    };

    const auto copyBuffer = [numChannels, numSamples](juce::AudioBuffer<float>& destination, const juce::AudioBuffer<float>& source)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            destination.copyFrom(channel, 0, source, channel, 0, numSamples);
    };

    struct ParallelSlotScope
    {
        ParallelSlotScope(bool shouldApply,
                          juce::AudioBuffer<float>& processedBufferIn,
                          const juce::AudioBuffer<float>& serialTapIn,
                          const juce::AudioBuffer<float>& dryMixIn,
                          int channelsIn,
                          int samplesIn)
            : active(shouldApply),
              processedBuffer(processedBufferIn),
              serialTap(serialTapIn),
              dryMix(dryMixIn),
              channels(channelsIn),
              samples(samplesIn)
        {
        }

        ~ParallelSlotScope()
        {
            if (! active)
                return;

            for (int channel = 0; channel < channels; ++channel)
            {
                auto* processed = processedBuffer.getWritePointer(channel);
                const auto* tapped = serialTap.getReadPointer(channel);
                const auto* dry = dryMix.getReadPointer(channel);
                for (int sample = 0; sample < samples; ++sample)
                    processed[sample] = dry[sample] + (processed[sample] - tapped[sample]);
            }
        }

        bool active = false;
        juce::AudioBuffer<float>& processedBuffer;
        const juce::AudioBuffer<float>& serialTap;
        const juce::AudioBuffer<float>& dryMix;
        int channels = 0;
        int samples = 0;
    };

    const auto fxOrder = getFXOrder();
    bool parallelGroupActive = false;
    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        const int moduleType = fxOrder[slot];
        if (! getFXSlotOnValue(slot))
            continue;

        const bool parallelSlot = getFXSlotParallelValue(slot);
        if (parallelSlot)
        {
            if (! parallelGroupActive)
            {
                copyBuffer(effectParallelSerialTap, buffer);
                parallelGroupActive = true;
            }

            copyBuffer(effectParallelDryMix, buffer);
            copyBuffer(buffer, effectParallelSerialTap);
        }
        else
        {
            parallelGroupActive = false;
        }

        ParallelSlotScope parallelScope(parallelSlot, buffer, effectParallelSerialTap, effectParallelDryMix, numChannels, numSamples);

        if (moduleType == fxDrive)
        {
            const float a = getFXSlotFloatValue(slot, 0);
            const float b = getFXSlotFloatValue(slot, 1);
            const float c = getFXSlotFloatValue(slot, 2);
            const int variant = getFXSlotVariant(slot);
            if (c <= 0.0001f)
                continue;

            const int driveType = juce::jlimit(0, getFXDriveTypeNames().size() - 1, variant);
            const float driveGain = 1.0f + a * 10.0f;
            const float toneCoeff = 0.04f + b * 0.28f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    const float input = dry * driveGain;
                    float driven = std::tanh(input);

                    switch (driveType)
                    {
                        case 1:
                            driven = juce::jlimit(-1.0f, 1.0f, input * 0.72f);
                            break;

                        case 2:
                        {
                            const float asymmetric = input + 0.32f * input * input;
                            driven = std::tanh(asymmetric) * 0.92f;
                            break;
                        }

                        case 3:
                        {
                            const float threshold = juce::jmax(0.10f, 0.62f - a * 0.24f);
                            float folded = input * 0.65f;
                            if (std::abs(folded) > threshold)
                            {
                                const float range = threshold * 4.0f;
                                folded = std::abs(std::fmod(folded - threshold, range));
                                folded = std::abs(folded - threshold * 2.0f) - threshold;
                            }
                            driven = juce::jlimit(-1.0f, 1.0f, folded / juce::jmax(0.05f, threshold));
                            break;
                        }

                        case 4:
                            driven = std::tanh(input * 0.70f + std::sin(input * (1.8f + a * 2.4f)) * 0.45f);
                            break;

                        case 5:
                        {
                            const float diode = std::tanh(juce::jmax(0.0f, input) * (1.4f + a * 5.0f))
                                              - std::tanh(juce::jmax(0.0f, -input) * (0.5f + a * 1.8f));
                            driven = juce::jlimit(-1.0f, 1.0f, diode);
                            break;
                        }

                        case 6:
                            driven = std::sin(juce::jlimit(-juce::MathConstants<float>::pi,
                                                           juce::MathConstants<float>::pi,
                                                           input * (0.9f + a * 1.8f)));
                            break;

                        case 7:
                        {
                            const float clamped = juce::jlimit(-1.0f, 1.0f, input * (0.42f + a * 0.55f));
                            const float crushed = std::round(clamped * (10.0f + a * 30.0f)) / (10.0f + a * 30.0f);
                            driven = std::tanh(crushed * (1.8f + a * 2.4f));
                            break;
                        }

                        default:
                            break;
                    }

                    float& toneState = driveToneStates[(size_t) slot][(size_t) channel];
                    toneState += toneCoeff * (driven - toneState);
                    const float dark = toneState;
                    const float bright = juce::jlimit(-1.0f, 1.0f, driven + (driven - dark) * 0.42f);
                    const float shaped = juce::jmap(b, dark, bright);
                    data[sample] = dry * (1.0f - c) + shaped * c;
                }
            }

            continue;
        }

        if (moduleType == fxFilter)
        {
            const float p0 = getFXSlotFloatValue(slot, 0);
            const float p1 = getFXSlotFloatValue(slot, 1);
            const float p2 = getFXSlotFloatValue(slot, 2);
            const float p3 = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            if (mix <= 0.0001f)
                continue;

            const int filterType = juce::jlimit(0, getFXFilterTypeNames().size() - 1, getFXSlotVariant(slot));
            const float cutoff = juce::jlimit(24.0f, (float) currentSampleRate * 0.45f, normalToFXFilterFrequency(p0));
            bool useStage2 = false;
            bool useComb = false;
            bool useLadder = false;
            float driveParam = p2;
            float combFeedback = 0.0f;
            float combDampCoeff = 0.0f;
            float combSign = 1.0f;
            float ladderBias = 0.0f;
            juce::IIRCoefficients stage1Coefficients;
            juce::IIRCoefficients stage2Coefficients;

            switch (filterType)
            {
                case 0:
                    stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 6.5f, 0.55f + p1 * 5.95f));
                    break;

                case 1:
                    stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, juce::jlimit(0.65f, 4.0f, 0.65f + p1 * 3.35f));
                    stage2Coefficients = stage1Coefficients;
                    useStage2 = true;
                    break;

                case 2:
                    stage1Coefficients = juce::IIRCoefficients::makeHighPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 6.5f, 0.55f + p1 * 5.95f));
                    break;

                case 3:
                    stage1Coefficients = juce::IIRCoefficients::makeHighPass(currentSampleRate, cutoff, juce::jlimit(0.65f, 4.0f, 0.65f + p1 * 3.35f));
                    stage2Coefficients = stage1Coefficients;
                    useStage2 = true;
                    break;

                case 4:
                    stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate, cutoff, juce::jlimit(0.45f, 10.0f, 0.85f + p1 * 6.5f));
                    break;

                case 5:
                    stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 8.0f, 0.85f + p1 * 5.0f));
                    stage2Coefficients = stage1Coefficients;
                    useStage2 = true;
                    break;

                case 6:
                    stage1Coefficients = juce::IIRCoefficients::makeNotchFilter(currentSampleRate, cutoff, juce::jlimit(0.75f, 10.0f, 0.75f + p1 * 9.25f));
                    break;

                case 7:
                    driveParam = p3;
                    stage1Coefficients = juce::IIRCoefficients::makePeakFilter(currentSampleRate,
                                                                                cutoff,
                                                                                juce::jlimit(0.45f, 8.0f, 0.8f + p1 * 5.5f),
                                                                                juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2)));
                    break;

                case 8:
                    driveParam = p3;
                    stage1Coefficients = juce::IIRCoefficients::makeLowShelf(currentSampleRate,
                                                                              cutoff,
                                                                              juce::jlimit(0.5f, 1.5f, 0.65f + p1 * 0.8f),
                                                                              juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2)));
                    break;

                case 9:
                    driveParam = p3;
                    stage1Coefficients = juce::IIRCoefficients::makeHighShelf(currentSampleRate,
                                                                               cutoff,
                                                                               juce::jlimit(0.5f, 1.5f, 0.65f + p1 * 0.8f),
                                                                               juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2)));
                    break;

                case 10:
                    driveParam = p3;
                    stage1Coefficients = juce::IIRCoefficients::makeAllPass(currentSampleRate,
                                                                             cutoff,
                                                                             juce::jlimit(0.45f, 1.4f, 0.55f + p1 * 0.9f));
                    stage2Coefficients = juce::IIRCoefficients::makeAllPass(currentSampleRate,
                                                                             juce::jlimit(32.0f, (float) currentSampleRate * 0.46f, cutoff * (1.08f + p2 * 1.8f)),
                                                                             juce::jlimit(0.35f, 1.2f, 0.45f + p1 * 0.7f));
                    useStage2 = true;
                    break;

                case 11:
                case 12:
                    useComb = true;
                    driveParam = p3;
                    combFeedback = 0.12f + p1 * 0.82f;
                    combDampCoeff = 0.02f + p2 * 0.26f;
                    combSign = filterType == 11 ? 1.0f : -1.0f;
                    break;

                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                {
                    driveParam = p3;
                    const auto formants = getVowelFormants(filterType, p0, p2);
                    const float centreA = juce::jlimit(120.0f, (float) currentSampleRate * 0.38f, formants.first);
                    const float centreB = juce::jlimit(280.0f, (float) currentSampleRate * 0.45f, formants.second);
                    stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate,
                                                                             centreA,
                                                                             juce::jlimit(1.0f, 12.0f, 1.4f + p1 * 7.0f));
                    stage2Coefficients = juce::IIRCoefficients::makePeakFilter(currentSampleRate,
                                                                                centreB,
                                                                                juce::jlimit(0.7f, 7.0f, 1.0f + p1 * 4.8f),
                                                                                juce::Decibels::decibelsToGain(4.0f + p1 * 10.0f));
                    useStage2 = true;
                    break;
                }

                case 19:
                    useLadder = true;
                    driveParam = p2;
                    ladderBias = juce::jmap(p3, -0.28f, 0.28f);
                    stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate,
                                                                             cutoff,
                                                                             juce::jlimit(0.75f, 3.0f, 0.75f + p1 * 2.25f));
                    stage2Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate,
                                                                             juce::jlimit(24.0f, (float) currentSampleRate * 0.45f, cutoff * (0.78f + p3 * 0.34f)),
                                                                             juce::jlimit(0.70f, 1.8f, 0.72f + p1 * 1.08f));
                    useStage2 = true;
                    break;

                default:
                    stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff);
                    break;
            }

            if (! useComb)
            {
                fxFilterSlotsL[(size_t) slot].setCoefficients(stage1Coefficients);
                fxFilterSlotsR[(size_t) slot].setCoefficients(stage1Coefficients);
                if (useStage2)
                {
                    fxFilterStage2SlotsL[(size_t) slot].setCoefficients(stage2Coefficients);
                    fxFilterStage2SlotsR[(size_t) slot].setCoefficients(stage2Coefficients);
                }
            }

            const float driveGain = normalToFXFilterDriveGain(driveParam);

            if (useComb)
            {
                auto& delayState = delayStates[(size_t) slot];
                const int delayBufferSize = juce::jmin((int) delayState.left.size(), (int) delayState.right.size());
                if (delayBufferSize <= 0)
                    continue;

                const float tunedFrequency = juce::jlimit(18.0f,
                                                          (float) currentSampleRate * 0.25f,
                                                          normalToFXFilterFrequency(p0) * 0.42f);
                const int delaySamples = juce::jlimit(1, delayBufferSize - 1, (int) std::round(currentSampleRate / tunedFrequency));

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const int readPosition = (delayState.writePosition - delaySamples + delayBufferSize) % delayBufferSize;

                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        auto* data = buffer.getWritePointer(channel);
                        const float dry = data[sample];
                        auto& delayLine = channel == 0 ? delayState.left : delayState.right;
                        float& toneState = driveToneStates[(size_t) slot][(size_t) channel];

                        const float delayed = delayLine[(size_t) readPosition];
                        toneState += combDampCoeff * (delayed - toneState);
                        const float filteredDelay = toneState;
                        const float feedbackSample = dry + filteredDelay * combFeedback * combSign;
                        delayLine[(size_t) delayState.writePosition] = juce::jlimit(-1.0f, 1.0f, std::tanh(feedbackSample));

                        const float wet = std::tanh((dry + filteredDelay * 0.92f * combSign) * driveGain);
                        data[sample] = dry * (1.0f - mix) + wet * mix;
                    }

                    delayState.writePosition = (delayState.writePosition + 1) % delayBufferSize;
                }

                continue;
            }

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    auto& stage1 = channel == 0 ? fxFilterSlotsL[(size_t) slot] : fxFilterSlotsR[(size_t) slot];
                    auto& stage2 = channel == 0 ? fxFilterStage2SlotsL[(size_t) slot] : fxFilterStage2SlotsR[(size_t) slot];
                    float wet = dry;

                    if (useLadder)
                    {
                        const float driven = std::tanh((dry + ladderBias) * driveGain);
                        wet = stage1.processSingleSampleRaw(driven);
                        wet = stage2.processSingleSampleRaw(wet + (wet - driven) * (0.05f + p1 * 0.18f));
                        wet = std::tanh(wet * (1.15f + p2 * 1.8f)) - ladderBias * 0.35f;
                    }
                    else
                    {
                        wet = stage1.processSingleSampleRaw(dry);
                        if (useStage2)
                            wet = stage2.processSingleSampleRaw(wet);

                        wet = std::tanh(wet * driveGain);

                        if (filterType == 10)
                            wet = juce::jlimit(-1.0f, 1.0f, dry * 0.35f + wet * 0.95f + (wet - dry) * (p2 * 0.22f));
                        else if (filterType >= 13 && filterType <= 18)
                            wet = dry * 0.16f + wet * 1.08f;
                    }

                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }

            continue;
        }

        if (moduleType == fxChorus)
        {
            const float rate = getFXSlotFloatValue(slot, 0);
            const float depth = getFXSlotFloatValue(slot, 1);
            const float delay = getFXSlotFloatValue(slot, 2);
            const float feedback = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const bool syncEnabled = getFXSlotVariant(slot) > 0;
            if (mix <= 0.0001f)
                continue;

            copyBufferToScratch();

            auto& chorus = chorusSlots[(size_t) slot];
            chorus.setRate(syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm)
                                       : normalToFXChorusRateHz(rate));
            chorus.setDepth(juce::jlimit(0.06f, 1.0f, 0.08f + depth * 0.90f));
            chorus.setCentreDelay(normalToFXChorusDelayMs(delay));
            chorus.setFeedback(normalToFXChorusFeedback(feedback));
            chorus.setMix(1.0f);

            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            chorus.process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    auto* wetData = effectScratch.getWritePointer(channel);
                    const float wet = wetData[sample];
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }

            continue;
        }

        if (moduleType == fxEQ)
        {
            const int variant = getFXSlotVariant(slot);
            bool anyBandActive = false;
            std::array<bool, fxEQBandCount> midBandActive{};
            std::array<bool, fxEQBandCount> sideBandActive{};

            for (int band = 0; band < fxEQBandCount; ++band)
            {
                const bool bandActive = isFXEQBandActive(variant, band);
                if (! bandActive)
                    continue;

                anyBandActive = true;
                const int type = juce::jlimit(0, fxEQTypeCount - 1, getFXEQBandType(variant, band));
                const int target = juce::jlimit(0, fxEQTargetCount - 1, getFXEQBandTarget(variant, band));
                const float frequency = normalToFXEQFrequency(getFXSlotFloatValue(slot, band * 2));
                const float gainDb = normalToFXEQGainDb(getFXSlotFloatValue(slot, band * 2 + 1));
                const float q = normalToFXEQQ(getFXSlotFloatValue(slot, 8 + band));
                const auto coefficients = makeFXEQCoefficients(type, currentSampleRate, frequency, q, gainDb);

                midBandActive[(size_t) band] = target != 2;
                sideBandActive[(size_t) band] = target != 1;

                if (midBandActive[(size_t) band])
                    fxEQSlotsL[(size_t) slot][(size_t) band].setCoefficients(coefficients);

                if (sideBandActive[(size_t) band])
                    fxEQSlotsR[(size_t) slot][(size_t) band].setCoefficients(coefficients);
            }

            if (! anyBandActive)
            {
                clearFXEQSpectrum(slot);
                continue;
            }

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float leftIn = buffer.getSample(0, sample);
                const float rightIn = numChannels > 1 ? buffer.getSample(1, sample) : leftIn;
                float mid = 0.5f * (leftIn + rightIn);
                float side = 0.5f * (leftIn - rightIn);

                float wetMid = mid;
                float wetSide = side;

                for (int band = 0; band < fxEQBandCount; ++band)
                {
                    if (midBandActive[(size_t) band])
                        wetMid = fxEQSlotsL[(size_t) slot][(size_t) band].processSingleSampleRaw(wetMid);

                    if (sideBandActive[(size_t) band])
                        wetSide = fxEQSlotsR[(size_t) slot][(size_t) band].processSingleSampleRaw(wetSide);
                }

                buffer.setSample(0, sample, wetMid + wetSide);
                if (numChannels > 1)
                    buffer.setSample(1, sample, wetMid - wetSide);
            }

            updateFXEQSpectrum(slot, buffer, numChannels, numSamples);

            continue;
        }

        if (moduleType == fxCompressor)
        {
            const float input = getFXSlotFloatValue(slot, 0);
            const float attack = getFXSlotFloatValue(slot, 1);
            const float release = getFXSlotFloatValue(slot, 2);
            const float output = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            if (mix <= 0.0001f)
            {
                compressorGainReductionMeters[(size_t) slot].store(0.0f);
                continue;
            }

            auto& state = compressorStates[(size_t) slot];
            const int compressorType = juce::jlimit(0, getFXCompressorTypeNames().size() - 1, getFXSlotVariant(slot));
            const float ratio = compressorRatioFromType(compressorType);
            const float thresholdDb = compressorType == 4 ? -20.0f : -16.0f;
            const float kneeDb = compressorType == 4 ? 9.0f : 5.0f;
            const float inputGain = juce::Decibels::decibelsToGain(normalToFXCompressorInputDb(input));
            const float outputGain = juce::Decibels::decibelsToGain(normalToFXCompressorOutputDb(output));
            const float attackMs = normalToFXCompressorAttackMs(attack);
            const float releaseMs = normalToFXCompressorReleaseMs(release);
            const float attackCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * attackMs * 0.001f));
            const float releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * releaseMs * 0.001f));

            for (int sample = 0; sample < numSamples; ++sample)
            {
                float detector = 0.0f;
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    const float dry = buffer.getSample(channel, sample) * inputGain;
                    detector = juce::jmax(detector, std::abs(dry));
                }

                const float detectorDb = juce::Decibels::gainToDecibels(detector + 1.0e-6f, -60.0f);
                float overDb = detectorDb - thresholdDb;
                if (overDb < -kneeDb * 0.5f)
                    overDb = 0.0f;
                else if (std::abs(overDb) < kneeDb * 0.5f)
                    overDb = std::pow((overDb + kneeDb * 0.5f) / juce::jmax(0.001f, kneeDb), 2.0f) * kneeDb * 0.5f;

                const float targetReductionDb = overDb > 0.0f ? overDb * (1.0f - 1.0f / ratio) : 0.0f;
                const float coeff = targetReductionDb > state.envelope ? attackCoeff : releaseCoeff;
                state.envelope = targetReductionDb + coeff * (state.envelope - targetReductionDb);

                const float reductionGain = juce::Decibels::decibelsToGain(-state.envelope);
                const float colourDrive = compressorType == 4 ? 1.22f : 1.06f + 0.06f * (float) compressorType;

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    float wet = dry * inputGain * reductionGain;
                    wet = std::tanh(wet * colourDrive) * outputGain;
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }

                state.makeup = 0.92f * state.makeup + 0.08f * juce::jlimit(0.0f, 1.0f, state.envelope / 24.0f);
            }

            compressorGainReductionMeters[(size_t) slot].store(juce::jlimit(0.0f, 1.0f, state.makeup));
            continue;
        }

        if (moduleType == fxMultiband)
        {
            const std::array<float, 3> downwardAmounts{
                getFXSlotFloatValue(slot, 0),
                getFXSlotFloatValue(slot, 1),
                getFXSlotFloatValue(slot, 2)
            };
            const std::array<float, 3> upwardAmounts{
                getFXSlotFloatValue(slot, 3),
                getFXSlotFloatValue(slot, 4),
                getFXSlotFloatValue(slot, 5)
            };
            const float depth = getFXSlotFloatValue(slot, 6);
            const float time = getFXSlotFloatValue(slot, 7);
            const float output = getFXSlotFloatValue(slot, 8);
            const float mix = getFXSlotFloatValue(slot, 9);
            if (mix <= 0.0001f)
            {
                for (int band = 0; band < fxMultibandBandCount; ++band)
                {
                    multibandUpwardMeters[(size_t) slot][(size_t) band].store(0.0f);
                    multibandDownwardMeters[(size_t) slot][(size_t) band].store(0.0f);
                }
                continue;
            }

            auto& state = compressorStates[(size_t) slot];
            const float lowCrossoverHz = 180.0f;
            const float highCrossoverHz = 4200.0f;
            const float lowCoeff = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * lowCrossoverHz / (float) currentSampleRate));
            const float highCoeff = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * highCrossoverHz / (float) currentSampleRate));
            const float baseTimeMs = normalToFXMultibandTimeMs(time);
            const float attackMs = juce::jmax(1.0f, baseTimeMs * 0.35f);
            const float releaseMs = juce::jmax(6.0f, baseTimeMs * 2.4f);
            const float attackCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * attackMs * 0.001f));
            const float releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * releaseMs * 0.001f));
            const float outputGain = juce::Decibels::decibelsToGain(juce::jmap(output, -8.0f, 8.0f));

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];

                    state.lowpassState[(size_t) channel] += lowCoeff * (dry - state.lowpassState[(size_t) channel]);
                    state.highpassState[(size_t) channel] += highCoeff * (dry - state.highpassState[(size_t) channel]);

                    const float lowBand = state.lowpassState[(size_t) channel];
                    const float lowMidBand = state.highpassState[(size_t) channel];
                    const float midBand = lowMidBand - lowBand;
                    const float highBand = dry - lowMidBand;
                    std::array<float, 3> bands{ lowBand, midBand, highBand };

                    for (int band = 0; band < fxMultibandBandCount; ++band)
                    {
                        const float bandLevelDb = juce::Decibels::gainToDecibels(std::abs(bands[(size_t) band]) + 1.0e-5f, -60.0f);
                        constexpr std::array<float, 3> downThresholdDb{ -26.0f, -24.0f, -22.0f };
                        constexpr std::array<float, 3> upThresholdDb{ -48.0f, -44.0f, -40.0f };

                        const float targetDownDb = juce::jlimit(0.0f, 24.0f,
                                                                juce::jmax(0.0f, bandLevelDb - downThresholdDb[(size_t) band])
                                                                    * (0.72f + depth * 0.28f)
                                                                    * downwardAmounts[(size_t) band]);
                        const float coeffDown = targetDownDb > state.downwardEnvelopes[(size_t) band] ? attackCoeff : releaseCoeff;
                        state.downwardEnvelopes[(size_t) band] = targetDownDb
                                                               + coeffDown * (state.downwardEnvelopes[(size_t) band] - targetDownDb);

                        const float targetUpDb = juce::jlimit(0.0f, 18.0f,
                                                              juce::jmax(0.0f, upThresholdDb[(size_t) band] - bandLevelDb)
                                                                  * (0.18f + depth * 0.32f)
                                                                  * upwardAmounts[(size_t) band]);
                        const float coeffUp = targetUpDb > state.upwardEnvelopes[(size_t) band] ? attackCoeff : releaseCoeff;
                        state.upwardEnvelopes[(size_t) band] = targetUpDb
                                                             + coeffUp * (state.upwardEnvelopes[(size_t) band] - targetUpDb);

                        const float bandGain = juce::Decibels::decibelsToGain(state.upwardEnvelopes[(size_t) band] - state.downwardEnvelopes[(size_t) band]);
                        bands[(size_t) band] *= bandGain;
                    }

                    const float recombined = (bands[0] + bands[1] + bands[2]) * outputGain;
                    const float wet = std::tanh(recombined * 1.06f);
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }

            for (int band = 0; band < fxMultibandBandCount; ++band)
            {
                const float upMeter = juce::jlimit(0.0f, 1.0f, state.upwardEnvelopes[(size_t) band] / 18.0f);
                const float downMeter = juce::jlimit(0.0f, 1.0f, state.downwardEnvelopes[(size_t) band] / 24.0f);
                multibandUpwardMeters[(size_t) slot][(size_t) band].store(upMeter);
                multibandDownwardMeters[(size_t) slot][(size_t) band].store(downMeter);
            }

            continue;
        }

        if (moduleType == fxBitcrusher)
        {
            const float bits = getFXSlotFloatValue(slot, 0);
            const float rate = getFXSlotFloatValue(slot, 1);
            const float jitter = getFXSlotFloatValue(slot, 2);
            const float tone = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            if (mix <= 0.0001f)
                continue;

            auto& state = bitcrusherStates[(size_t) slot];
            const auto& modes = getFXBitcrusherModes();
            const int modeIndex = juce::jlimit(0, (int) modes.size() - 1, getFXSlotVariant(slot));
            const auto& mode = modes[(size_t) modeIndex];

            const float effectiveBitsNorm = juce::jlimit(0.0f, 1.0f, bits + mode.bitBias);
            const float effectiveRateNorm = juce::jlimit(0.0f, 1.0f, rate + mode.rateBias);
            const float effectiveJitterNorm = juce::jlimit(0.0f, 1.0f, jitter + mode.jitterBias);
            const float effectiveToneNorm = juce::jlimit(0.0f, 1.0f, tone + mode.toneBias);

            const int bitDepth = normalToFXBitcrusherBitDepth(effectiveBitsNorm);
            const float quantisationSteps = (float) (1 << bitDepth);
            const int baseHoldSamples = normalToFXBitcrusherHoldSamples(effectiveRateNorm);
            const float jitterAmount = effectiveJitterNorm * 0.65f;
            const float toneHz = normalToFXBitcrusherToneHz(effectiveToneNorm);
            const float toneCoeff = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * toneHz / (float) currentSampleRate));
            const float outputTrim = juce::Decibels::decibelsToGain(mode.outputTrimDb);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];

                    if (state.holdCounter[(size_t) channel] <= 0)
                    {
                        const float quantised = std::round(dry * quantisationSteps) / quantisationSteps;
                        state.heldSample[(size_t) channel] = juce::jlimit(-1.0f, 1.0f, quantised);

                        const float jitterSpread = (fxRandom.nextFloat() * 2.0f - 1.0f) * jitterAmount;
                        const float holdScale = juce::jlimit(0.35f, 1.8f, 1.0f + jitterSpread);
                        state.holdSamplesTarget[(size_t) channel] = juce::jmax(1, juce::roundToInt((float) baseHoldSamples * holdScale));
                        state.holdCounter[(size_t) channel] = state.holdSamplesTarget[(size_t) channel];
                    }

                    float wet = state.heldSample[(size_t) channel];
                    state.toneState[(size_t) channel] += toneCoeff * (wet - state.toneState[(size_t) channel]);
                    wet = state.toneState[(size_t) channel] * outputTrim;
                    data[sample] = dry * (1.0f - mix) + wet * mix;

                    --state.holdCounter[(size_t) channel];
                }
            }

            continue;
        }

        if (moduleType == fxImager)
        {
            std::array<float, 4> widths{
                normalToFXImagerWidth(getFXSlotFloatValue(slot, 0)),
                normalToFXImagerWidth(getFXSlotFloatValue(slot, 1)),
                normalToFXImagerWidth(getFXSlotFloatValue(slot, 2)),
                normalToFXImagerWidth(getFXSlotFloatValue(slot, 3))
            };

            std::array<float, 3> crossovers{
                juce::jlimit(0.06f, 0.92f, getFXSlotFloatValue(slot, 4)),
                juce::jlimit(0.12f, 0.96f, getFXSlotFloatValue(slot, 5)),
                juce::jlimit(0.18f, 0.98f, getFXSlotFloatValue(slot, 6))
            };

            constexpr float minGap = 0.07f;
            crossovers[0] = juce::jlimit(0.06f, 0.92f - minGap * 2.0f, crossovers[0]);
            crossovers[1] = juce::jlimit(crossovers[0] + minGap, 0.96f - minGap, crossovers[1]);
            crossovers[2] = juce::jlimit(crossovers[1] + minGap, 0.98f, crossovers[2]);

            const float coeff1 = juce::jlimit(0.001f, 1.0f,
                                              1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                                              * normalToFXImagerFrequency(crossovers[0])
                                                              / (float) currentSampleRate));
            const float coeff2 = juce::jlimit(0.001f, 1.0f,
                                              1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                                              * normalToFXImagerFrequency(crossovers[1])
                                                              / (float) currentSampleRate));
            const float coeff3 = juce::jlimit(0.001f, 1.0f,
                                              1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                                              * normalToFXImagerFrequency(crossovers[2])
                                                              / (float) currentSampleRate));

            auto& state = imagerStates[(size_t) slot];
            constexpr float scopeScale = 0.90f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float leftIn = buffer.getSample(0, sample);
                const float rightIn = numChannels > 1 ? buffer.getSample(1, sample) : leftIn;

                state.lowState1[0] += coeff1 * (leftIn - state.lowState1[0]);
                state.lowState1[1] += coeff1 * (rightIn - state.lowState1[1]);
                state.lowState2[0] += coeff2 * (leftIn - state.lowState2[0]);
                state.lowState2[1] += coeff2 * (rightIn - state.lowState2[1]);
                state.lowState3[0] += coeff3 * (leftIn - state.lowState3[0]);
                state.lowState3[1] += coeff3 * (rightIn - state.lowState3[1]);

                std::array<float, 4> bandsL{
                    state.lowState1[0],
                    state.lowState2[0] - state.lowState1[0],
                    state.lowState3[0] - state.lowState2[0],
                    leftIn - state.lowState3[0]
                };
                std::array<float, 4> bandsR{
                    state.lowState1[1],
                    state.lowState2[1] - state.lowState1[1],
                    state.lowState3[1] - state.lowState2[1],
                    rightIn - state.lowState3[1]
                };

                float leftOut = 0.0f;
                float rightOut = 0.0f;
                for (int band = 0; band < fxImagerBandCount; ++band)
                {
                    const float mid = 0.5f * (bandsL[(size_t) band] + bandsR[(size_t) band]);
                    const float side = 0.5f * (bandsL[(size_t) band] - bandsR[(size_t) band]) * widths[(size_t) band];
                    leftOut += mid + side;
                    rightOut += mid - side;
                }

                buffer.setSample(0, sample, leftOut);
                if (numChannels > 1)
                    buffer.setSample(1, sample, rightOut);

                if (++state.scopeCounter >= 4)
                {
                    state.scopeCounter = 0;
                    const int writeIndex = state.scopeWriteIndex % fxImagerScopePointCount;
                    state.scopeWriteIndex = (state.scopeWriteIndex + 1) % fxImagerScopePointCount;

                    imagerScopeX[(size_t) slot][(size_t) writeIndex].store(juce::jlimit(-1.0f, 1.0f, (leftOut - rightOut) * scopeScale));
                    imagerScopeY[(size_t) slot][(size_t) writeIndex].store(juce::jlimit(-1.0f, 1.0f, (leftOut + rightOut) * 0.5f * scopeScale));
                }
            }

            continue;
        }

        if (moduleType == fxShift)
        {
            const float amount = getFXSlotFloatValue(slot, 0);
            const float spread = getFXSlotFloatValue(slot, 1);
            const float delay = getFXSlotFloatValue(slot, 2);
            const float feedback = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            if (mix <= 0.0001f)
                continue;

            auto& state = shiftStates[(size_t) slot];
            const int delayBufferSize = juce::jmin((int) state.left.size(), (int) state.right.size());
            if (delayBufferSize <= 8)
                continue;

            const int mode = juce::jlimit(0, getFXShiftTypeNames().size() - 1, getFXSlotVariant(slot));
            const float stereoSpread = juce::jlimit(0.0f, 1.0f, spread);
            const float feedbackAmount = juce::jlimit(0.0f, 0.86f, feedback * 0.82f);

            auto readDelaySample = [delayBufferSize] (const std::vector<float>& line, int writePosition, float delaySamples)
            {
                const float clampedDelay = juce::jlimit(1.0f, (float) delayBufferSize - 2.0f, delaySamples);
                const float readPosition = (float) writePosition - clampedDelay;
                const float wrapped = readPosition >= 0.0f ? readPosition : readPosition + (float) delayBufferSize;
                const int indexA = juce::jlimit(0, delayBufferSize - 1, (int) std::floor(wrapped));
                const int indexB = (indexA + 1) % delayBufferSize;
                const float alpha = wrapped - (float) indexA;
                return juce::jmap(alpha, line[(size_t) indexA], line[(size_t) indexB]);
            };

            auto processAllpass = [] (float input, float coeff, float& x1, float& y1)
            {
                const float output = -coeff * input + x1 + coeff * y1;
                x1 = input;
                y1 = output;
                return output;
            };

            const float sampleRateFloat = (float) currentSampleRate;
            const float baseDelayMs = mode <= 1 ? normalToFXShiftDelayMs(delay) : normalToFXShiftPreDelayMs(delay);
            const float stereoDelayMs = stereoSpread * (mode <= 1 ? 8.5f : 18.0f);
            const float detuneCents = normalToFXShiftDetuneCents(amount);
            const float ringRateHz = normalToFXShiftRateHz(amount);
            const float microDepthMs = (mode == 0 ? 0.22f : 0.75f) + detuneCents * (mode == 0 ? 0.085f : 0.17f);
            const float microRateHz = (mode == 0 ? 0.045f : 0.11f) + detuneCents * (mode == 0 ? 0.0025f : 0.0055f);
            const float crossBlend = mode <= 1 ? (0.12f + stereoSpread * 0.24f) : 0.0f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float leftIn = buffer.getSample(0, sample);
                const float rightIn = numChannels > 1 ? buffer.getSample(1, sample) : leftIn;

                state.left[(size_t) state.writePosition] = juce::jlimit(-1.0f, 1.0f, leftIn + state.feedbackState[0] * feedbackAmount);
                state.right[(size_t) state.writePosition] = juce::jlimit(-1.0f, 1.0f, rightIn + state.feedbackState[1] * feedbackAmount);

                float wetLeft = leftIn;
                float wetRight = rightIn;

                if (mode <= 1)
                {
                    state.modPhaseL += juce::MathConstants<float>::twoPi * microRateHz / sampleRateFloat;
                    state.modPhaseR += juce::MathConstants<float>::twoPi * microRateHz * (1.0f + stereoSpread * 0.08f) / sampleRateFloat;
                    if (state.modPhaseL > juce::MathConstants<float>::twoPi)
                        state.modPhaseL -= juce::MathConstants<float>::twoPi;
                    if (state.modPhaseR > juce::MathConstants<float>::twoPi)
                        state.modPhaseR -= juce::MathConstants<float>::twoPi;

                    const float modulationL = std::sin(state.modPhaseL);
                    const float modulationR = std::sin(state.modPhaseR + juce::MathConstants<float>::halfPi * (0.6f + stereoSpread * 0.35f));
                    const float delaySamplesL = juce::jmax(1.0f, (baseDelayMs + stereoDelayMs + modulationL * microDepthMs) * sampleRateFloat * 0.001f);
                    const float delaySamplesR = juce::jmax(1.0f, (baseDelayMs + stereoDelayMs - modulationR * microDepthMs) * sampleRateFloat * 0.001f);

                    const float tapL = readDelaySample(state.left, state.writePosition, delaySamplesL);
                    const float tapR = readDelaySample(state.right, state.writePosition, delaySamplesR);
                    const float crossL = readDelaySample(state.right, state.writePosition, juce::jmax(1.0f, delaySamplesR * (0.92f + stereoSpread * 0.12f)));
                    const float crossR = readDelaySample(state.left, state.writePosition, juce::jmax(1.0f, delaySamplesL * (0.92f + stereoSpread * 0.12f)));

                    wetLeft = juce::jlimit(-1.0f, 1.0f, tapL * (1.0f - crossBlend) + crossL * crossBlend);
                    wetRight = juce::jlimit(-1.0f, 1.0f, tapR * (1.0f - crossBlend) + crossR * crossBlend);

                    if (mode == 1)
                    {
                        wetLeft = std::tanh(wetLeft * (1.08f + stereoSpread * 0.18f));
                        wetRight = std::tanh(wetRight * (1.08f + stereoSpread * 0.18f));
                    }
                }
                else
                {
                    const float preDelaySamplesL = juce::jmax(1.0f, (baseDelayMs + stereoDelayMs * 0.35f) * sampleRateFloat * 0.001f);
                    const float preDelaySamplesR = juce::jmax(1.0f, (baseDelayMs + stereoDelayMs * 0.75f) * sampleRateFloat * 0.001f);
                    const float delayedL = readDelaySample(state.left, state.writePosition, preDelaySamplesL);
                    const float delayedR = readDelaySample(state.right, state.writePosition, preDelaySamplesR);

                    const float rateSkew = 1.0f + stereoSpread * 0.22f;
                    state.carrierPhaseL += juce::MathConstants<float>::twoPi * ringRateHz / sampleRateFloat;
                    state.carrierPhaseR += juce::MathConstants<float>::twoPi * ringRateHz * rateSkew / sampleRateFloat;
                    if (state.carrierPhaseL > juce::MathConstants<float>::twoPi)
                        state.carrierPhaseL -= juce::MathConstants<float>::twoPi;
                    if (state.carrierPhaseR > juce::MathConstants<float>::twoPi)
                        state.carrierPhaseR -= juce::MathConstants<float>::twoPi;

                    if (mode == 2)
                    {
                        wetLeft = delayedL * std::sin(state.carrierPhaseL);
                        wetRight = delayedR * std::sin(state.carrierPhaseR + stereoSpread * juce::MathConstants<float>::halfPi * 0.8f);
                    }
                    else
                    {
                        const float quadL = processAllpass(processAllpass(delayedL, 0.692f, state.allpassX1[0], state.allpassY1[0]),
                                                           0.231f, state.allpassX2[0], state.allpassY2[0]);
                        const float quadR = processAllpass(processAllpass(delayedR, 0.692f, state.allpassX1[1], state.allpassY1[1]),
                                                           0.231f, state.allpassX2[1], state.allpassY2[1]);
                        wetLeft = delayedL * std::cos(state.carrierPhaseL) - quadL * std::sin(state.carrierPhaseL);
                        wetRight = delayedR * std::cos(state.carrierPhaseR + stereoSpread * juce::MathConstants<float>::halfPi * 0.5f)
                                 - quadR * std::sin(state.carrierPhaseR + stereoSpread * juce::MathConstants<float>::halfPi * 0.5f);
                    }

                    wetLeft = std::tanh(wetLeft * (1.0f + feedback * 0.75f));
                    wetRight = std::tanh(wetRight * (1.0f + feedback * 0.75f));
                }

                state.feedbackState[0] = wetLeft;
                state.feedbackState[1] = wetRight;

                buffer.setSample(0, sample, leftIn * (1.0f - mix) + wetLeft * mix);
                if (numChannels > 1)
                    buffer.setSample(1, sample, rightIn * (1.0f - mix) + wetRight * mix);

                state.writePosition = (state.writePosition + 1) % delayBufferSize;
            }

            continue;
        }

        if (moduleType == fxTremolo)
        {
            const float rate = getFXSlotFloatValue(slot, 0);
            const float depth = getFXSlotFloatValue(slot, 1);
            const float shape = getFXSlotFloatValue(slot, 2);
            const float stereo = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const bool syncEnabled = getFXSlotVariant(slot) > 0;
            if (mix <= 0.0001f)
                continue;

            const float rateHz = syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm)
                                             : normalToFXChorusRateHz(rate);
            const float phaseOffset = normalToFXTremoloStereoPhase(stereo);
            auto& phase = tremoloPhases[(size_t) slot];

            auto tremoloShape = [shape] (float value)
            {
                const float sine = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * value);
                const float driven = 0.5f + 0.5f * std::tanh(std::sin(juce::MathConstants<float>::twoPi * value) * (1.0f + shape * 10.0f));
                return juce::jmap(shape, sine, driven);
            };

            for (int sample = 0; sample < numSamples; ++sample)
            {
                phase += rateHz / (float) currentSampleRate;
                if (phase >= 1.0f)
                    phase -= std::floor(phase);

                const float leftLfo = tremoloShape(phase);
                const float rightLfo = tremoloShape(std::fmod(phase + phaseOffset / juce::MathConstants<float>::twoPi, 1.0f));
                const float leftGain = (1.0f - depth) + depth * leftLfo;
                const float rightGain = (1.0f - depth) + depth * rightLfo;

                const float leftIn = buffer.getSample(0, sample);
                const float rightIn = numChannels > 1 ? buffer.getSample(1, sample) : leftIn;
                buffer.setSample(0, sample, leftIn * (1.0f - mix) + leftIn * leftGain * mix);
                if (numChannels > 1)
                    buffer.setSample(1, sample, rightIn * (1.0f - mix) + rightIn * rightGain * mix);
            }

            continue;
        }

        if (moduleType == fxGate)
        {
            const float p0 = getFXSlotFloatValue(slot, 0);
            const float p1 = getFXSlotFloatValue(slot, 1);
            const float p2 = getFXSlotFloatValue(slot, 2);
            const float p3 = getFXSlotFloatValue(slot, 3);
            const float gateRate = getFXSlotFloatValue(slot, 4);
            const float mix = getFXSlotFloatValue(slot, 5);
            if (mix <= 0.0001f)
                continue;

            auto& state = gateStates[(size_t) slot];
            const int variant = getFXSlotVariant(slot);
            const bool rhythmic = isFXGateRhythmic(variant);

            if (! rhythmic)
            {
                const float thresholdDb = normalToFXGateThresholdDb(p0);
                const float attackMs = normalToFXGateAttackMs(p1);
                const float holdMs = normalToFXGateHoldMs(p2);
                const float releaseMs = normalToFXGateReleaseMs(p3);
                const float attackCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * attackMs * 0.001f));
                const float releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * releaseMs * 0.001f));
                const int holdSamples = juce::jmax(0, juce::roundToInt(holdMs * 0.001f * (float) currentSampleRate));

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    float detector = 0.0f;
                    for (int channel = 0; channel < numChannels; ++channel)
                        detector = juce::jmax(detector, std::abs(buffer.getSample(channel, sample)));

                    const float detectorCoeff = detector > state.detectorEnvelope ? 0.55f : 0.08f;
                    state.detectorEnvelope += (detector - state.detectorEnvelope) * detectorCoeff;
                    const float detectorDb = juce::Decibels::gainToDecibels(state.detectorEnvelope + 1.0e-5f, -80.0f);

                    float target = 0.0f;
                    if (detectorDb >= thresholdDb)
                    {
                        target = 1.0f;
                        state.holdSamplesRemaining = holdSamples;
                    }
                    else if (state.holdSamplesRemaining > 0)
                    {
                        target = 1.0f;
                        --state.holdSamplesRemaining;
                    }

                    const float coeff = target > state.gateEnvelope ? attackCoeff : releaseCoeff;
                    state.gateEnvelope = target + coeff * (state.gateEnvelope - target);

                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        const float dry = buffer.getSample(channel, sample);
                        const float wet = dry * state.gateEnvelope;
                        buffer.setSample(channel, sample, dry * (1.0f - mix) + wet * mix);
                    }
                }
            }
            else
            {
                const float bpm = (float) juce::jmax(40.0, currentHostBpm);
                const float stepBeats = getGateRateDivisions()[(size_t) normalToGateRateIndex(gateRate)].beats;
                const float phaseIncrement = bpm / 60.0f / ((float) currentSampleRate * 16.0f * stepBeats);
                const float attackFrac = 0.01f + p0 * 0.44f;
                const float decayCurve = 1.2f + p1 * 10.0f;
                const float sustainLevel = juce::jlimit(0.0f, 1.0f, p2);
                const float releaseMs = normalToFXGateReleaseMs(p3);
                const float releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * releaseMs * 0.001f));
                const float attackMs = 0.4f + p0 * 22.0f;
                const float attackCoeff = std::exp(-1.0f / juce::jmax(1.0f, (float) currentSampleRate * attackMs * 0.001f));

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    state.rhythmPhase += phaseIncrement;
                    if (state.rhythmPhase >= 1.0f)
                        state.rhythmPhase -= std::floor(state.rhythmPhase);

                    const float stepPosition = state.rhythmPhase * 16.0f;
                    const int stepIndex = juce::jlimit(0, 15, (int) std::floor(stepPosition));
                    const bool stepEnabled = getFXGateStepEnabled(variant, stepIndex);

                    float target = 0.0f;
                    if (stepEnabled)
                    {
                        const float stepFrac = stepPosition - (float) stepIndex;

                        if (stepFrac < attackFrac)
                        {
                            target = stepFrac / juce::jmax(0.001f, attackFrac);
                        }
                        else
                        {
                            const float postAttack = (stepFrac - attackFrac) / juce::jmax(0.001f, 1.0f - attackFrac);
                            target = sustainLevel + (1.0f - sustainLevel) * std::exp(-postAttack * decayCurve);
                        }
                    }

                    const float coeff = target > state.gateEnvelope ? attackCoeff : releaseCoeff;
                    state.gateEnvelope = target + coeff * (state.gateEnvelope - target);

                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        const float dry = buffer.getSample(channel, sample);
                        const float wet = dry * state.gateEnvelope;
                        buffer.setSample(channel, sample, dry * (1.0f - mix) + wet * mix);
                    }
                }
            }

            continue;
        }

        if (moduleType == fxFlanger)
        {
            const float rate = getFXSlotFloatValue(slot, 0);
            const float depth = getFXSlotFloatValue(slot, 1);
            const float delay = getFXSlotFloatValue(slot, 2);
            const float feedback = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const bool syncEnabled = getFXSlotVariant(slot) > 0;
            if (mix <= 0.0001f)
                continue;

            copyBufferToScratch();

            auto& flanger = flangerSlots[(size_t) slot];
            flanger.setRate(syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm)
                                        : normalToFXChorusRateHz(rate));
            flanger.setDepth(juce::jlimit(0.05f, 1.0f, 0.08f + depth * 0.92f));
            flanger.setCentreDelay(normalToFXFlangerDelayMs(delay));
            flanger.setFeedback(normalToFXFlangerFeedback(feedback));
            flanger.setMix(1.0f);

            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            flanger.process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    auto* wetData = effectScratch.getWritePointer(channel);
                    const float wet = wetData[sample];
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }

            continue;
        }

        if (moduleType == fxPhaser)
        {
            const float rate = getFXSlotFloatValue(slot, 0);
            const float depth = getFXSlotFloatValue(slot, 1);
            const float feedback = getFXSlotFloatValue(slot, 2);
            const float centre = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const bool syncEnabled = getFXSlotVariant(slot) > 0;
            if (mix <= 0.0001f)
                continue;

            auto& state = phaserStates[(size_t) slot];
            const float rateHz = syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm)
                                             : normalToFXPhaserRateHz(rate);
            const float centreHz = normalToFXPhaserCentreHz(centre);
            const float sweepDepth = 0.08f + depth * 0.90f;
            const float feedbackAmount = juce::jmap(feedback, -0.85f, 0.85f);
            const float stereoOffset = 0.08f + depth * 0.18f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float phase = state.lfoPhase;
                const float leftMod = std::sin(juce::MathConstants<float>::twoPi * phase);
                const float rightMod = std::sin(juce::MathConstants<float>::twoPi * (phase + stereoOffset));
                const float leftFreq = juce::jlimit(80.0f, (float) currentSampleRate * 0.2f, centreHz * (1.0f + leftMod * sweepDepth));
                const float rightFreq = juce::jlimit(80.0f, (float) currentSampleRate * 0.2f, centreHz * (1.0f + rightMod * sweepDepth));
                const float leftG = std::tan(juce::MathConstants<float>::pi * leftFreq / (float) currentSampleRate);
                const float rightG = std::tan(juce::MathConstants<float>::pi * rightFreq / (float) currentSampleRate);
                const float leftCoeff = (1.0f - leftG) / (1.0f + leftG);
                const float rightCoeff = (1.0f - rightG) / (1.0f + rightG);

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];

                    if (channel == 0)
                    {
                        float wet = processPhaserSample(dry, state.left, leftCoeff, state.feedbackLeft);
                        state.feedbackLeft = wet * feedbackAmount;
                        data[sample] = dry * (1.0f - mix) + wet * mix;
                    }
                    else
                    {
                        float wet = processPhaserSample(dry, state.right, rightCoeff, state.feedbackRight);
                        state.feedbackRight = wet * feedbackAmount;
                        data[sample] = dry * (1.0f - mix) + wet * mix;
                    }
                }

                state.lfoPhase += rateHz / (float) currentSampleRate;
                state.lfoPhase -= std::floor(state.lfoPhase);
            }

            continue;
        }

        if (moduleType == fxDelay)
        {
            const float leftTime = getFXSlotFloatValue(slot, 0);
            const float rightTime = getFXSlotFloatValue(slot, 1);
            const float highPass = getFXSlotFloatValue(slot, 2);
            const float lowPass = getFXSlotFloatValue(slot, 3);
            const float feedback = getFXSlotFloatValue(slot, 4);
            const float mix = getFXSlotFloatValue(slot, 5);
            const bool syncEnabled = getFXSlotFloatValue(slot, 6) >= 0.5f;
            const bool linkEnabled = getFXSlotFloatValue(slot, 7) >= 0.5f;
            if (mix <= 0.0001f)
                continue;

            auto& delayState = delayStates[(size_t) slot];
            const int delayBufferSize = juce::jmin((int) delayState.left.size(), (int) delayState.right.size());
            if (delayBufferSize <= 0)
                continue;

            const float effectiveRightTime = linkEnabled ? leftTime : rightTime;
            const float leftDelayMs = syncEnabled ? normalToFXDelaySyncedMs(leftTime, currentHostBpm)
                                                  : normalToFXDelayFreeMs(leftTime);
            const float rightDelayMs = syncEnabled ? normalToFXDelaySyncedMs(effectiveRightTime, currentHostBpm)
                                                   : normalToFXDelayFreeMs(effectiveRightTime);
            const int leftDelaySamples = juce::jlimit(1, delayBufferSize - 1, (int) std::round(leftDelayMs * (float) currentSampleRate / 1000.0f));
            const int rightDelaySamples = juce::jlimit(1, delayBufferSize - 1, (int) std::round(rightDelayMs * (float) currentSampleRate / 1000.0f));
            const float feedbackGain = juce::jlimit(0.0f, 0.95f, 0.08f + feedback * 0.84f);
            const float highPassHz = normalToFXDelayHighPassFrequency(highPass);
            const float lowPassHz = normalToFXDelayLowPassFrequency(lowPass);
            const bool highPassEnabled = highPass > 0.015f;
            const bool lowPassEnabled = lowPass < 0.985f;
            const float dt = 1.0f / (float) juce::jmax(1.0, currentSampleRate);
            const float rc = highPassEnabled ? 1.0f / (juce::MathConstants<float>::twoPi * highPassHz) : 0.0f;
            const float highPassCoeff = highPassEnabled ? rc / (rc + dt) : 0.0f;
            const float lowPassCoeff = lowPassEnabled
                                     ? juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * lowPassHz / (float) currentSampleRate))
                                     : 1.0f;
            const int delayType = juce::jlimit(0, getFXDelayTypeNames().size() - 1, getFXSlotVariant(slot));

            auto processDelayFilter = [&delayState, highPassEnabled, lowPassEnabled, highPassCoeff, lowPassCoeff] (float input, int channel) mutable
            {
                float sample = input;

                if (highPassEnabled)
                {
                    const float hpInput = sample;
                    const float hp = highPassCoeff * (delayState.highpassState[(size_t) channel] + hpInput - delayState.previousHighpassInput[(size_t) channel]);
                    delayState.previousHighpassInput[(size_t) channel] = hpInput;
                    delayState.highpassState[(size_t) channel] = hp;
                    sample = hp;
                }
                else
                {
                    delayState.highpassState[(size_t) channel] = 0.0f;
                    delayState.previousHighpassInput[(size_t) channel] = sample;
                }

                if (lowPassEnabled)
                {
                    delayState.lowpassState[(size_t) channel] += lowPassCoeff * (sample - delayState.lowpassState[(size_t) channel]);
                    sample = delayState.lowpassState[(size_t) channel];
                }
                else
                {
                    delayState.lowpassState[(size_t) channel] = sample;
                }

                return sample;
            };

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const int readLeft = (delayState.writePosition - leftDelaySamples + delayBufferSize) % delayBufferSize;
                const int readRight = (delayState.writePosition - rightDelaySamples + delayBufferSize) % delayBufferSize;
                auto* leftData = buffer.getWritePointer(0);
                auto* rightData = buffer.getWritePointer(numChannels > 1 ? 1 : 0);
                const float dryLeft = leftData[sample];
                const float dryRight = rightData[sample];

                float wetLeft = processDelayFilter(delayState.left[(size_t) readLeft], 0);
                float wetRight = processDelayFilter(delayState.right[(size_t) readRight], juce::jmin(1, numChannels - 1));

                if (delayType == 2)
                {
                    wetLeft = std::tanh(wetLeft * 1.28f) * 0.92f;
                    wetRight = std::tanh(wetRight * 1.28f) * 0.92f;
                }
                else if (delayType == 3)
                {
                    const float diffusedLeft = wetLeft * 0.80f + wetRight * 0.20f;
                    const float diffusedRight = wetRight * 0.80f + wetLeft * 0.20f;
                    wetLeft = diffusedLeft;
                    wetRight = diffusedRight;
                }

                leftData[sample] = dryLeft * (1.0f - mix) + wetLeft * mix;
                rightData[sample] = dryRight * (1.0f - mix) + wetRight * mix;

                float writeLeft = dryLeft;
                float writeRight = dryRight;

                switch (delayType)
                {
                    case 1:
                        writeLeft += wetRight * feedbackGain;
                        writeRight += wetLeft * feedbackGain;
                        break;

                    case 2:
                        writeLeft += std::tanh(wetLeft * (0.9f + feedback * 0.7f)) * feedbackGain;
                        writeRight += std::tanh(wetRight * (0.9f + feedback * 0.7f)) * feedbackGain;
                        break;

                    case 3:
                        writeLeft += (wetLeft * 0.74f + wetRight * 0.26f) * feedbackGain;
                        writeRight += (wetRight * 0.74f + wetLeft * 0.26f) * feedbackGain;
                        break;

                    default:
                        writeLeft += wetLeft * feedbackGain;
                        writeRight += wetRight * feedbackGain;
                        break;
                }

                delayState.left[(size_t) delayState.writePosition] = std::tanh(writeLeft);
                delayState.right[(size_t) delayState.writePosition] = std::tanh(writeRight);
                delayState.writePosition = (delayState.writePosition + 1) % delayBufferSize;
            }

            continue;
        }

        if (moduleType == fxReverb)
        {
            const float size = getFXSlotFloatValue(slot, 0);
            const float damp = getFXSlotFloatValue(slot, 1);
            const float width = getFXSlotFloatValue(slot, 2);
            const float preDelay = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            if (mix <= 0.0001f)
                continue;

            copyBufferToScratch();

            auto& delayState = delayStates[(size_t) slot];
            const int delayBufferSize = juce::jmin((int) delayState.left.size(), (int) delayState.right.size());
            const float preDelayMs = juce::jmap(preDelay, 0.0f, 1.0f, 0.0f, 180.0f);
            const int delaySamples = delayBufferSize > 0
                                   ? juce::jlimit(0, delayBufferSize - 1, (int) std::round(preDelayMs * (float) currentSampleRate / 1000.0f))
                                   : 0;

            auto* wetLeft = effectScratch.getWritePointer(0);
            auto* wetRight = effectScratch.getWritePointer(numChannels > 1 ? 1 : 0);
            const auto* dryLeft = buffer.getReadPointer(0);
            const auto* dryRight = buffer.getReadPointer(numChannels > 1 ? 1 : 0);

            if (delayBufferSize > 0)
            {
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const int readPosition = (delayState.writePosition - delaySamples + delayBufferSize) % delayBufferSize;
                    const float inputLeft = dryLeft[sample];
                    const float inputRight = dryRight[sample];

                    wetLeft[sample] = delaySamples > 0 ? delayState.left[(size_t) readPosition] : inputLeft;
                    wetRight[sample] = delaySamples > 0 ? delayState.right[(size_t) readPosition] : inputRight;

                    delayState.left[(size_t) delayState.writePosition] = inputLeft;
                    delayState.right[(size_t) delayState.writePosition] = inputRight;
                    delayState.writePosition = (delayState.writePosition + 1) % delayBufferSize;
                }
            }

            juce::dsp::Reverb::Parameters parameters;
            parameters.roomSize = juce::jlimit(0.12f, 0.98f, 0.20f + size * 0.74f);
            parameters.damping = juce::jlimit(0.08f, 0.96f, 0.10f + damp * 0.82f);
            parameters.width = juce::jlimit(0.0f, 1.0f, width);
            parameters.wetLevel = 1.0f;
            parameters.dryLevel = 0.0f;
            parameters.freezeMode = 0.0f;

            const int reverbType = juce::jlimit(0, getFXReverbTypeNames().size() - 1, getFXSlotVariant(slot));
            switch (reverbType)
            {
                case 1:
                    parameters.roomSize = juce::jlimit(0.12f, 0.92f, parameters.roomSize * 0.82f);
                    parameters.damping = juce::jlimit(0.08f, 0.90f, parameters.damping * 0.74f);
                    break;

                case 2:
                    parameters.roomSize = juce::jlimit(0.10f, 0.78f, parameters.roomSize * 0.60f);
                    parameters.damping = juce::jlimit(0.18f, 0.90f, 0.48f + damp * 0.32f);
                    parameters.width = juce::jlimit(0.0f, 1.0f, parameters.width * 0.86f);
                    break;

                case 3:
                    parameters.roomSize = juce::jlimit(0.20f, 0.99f, parameters.roomSize + 0.12f);
                    parameters.damping = juce::jlimit(0.05f, 0.84f, parameters.damping * 0.58f);
                    parameters.width = 1.0f;
                    break;

                case 4:
                    parameters.roomSize = juce::jlimit(0.10f, 0.88f, parameters.roomSize * 0.72f);
                    parameters.damping = juce::jlimit(0.40f, 0.98f, 0.72f + damp * 0.22f);
                    parameters.width = juce::jlimit(0.0f, 1.0f, parameters.width * 0.64f);
                    break;

                default:
                    break;
            }

            reverbSlots[(size_t) slot].setParameters(parameters);
            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            reverbSlots[(size_t) slot].process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    auto* wetData = effectScratch.getWritePointer(channel);
                    float wet = wetData[sample];

                    if (reverbType == 3)
                    {
                        const float sparkle = std::sin(wet * (6.0f + size * 7.0f)) * 0.10f;
                        wet = juce::jlimit(-1.0f, 1.0f, wet * 0.90f + std::tanh(wet * 1.4f + sparkle) * 0.24f);
                    }
                    else if (reverbType == 4)
                    {
                        const float steps = 18.0f + size * 34.0f;
                        wet = std::round(wet * steps) / steps;
                        wet *= 0.86f;
                    }

                    auto* data = buffer.getWritePointer(channel);
                    const float dry = data[sample];
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }
        }
    }

    effectModulationActive = false;
}
