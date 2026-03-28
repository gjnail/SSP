#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

namespace reactormod
{
constexpr int lfoCount = 4;
constexpr int lfoPointCount = 4;
constexpr int matrixSlotCount = 64;
constexpr int fxSlotCount = 9;
constexpr int fxParameterCount = 12;
constexpr int fxDestinationCount = fxSlotCount * fxParameterCount;

enum class TriggerMode
{
    retrigger = 0,
    free,
    oneShot
};

enum class LfoType
{
    shape = 0,
    chaosSmooth,
    chaosSteps,
    lorenz
};

struct LfoNode
{
    float x = 0.0f;
    float y = 0.5f;
    float curveToNext = 0.0f;
};

struct DynamicLfoData
{
    juce::String name;
    float rateHz = 2.0f;
    bool syncEnabled = true;
    bool dotted = false;
    int syncDivisionIndex = 4;
    TriggerMode triggerMode = TriggerMode::retrigger;
    LfoType type = LfoType::shape;
    std::vector<LfoNode> nodes;
};

enum class Destination
{
    none = 0,
    filterCutoff,
    filterResonance,
    filterDrive,
    filterEnvAmount,
    ampAttack,
    ampDecay,
    ampSustain,
    ampRelease,
    filterAttack,
    filterDecay,
    filterSustain,
    filterRelease,
    noiseAmount,
    subLevel,
    voiceGlide,
    warpSaturator,
    warpMutate,
    masterSpread,
    masterGain,
    osc1Level,
    osc1Detune,
    osc1Pan,
    osc1WTPos,
    osc1Warp1,
    osc1Warp2,
    osc1Unison,
    osc2Level,
    osc2Detune,
    osc2Pan,
    osc2WTPos,
    osc2Warp1,
    osc2Warp2,
    osc2Unison,
    osc3Level,
    osc3Detune,
    osc3Pan,
    osc3WTPos,
    osc3Warp1,
    osc3Warp2,
    osc3Unison,
    fxParamStart,
    osc1Coarse = fxParamStart + fxDestinationCount,
    osc1WarpFM,
    osc1WarpSync,
    osc1WarpBend,
    osc2Coarse,
    osc2WarpFM,
    osc2WarpSync,
    osc2WarpBend,
    osc3Coarse,
    osc3WarpFM,
    osc3WarpSync,
    osc3WarpBend,
    count
};

inline juce::String getLfoRateParamID(int lfoIndex)
{
    if (lfoIndex == 1)
        return "lfoRate";

    return "lfo" + juce::String(lfoIndex) + "Rate";
}

inline juce::String getLfoPointParamID(int lfoIndex, int pointIndex)
{
    return "lfo" + juce::String(lfoIndex) + "Point" + juce::String(pointIndex);
}

inline juce::String getMatrixSourceParamID(int slotIndex)
{
    return "modSlot" + juce::String(slotIndex) + "Source";
}

inline juce::String getMatrixDestinationParamID(int slotIndex)
{
    return "modSlot" + juce::String(slotIndex) + "Destination";
}

inline juce::String getMatrixAmountParamID(int slotIndex)
{
    return "modSlot" + juce::String(slotIndex) + "Amount";
}

inline const juce::StringArray& getSourceNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray values;
        values.add("Off");
        for (int i = 1; i <= 128; ++i)
            values.add("LFO " + juce::String(i));
        return values;
    }();

    return names;
}

inline const juce::StringArray& getDestinationNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray values
        {
            "Off",
            "Filter Cutoff",
            "Filter Resonance",
            "Filter Drive",
            "Filter Env Amount",
            "Amp Attack",
            "Amp Decay",
            "Amp Sustain",
            "Amp Release",
            "Filter Attack",
            "Filter Decay",
            "Filter Sustain",
            "Filter Release",
            "Noise Amount",
            "Sub Level",
            "Glide",
            "Warp Saturator",
            "Warp Mutate",
            "Master Width",
            "Master Gain",
            "Osc 1 Level",
            "Osc 1 Detune",
            "Osc 1 Pan",
            "Osc 1 WT Pos",
            "Osc 1 Warp 1",
            "Osc 1 Warp 2",
            "Osc 1 Unison",
            "Osc 2 Level",
            "Osc 2 Detune",
            "Osc 2 Pan",
            "Osc 2 WT Pos",
            "Osc 2 Warp 1",
            "Osc 2 Warp 2",
            "Osc 2 Unison",
            "Osc 3 Level",
            "Osc 3 Detune",
            "Osc 3 Pan",
            "Osc 3 WT Pos",
            "Osc 3 Warp 1",
            "Osc 3 Warp 2",
            "Osc 3 Unison"
        };

        for (int slotIndex = 0; slotIndex < fxSlotCount; ++slotIndex)
            for (int parameterIndex = 0; parameterIndex < fxParameterCount; ++parameterIndex)
                values.add("FX " + juce::String(slotIndex + 1) + " Param " + juce::String(parameterIndex + 1));

        values.add("Osc 1 Coarse");
        values.add("Osc 1 FM");
        values.add("Osc 1 Sync");
        values.add("Osc 1 Bend");
        values.add("Osc 2 Coarse");
        values.add("Osc 2 FM");
        values.add("Osc 2 Sync");
        values.add("Osc 2 Bend");
        values.add("Osc 3 Coarse");
        values.add("Osc 3 FM");
        values.add("Osc 3 Sync");
        values.add("Osc 3 Bend");

        return values;
    }();

    return names;
}

inline bool isFXDestination(Destination destination) noexcept
{
    const int index = (int) destination;
    return index >= (int) Destination::fxParamStart
        && index < (int) Destination::fxParamStart + fxDestinationCount;
}

inline Destination makeFXDestination(int slotIndex, int parameterIndex) noexcept
{
    if (! juce::isPositiveAndBelow(slotIndex, fxSlotCount)
        || ! juce::isPositiveAndBelow(parameterIndex, fxParameterCount))
        return Destination::none;

    return static_cast<Destination>((int) Destination::fxParamStart
                                    + slotIndex * fxParameterCount
                                    + parameterIndex);
}

inline int getFXSlotIndexForDestination(Destination destination) noexcept
{
    if (! isFXDestination(destination))
        return -1;

    return ((int) destination - (int) Destination::fxParamStart) / fxParameterCount;
}

inline int getFXParameterIndexForDestination(Destination destination) noexcept
{
    if (! isFXDestination(destination))
        return -1;

    return ((int) destination - (int) Destination::fxParamStart) % fxParameterCount;
}

inline Destination destinationFromChoiceIndex(int index)
{
    return static_cast<Destination>(juce::jlimit(0, (int) Destination::count - 1, index));
}

inline int destinationToChoiceIndex(Destination destination)
{
    return juce::jlimit(0, (int) Destination::count - 1, (int) destination);
}

inline juce::String getParameterIDForDestination(Destination destination)
{
    if (isFXDestination(destination))
    {
        const int slotIndex = getFXSlotIndexForDestination(destination);
        const int parameterIndex = getFXParameterIndexForDestination(destination);
        if (slotIndex >= 0 && parameterIndex >= 0)
            return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
    }

    switch (destination)
    {
        case Destination::filterCutoff:    return "filterCutoff";
        case Destination::filterResonance: return "filterResonance";
        case Destination::filterDrive:     return "filterDrive";
        case Destination::filterEnvAmount: return "filterEnvAmount";
        case Destination::ampAttack:       return "ampAttack";
        case Destination::ampDecay:        return "ampDecay";
        case Destination::ampSustain:      return "ampSustain";
        case Destination::ampRelease:      return "ampRelease";
        case Destination::filterAttack:    return "filterAttack";
        case Destination::filterDecay:     return "filterDecay";
        case Destination::filterSustain:   return "filterSustain";
        case Destination::filterRelease:   return "filterRelease";
        case Destination::noiseAmount:     return "noiseAmount";
        case Destination::subLevel:        return "subLevel";
        case Destination::voiceGlide:      return "voiceGlide";
        case Destination::warpSaturator:   return "warpSaturator";
        case Destination::warpMutate:      return "warpMutate";
        case Destination::masterSpread:    return "masterSpread";
        case Destination::masterGain:      return "masterGain";
        case Destination::osc1Level:       return "osc1Level";
        case Destination::osc1Detune:      return "osc1Detune";
        case Destination::osc1Pan:         return "osc1Pan";
        case Destination::osc1WTPos:       return "osc1WTPos";
        case Destination::osc1Warp1:       return "osc1Warp1Amount";
        case Destination::osc1Warp2:       return "osc1Warp2Amount";
        case Destination::osc1Unison:      return "osc1Unison";
        case Destination::osc2Level:       return "osc2Level";
        case Destination::osc2Detune:      return "osc2Detune";
        case Destination::osc2Pan:         return "osc2Pan";
        case Destination::osc2WTPos:       return "osc2WTPos";
        case Destination::osc2Warp1:       return "osc2Warp1Amount";
        case Destination::osc2Warp2:       return "osc2Warp2Amount";
        case Destination::osc2Unison:      return "osc2Unison";
        case Destination::osc3Level:       return "osc3Level";
        case Destination::osc3Detune:      return "osc3Detune";
        case Destination::osc3Pan:         return "osc3Pan";
        case Destination::osc3WTPos:       return "osc3WTPos";
        case Destination::osc3Warp1:       return "osc3Warp1Amount";
        case Destination::osc3Warp2:       return "osc3Warp2Amount";
        case Destination::osc3Unison:      return "osc3Unison";
        case Destination::osc1Coarse:      return "osc1Coarse";
        case Destination::osc1WarpFM:      return "osc1WarpFM";
        case Destination::osc1WarpSync:    return "osc1WarpSync";
        case Destination::osc1WarpBend:    return "osc1WarpBend";
        case Destination::osc2Coarse:      return "osc2Coarse";
        case Destination::osc2WarpFM:      return "osc2WarpFM";
        case Destination::osc2WarpSync:    return "osc2WarpSync";
        case Destination::osc2WarpBend:    return "osc2WarpBend";
        case Destination::osc3Coarse:      return "osc3Coarse";
        case Destination::osc3WarpFM:      return "osc3WarpFM";
        case Destination::osc3WarpSync:    return "osc3WarpSync";
        case Destination::osc3WarpBend:    return "osc3WarpBend";
        case Destination::fxParamStart:
        case Destination::none:
        case Destination::count:
        default:
            break;
    }

    return {};
}

inline Destination destinationForParameterID(const juce::String& parameterID)
{
    if (parameterID.startsWith("fxSlot"))
    {
        const auto slotAndParam = parameterID.substring(6);
        const int paramTokenIndex = slotAndParam.indexOf("Param");
        if (paramTokenIndex > 0)
        {
            const int slotIndex = slotAndParam.substring(0, paramTokenIndex).getIntValue() - 1;
            const int parameterIndex = slotAndParam.substring(paramTokenIndex + 5).getIntValue() - 1;
            return makeFXDestination(slotIndex, parameterIndex);
        }
    }

    for (int destinationIndex = 1; destinationIndex < (int) Destination::count; ++destinationIndex)
    {
        const auto destination = static_cast<Destination>(destinationIndex);
        if (getParameterIDForDestination(destination) == parameterID)
            return destination;
    }

    return Destination::none;
}

struct SyncDivisionDefinition
{
    const char* label = "1/4";
    float beats = 1.0f;
};

inline const std::vector<SyncDivisionDefinition>& getSyncDivisionDefinitions()
{
    static const std::vector<SyncDivisionDefinition> divisions
    {
        { "1/64", 0.0625f },
        { "1/32T", 1.0f / 12.0f },
        { "1/32", 0.125f },
        { "1/16T", 1.0f / 6.0f },
        { "1/16", 0.25f },
        { "1/8T", 1.0f / 3.0f },
        { "1/8", 0.5f },
        { "1/4T", 2.0f / 3.0f },
        { "1/4", 1.0f },
        { "1/2T", 4.0f / 3.0f },
        { "1/2", 2.0f },
        { "1 Bar", 4.0f },
        { "2 Bars", 8.0f },
        { "4 Bars", 16.0f }
    };

    return divisions;
}

inline juce::String getSyncDivisionName(int index, bool dotted = false)
{
    const auto& divisions = getSyncDivisionDefinitions();
    if (divisions.empty())
        return {};

    const auto& division = divisions[(size_t) juce::jlimit(0, (int) divisions.size() - 1, index)];
    auto name = juce::String(division.label);
    if (dotted)
        name << ".";

    return name;
}

inline juce::StringArray getSyncDivisionNames(bool dotted = false)
{
    juce::StringArray names;
    for (int i = 0; i < (int) getSyncDivisionDefinitions().size(); ++i)
        names.add(getSyncDivisionName(i, dotted));

    return names;
}

inline DynamicLfoData makeDefaultLfo(int lfoIndex)
{
    DynamicLfoData data;
    data.name = "LFO " + juce::String(lfoIndex);
    data.rateHz = 2.0f;
    data.syncEnabled = true;
    data.dotted = false;
    data.syncDivisionIndex = 4;
    data.triggerMode = TriggerMode::retrigger;
    data.type = LfoType::shape;
    data.nodes = {
        { 0.0f, 0.5f, 0.0f },
        { 0.25f, 1.0f, 0.0f },
        { 0.75f, 0.0f, 0.0f },
        { 1.0f, 0.5f, 0.0f }
    };
    return data;
}

inline std::array<float, (size_t) lfoPointCount> defaultShapeForIndex(int lfoIndex)
{
    const auto data = makeDefaultLfo(lfoIndex);
    std::array<float, (size_t) lfoPointCount> shape{};

    for (size_t i = 0; i < shape.size(); ++i)
    {
        if (i < data.nodes.size())
            shape[i] = juce::jlimit(0.0f, 1.0f, data.nodes[i].y);
        else
            shape[i] = 0.5f;
    }

    return shape;
}

inline float applyCurve(float t, float curve) noexcept
{
    const float c = juce::jlimit(-0.95f, 0.95f, curve);
    if (std::abs(c) < 0.0001f)
        return juce::jlimit(0.0f, 1.0f, t);

    if (c > 0.0f)
        return std::pow(juce::jlimit(0.0f, 1.0f, t), 1.0f + c * 3.5f);

    return 1.0f - std::pow(1.0f - juce::jlimit(0.0f, 1.0f, t), 1.0f + (-c) * 3.5f);
}

inline float evaluateShape(const std::vector<LfoNode>& nodes, float phase) noexcept
{
    if (nodes.empty())
        return 0.0f;

    if (nodes.size() == 1)
        return juce::jmap(juce::jlimit(0.0f, 1.0f, nodes.front().y), -1.0f, 1.0f);

    const float wrapped = phase - std::floor(phase);
    for (size_t i = 0; i + 1 < nodes.size(); ++i)
    {
        const auto& a = nodes[i];
        const auto& b = nodes[i + 1];
        if (wrapped <= b.x || i + 2 == nodes.size())
        {
            const float width = juce::jmax(0.0001f, b.x - a.x);
            const float t = juce::jlimit(0.0f, 1.0f, (wrapped - a.x) / width);
            const float curvedT = applyCurve(t, a.curveToNext);
            return juce::jmap(curvedT,
                              juce::jmap(juce::jlimit(0.0f, 1.0f, a.y), -1.0f, 1.0f),
                              juce::jmap(juce::jlimit(0.0f, 1.0f, b.y), -1.0f, 1.0f));
        }
    }

    return juce::jmap(juce::jlimit(0.0f, 1.0f, nodes.back().y), -1.0f, 1.0f);
}

inline bool isChaosType(LfoType type) noexcept
{
    return type == LfoType::chaosSmooth
        || type == LfoType::chaosSteps
        || type == LfoType::lorenz;
}

inline float hashChaosValue(int index, int seed) noexcept
{
    const float x = std::sin((float) (index * 37 + seed * 73) * 12.9898f + 78.233f) * 43758.5453f;
    return (x - std::floor(x)) * 2.0f - 1.0f;
}

inline float evaluateChaosStepped(float phase, int seed) noexcept
{
    constexpr int segments = 12;
    const float wrapped = phase - std::floor(phase);
    const int index = juce::jlimit(0, segments - 1, (int) std::floor(wrapped * (float) segments));
    return hashChaosValue(index, seed);
}

inline float evaluateChaosSmooth(float phase, int seed) noexcept
{
    constexpr int segments = 10;
    const float wrapped = phase - std::floor(phase);
    const float scaled = wrapped * (float) segments;
    const int indexA = (int) std::floor(scaled);
    const int indexB = indexA + 1;
    const float t = juce::jlimit(0.0f, 1.0f, scaled - (float) indexA);
    const float smoothT = t * t * (3.0f - 2.0f * t);
    return juce::jmap(smoothT,
                      hashChaosValue(indexA, seed + 11),
                      hashChaosValue(indexB, seed + 11));
}

inline float evaluateLorenzChaos(float phase, int seed) noexcept
{
    constexpr int lorenzPointCount = 512;
    static const std::array<float, 512> baseTable = []
    {
        std::array<float, 512> values{};
        constexpr int tablePointCount = 512;
        constexpr float sigma = 10.0f;
        constexpr float rho = 28.0f;
        constexpr float beta = 8.0f / 3.0f;
        constexpr float dt = 0.005f;

        float x = 0.12f;
        float y = 0.0f;
        float z = 24.0f;

        for (int i = 0; i < 4000; ++i)
        {
            const float dx = sigma * (y - x);
            const float dy = x * (rho - z) - y;
            const float dz = x * y - beta * z;
            x += dx * dt;
            y += dy * dt;
            z += dz * dt;
        }

        float minValue = std::numeric_limits<float>::max();
        float maxValue = std::numeric_limits<float>::lowest();
        for (int i = 0; i < tablePointCount; ++i)
        {
            for (int step = 0; step < 3; ++step)
            {
                const float dx = sigma * (y - x);
                const float dy = x * (rho - z) - y;
                const float dz = x * y - beta * z;
                x += dx * dt;
                y += dy * dt;
                z += dz * dt;
            }

            values[(size_t) i] = x;
            minValue = juce::jmin(minValue, x);
            maxValue = juce::jmax(maxValue, x);
        }

        const float range = juce::jmax(0.0001f, maxValue - minValue);
        for (auto& value : values)
            value = juce::jmap((value - minValue) / range, -1.0f, 1.0f);

        return values;
    }();

    const float wrapped = phase - std::floor(phase);
    const float seedOffset = std::fmod((float) seed * 0.173f, 1.0f);
    const float position = (wrapped + seedOffset - std::floor(wrapped + seedOffset)) * (float) (lorenzPointCount - 1);
    const int indexA = juce::jlimit(0, lorenzPointCount - 1, (int) std::floor(position));
    const int indexB = juce::jlimit(0, lorenzPointCount - 1, indexA + 1);
    const float t = juce::jlimit(0.0f, 1.0f, position - (float) indexA);
    const float smoothT = t * t * (3.0f - 2.0f * t);
    return juce::jmap(smoothT, baseTable[(size_t) indexA], baseTable[(size_t) indexB]);
}

inline float evaluateLfoValue(const DynamicLfoData& data, float phase, int seed = 0) noexcept
{
    switch (data.type)
    {
        case LfoType::chaosSmooth:
            return evaluateChaosSmooth(phase, seed);

        case LfoType::chaosSteps:
            return evaluateChaosStepped(phase, seed + 23);

        case LfoType::lorenz:
            return evaluateLorenzChaos(phase, seed + 41);

        case LfoType::shape:
        default:
            return evaluateShape(data.nodes, phase);
    }
}

inline float syncDivisionToBeats(int index, bool dotted = false) noexcept
{
    const auto& divisions = getSyncDivisionDefinitions();
    if (divisions.empty())
        return 1.0f;

    float beats = divisions[(size_t) juce::jlimit(0, (int) divisions.size() - 1, index)].beats;
    if (dotted)
        beats *= 1.5f;

    return beats;
}

inline float computeLfoRateHz(const DynamicLfoData& data, double bpm) noexcept
{
    if (! data.syncEnabled)
        return juce::jlimit(0.05f, 20.0f, data.rateHz);

    float beats = syncDivisionToBeats(data.syncDivisionIndex, data.dotted);

    const float beatsPerSecond = (float) juce::jmax(1.0, bpm) / 60.0f;
    return juce::jlimit(0.01f, 20.0f, beatsPerSecond / juce::jmax(0.03125f, beats));
}

inline const juce::StringArray& getTriggerModeNames()
{
    static const juce::StringArray names
    {
        "Retrigger",
        "Free",
        "One Shot"
    };
    return names;
}

inline const juce::StringArray& getLfoTypeNames()
{
    static const juce::StringArray names
    {
        "Shape",
        "Chaos Smooth",
        "Chaos Steps",
        "Lorenz"
    };
    return names;
}
}
