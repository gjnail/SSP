#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String getFXOrderParamID(int slotIndex)
{
    return "fxOrder" + juce::String(slotIndex + 1);
}

juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotFloatParamID(int slotIndex, int parameterIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
}

juce::String getFXSlotVariantParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Variant";
}

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
constexpr float detectorFloorDb = -120.0f;

struct FXBitcrusherMode
{
    const char* name;
    float bitBias;
    float rateBias;
    float jitterBias;
    float toneBias;
    float outputTrimDb;
};

const std::array<FXBitcrusherMode, 5>& getFXBitcrusherModes()
{
    static const std::array<FXBitcrusherMode, 5> modes{{
        { "Smooth", 0.12f, 0.10f, 0.04f, 0.18f, -0.5f },
        { "Crunch", 0.28f, 0.24f, 0.08f, -0.02f, -1.0f },
        { "Retro",  0.40f, 0.42f, 0.14f, -0.14f, -1.5f },
        { "Digital",0.18f, 0.54f, 0.10f, 0.12f, -1.2f },
        { "Destroy",0.60f, 0.72f, 0.22f, -0.24f, -2.0f }
    }};
    return modes;
}

struct FactoryPresetDefinition
{
    const char* category;
    const char* name;
    const char* description;
};

const std::array<FactoryPresetDefinition, 28>& getFactoryPresetDefinitions()
{
    static const std::array<FactoryPresetDefinition, 28> definitions{{
        { "Drums", "Punchy Drums", "Compressed, lightly driven transient punch with an EQ and short-room body lane." },
        { "Drums", "Parallel Smash", "Heavy transient crush blended against a cleaner body lane for true parallel drum impact." },
        { "Drums", "Drum Crush", "Crushed transient hits blended against a controlled body lane for aggressive loop energy." },
        { "Drums", "Snare Designer", "Shapes the crack separately from the ring so a snare can hit and bloom independently." },
        { "Drums", "Hard Clap Driver", "High-band triggering pushes clap crack and drive while keeping the tail shaped and musical." },
        { "Drums", "Kick To Rumble", "Pushes the kick click separately from a darker, longer low-end tail lane." },
        { "Drums", "Pumped Loop Split", "Loop-friendly split with controlled transient bite and a moving, pumped body lane." },
        { "Drums", "Hat Tamer", "Targets harsh high-frequency attacks so hats and cymbals keep body without painful front-edge spikes." },
        { "Drums", "Room Tail Lift", "Tightens the hit and lifts the roomy sustain so shells and percussion bloom wider behind the attack." },
        { "Synths", "Wide Sustain", "Keeps attacks centered while the sustain lane widens and swirls." },
        { "Synths", "Center Punch / Wide Tail", "Focused center transient image with a wider, more spacious sustain lane behind it." },
        { "Synths", "Pitch Tail", "Leaves the attack natural and pushes the tail into pitched ambience." },
        { "Synths", "Tremolo Body", "Steady transient definition up front with a pulsing, filtered sustain lane." },
        { "Synths", "Pluck Polish", "Keeps pluck articulation crisp while the body lane adds width and a polished halo." },
        { "Synths", "Pad Bloom", "De-emphasizes the front edge so the sustain lane can widen, detune, and wash out behind the note." },
        { "Synths", "Acid Tail Motion", "Leaves the stab or note attack readable while the tail moves through resonant, animated filtering." },
        { "Basses", "808 Bounce", "808 front edge stays controlled while the body lane bounces, saturates, and flexes underneath." },
        { "Basses", "Hard Techno Pogo", "Hard-techno bounce with aggressive transient drive and a body lane that ducks then rebounds." },
        { "Basses", "Sub Tightener", "Preserves the front of the note while compressing and shortening the sustained low-end bloom." },
        { "Basses", "Bass Click Control", "Tames string or pick click on the transient lane while the body keeps weight and growl." },
        { "Basses", "Reese Width Split", "Keeps the bass front mono and focused while the sustain spreads and detunes outward." },
        { "Basses", "Grit Tail Bass", "Lets the transient stay legible while the sustain lane takes the heavier drive and tone shaping." },
        { "FX", "Lo-fi Body", "Leaves the transient lane clean while the body is degraded and low-passed." },
        { "FX", "Gate Chop", "Lets the transient speak while the body is aggressively chopped down." },
        { "FX", "Riser Bloom", "Keeps the start readable, then explodes the tail into wide, pitched, shimmering movement." },
        { "FX", "Impact Tail Designer", "Separates the smack from the cinematic body so impacts stay punchy while the tail blooms large." },
        { "FX", "Glitch Tail", "Protects the initial hit while the tail lane gets chewed up, pulsed, and filtered into motion." },
        { "Utility", "Init", "Empty default state with both chains cleared and ready to build from scratch." }
    }};
    return definitions;
}

const juce::StringArray& getFactoryPresetNameList()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& preset : getFactoryPresetDefinitions())
            result.add(preset.name);
        return result;
    }();
    return names;
}

const juce::StringArray& getFactoryPresetCategoryList()
{
    static const juce::StringArray categories = []
    {
        juce::StringArray result;
        for (const auto& preset : getFactoryPresetDefinitions())
            result.addIfNotAlreadyThere(preset.category);
        return result;
    }();
    return categories;
}

juce::String getFactoryPresetDescriptionForIndex(int presetIndex)
{
    if (juce::isPositiveAndBelow(presetIndex, (int) getFactoryPresetDefinitions().size()))
        return getFactoryPresetDefinitions()[(size_t) presetIndex].description;

    return {};
}

juce::String getFactoryPresetCategoryForIndex(int presetIndex)
{
    if (juce::isPositiveAndBelow(presetIndex, (int) getFactoryPresetDefinitions().size()))
        return getFactoryPresetDefinitions()[(size_t) presetIndex].category;

    return {};
}

constexpr auto userPresetExtension = ".ssptspreset";

juce::String sanitisePresetName(const juce::String& presetName)
{
    auto safe = presetName.trim().retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_() ");
    if (safe.isEmpty())
        safe = "Preset";
    return safe;
}

int getFXVariantLimit(int moduleType)
{
    switch (moduleType)
    {
        case PluginProcessor::fxDrive: return PluginProcessor::getFXDriveTypeNames().size();
        case PluginProcessor::fxFilter: return PluginProcessor::getFXFilterTypeNames().size();
        case PluginProcessor::fxChorus: return 2;
        case PluginProcessor::fxFlanger: return 2;
        case PluginProcessor::fxPhaser: return 2;
        case PluginProcessor::fxDelay: return PluginProcessor::getFXDelayTypeNames().size();
        case PluginProcessor::fxReverb: return PluginProcessor::getFXReverbTypeNames().size();
        case PluginProcessor::fxCompressor: return PluginProcessor::getFXCompressorTypeNames().size();
        case PluginProcessor::fxEQ: return fxEQVariantLimit;
        case PluginProcessor::fxMultiband: return 1;
        case PluginProcessor::fxBitcrusher: return PluginProcessor::getFXBitcrusherTypeNames().size();
        case PluginProcessor::fxImager: return 1;
        case PluginProcessor::fxShift: return PluginProcessor::getFXShiftTypeNames().size();
        case PluginProcessor::fxTremolo: return 2;
        case PluginProcessor::fxGate: return fxEQVariantLimit;
        default: return 1;
    }
}

float getRawFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback = 0.0f)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();

    return fallback;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return param->getIndex();

    return juce::roundToInt(getRawFloatParam(apvts, id));
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(value ? 1.0f : 0.0f);
}

void setFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
        param->setValueNotifyingHost(param->convertTo0to1(value));
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        choice->setValueNotifyingHost(choice->convertTo0to1(value));
    else if (auto* integerParam = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id)))
        integerParam->setValueNotifyingHost(integerParam->convertTo0to1(value));
}

juce::IIRCoefficients makeIdentityCoefficients()
{
    return { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
}

float msToCoeff(float ms, double sampleRate)
{
    return std::exp(-1.0f / juce::jmax(1.0f, (float) sampleRate * juce::jmax(0.0001f, ms * 0.001f)));
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
        { 0.125f }, { 1.0f / 6.0f }, { 0.25f }, { 1.0f / 3.0f }, { 0.5f }, { 2.0f / 3.0f },
        { 1.0f }, { 4.0f / 3.0f }, { 2.0f }, { 4.0f }, { 8.0f }, { 16.0f }
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
        { 0.125f }, { 1.0f / 6.0f }, { 0.25f }, { 1.0f / 3.0f }, { 0.5f }, { 2.0f / 3.0f },
        { 1.0f }, { 4.0f / 3.0f }, { 2.0f }, { 4.0f }, { 8.0f }, { 12.0f }, { 16.0f }
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

int withFXGateStepEnabled(int variant, int stepIndex, bool enabled)
{
    const int mask = 0x1 << (stepIndex + 1);
    if (enabled)
        return variant | mask;

    return variant & ~mask;
}

bool getFXGateStepEnabled(int variant, int stepIndex)
{
    return ((variant >> (stepIndex + 1)) & 0x1) != 0;
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
        { 0.125f }, { 1.0f / 6.0f }, { 0.25f }, { 1.0f / 3.0f }, { 0.5f },
        { 2.0f / 3.0f }, { 1.0f }, { 2.0f }, { 4.0f }, { 8.0f }
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
        default: return juce::IIRCoefficients::makePeakFilter(sampleRate, safeFrequency, safeQ, gain);
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

void applyPan(float pan, float& leftGain, float& rightGain)
{
    const float clamped = juce::jlimit(-1.0f, 1.0f, pan);
    leftGain = clamped > 0.0f ? 1.0f - clamped : 1.0f;
    rightGain = clamped < 0.0f ? 1.0f + clamped : 1.0f;
}

float dbFollowerUpdate(float inputDb, float currentDb, float attackCoeff, float releaseCoeff)
{
    const float coeff = inputDb > currentDb ? attackCoeff : releaseCoeff;
    return inputDb + coeff * (currentDb - inputDb);
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputGainDb", "Input Gain",
                                                                 juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sensitivity", "Sensitivity",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("speedMs", "Speed",
                                                                 juce::NormalisableRange<float>(0.01f, 5.0f, 0.001f, 0.3f),
                                                                 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("holdMs", "Hold",
                                                                 juce::NormalisableRange<float>(0.0f, 50.0f, 0.01f, 0.35f),
                                                                 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("releaseMs", "Release",
                                                                 juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.35f),
                                                                 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lookaheadMs", "Lookahead",
                                                                 juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f, 0.35f),
                                                                 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("triggerFilter", "Trigger Filter",
                                                                  juce::StringArray{"Full", "Bass", "Highs"},
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sidechainHPFHz", "Sidechain HPF",
                                                                 juce::NormalisableRange<float>(20.0f, 500.0f, 0.1f, 0.35f),
                                                                 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("focus", "Focus",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("splitMode", "Split Mode",
                                                                  juce::StringArray{ "Auto", "Assisted", "Manual" },
                                                                  splitModeAuto));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("captureLength", "Capture Length",
                                                                  juce::StringArray{ "250 ms", "500 ms", "1 Second", "1 Bar", "2 Bars" },
                                                                  2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("manualTransientStartMs", "Manual Transient Start",
                                                                 juce::NormalisableRange<float>(0.0f, splitEditorCaptureDurationMs, 0.01f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("manualTransientEndMs", "Manual Transient End",
                                                                 juce::NormalisableRange<float>(0.0f, splitEditorCaptureDurationMs, 0.01f),
                                                                 12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("manualBodyStartMs", "Manual Body Start",
                                                                 juce::NormalisableRange<float>(0.0f, splitEditorCaptureDurationMs, 0.01f),
                                                                 22.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("manualBodyEndMs", "Manual Body End",
                                                                 juce::NormalisableRange<float>(0.0f, splitEditorCaptureDurationMs, 0.01f),
                                                                 320.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("transientLevelDb", "Transient Level",
                                                                 juce::NormalisableRange<float>(-100.0f, 12.0f, 0.1f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bodyLevelDb", "Body Level",
                                                                 juce::NormalisableRange<float>(-100.0f, 12.0f, 0.1f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("transientPan", "Transient Pan",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("bodyPan", "Body Pan",
                                                                 juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("transientMute", "Transient Mute", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bodyMute", "Body Mute", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("transientSolo", "Transient Solo", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bodySolo", "Body Solo", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dryWet", "Dry Wet",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pogoAmount", "POGO",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputGainDb", "Output Gain",
                                                                 juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
                                                                 0.0f));

    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        const bool isTransientSlot = slot < slotsPerPath;
        const int displaySlot = (slot % slotsPerPath) + 1;
        const auto prefix = (isTransientSlot ? "Transient" : "Body") + juce::String(" > Slot ") + juce::String(displaySlot) + " > ";

        params.push_back(std::make_unique<juce::AudioParameterChoice>(getFXOrderParamID(slot),
                                                                      prefix + "Effect",
                                                                      getFXModuleNames(),
                                                                      fxOff));
        params.push_back(std::make_unique<juce::AudioParameterBool>(getFXSlotOnParamID(slot),
                                                                    prefix + "Enabled",
                                                                    false));

        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(getFXSlotFloatParamID(slot, parameterIndex),
                                                                         prefix + "Control " + juce::String(parameterIndex + 1),
                                                                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                         getFXSlotDefaultValue(parameterIndex)));
        }

        params.push_back(std::make_unique<juce::AudioParameterInt>(getFXSlotVariantParamID(slot),
                                                                   prefix + "Variant",
                                                                   0,
                                                                   fxEQVariantLimit - 1,
                                                                   0));
    }

    return { params.begin(), params.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto& meter : compressorGainReductionMeters)
        meter.store(0.0f);

    for (auto& group : multibandUpwardMeters)
        for (auto& meter : group)
            meter.store(0.0f);

    for (auto& group : multibandDownwardMeters)
        for (auto& meter : group)
            meter.store(0.0f);

    for (auto& history : waveformHistory)
        history.store(0.0f);

    for (auto& history : transientHistory)
        history.store(0.0f);

    for (auto& history : bodyHistory)
        history.store(1.0f);

    for (auto& history : beforeSignalHistory)
        history.store(0.0f);

    for (auto& history : afterSignalHistory)
        history.store(0.0f);

    resetSplitterParametersToDefaults();
    storeABState(0);
    storeABState(1);
}

PluginProcessor::~PluginProcessor() = default;

const juce::String& PluginProcessor::getCurrentPresetName() const noexcept
{
    return currentPresetName;
}

juce::String PluginProcessor::getCurrentPresetDescription() const
{
    const auto& factoryNames = getFactoryPresetNames();
    if (const int index = factoryNames.indexOf(currentPresetName); index >= 0)
        return getFactoryPresetDescriptionForIndex(index);

    if (currentPresetName.equalsIgnoreCase("Init"))
        return "Empty default state with both chains cleared and ready to build from scratch.";

    return "User preset recall.";
}

int PluginProcessor::getActiveABSlot() const noexcept
{
    return juce::jlimit(0, 1, activeABSlot.load());
}

bool PluginProcessor::isABStateValid(int slotIndex) const noexcept
{
    return juce::isPositiveAndBelow(slotIndex, (int) abStateValid.size()) && abStateValid[(size_t) slotIndex];
}

const juce::StringArray& PluginProcessor::getFactoryPresetNames()
{
    return getFactoryPresetNameList();
}

const juce::StringArray& PluginProcessor::getFactoryPresetCategories()
{
    return getFactoryPresetCategoryList();
}

juce::String PluginProcessor::getFactoryPresetCategory(int presetIndex)
{
    return getFactoryPresetCategoryForIndex(presetIndex);
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
    return moduleType == fxOff || moduleType == fxDrive || moduleType == fxFilter || moduleType == fxChorus
        || moduleType == fxFlanger || moduleType == fxPhaser || moduleType == fxDelay || moduleType == fxReverb
        || moduleType == fxCompressor || moduleType == fxEQ || moduleType == fxMultiband || moduleType == fxBitcrusher
        || moduleType == fxImager || moduleType == fxShift || moduleType == fxTremolo || moduleType == fxGate;
}

const juce::StringArray& PluginProcessor::getFXFilterTypeNames()
{
    static const juce::StringArray names{
        "Low 12", "Low 24", "High 12", "High 24", "Band 12", "Band 24", "Notch", "Peak", "Low Shelf",
        "High Shelf", "All Pass", "Comb +", "Comb -", "Formant", "Vowel A", "Vowel E", "Vowel I", "Vowel O",
        "Vowel U", "Ladder Drive"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXDriveTypeNames()
{
    static const juce::StringArray names{
        "Soft Clip", "Hard Clip", "Tube", "Foldback", "Fuzz", "Diode", "Sine Drive", "Speaker Crush"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getFXEQTypeNames()
{
    static const juce::StringArray names{ "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut" };
    return names;
}

const juce::StringArray& PluginProcessor::getFXReverbTypeNames()
{
    static const juce::StringArray names{ "Hall", "Plate", "Room", "Shimmer", "Lo-Fi" };
    return names;
}

const juce::StringArray& PluginProcessor::getFXDelayTypeNames()
{
    static const juce::StringArray names{ "Stereo", "Ping Pong", "Tape", "Diffused" };
    return names;
}

const juce::StringArray& PluginProcessor::getFXCompressorTypeNames()
{
    static const juce::StringArray names{ "4:1", "8:1", "12:1", "20:1", "All" };
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
    static const juce::StringArray names{ "Microshift", "Dual Detune", "Ring Mod", "Freq Shift" };
    return names;
}

const juce::StringArray& PluginProcessor::getFXGateTypeNames()
{
    static const juce::StringArray names{ "Gate", "Rhythmic Gate" };
    return names;
}

PluginProcessor::DestinationModulationInfo PluginProcessor::getDestinationModulationInfo(reactormod::Destination, int) const
{
    return {};
}

int PluginProcessor::getSelectedModulationSourceIndex() const noexcept
{
    return selectedModulationSourceIndex.load();
}

void PluginProcessor::setSelectedModulationSourceIndex(int sourceIndex) noexcept
{
    selectedModulationSourceIndex.store(sourceIndex);
}

int PluginProcessor::assignSourceToDestination(int, reactormod::Destination, float)
{
    return -1;
}

void PluginProcessor::setMatrixAmountForSlot(int, float)
{
}

int PluginProcessor::getSplitModeIndex() const noexcept
{
    return juce::jlimit(0, 2, getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "splitMode"));
}

float PluginProcessor::getConfiguredSplitEditorCaptureDurationMs() const noexcept
{
    const int captureLengthMode = juce::jlimit(0, 4, getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "captureLength"));

    switch (captureLengthMode)
    {
        case 0: return 250.0f;
        case 1: return 500.0f;
        case 3:
        {
            const float bpm = (float) juce::jmax(40.0, currentHostBpm);
            return 240000.0f / bpm;
        }
        case 4:
        {
            const float bpm = (float) juce::jmax(40.0, currentHostBpm);
            return 480000.0f / bpm;
        }
        case 2:
        default:
            return 1000.0f;
    }
}

void PluginProcessor::updateSplitEditorCaptureWindow() noexcept
{
    const float captureDurationMs = juce::jlimit(250.0f, splitEditorCaptureDurationMs, getConfiguredSplitEditorCaptureDurationMs());
    splitEditorCurrentCaptureDurationMs.store(captureDurationMs);

    if (currentSampleRate <= 0.0 || splitEditorCaptureMaxLengthSamples <= 0)
        return;

    splitEditorCaptureLengthSamples = juce::jlimit(512,
                                                   splitEditorCaptureMaxLengthSamples,
                                                   juce::roundToInt(captureDurationMs * 0.001f * (float) currentSampleRate));
    splitEditorLiveStride = juce::jmax(1, splitEditorCaptureLengthSamples / splitEditorDisplayPointCount);
    splitEditorLiveCounter = splitEditorLiveStride;
}

void PluginProcessor::armSplitEditorCapture() noexcept
{
    updateSplitEditorCaptureWindow();
    splitEditorCaptureArmed.store(true);
    splitEditorCaptureRecording = false;
    splitEditorCaptureWritePosition = 0;
    splitEditorCaptureBuffer.clear();
}

void PluginProcessor::clearSplitEditorCapture() noexcept
{
    updateSplitEditorCaptureWindow();
    splitEditorCaptureArmed.store(false);
    splitEditorCaptureRecording = false;
    splitEditorCaptureWritePosition = 0;
    splitEditorCaptureFrozen.store(false);
    splitEditorSuggestedMarkersPending.store(false);
    for (auto& value : splitEditorFrozenWaveform)
        value.store(0.0f);
}

bool PluginProcessor::isSplitEditorCaptureFrozen() const noexcept
{
    return splitEditorCaptureFrozen.load();
}

bool PluginProcessor::isSplitEditorCaptureArmed() const noexcept
{
    return splitEditorCaptureArmed.load();
}

float PluginProcessor::getSplitEditorCaptureDurationMs() const noexcept
{
    return splitEditorCurrentCaptureDurationMs.load();
}

std::array<float, PluginProcessor::manualMarkerCount> PluginProcessor::getOrderedManualMarkerValuesMs() const
{
    std::array<float, manualMarkerCount> values{
        getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "manualTransientStartMs", 0.0f),
        getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "manualTransientEndMs", 12.0f),
        getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "manualBodyStartMs", 22.0f),
        getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "manualBodyEndMs", 320.0f)
    };

    const float captureDurationMs = getSplitEditorCaptureDurationMs();
    values[0] = juce::jlimit(0.0f, captureDurationMs, values[0]);
    values[1] = juce::jlimit(values[0], captureDurationMs, values[1]);
    values[2] = juce::jlimit(values[1], captureDurationMs, values[2]);
    values[3] = juce::jlimit(values[2], captureDurationMs, values[3]);
    return values;
}

std::array<float, PluginProcessor::manualMarkerCount> PluginProcessor::getManualMarkerValuesMs() const
{
    return getOrderedManualMarkerValuesMs();
}

void PluginProcessor::setManualMarkerValuesMs(const std::array<float, manualMarkerCount>& valuesMs)
{
    setFloatParam(apvts, "manualTransientStartMs", valuesMs[0]);
    setFloatParam(apvts, "manualTransientEndMs", valuesMs[1]);
    setFloatParam(apvts, "manualBodyStartMs", valuesMs[2]);
    setFloatParam(apvts, "manualBodyEndMs", valuesMs[3]);
}

void PluginProcessor::setManualMarkerValueMs(int markerIndex, float valueMs)
{
    auto values = getOrderedManualMarkerValuesMs();
    if (! juce::isPositiveAndBelow(markerIndex, manualMarkerCount))
        return;

    values[(size_t) markerIndex] = juce::jlimit(0.0f, getSplitEditorCaptureDurationMs(), valueMs);
    for (int i = 1; i < manualMarkerCount; ++i)
        values[(size_t) i] = juce::jmax(values[(size_t) i], values[(size_t) i - 1]);
    for (int i = manualMarkerCount - 2; i >= 0; --i)
        values[(size_t) i] = juce::jmin(values[(size_t) i], values[(size_t) i + 1]);

    setManualMarkerValuesMs(values);
}

bool PluginProcessor::hasPendingSplitEditorSuggestedMarkers() const noexcept
{
    return splitEditorSuggestedMarkersPending.load();
}

void PluginProcessor::applyPendingSplitEditorSuggestedMarkers()
{
    if (! splitEditorSuggestedMarkersPending.exchange(false))
        return;

    std::array<float, manualMarkerCount> values{};
    for (int i = 0; i < manualMarkerCount; ++i)
        values[(size_t) i] = splitEditorSuggestedMarkerValues[(size_t) i].load();

    setManualMarkerValuesMs(values);
}

void PluginProcessor::getSplitEditorWaveform(std::array<float, splitEditorDisplayPointCount>& waveform, bool& frozen) const
{
    frozen = splitEditorCaptureFrozen.load();

    if (frozen)
    {
        for (int i = 0; i < splitEditorDisplayPointCount; ++i)
            waveform[(size_t) i] = splitEditorFrozenWaveform[(size_t) i].load();
        return;
    }

    const int writePosition = splitEditorLiveWritePosition.load();
    for (int i = 0; i < splitEditorDisplayPointCount; ++i)
    {
        const int index = (writePosition + i) % splitEditorDisplayPointCount;
        waveform[(size_t) i] = splitEditorLiveWaveform[(size_t) index].load();
    }
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SSP")
        .getChildFile("SSP Transient Splitter")
        .getChildFile("Presets");
}

juce::StringArray PluginProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    const auto directory = getUserPresetDirectory();
    if (! directory.exists())
        return names;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*" + juce::String(userPresetExtension));
    for (const auto& file : files)
        names.add(file.getFileNameWithoutExtension());

    names.sort(true);
    names.removeDuplicates(false);
    return names;
}

juce::ValueTree PluginProcessor::capturePresetState() const
{
    auto state = const_cast<juce::AudioProcessorValueTreeState&>(apvts).copyState();
    state.setProperty("presetName", currentPresetName, nullptr);
    return state;
}

bool PluginProcessor::applyPresetState(const juce::ValueTree& state, const juce::String& presetNameHint)
{
    if (! state.isValid())
        return false;

    auto presetState = state.createCopy();
    const auto resolvedName = presetState.getProperty("presetName", presetNameHint.isNotEmpty() ? presetNameHint : "Init").toString();

    suspendProcessing(true);
    apvts.replaceState(presetState);
    currentPresetName = resolvedName.isNotEmpty() ? resolvedName : "Init";
    suspendProcessing(false);

    resetDetectionState();
    updateLatencyFromParameters();
    updateSidechainFilters();
    resetEffectState();
    return true;
}

void PluginProcessor::storeABState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) abStateSnapshots.size()))
        return;

    abStateSnapshots[(size_t) slotIndex] = capturePresetState();
    abStateValid[(size_t) slotIndex] = true;
}

bool PluginProcessor::recallABState(int slotIndex)
{
    if (! isABStateValid(slotIndex))
        return false;

    if (! applyPresetState(abStateSnapshots[(size_t) slotIndex], currentPresetName))
        return false;

    activeABSlot.store(slotIndex);
    return true;
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();
    if (trimmedName.isEmpty())
        return false;

    const auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    currentPresetName = trimmedName;
    const auto file = directory.getChildFile(sanitisePresetName(trimmedName) + juce::String(userPresetExtension));
    if (auto xml = capturePresetState().createXml())
    {
        if (xml->writeTo(file))
            return true;
    }

    return false;
}

bool PluginProcessor::loadUserPreset(const juce::String& presetName)
{
    const auto file = getUserPresetDirectory().getChildFile(sanitisePresetName(presetName.trim()) + juce::String(userPresetExtension));
    if (! file.existsAsFile())
        return false;

    juce::XmlDocument document(file);
    auto xml = document.getDocumentElement();
    if (xml == nullptr)
        return false;

    return applyPresetState(juce::ValueTree::fromXml(*xml), presetName);
}

bool PluginProcessor::deleteUserPreset(const juce::String& presetName)
{
    const auto file = getUserPresetDirectory().getChildFile(sanitisePresetName(presetName.trim()) + juce::String(userPresetExtension));
    return file.existsAsFile() && file.deleteFile();
}

void PluginProcessor::resetDetectionState()
{
    if (lookaheadBufferSize > 0)
        lookaheadBuffer.clear();

    lookaheadWritePosition = 0;
    fastEnvelopeDb = detectorFloorDb;
    previousFastEnvelopeDb = detectorFloorDb;
    slowEnvelopeDb = detectorFloorDb;
    transientHoldValue = 0.0f;
    transientHoldSamplesRemaining = 0;
    transientWeightState = 0.0f;
    bodyWeightState = 1.0f;
    manualTransientWeightState = 0.0f;
    onsetTriggerArmed = true;
    onsetNeedsRearm = false;
    onsetRearmThresholdDb = detectorFloorDb;
    manualEventActive = false;
    manualEventAgeSamples = 0;
    manualRetriggerCooldownSamples = 0;
    pogoEventActive = false;
    pogoEventAgeSamples = 0;
    splitEditorCaptureRecording = false;
    splitEditorCaptureWritePosition = 0;
    beforeAfterVisualizerCounter = 0;
}

void PluginProcessor::pushSplitEditorLiveSample(float sample) noexcept
{
    if (--splitEditorLiveCounter > 0)
        return;

    splitEditorLiveCounter = splitEditorLiveStride;
    const int writePosition = splitEditorLiveWritePosition.load();
    splitEditorLiveWaveform[(size_t) writePosition].store(juce::jlimit(-1.0f, 1.0f, sample));
    splitEditorLiveWritePosition.store((writePosition + 1) % splitEditorDisplayPointCount);
}

float PluginProcessor::getManualTransientWeightForAgeSamples(int ageSamples) const
{
    const auto markers = getOrderedManualMarkerValuesMs();
    const float ageMs = 1000.0f * (float) ageSamples / (float) juce::jmax(1.0, currentSampleRate);
    const float transientStartMs = markers[manualMarkerTransientStart];
    const float transientEndMs = markers[manualMarkerTransientEnd];
    const float bodyStartMs = markers[manualMarkerBodyStart];

    if (ageMs < transientStartMs)
        return 0.0f;

    if (ageMs <= transientEndMs)
        return 1.0f;

    if (ageMs >= bodyStartMs)
        return 0.0f;

    const float fadeLengthMs = juce::jmax(0.01f, bodyStartMs - transientEndMs);
    return juce::jlimit(0.0f, 1.0f, 1.0f - ((ageMs - transientEndMs) / fadeLengthMs));
}

void PluginProcessor::applySuggestedManualMarkersFromCapture()
{
    if (splitEditorCaptureLengthSamples <= 0 || splitEditorCaptureWritePosition <= 0)
        return;

    const int numSamples = juce::jmin(splitEditorCaptureLengthSamples, splitEditorCaptureWritePosition);
    auto* envelope = splitEditorCaptureBuffer.getWritePointer(0);

    const float attackCoeff = msToCoeff(0.12f, currentSampleRate);
    const float releaseCoeff = msToCoeff(18.0f, currentSampleRate);
    float envelopeValue = 0.0f;

    float peak = 0.0f;
    int peakIndex = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float magnitude = std::abs(envelope[i]);
        const float coeff = magnitude > envelopeValue ? attackCoeff : releaseCoeff;
        envelopeValue = magnitude + coeff * (envelopeValue - magnitude);
        envelope[i] = envelopeValue;

        if (envelopeValue > peak)
        {
            peak = envelopeValue;
            peakIndex = i;
        }
    }

    if (peak <= 1.0e-5f)
    {
        splitEditorSuggestedMarkerValues[0].store(0.0f);
        splitEditorSuggestedMarkerValues[1].store(12.0f);
        splitEditorSuggestedMarkerValues[2].store(22.0f);
        splitEditorSuggestedMarkerValues[3].store(320.0f);
        splitEditorSuggestedMarkersPending.store(true);
        return;
    }

    const int minimumTransientSamples = juce::jmax(1, juce::roundToInt(0.004f * (float) currentSampleRate));
    const int minimumCrossfadeSamples = juce::jmax(1, juce::roundToInt(0.0035f * (float) currentSampleRate));
    const int minimumBodySamples = juce::jmax(1, juce::roundToInt(0.035f * (float) currentSampleRate));

    const float startThreshold = peak * 0.14f;
    const float transientEndThreshold = peak * 0.60f;
    const float bodyThreshold = juce::jmax(peak * 0.07f, 0.0025f);

    int transientStart = 0;
    for (int i = 0; i < juce::jmin(numSamples, peakIndex + 1); ++i)
    {
        if (envelope[i] >= startThreshold)
        {
            transientStart = i;
            break;
        }
    }

    int transientEnd = juce::jmin(numSamples - 1, peakIndex + juce::roundToInt(0.010f * (float) currentSampleRate));
    for (int i = juce::jmin(numSamples - 1, peakIndex + minimumTransientSamples); i < numSamples; ++i)
    {
        if (envelope[i] <= transientEndThreshold)
        {
            transientEnd = i;
            break;
        }
    }

    const int transientSpan = juce::jmax(1, transientEnd - transientStart);
    const int crossfadeSamples = juce::jmax(minimumCrossfadeSamples, juce::jmin(transientSpan, juce::roundToInt(0.010f * (float) currentSampleRate)));
    int bodyStart = juce::jmin(numSamples - 1, transientEnd + crossfadeSamples);
    int bodyEnd = bodyStart;
    for (int i = numSamples - 1; i > bodyStart; --i)
    {
        if (envelope[i] >= bodyThreshold)
        {
            bodyEnd = i;
            break;
        }
    }

    bodyEnd = juce::jmax(bodyEnd, juce::jmin(numSamples - 1, bodyStart + minimumBodySamples));

    auto samplesToMs = [this](int sampleIndex)
    {
        return 1000.0f * (float) sampleIndex / (float) juce::jmax(1.0, currentSampleRate);
    };

    splitEditorSuggestedMarkerValues[0].store(samplesToMs(transientStart));
    splitEditorSuggestedMarkerValues[1].store(samplesToMs(transientEnd));
    splitEditorSuggestedMarkerValues[2].store(samplesToMs(bodyStart));
    splitEditorSuggestedMarkerValues[3].store(samplesToMs(bodyEnd));
    splitEditorSuggestedMarkersPending.store(true);
}

void PluginProcessor::finaliseSplitEditorCapture(bool updateSuggestedMarkers)
{
    const int numSamples = juce::jmin(splitEditorCaptureLengthSamples, splitEditorCaptureWritePosition);
    if (numSamples <= 0)
        return;

    auto* samples = splitEditorCaptureBuffer.getWritePointer(0);
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax(peak, std::abs(samples[i]));

    const float normaliser = peak > 1.0e-5f ? 1.0f / peak : 1.0f;
    for (int point = 0; point < splitEditorDisplayPointCount; ++point)
    {
        const float start = (float) point / (float) splitEditorDisplayPointCount * (float) numSamples;
        const float end = (float) (point + 1) / (float) splitEditorDisplayPointCount * (float) numSamples;
        const int startIndex = juce::jlimit(0, numSamples - 1, (int) std::floor(start));
        const int endIndex = juce::jlimit(startIndex + 1, numSamples, (int) std::ceil(end));

        float peakValue = 0.0f;
        for (int i = startIndex; i < endIndex; ++i)
        {
            const float value = samples[i] * normaliser;
            if (std::abs(value) > std::abs(peakValue))
                peakValue = value;
        }

        splitEditorFrozenWaveform[(size_t) point].store(juce::jlimit(-1.0f, 1.0f, peakValue));
    }

    splitEditorCaptureFrozen.store(true);

    if (updateSuggestedMarkers)
        applySuggestedManualMarkersFromCapture();
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    workInput.setSize(2, maxInternalBlockSize, false, true, true);
    transientBuffer.setSize(2, maxInternalBlockSize, false, true, true);
    bodyBuffer.setSize(2, maxInternalBlockSize, false, true, true);
    dryBuffer.setSize(2, maxInternalBlockSize, false, true, true);
    effectScratch.setSize(2, maxInternalBlockSize, false, true, true);
    pogoEnvelopeBuffer.setSize(1, maxInternalBlockSize, false, true, true);

    const int maxLookaheadSamples = juce::jmax(8, juce::roundToInt((float) sampleRate * 0.011f));
    lookaheadBufferSize = maxLookaheadSamples + 16;
    lookaheadBuffer.setSize(2, lookaheadBufferSize, false, true, true);
    splitEditorCaptureMaxLengthSamples = juce::jmax(512, juce::roundToInt((float) sampleRate * (splitEditorCaptureDurationMs * 0.001f)));
    splitEditorCaptureBuffer.setSize(1, splitEditorCaptureMaxLengthSamples, false, true, true);
    splitEditorCaptureBuffer.clear();
    splitEditorCaptureWritePosition = 0;
    splitEditorCaptureRecording = false;
    splitEditorCaptureFrozen.store(false);
    splitEditorCaptureArmed.store(false);
    splitEditorSuggestedMarkersPending.store(false);
    updateSplitEditorCaptureWindow();
    splitEditorLiveWritePosition.store(0);
    for (auto& value : splitEditorLiveWaveform)
        value.store(0.0f);
    for (auto& value : splitEditorFrozenWaveform)
        value.store(0.0f);
    for (auto& value : splitEditorSuggestedMarkerValues)
        value.store(0.0f);

    resetDetectionState();
    detectionActivity.store(0.0f);
    visualizerCounter = 0;
    visualizerStride = juce::jmax(1, juce::roundToInt((float) sampleRate / 180.0f));
    beforeAfterVisualizerCounter = 0;
    visualizerWritePosition.store(0);
    beforeAfterVisualizerWritePosition.store(0);
    for (auto& value : waveformHistory)
        value.store(0.0f);
    for (auto& value : transientHistory)
        value.store(0.0f);
    for (auto& value : bodyHistory)
        value.store(0.0f);
    for (auto& value : beforeSignalHistory)
        value.store(0.0f);
    for (auto& value : afterSignalHistory)
        value.store(0.0f);

    inputGainSmoothed.reset(sampleRate, 0.02);
    outputGainSmoothed.reset(sampleRate, 0.02);
    dryWetSmoothed.reset(sampleRate, 0.02);
    pogoAmountSmoothed.reset(sampleRate, 0.02);
    for (auto& smoothed : pathLevelSmoothed)
        smoothed.reset(sampleRate, 0.02);
    for (auto& smoothed : pathPanSmoothed)
        smoothed.reset(sampleRate, 0.02);

    prepareEffectProcessors(sampleRate, samplesPerBlock);
    updateSidechainFilters();
    updateLatencyFromParameters();
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (! (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()))
        return false;

    if (! (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()))
        return false;

    return out == in || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void PluginProcessor::updateHostTempo()
{
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                currentHostBpm = *bpm;
        }
    }
}

void PluginProcessor::updateLatencyFromParameters()
{
    const float lookaheadMs = getRawFloatParam(apvts, "lookaheadMs", 2.0f);
    const int latencySamples = juce::jmax(0, juce::roundToInt(lookaheadMs * 0.001f * (float) currentSampleRate));
    if (latencySamples != currentLatencySamples)
    {
        currentLatencySamples = latencySamples;
        setLatencySamples(currentLatencySamples);
    }
}

void PluginProcessor::updateSidechainFilters()
{
    const float hpfHz = getRawFloatParam(apvts, "sidechainHPFHz", 20.0f);
    if (hpfHz <= 21.0f)
        sidechainHPF.setCoefficients(makeIdentityCoefficients());
    else
        sidechainHPF.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, hpfHz));

    sidechainBassLPF.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, 300.0));
    sidechainHighHPF.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, 2000.0));
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    updateHostTempo();
    updateSidechainFilters();
    updateLatencyFromParameters();
    updateSplitEditorCaptureWindow();

    const int totalSamples = buffer.getNumSamples();
    const int totalOutChannels = getTotalNumOutputChannels();
    for (int channel = getTotalNumInputChannels(); channel < totalOutChannels; ++channel)
        buffer.clear(channel, 0, totalSamples);

    int offset = 0;
    while (offset < totalSamples)
    {
        const int chunk = juce::jmin(maxInternalBlockSize, totalSamples - offset);
        processChunk(buffer, offset, chunk);
        offset += chunk;
    }
}

void PluginProcessor::processChunk(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int inputChannels = juce::jmin(2, getTotalNumInputChannels());
    const int outputChannels = juce::jmin(2, getTotalNumOutputChannels());
    if (numSamples <= 0 || outputChannels <= 0)
        return;

    workInput.clear();
    transientBuffer.clear();
    bodyBuffer.clear();
    dryBuffer.clear();
    pogoEnvelopeBuffer.clear();

    const float inputGainTarget = juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "inputGainDb"));
    const float outputGainTarget = juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "outputGainDb"));
    const float dryWetTarget = juce::jlimit(0.0f, 1.0f, getRawFloatParam(apvts, "dryWet", 100.0f) * 0.01f);
    const float pogoAmountTarget = juce::jlimit(0.0f, 1.0f, getRawFloatParam(apvts, "pogoAmount", 0.0f) * 0.01f);
    inputGainSmoothed.setTargetValue(inputGainTarget);
    outputGainSmoothed.setTargetValue(outputGainTarget);
    dryWetSmoothed.setTargetValue(dryWetTarget);
    pogoAmountSmoothed.setTargetValue(pogoAmountTarget);
    pathLevelSmoothed[transientPath].setTargetValue(juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "transientLevelDb")));
    pathLevelSmoothed[bodyPath].setTargetValue(juce::Decibels::decibelsToGain(getRawFloatParam(apvts, "bodyLevelDb")));
    pathPanSmoothed[transientPath].setTargetValue(getRawFloatParam(apvts, "transientPan"));
    pathPanSmoothed[bodyPath].setTargetValue(getRawFloatParam(apvts, "bodyPan"));

    const float sensitivity = juce::jlimit(0.0f, 1.0f, getRawFloatParam(apvts, "sensitivity", 50.0f) * 0.01f);
    const float speedMs = getRawFloatParam(apvts, "speedMs", 0.5f);
    const float holdMs = getRawFloatParam(apvts, "holdMs", 5.0f);
    const float releaseMs = getRawFloatParam(apvts, "releaseMs", 50.0f);
    const float focusNorm = juce::jlimit(0.0f, 1.0f, getRawFloatParam(apvts, "focus", 50.0f) * 0.01f);
    const int triggerFilter = getChoiceIndex(apvts, "triggerFilter");
    const int splitMode = getSplitModeIndex();
    const int lookaheadSamples = currentLatencySamples;
    const auto manualMarkersMs = getOrderedManualMarkerValuesMs();
    const int manualBodyEndSamples = juce::jmax(1, juce::roundToInt(manualMarkersMs[manualMarkerBodyEnd] * 0.001f * (float) currentSampleRate));
    const bool useManualSplitWindows = splitMode != splitModeAuto;
    const bool updateSuggestedMarkersOnCapture = splitMode == splitModeAssisted;

    const float fastAttackCoeff = msToCoeff(speedMs, currentSampleRate);
    const float slowAttackMs = juce::jlimit(12.0f, 140.0f, 14.0f + speedMs * 26.0f + (1.0f - sensitivity) * 42.0f);
    const float slowAttackCoeff = msToCoeff(slowAttackMs, currentSampleRate);
    const float releaseCoeff = msToCoeff(releaseMs, currentSampleRate);
    const int holdSamples = juce::jmax(0, juce::roundToInt(holdMs * 0.001f * (float) currentSampleRate));
    const float envelopeSmoothCoeff = msToCoeff(2.8f, currentSampleRate);
    const float manualEnvelopeSmoothCoeff = msToCoeff(4.0f, currentSampleRate);
    const float bodyEnvelopeSmoothCoeff = msToCoeff(8.5f, currentSampleRate);
    const float focusScale = 0.72f + focusNorm * 1.35f;
    const float diffThresholdDb = juce::jmap(sensitivity, 0.0f, 1.0f, 9.5f, 2.0f);
    const float riseThresholdDb = juce::jmap(sensitivity, 0.0f, 1.0f, 5.0f, 1.1f);
    const float transientCurve = juce::jmax(0.5f, juce::jmap(focusNorm, 0.0f, 1.0f, 1.5f, 0.78f)
                                                   * juce::jmap(sensitivity, 0.0f, 1.0f, 1.18f, 0.88f));
    const float onsetTriggerThreshold = juce::jmap(sensitivity, 0.0f, 1.0f, 0.58f, 0.22f);
    const float onsetResetThreshold = onsetTriggerThreshold * 0.34f;
    const int retriggerGuardSamples = juce::jmax(1, juce::roundToInt(0.006f * (float) currentSampleRate));
    const float pogoBpm = (float) juce::jmax(40.0, currentHostBpm);
    const int pogoWindowSamples = juce::jmax(1, juce::roundToInt((60.0f / pogoBpm) * 1.75f * (float) currentSampleRate));
    const float pogoDelayMs = useManualSplitWindows
                                ? juce::jlimit(4.0f, 90.0f, manualMarkersMs[manualMarkerBodyStart])
                                : juce::jlimit(10.0f, 42.0f, holdMs + 10.0f + releaseMs * 0.035f);
    const int pogoDelaySamples = juce::jmax(0, juce::roundToInt(pogoDelayMs * 0.001f * (float) currentSampleRate));

    const bool transientMute = getRawFloatParam(apvts, "transientMute") >= 0.5f;
    const bool bodyMute = getRawFloatParam(apvts, "bodyMute") >= 0.5f;
    const bool transientSolo = getRawFloatParam(apvts, "transientSolo") >= 0.5f;
    const bool bodySolo = getRawFloatParam(apvts, "bodySolo") >= 0.5f;
    const bool anySolo = transientSolo || bodySolo;
    const bool transientAudible = ! transientMute && (! anySolo || transientSolo);
    const bool bodyAudible = ! bodyMute && (! anySolo || bodySolo);

    float maxDetection = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputGain = inputGainSmoothed.getNextValue();
        float leftIn = inputChannels > 0 ? buffer.getSample(0, startSample + sample) * inputGain : 0.0f;
        float rightIn = inputChannels > 1 ? buffer.getSample(1, startSample + sample) * inputGain : leftIn;
        if (inputChannels == 1)
            rightIn = leftIn;

        workInput.setSample(0, sample, leftIn);
        workInput.setSample(1, sample, rightIn);

        float sidechain = 0.5f * (leftIn + rightIn);
        sidechain = sidechainHPF.processSingleSampleRaw(sidechain);
        if (triggerFilter == 1)
            sidechain = sidechainBassLPF.processSingleSampleRaw(sidechain);
        else if (triggerFilter == 2)
            sidechain = sidechainHighHPF.processSingleSampleRaw(sidechain);

        const float rectified = std::abs(sidechain);
        const float detectorDb = juce::Decibels::gainToDecibels(rectified + 1.0e-6f, detectorFloorDb);
        fastEnvelopeDb = dbFollowerUpdate(detectorDb, fastEnvelopeDb, fastAttackCoeff, releaseCoeff);
        slowEnvelopeDb = dbFollowerUpdate(detectorDb, slowEnvelopeDb, slowAttackCoeff, releaseCoeff);

        const float diffDb = juce::jmax(0.0f, fastEnvelopeDb - slowEnvelopeDb);
        const float riseDb = juce::jmax(0.0f, fastEnvelopeDb - previousFastEnvelopeDb);
        previousFastEnvelopeDb = fastEnvelopeDb;
        const float fastLin = juce::Decibels::decibelsToGain(fastEnvelopeDb);
        const float slowLin = juce::Decibels::decibelsToGain(slowEnvelopeDb);
        const float diffLin = juce::jmax(0.0f, fastLin - slowLin);
        const float energyRatio = diffLin / juce::jmax(1.0e-5f, fastLin);
        const float diffNorm = juce::jlimit(0.0f, 1.0f, diffDb / diffThresholdDb);
        const float riseNorm = juce::jlimit(0.0f, 1.0f, riseDb / riseThresholdDb);
        const float onsetRearmDropDb = useManualSplitWindows
                                         ? juce::jmap(sensitivity, 24.0f, 15.0f)
                                         : juce::jmap(sensitivity, 18.0f, 10.0f);

        if (onsetNeedsRearm && slowEnvelopeDb <= onsetRearmThresholdDb)
            onsetNeedsRearm = false;

        float transientDrive = juce::jmax(diffNorm * (0.90f + 0.35f * focusScale),
                                          riseNorm * (1.05f + 0.25f * sensitivity));
        transientDrive = juce::jmax(transientDrive, energyRatio * 0.75f * focusScale);
        float targetTransient = std::pow(juce::jlimit(0.0f, 1.0f, transientDrive), transientCurve);
        const float onsetScore = juce::jmax(transientDrive, juce::jmax(diffNorm, riseNorm * 1.08f));
        if (! onsetTriggerArmed
            && onsetScore <= onsetResetThreshold
            && diffDb <= diffThresholdDb * 0.18f
            && riseDb <= riseThresholdDb * 0.08f)
        {
            onsetTriggerArmed = true;
        }

        const bool onsetTriggered = ! onsetNeedsRearm
                                 && onsetTriggerArmed
                                 && onsetScore >= onsetTriggerThreshold
                                 && diffDb >= diffThresholdDb * 0.26f
                                 && riseDb >= riseThresholdDb * 0.16f;

        if (targetTransient > transientHoldValue)
        {
            transientHoldValue = targetTransient;
            transientHoldSamplesRemaining = holdSamples;
        }
        else if (transientHoldSamplesRemaining > 0)
        {
            --transientHoldSamplesRemaining;
        }
        else
        {
            transientHoldValue = targetTransient + releaseCoeff * (transientHoldValue - targetTransient);
        }

        const int readPosition = (lookaheadWritePosition - lookaheadSamples + lookaheadBufferSize) % lookaheadBufferSize;
        const float delayedLeft = lookaheadBuffer.getSample(0, readPosition);
        const float delayedRight = lookaheadBuffer.getSample(1, readPosition);
        lookaheadBuffer.setSample(0, lookaheadWritePosition, leftIn);
        lookaheadBuffer.setSample(1, lookaheadWritePosition, rightIn);
        lookaheadWritePosition = (lookaheadWritePosition + 1) % lookaheadBufferSize;
        const float delayedMono = 0.5f * (delayedLeft + delayedRight);

        pushSplitEditorLiveSample(delayedMono);

        if (manualRetriggerCooldownSamples > 0)
            --manualRetriggerCooldownSamples;

        const bool canStartManualWindow = ! useManualSplitWindows || ! manualEventActive;
        if (onsetTriggered && manualRetriggerCooldownSamples <= 0 && canStartManualWindow)
        {
            onsetTriggerArmed = false;
            onsetNeedsRearm = true;
            onsetRearmThresholdDb = juce::jmax(detectorFloorDb + 6.0f, slowEnvelopeDb - onsetRearmDropDb);
            manualEventActive = true;
            manualEventAgeSamples = 0;
            pogoEventActive = true;
            pogoEventAgeSamples = 0;
            manualRetriggerCooldownSamples = useManualSplitWindows ? juce::jmax(retriggerGuardSamples, manualBodyEndSamples)
                                                                  : retriggerGuardSamples;

            if (splitEditorCaptureArmed.load())
            {
                splitEditorCaptureArmed.store(false);
                splitEditorCaptureFrozen.store(false);
                splitEditorCaptureRecording = true;
                splitEditorCaptureWritePosition = 0;
                splitEditorCaptureBuffer.clear();
            }
        }

        if (splitEditorCaptureRecording && splitEditorCaptureWritePosition < splitEditorCaptureLengthSamples)
        {
            splitEditorCaptureBuffer.setSample(0, splitEditorCaptureWritePosition++, delayedMono);
            if (splitEditorCaptureWritePosition >= splitEditorCaptureLengthSamples)
            {
                splitEditorCaptureRecording = false;
                finaliseSplitEditorCapture(updateSuggestedMarkersOnCapture);
            }
        }

        transientWeightState = transientHoldValue + envelopeSmoothCoeff * (transientWeightState - transientHoldValue);
        float transientWeight = juce::jlimit(0.0f, 1.0f, transientWeightState);
        float bodyWeight = 0.0f;

        if (useManualSplitWindows)
        {
            const float manualTarget = manualEventActive ? getManualTransientWeightForAgeSamples(manualEventAgeSamples) : 0.0f;

            manualTransientWeightState = manualTarget + manualEnvelopeSmoothCoeff * (manualTransientWeightState - manualTarget);
            transientWeight = juce::jlimit(0.0f, 1.0f, manualTransientWeightState);
            bodyWeight = 1.0f - transientWeight;

            if (manualEventActive)
            {
                ++manualEventAgeSamples;
                if (manualEventAgeSamples >= manualBodyEndSamples)
                    manualEventActive = false;
            }
        }
        else
        {
            const float bodyRatio = juce::jlimit(0.0f, 1.0f, slowLin / juce::jmax(1.0e-4f, fastLin));
            const float bodyCurve = juce::jmap(focusNorm, 0.0f, 1.0f, 0.92f, 1.16f);
            const float bodyTarget = std::pow(bodyRatio, bodyCurve);
            bodyWeightState = bodyTarget + bodyEnvelopeSmoothCoeff * (bodyWeightState - bodyTarget);
            bodyWeight = juce::jlimit(0.0f, 1.0f, bodyWeightState);

            const float weightSum = juce::jmax(1.0e-4f, transientWeight + bodyWeight);
            transientWeight = juce::jlimit(0.0f, 1.0f, transientWeight / weightSum);
            bodyWeight = juce::jlimit(0.0f, 1.0f, bodyWeight / weightSum);
        }

        const float pogoAmount = pogoAmountSmoothed.getNextValue();
        float bodyPogoGain = 1.0f;
        if (pogoAmount <= 0.0001f)
        {
            pogoEventActive = false;
            pogoEventAgeSamples = 0;
        }
        else if (pogoEventActive)
        {
            if (pogoEventAgeSamples >= pogoDelaySamples)
            {
                const int localAgeSamples = pogoEventAgeSamples - pogoDelaySamples;
                const float localSeconds = (float) localAgeSamples / (float) juce::jmax(1.0, currentSampleRate);
                const float duckTimeMs = juce::jmap(pogoAmount, 9.0f, 26.0f);
                const float reboundTimeSeconds = juce::jlimit(0.05f, 0.42f, (60.0f / pogoBpm) * juce::jmap(pogoAmount, 0.22f, 0.62f));
                const float duckProgress = juce::jlimit(0.0f, 1.0f, localSeconds / (duckTimeMs * 0.001f));
                const float duckShape = duckProgress * duckProgress * (3.0f - 2.0f * duckProgress);
                const float recoverProgress = juce::jlimit(0.0f, 1.0f, (localSeconds - duckTimeMs * 0.001f) / juce::jmax(0.01f, reboundTimeSeconds));
                const float recoverShape = recoverProgress * recoverProgress * (3.0f - 2.0f * recoverProgress);
                const float bounceShape = std::sin(juce::MathConstants<float>::pi * std::pow(recoverProgress, 0.72f))
                                        * std::exp(-recoverProgress * juce::jmap(pogoAmount, 1.4f, 2.9f));
                const float duckDepth = juce::jmap(pogoAmount, 0.28f, 0.995f);
                const float reboundDepth = juce::jmap(pogoAmount, 0.12f, 1.18f);
                const float duckedGain = 1.0f - duckDepth * duckShape;
                const float recoveredGain = duckedGain + duckDepth * recoverShape;
                bodyPogoGain = juce::jlimit(0.02f, 2.2f, recoveredGain + reboundDepth * bounceShape);
            }

            ++pogoEventAgeSamples;
            if (pogoEventAgeSamples >= pogoWindowSamples)
                pogoEventActive = false;
        }

        maxDetection = juce::jmax(maxDetection, transientWeight);

        dryBuffer.setSample(0, sample, delayedLeft);
        dryBuffer.setSample(1, sample, delayedRight);
        transientBuffer.setSample(0, sample, delayedLeft * transientWeight);
        transientBuffer.setSample(1, sample, delayedRight * transientWeight);
        bodyBuffer.setSample(0, sample, delayedLeft * bodyWeight);
        bodyBuffer.setSample(1, sample, delayedRight * bodyWeight);
        pogoEnvelopeBuffer.setSample(0, sample, bodyPogoGain);

        if (++visualizerCounter >= visualizerStride)
        {
            visualizerCounter = 0;
            const float waveform = juce::jlimit(0.0f, 1.0f, 0.5f * (std::abs(delayedLeft) + std::abs(delayedRight)));
            pushVisualizerSample(waveform, transientWeight, bodyWeight);
        }
    }

    detectionActivity.store(maxDetection);

    applyEffects(transientBuffer, 0, slotsPerPath);
    applyEffects(bodyBuffer, slotsPerPath, maxFXSlots);

    const float transientLevel = transientAudible ? 1.0f : 0.0f;
    const float bodyLevel = bodyAudible ? 1.0f : 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryWet = dryWetSmoothed.getNextValue();
        const float dryMix = 1.0f - dryWet;
        const float outputGain = outputGainSmoothed.getNextValue();

        const float transientGain = pathLevelSmoothed[transientPath].getNextValue() * transientLevel;
        const float bodyGain = pathLevelSmoothed[bodyPath].getNextValue() * bodyLevel;
        const float transientPan = pathPanSmoothed[transientPath].getNextValue();
        const float bodyPan = pathPanSmoothed[bodyPath].getNextValue();

        float transL = 1.0f;
        float transR = 1.0f;
        float bodyL = 1.0f;
        float bodyR = 1.0f;
        applyPan(transientPan, transL, transR);
        applyPan(bodyPan, bodyL, bodyR);

        const float transientLeft = transientBuffer.getSample(0, sample) * transientGain * transL;
        const float transientRight = transientBuffer.getSample(1, sample) * transientGain * transR;
        const float bodyPostGain = pogoEnvelopeBuffer.getSample(0, sample);
        const float bodyLeft = bodyBuffer.getSample(0, sample) * bodyGain * bodyL * bodyPostGain;
        const float bodyRight = bodyBuffer.getSample(1, sample) * bodyGain * bodyR * bodyPostGain;

        const float wetLeft = transientLeft + bodyLeft;
        const float wetRight = transientRight + bodyRight;
        const float outLeft = (dryBuffer.getSample(0, sample) * dryMix + wetLeft * dryWet) * outputGain;
        const float outRight = (dryBuffer.getSample(1, sample) * dryMix + wetRight * dryWet) * outputGain;

        if (++beforeAfterVisualizerCounter >= visualizerStride)
        {
            beforeAfterVisualizerCounter = 0;
            const float before = juce::jlimit(0.0f, 1.0f,
                                              0.5f * (std::abs(dryBuffer.getSample(0, sample))
                                                    + std::abs(dryBuffer.getSample(1, sample))));
            const float after = juce::jlimit(0.0f, 1.0f,
                                             0.5f * (std::abs(outLeft) + std::abs(outRight)));
            pushBeforeAfterVisualizerSample(before, after);
        }

        if (outputChannels == 1)
        {
            const float mono = 0.5f * (outLeft + outRight);
            buffer.setSample(0, startSample + sample, mono);
        }
        else
        {
            buffer.setSample(0, startSample + sample, outLeft);
            buffer.setSample(1, startSample + sample, outRight);
        }
    }

    updateMeterState(inputMeter, workInput, 2, numSamples, false);
    updateMeterState(transientMeter, transientBuffer, 2, numSamples, false);
    updateMeterState(bodyMeter, bodyBuffer, 2, numSamples, false);

    for (int channel = 0; channel < 2; ++channel)
        effectScratch.copyFrom(channel, 0, buffer, juce::jmin(channel, outputChannels - 1), startSample, numSamples);
    updateMeterState(outputMeter, effectScratch, 2, numSamples, true);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = capturePresetState().createXml())
        copyXmlToBinary(*state, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            applyPresetState(juce::ValueTree::fromXml(*xmlState), "Init");
    }

    storeABState(0);
    storeABState(1);
    activeABSlot.store(0);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

int PluginProcessor::getGlobalSlotIndex(int path, int localSlotIndex) const noexcept
{
    return juce::jlimit(0, maxFXSlots - 1, juce::jlimit(0, 1, path) * slotsPerPath + juce::jlimit(0, slotsPerPath - 1, localSlotIndex));
}

juce::Array<int> PluginProcessor::getFXOrderForPath(int path) const
{
    juce::Array<int> order;
    const int start = juce::jlimit(0, 1, path) * slotsPerPath;
    for (int slot = 0; slot < slotsPerPath; ++slot)
        order.add(getFXOrderSlot(start + slot));
    return order;
}

bool PluginProcessor::addFXModule(int path, int typeIndex)
{
    const int clampedType = juce::jlimit(1, getFXModuleNames().size() - 1, typeIndex);
    if (! isSupportedFXModuleType(clampedType) || clampedType == fxOff)
        return false;

    const int start = juce::jlimit(0, 1, path) * slotsPerPath;
    const int end = start + slotsPerPath;
    for (int slot = start; slot < end; ++slot)
    {
        if (getFXOrderSlot(slot) == fxOff)
        {
            setFXOrderSlot(slot, clampedType);
            initialiseFXSlotDefaults(slot, clampedType);
            return true;
        }
    }

    return false;
}

void PluginProcessor::removeFXModuleFromPath(int path, int localSlotIndex)
{
    clearFXSlotState(getGlobalSlotIndex(path, localSlotIndex));
    compactPathSlots(path);
}

void PluginProcessor::moveFXModuleWithinPath(int path, int fromLocalIndex, int toLocalIndex)
{
    const int from = getGlobalSlotIndex(path, fromLocalIndex);
    const int to = getGlobalSlotIndex(path, toLocalIndex);
    if (from == to || getFXOrderSlot(from) == fxOff)
        return;

    struct SlotSnapshot
    {
        int type = fxOff;
        bool enabled = false;
        std::array<float, fxSlotParameterCount> parameters{};
        int variant = 0;
    };

    auto readSlot = [this](int slotIndex)
    {
        SlotSnapshot snapshot;
        snapshot.type = getFXOrderSlot(slotIndex);
        snapshot.enabled = getFXSlotOnValue(slotIndex);
        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
            snapshot.parameters[(size_t) parameterIndex] = getFXSlotFloatValue(slotIndex, parameterIndex);
        snapshot.variant = getFXSlotVariant(slotIndex);
        return snapshot;
    };

    auto writeSlot = [this](int slotIndex, const SlotSnapshot& snapshot)
    {
        setFXOrderSlot(slotIndex, snapshot.type);
        setBoolParam(apvts, getFXSlotOnParamID(slotIndex), snapshot.enabled);
        for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
            setFloatParam(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), snapshot.parameters[(size_t) parameterIndex]);
        setFloatParam(apvts, getFXSlotVariantParamID(slotIndex), (float) snapshot.variant);
    };

    const auto moving = readSlot(from);
    if (from < to)
    {
        for (int slot = from; slot < to; ++slot)
            writeSlot(slot, readSlot(slot + 1));
    }
    else
    {
        for (int slot = from; slot > to; --slot)
            writeSlot(slot, readSlot(slot - 1));
    }

    writeSlot(to, moving);
}

void PluginProcessor::copyPathChain(int sourcePath, int destinationPath)
{
    const int source = juce::jlimit(0, 1, sourcePath);
    const int destination = juce::jlimit(0, 1, destinationPath);
    if (source == destination)
        return;

    suspendProcessing(true);
    for (int slot = 0; slot < slotsPerPath; ++slot)
    {
        const int sourceIndex = getGlobalSlotIndex(source, slot);
        const int destinationIndex = getGlobalSlotIndex(destination, slot);
        if (getFXOrderSlot(sourceIndex) == fxOff)
            clearFXSlotState(destinationIndex);
        else
            copyFXSlotState(sourceIndex, destinationIndex);
    }
    suspendProcessing(false);

    resetEffectState();
}

void PluginProcessor::resetSplitterParametersToDefaults()
{
    currentPresetName = "Init";
    setFloatParam(apvts, "inputGainDb", 0.0f);
    setFloatParam(apvts, "sensitivity", 50.0f);
    setFloatParam(apvts, "speedMs", 0.5f);
    setFloatParam(apvts, "holdMs", 5.0f);
    setFloatParam(apvts, "releaseMs", 50.0f);
    setFloatParam(apvts, "lookaheadMs", 2.0f);
    setFloatParam(apvts, "triggerFilter", 0.0f);
    setFloatParam(apvts, "sidechainHPFHz", 20.0f);
    setFloatParam(apvts, "focus", 50.0f);
    setFloatParam(apvts, "splitMode", (float) splitModeAuto);
    setFloatParam(apvts, "captureLength", 2.0f);
    setManualMarkerValuesMs({ 0.0f, 5.0f, 12.0f, 180.0f });

    setFloatParam(apvts, "transientLevelDb", 0.0f);
    setFloatParam(apvts, "bodyLevelDb", 0.0f);
    setFloatParam(apvts, "transientPan", 0.0f);
    setFloatParam(apvts, "bodyPan", 0.0f);
    setBoolParam(apvts, "transientMute", false);
    setBoolParam(apvts, "bodyMute", false);
    setBoolParam(apvts, "transientSolo", false);
    setBoolParam(apvts, "bodySolo", false);

    setFloatParam(apvts, "dryWet", 100.0f);
    setFloatParam(apvts, "pogoAmount", 0.0f);
    setFloatParam(apvts, "outputGainDb", 0.0f);

    for (int slot = 0; slot < maxFXSlots; ++slot)
        clearFXSlotState(slot);
}

bool PluginProcessor::loadFactoryPreset(int presetIndex)
{
    const auto& names = getFactoryPresetNames();
    if (! juce::isPositiveAndBelow(presetIndex, names.size()))
        return false;

    suspendProcessing(true);
    resetSplitterParametersToDefaults();

    auto setSlot = [this](int path, int localSlot, int moduleType, std::initializer_list<std::pair<int, float>> values = {}, int variant = 0)
    {
        const int globalSlot = getGlobalSlotIndex(path, localSlot);
        setFXOrderSlot(globalSlot, moduleType);
        initialiseFXSlotDefaults(globalSlot, moduleType);
        setFloatParam(apvts, getFXSlotVariantParamID(globalSlot), (float) juce::jlimit(0, getFXVariantLimit(moduleType) - 1, variant));
        for (const auto& [index, value] : values)
        {
            if (juce::isPositiveAndBelow(index, fxSlotParameterCount))
                setFloatParam(apvts, getFXSlotFloatParamID(globalSlot, index), juce::jlimit(0.0f, 1.0f, value));
        }
    };

    switch (presetIndex)
    {
        case 0: // Punchy Drums
            setFloatParam(apvts, "sensitivity", 60.0f);
            setFloatParam(apvts, "speedMs", 0.22f);
            setFloatParam(apvts, "holdMs", 7.0f);
            setFloatParam(apvts, "releaseMs", 40.0f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.62f }, { 1, 0.84f }, { 2, 0.08f }, { 3, 0.28f }}, 1);
            setSlot(transientPath, 1, fxDrive, {{ 0, 0.22f }, { 1, 0.56f }, { 2, 0.72f }}, 0);
            setSlot(bodyPath, 0, fxEQ, {{ 0, 0.22f }, { 1, 0.58f }, { 4, 0.44f }, { 5, 0.60f }});
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.24f }, { 1, 0.30f }, { 2, 0.68f }, { 3, 0.05f }}, 2);
            break;
        case 1: // Parallel Smash
            setFloatParam(apvts, "sensitivity", 62.0f);
            setFloatParam(apvts, "speedMs", 0.18f);
            setFloatParam(apvts, "holdMs", 6.0f);
            setFloatParam(apvts, "releaseMs", 52.0f);
            setFloatParam(apvts, "focus", 62.0f);
            setFloatParam(apvts, "transientLevelDb", 1.5f);
            setFloatParam(apvts, "bodyLevelDb", 0.0f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.76f }, { 1, 0.92f }, { 2, 0.04f }, { 3, 0.18f }}, 3);
            setSlot(transientPath, 1, fxDrive, {{ 0, 0.28f }, { 1, 0.58f }, { 2, 0.84f }}, 1);
            break;
        case 2: // Drum Crush
            setFloatParam(apvts, "sensitivity", 66.0f);
            setSlot(transientPath, 0, fxBitcrusher, {{ 0, 0.46f }, { 1, 0.34f }, { 3, 0.76f }}, 1);
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.68f }, { 1, 0.90f }, { 2, 0.05f }, { 3, 0.24f }}, 2);
            setSlot(bodyPath, 0, fxEQ, {{ 0, 0.20f }, { 1, 0.54f }, { 4, 0.48f }, { 5, 0.56f }});
            setSlot(bodyPath, 1, fxMultiband, {{ 0, 0.54f }, { 3, 0.62f }, { 6, 0.78f }, { 9, 1.0f }});
            break;
        case 3: // Snare Designer
            setFloatParam(apvts, "sensitivity", 62.0f);
            setSlot(transientPath, 0, fxDrive, {{ 0, 0.30f }, { 1, 0.62f }, { 2, 0.82f }}, 2);
            setSlot(transientPath, 1, fxEQ, {{ 4, 0.64f }, { 5, 0.62f }, { 6, 0.84f }, { 7, 0.56f }});
            setSlot(bodyPath, 0, fxGate, {{ 0, 0.44f }, { 1, 0.08f }, { 2, 0.20f }, { 3, 0.20f }}, 0);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.30f }, { 1, 0.36f }, { 2, 0.64f }, { 3, 0.10f }}, 2);
            break;
        case 4: // Hard Clap Driver
            setFloatParam(apvts, "sensitivity", 72.0f);
            setFloatParam(apvts, "speedMs", 0.11f);
            setFloatParam(apvts, "holdMs", 3.0f);
            setFloatParam(apvts, "releaseMs", 34.0f);
            setFloatParam(apvts, "triggerFilter", 2.0f);
            setFloatParam(apvts, "focus", 66.0f);
            setFloatParam(apvts, "pogoAmount", 18.0f);
            setSlot(transientPath, 0, fxDrive, {{ 0, 0.34f }, { 1, 0.60f }, { 2, 0.88f }}, 4);
            setSlot(transientPath, 1, fxEQ, {{ 4, 0.66f }, { 5, 0.62f }, { 6, 0.86f }, { 7, 0.58f }});
            setSlot(bodyPath, 0, fxGate, {{ 0, 0.50f }, { 1, 0.04f }, { 2, 0.08f }, { 3, 0.12f }}, 0);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.26f }, { 1, 0.34f }, { 2, 0.56f }, { 3, 0.08f }}, 1);
            break;
        case 5: // Kick To Rumble
            setFloatParam(apvts, "sensitivity", 60.0f);
            setFloatParam(apvts, "speedMs", 0.18f);
            setFloatParam(apvts, "holdMs", 6.0f);
            setFloatParam(apvts, "releaseMs", 90.0f);
            setFloatParam(apvts, "triggerFilter", 1.0f);
            setFloatParam(apvts, "focus", 62.0f);
            setFloatParam(apvts, "splitMode", (float) splitModeManual);
            setFloatParam(apvts, "transientLevelDb", 0.4f);
            setFloatParam(apvts, "bodyLevelDb", 1.8f);
            setFloatParam(apvts, "pogoAmount", 52.0f);
            setManualMarkerValuesMs({ 0.0f, 7.5f, 18.0f, 260.0f });
            setSlot(transientPath, 0, fxEQ, {{ 4, 0.62f }, { 5, 0.60f }, { 6, 0.82f }, { 7, 0.56f }});
            setSlot(transientPath, 1, fxDrive, {{ 0, 0.24f }, { 1, 0.58f }, { 2, 0.76f }}, 1);
            setSlot(bodyPath, 0, fxReverb, {{ 0, 0.46f }, { 1, 0.42f }, { 2, 0.54f }, { 3, 0.02f }}, 0);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.14f }, { 1, 0.18f }, { 2, 0.16f }, { 3, 0.50f }, { 4, 1.0f }}, 0);
            break;
        case 6: // Pumped Loop Split
            setFloatParam(apvts, "sensitivity", 58.0f);
            setFloatParam(apvts, "speedMs", 0.30f);
            setFloatParam(apvts, "holdMs", 7.0f);
            setFloatParam(apvts, "releaseMs", 72.0f);
            setFloatParam(apvts, "focus", 60.0f);
            setFloatParam(apvts, "bodyLevelDb", 0.8f);
            setFloatParam(apvts, "pogoAmount", 44.0f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.60f }, { 1, 0.82f }, { 2, 0.06f }, { 3, 0.24f }}, 1);
            setSlot(transientPath, 1, fxBitcrusher, {{ 0, 0.24f }, { 1, 0.18f }, { 3, 0.34f }}, 0);
            setSlot(bodyPath, 0, fxTremolo, {{ 0, 0.16f }, { 1, 0.68f }, { 2, 0.16f }, { 3, 0.56f }}, 1);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.32f }, { 1, 0.22f }, { 2, 0.18f }, { 3, 0.44f }, { 4, 1.0f }}, 14);
            setSlot(bodyPath, 2, fxImager, {{ 0, 0.64f }, { 1, 0.70f }, { 2, 0.74f }, { 3, 0.80f }});
            break;
        case 7: // Hat Tamer
            setFloatParam(apvts, "sensitivity", 70.0f);
            setFloatParam(apvts, "speedMs", 0.08f);
            setFloatParam(apvts, "holdMs", 2.0f);
            setFloatParam(apvts, "releaseMs", 18.0f);
            setFloatParam(apvts, "triggerFilter", 2.0f);
            setFloatParam(apvts, "focus", 72.0f);
            setFloatParam(apvts, "transientLevelDb", -3.5f);
            setSlot(transientPath, 0, fxFilter, {{ 0, 0.78f }, { 1, 0.16f }, { 2, 0.24f }, { 3, 0.10f }, { 4, 1.0f }}, 9);
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.38f }, { 1, 0.78f }, { 2, 0.05f }, { 3, 0.18f }}, 1);
            break;
        case 8: // Room Tail Lift
            setFloatParam(apvts, "sensitivity", 55.0f);
            setFloatParam(apvts, "speedMs", 0.18f);
            setFloatParam(apvts, "holdMs", 7.0f);
            setFloatParam(apvts, "releaseMs", 82.0f);
            setFloatParam(apvts, "focus", 52.0f);
            setFloatParam(apvts, "transientLevelDb", -0.4f);
            setFloatParam(apvts, "bodyLevelDb", 1.4f);
            setSlot(transientPath, 0, fxGate, {{ 0, 0.46f }, { 1, 0.05f }, { 2, 0.06f }, { 3, 0.18f }}, 0);
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.50f }, { 1, 0.76f }, { 2, 0.08f }, { 3, 0.22f }}, 0);
            setSlot(bodyPath, 0, fxReverb, {{ 0, 0.28f }, { 1, 0.34f }, { 2, 0.64f }, { 3, 0.06f }}, 2);
            setSlot(bodyPath, 1, fxImager, {{ 0, 0.66f }, { 1, 0.74f }, { 2, 0.82f }, { 3, 0.88f }});
            break;
        case 9: // Wide Sustain
            setSlot(transientPath, 0, fxImager, {{ 0, 0.40f }, { 1, 0.42f }, { 2, 0.44f }, { 3, 0.46f }});
            setSlot(bodyPath, 0, fxImager, {{ 0, 0.68f }, { 1, 0.74f }, { 2, 0.82f }, { 3, 0.90f }});
            setSlot(bodyPath, 1, fxChorus, {{ 0, 0.14f }, { 1, 0.34f }, { 2, 0.22f }, { 3, 0.42f }});
            break;
        case 10: // Center Punch / Wide Tail
            setFloatParam(apvts, "sensitivity", 56.0f);
            setFloatParam(apvts, "speedMs", 0.22f);
            setFloatParam(apvts, "holdMs", 6.0f);
            setFloatParam(apvts, "releaseMs", 64.0f);
            setFloatParam(apvts, "pogoAmount", 24.0f);
            setSlot(transientPath, 0, fxImager, {{ 0, 0.28f }, { 1, 0.34f }, { 2, 0.36f }, { 3, 0.38f }});
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.50f }, { 1, 0.74f }, { 2, 0.08f }, { 3, 0.22f }}, 0);
            setSlot(bodyPath, 0, fxImager, {{ 0, 0.74f }, { 1, 0.80f }, { 2, 0.88f }, { 3, 0.96f }});
            setSlot(bodyPath, 1, fxChorus, {{ 0, 0.14f }, { 1, 0.32f }, { 2, 0.22f }, { 3, 0.42f }});
            setSlot(bodyPath, 2, fxReverb, {{ 0, 0.20f }, { 1, 0.28f }, { 2, 0.50f }, { 3, 0.04f }}, 1);
            break;
        case 11: // Pitch Tail
            setFloatParam(apvts, "releaseMs", 96.0f);
            setSlot(bodyPath, 0, fxShift, {{ 0, 0.58f }, { 1, 0.54f }, { 2, 0.08f }, { 3, 0.12f }}, 0);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.36f }, { 1, 0.40f }, { 2, 0.60f }, { 3, 0.08f }}, 0);
            break;
        case 12: // Tremolo Body
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.46f }, { 1, 0.70f }, { 2, 0.08f }, { 3, 0.24f }}, 0);
            setSlot(bodyPath, 0, fxTremolo, {{ 0, 0.20f }, { 1, 0.74f }, { 2, 0.18f }, { 3, 0.56f }}, 0);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.34f }, { 1, 0.20f }, { 3, 0.44f }}, 0);
            break;
        case 13: // Pluck Polish
            setFloatParam(apvts, "sensitivity", 56.0f);
            setFloatParam(apvts, "speedMs", 0.16f);
            setFloatParam(apvts, "holdMs", 5.0f);
            setFloatParam(apvts, "releaseMs", 58.0f);
            setFloatParam(apvts, "triggerFilter", 2.0f);
            setFloatParam(apvts, "focus", 62.0f);
            setFloatParam(apvts, "transientLevelDb", 0.6f);
            setFloatParam(apvts, "bodyLevelDb", 0.8f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.42f }, { 1, 0.72f }, { 2, 0.06f }, { 3, 0.18f }}, 0);
            setSlot(transientPath, 1, fxEQ, {{ 4, 0.62f }, { 5, 0.58f }, { 6, 0.78f }, { 7, 0.54f }});
            setSlot(bodyPath, 0, fxChorus, {{ 0, 0.10f }, { 1, 0.24f }, { 2, 0.16f }, { 3, 0.30f }});
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.20f }, { 1, 0.28f }, { 2, 0.48f }, { 3, 0.05f }}, 1);
            break;
        case 14: // Pad Bloom
            setFloatParam(apvts, "sensitivity", 40.0f);
            setFloatParam(apvts, "speedMs", 0.48f);
            setFloatParam(apvts, "holdMs", 14.0f);
            setFloatParam(apvts, "releaseMs", 160.0f);
            setFloatParam(apvts, "focus", 38.0f);
            setFloatParam(apvts, "transientLevelDb", -0.8f);
            setFloatParam(apvts, "bodyLevelDb", 2.0f);
            setSlot(transientPath, 0, fxImager, {{ 0, 0.26f }, { 1, 0.30f }, { 2, 0.32f }, { 3, 0.34f }});
            setSlot(bodyPath, 0, fxShift, {{ 0, 0.36f }, { 1, 0.72f }, { 2, 0.08f }, { 3, 0.08f }}, 1);
            setSlot(bodyPath, 1, fxChorus, {{ 0, 0.08f }, { 1, 0.30f }, { 2, 0.16f }, { 3, 0.36f }});
            setSlot(bodyPath, 2, fxReverb, {{ 0, 0.44f }, { 1, 0.42f }, { 2, 0.64f }, { 3, 0.08f }}, 0);
            break;
        case 15: // Acid Tail Motion
            setFloatParam(apvts, "sensitivity", 58.0f);
            setFloatParam(apvts, "speedMs", 0.12f);
            setFloatParam(apvts, "holdMs", 5.0f);
            setFloatParam(apvts, "releaseMs", 92.0f);
            setFloatParam(apvts, "focus", 66.0f);
            setSlot(transientPath, 0, fxDrive, {{ 0, 0.16f }, { 1, 0.60f }, { 2, 0.78f }}, 2);
            setSlot(bodyPath, 0, fxFilter, {{ 0, 0.28f }, { 1, 0.32f }, { 2, 0.26f }, { 3, 0.48f }, { 4, 1.0f }}, 19);
            setSlot(bodyPath, 1, fxPhaser, {{ 0, 0.16f }, { 1, 0.34f }, { 2, 0.44f }, { 3, 0.34f }}, 0);
            setSlot(bodyPath, 2, fxDelay, {{ 0, 0.22f }, { 1, 0.20f }, { 3, 0.80f }, { 4, 0.18f }}, 2);
            break;
        case 16: // 808 Bounce
            setFloatParam(apvts, "sensitivity", 54.0f);
            setFloatParam(apvts, "speedMs", 0.24f);
            setFloatParam(apvts, "holdMs", 10.0f);
            setFloatParam(apvts, "releaseMs", 110.0f);
            setFloatParam(apvts, "triggerFilter", 1.0f);
            setFloatParam(apvts, "focus", 58.0f);
            setFloatParam(apvts, "splitMode", (float) splitModeManual);
            setFloatParam(apvts, "transientLevelDb", -0.5f);
            setFloatParam(apvts, "bodyLevelDb", 2.0f);
            setFloatParam(apvts, "pogoAmount", 86.0f);
            setManualMarkerValuesMs({ 0.0f, 6.0f, 18.0f, 420.0f });
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.56f }, { 1, 0.80f }, { 2, 0.10f }, { 3, 0.32f }}, 1);
            setSlot(transientPath, 1, fxGate, {{ 0, 0.46f }, { 1, 0.04f }, { 2, 0.08f }, { 3, 0.20f }}, 0);
            setSlot(bodyPath, 0, fxDrive, {{ 0, 0.20f }, { 1, 0.54f }, { 2, 0.82f }}, 2);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.16f }, { 1, 0.22f }, { 2, 0.18f }, { 3, 0.52f }, { 4, 1.0f }}, 19);
            break;
        case 17: // Hard Techno Pogo
            setFloatParam(apvts, "sensitivity", 68.0f);
            setFloatParam(apvts, "speedMs", 0.16f);
            setFloatParam(apvts, "holdMs", 10.0f);
            setFloatParam(apvts, "releaseMs", 78.0f);
            setFloatParam(apvts, "triggerFilter", 1.0f);
            setFloatParam(apvts, "focus", 64.0f);
            setFloatParam(apvts, "splitMode", (float) splitModeManual);
            setFloatParam(apvts, "transientLevelDb", 0.8f);
            setFloatParam(apvts, "bodyLevelDb", 1.0f);
            setFloatParam(apvts, "pogoAmount", 78.0f);
            setManualMarkerValuesMs({ 0.0f, 5.0f, 16.0f, 280.0f });
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.74f }, { 1, 0.90f }, { 2, 0.04f }, { 3, 0.18f }}, 3);
            setSlot(transientPath, 1, fxDrive, {{ 0, 0.30f }, { 1, 0.58f }, { 2, 0.86f }}, 5);
            setSlot(bodyPath, 0, fxFilter, {{ 0, 0.20f }, { 1, 0.24f }, { 2, 0.18f }, { 3, 0.48f }, { 4, 1.0f }}, 19);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.22f }, { 1, 0.28f }, { 2, 0.44f }, { 3, 0.04f }}, 2);
            break;
        case 18: // Sub Tightener
            setFloatParam(apvts, "sensitivity", 48.0f);
            setFloatParam(apvts, "speedMs", 0.22f);
            setFloatParam(apvts, "holdMs", 7.0f);
            setFloatParam(apvts, "releaseMs", 70.0f);
            setFloatParam(apvts, "triggerFilter", 1.0f);
            setFloatParam(apvts, "focus", 56.0f);
            setFloatParam(apvts, "transientLevelDb", 0.2f);
            setFloatParam(apvts, "bodyLevelDb", -1.2f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.52f }, { 1, 0.78f }, { 2, 0.08f }, { 3, 0.26f }}, 1);
            setSlot(transientPath, 1, fxDrive, {{ 0, 0.12f }, { 1, 0.54f }, { 2, 0.62f }}, 0);
            setSlot(bodyPath, 0, fxCompressor, {{ 0, 0.58f }, { 1, 0.82f }, { 2, 0.12f }, { 3, 0.38f }}, 2);
            setSlot(bodyPath, 1, fxMultiband, {{ 0, 0.58f }, { 3, 0.56f }, { 6, 0.72f }, { 9, 0.82f }});
            break;
        case 19: // Bass Click Control
            setFloatParam(apvts, "sensitivity", 46.0f);
            setFloatParam(apvts, "speedMs", 0.10f);
            setFloatParam(apvts, "holdMs", 3.0f);
            setFloatParam(apvts, "releaseMs", 36.0f);
            setFloatParam(apvts, "triggerFilter", 2.0f);
            setFloatParam(apvts, "focus", 64.0f);
            setFloatParam(apvts, "transientLevelDb", -2.0f);
            setFloatParam(apvts, "bodyLevelDb", 0.8f);
            setSlot(transientPath, 0, fxFilter, {{ 0, 0.78f }, { 1, 0.16f }, { 2, 0.26f }, { 3, 0.12f }, { 4, 1.0f }}, 9);
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.36f }, { 1, 0.74f }, { 2, 0.04f }, { 3, 0.18f }}, 1);
            setSlot(bodyPath, 0, fxDrive, {{ 0, 0.14f }, { 1, 0.52f }, { 2, 0.72f }}, 2);
            break;
        case 20: // Reese Width Split
            setFloatParam(apvts, "sensitivity", 52.0f);
            setFloatParam(apvts, "speedMs", 0.20f);
            setFloatParam(apvts, "holdMs", 7.0f);
            setFloatParam(apvts, "releaseMs", 96.0f);
            setFloatParam(apvts, "focus", 60.0f);
            setFloatParam(apvts, "transientLevelDb", 0.6f);
            setFloatParam(apvts, "bodyLevelDb", 1.0f);
            setSlot(transientPath, 0, fxImager, {{ 0, 0.24f }, { 1, 0.30f }, { 2, 0.30f }, { 3, 0.32f }});
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.46f }, { 1, 0.72f }, { 2, 0.08f }, { 3, 0.24f }}, 0);
            setSlot(bodyPath, 0, fxShift, {{ 0, 0.40f }, { 1, 0.76f }, { 2, 0.06f }, { 3, 0.06f }}, 1);
            setSlot(bodyPath, 1, fxImager, {{ 0, 0.72f }, { 1, 0.82f }, { 2, 0.88f }, { 3, 0.96f }});
            setSlot(bodyPath, 2, fxChorus, {{ 0, 0.10f }, { 1, 0.26f }, { 2, 0.18f }, { 3, 0.28f }});
            break;
        case 21: // Grit Tail Bass
            setFloatParam(apvts, "sensitivity", 55.0f);
            setFloatParam(apvts, "speedMs", 0.18f);
            setFloatParam(apvts, "holdMs", 6.0f);
            setFloatParam(apvts, "releaseMs", 88.0f);
            setFloatParam(apvts, "triggerFilter", 1.0f);
            setFloatParam(apvts, "focus", 60.0f);
            setFloatParam(apvts, "bodyLevelDb", 1.4f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.48f }, { 1, 0.74f }, { 2, 0.08f }, { 3, 0.26f }}, 0);
            setSlot(bodyPath, 0, fxDrive, {{ 0, 0.24f }, { 1, 0.56f }, { 2, 0.84f }}, 5);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.18f }, { 1, 0.24f }, { 2, 0.18f }, { 3, 0.50f }, { 4, 1.0f }}, 19);
            setSlot(bodyPath, 2, fxMultiband, {{ 0, 0.52f }, { 3, 0.58f }, { 6, 0.70f }, { 9, 0.84f }});
            break;
        case 22: // Lo-fi Body
            setFloatParam(apvts, "bodyLevelDb", 1.5f);
            setSlot(bodyPath, 0, fxBitcrusher, {{ 0, 0.54f }, { 1, 0.40f }, { 3, 0.70f }}, 2);
            setSlot(bodyPath, 1, fxFilter, {{ 0, 0.24f }, { 1, 0.18f }, { 3, 0.42f }}, 0);
            break;
        case 23: // Gate Chop
            setFloatParam(apvts, "holdMs", 3.0f);
            setFloatParam(apvts, "releaseMs", 24.0f);
            setSlot(transientPath, 0, fxEQ, {{ 6, 0.80f }, { 7, 0.58f }});
            setSlot(transientPath, 1, fxCompressor, {{ 0, 0.52f }, { 1, 0.84f }, { 2, 0.06f }, { 3, 0.20f }}, 1);
            setSlot(bodyPath, 0, fxGate, {{ 0, 0.70f }, { 1, 0.03f }, { 2, 0.06f }, { 3, 0.08f }}, 0);
            break;
        case 24: // Riser Bloom
            setFloatParam(apvts, "sensitivity", 44.0f);
            setFloatParam(apvts, "speedMs", 0.34f);
            setFloatParam(apvts, "holdMs", 12.0f);
            setFloatParam(apvts, "releaseMs", 160.0f);
            setFloatParam(apvts, "focus", 40.0f);
            setFloatParam(apvts, "transientLevelDb", -1.0f);
            setFloatParam(apvts, "bodyLevelDb", 2.2f);
            setSlot(bodyPath, 0, fxShift, {{ 0, 0.48f }, { 1, 0.70f }, { 2, 0.10f }, { 3, 0.10f }}, 1);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.54f }, { 1, 0.42f }, { 2, 0.70f }, { 3, 0.10f }}, 3);
            setSlot(bodyPath, 2, fxImager, {{ 0, 0.78f }, { 1, 0.86f }, { 2, 0.92f }, { 3, 1.0f }});
            break;
        case 25: // Impact Tail Designer
            setFloatParam(apvts, "sensitivity", 64.0f);
            setFloatParam(apvts, "speedMs", 0.14f);
            setFloatParam(apvts, "holdMs", 8.0f);
            setFloatParam(apvts, "releaseMs", 118.0f);
            setFloatParam(apvts, "focus", 64.0f);
            setFloatParam(apvts, "splitMode", (float) splitModeManual);
            setFloatParam(apvts, "transientLevelDb", 0.8f);
            setFloatParam(apvts, "bodyLevelDb", 1.4f);
            setFloatParam(apvts, "pogoAmount", 12.0f);
            setManualMarkerValuesMs({ 0.0f, 8.0f, 20.0f, 700.0f });
            setSlot(transientPath, 0, fxDrive, {{ 0, 0.28f }, { 1, 0.58f }, { 2, 0.82f }}, 1);
            setSlot(transientPath, 1, fxEQ, {{ 4, 0.62f }, { 5, 0.60f }, { 6, 0.82f }, { 7, 0.56f }});
            setSlot(bodyPath, 0, fxGate, {{ 0, 0.48f }, { 1, 0.06f }, { 2, 0.12f }, { 3, 0.18f }}, 0);
            setSlot(bodyPath, 1, fxReverb, {{ 0, 0.50f }, { 1, 0.44f }, { 2, 0.60f }, { 3, 0.04f }}, 0);
            setSlot(bodyPath, 2, fxFilter, {{ 0, 0.20f }, { 1, 0.18f }, { 2, 0.10f }, { 3, 0.36f }, { 4, 1.0f }}, 0);
            break;
        case 26: // Glitch Tail
            setFloatParam(apvts, "sensitivity", 66.0f);
            setFloatParam(apvts, "speedMs", 0.12f);
            setFloatParam(apvts, "holdMs", 4.0f);
            setFloatParam(apvts, "releaseMs", 64.0f);
            setFloatParam(apvts, "focus", 70.0f);
            setFloatParam(apvts, "bodyLevelDb", 1.2f);
            setSlot(transientPath, 0, fxCompressor, {{ 0, 0.60f }, { 1, 0.82f }, { 2, 0.04f }, { 3, 0.18f }}, 1);
            setSlot(bodyPath, 0, fxBitcrusher, {{ 0, 0.42f }, { 1, 0.36f }, { 3, 0.62f }}, 3);
            setSlot(bodyPath, 1, fxTremolo, {{ 0, 0.24f }, { 1, 0.82f }, { 2, 0.12f }, { 3, 0.60f }}, 1);
            setSlot(bodyPath, 2, fxFilter, {{ 0, 0.34f }, { 1, 0.42f }, { 2, 0.24f }, { 3, 0.32f }, { 4, 0.74f }}, 11);
            break;
        case 27: // Init
        default:
            break;
    }

    currentPresetName = names[presetIndex];
    suspendProcessing(false);

    resetDetectionState();
    updateLatencyFromParameters();
    updateSidechainFilters();
    resetEffectState();
    return true;
}

int PluginProcessor::getFXSlotType(int slotIndex) const
{
    return getFXOrderSlot(slotIndex);
}

bool PluginProcessor::isFXSlotActive(int slotIndex) const
{
    return getFXOrderSlot(slotIndex) != fxOff && getFXSlotOnValue(slotIndex);
}

int PluginProcessor::getFXSlotVariant(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return 0;

    return juce::roundToInt(getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), getFXSlotVariantParamID(slotIndex), 0.0f));
}

void PluginProcessor::setFXSlotVariant(int slotIndex, int variantIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setFloatParam(apvts, getFXSlotVariantParamID(slotIndex), (float) juce::jmax(0, variantIndex));
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
        upward[(size_t) band] = multibandUpwardMeters[(size_t) slotIndex][(size_t) band].load();
        downward[(size_t) band] = multibandDownwardMeters[(size_t) slotIndex][(size_t) band].load();
    }
}

void PluginProcessor::getFXEQSpectrum(int slotIndex, std::array<float, fxEQSpectrumBinCount>& spectrum) const
{
    spectrum.fill(0.0f);
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    for (int i = 0; i < fxEQSpectrumBinCount; ++i)
        spectrum[(size_t) i] = eqSpectrumBins[(size_t) slotIndex][(size_t) i].load();
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
        xs[(size_t) i] = imagerScopeX[(size_t) slotIndex][(size_t) i].load();
        ys[(size_t) i] = imagerScopeY[(size_t) slotIndex][(size_t) i].load();
    }
}

float PluginProcessor::getFXSlotFloatValue(int slotIndex, int parameterIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots) || ! juce::isPositiveAndBelow(parameterIndex, fxSlotParameterCount))
        return getFXSlotDefaultValue(parameterIndex);

    return getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts),
                            getFXSlotFloatParamID(slotIndex, parameterIndex),
                            getFXSlotDefaultValue(parameterIndex));
}

bool PluginProcessor::getFXSlotOnValue(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return false;

    return getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), getFXSlotOnParamID(slotIndex), 0.0f) >= 0.5f;
}

int PluginProcessor::getFXOrderSlot(int slotIndex) const
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return fxOff;

    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), getFXOrderParamID(slotIndex));
}

void PluginProcessor::setFXOrderSlot(int slotIndex, int moduleTypeIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(getFXOrderParamID(slotIndex))))
        param->setValueNotifyingHost(param->convertTo0to1((float) moduleTypeIndex));
}

void PluginProcessor::copyFXSlotState(int sourceIndex, int destinationIndex)
{
    setFXOrderSlot(destinationIndex, getFXOrderSlot(sourceIndex));
    setBoolParam(apvts, getFXSlotOnParamID(destinationIndex), getFXSlotOnValue(sourceIndex));
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setFloatParam(apvts, getFXSlotFloatParamID(destinationIndex, parameterIndex), getFXSlotFloatValue(sourceIndex, parameterIndex));
    setFloatParam(apvts, getFXSlotVariantParamID(destinationIndex), (float) getFXSlotVariant(sourceIndex));
}

void PluginProcessor::clearFXSlotState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, maxFXSlots))
        return;

    setFXOrderSlot(slotIndex, fxOff);
    setBoolParam(apvts, getFXSlotOnParamID(slotIndex), false);
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setFloatParam(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), getFXSlotDefaultValue(parameterIndex));
    setFloatParam(apvts, getFXSlotVariantParamID(slotIndex), 0.0f);
}

void PluginProcessor::compactPathSlots(int path)
{
    const int start = juce::jlimit(0, 1, path) * slotsPerPath;
    const int end = start + slotsPerPath;
    int writeIndex = start;
    for (int readIndex = start; readIndex < end; ++readIndex)
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

    while (writeIndex < end)
        clearFXSlotState(writeIndex++);
}

void PluginProcessor::initialiseFXSlotDefaults(int slotIndex, int moduleTypeIndex)
{
    setBoolParam(apvts, getFXSlotOnParamID(slotIndex), true);
    int initialVariant = 0;
    std::array<float, fxSlotParameterCount> parameters{};
    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        parameters[(size_t) parameterIndex] = getFXSlotDefaultValue(parameterIndex);

    switch (moduleTypeIndex)
    {
        case fxDrive: parameters[0] = 0.34f; parameters[1] = 0.56f; parameters[2] = 1.0f; break;
        case fxFilter: parameters[0] = 0.72f; parameters[1] = 0.22f; parameters[2] = 0.18f; parameters[3] = 0.40f; parameters[4] = 1.0f; break;
        case fxChorus: parameters[0] = 0.18f; parameters[1] = 0.46f; parameters[2] = 0.30f; parameters[3] = 0.50f; parameters[4] = 1.0f; break;
        case fxFlanger: parameters[0] = 0.22f; parameters[1] = 0.48f; parameters[2] = 0.22f; parameters[3] = 0.58f; parameters[4] = 1.0f; break;
        case fxPhaser: parameters[0] = 0.24f; parameters[1] = 0.52f; parameters[2] = 0.50f; parameters[3] = 0.48f; parameters[4] = 1.0f; break;
        case fxDelay: parameters[0] = 0.42f; parameters[1] = 0.42f; parameters[2] = 0.0f; parameters[3] = 1.0f; parameters[4] = 0.38f; parameters[5] = 1.0f; parameters[6] = 1.0f; parameters[7] = 1.0f; break;
        case fxReverb: parameters[0] = 0.28f; parameters[1] = 0.36f; parameters[2] = 0.76f; parameters[3] = 0.05f; parameters[4] = 0.34f; break;
        case fxCompressor: parameters[0] = 0.42f; parameters[1] = 0.76f; parameters[2] = 0.52f; parameters[3] = 0.46f; parameters[4] = 1.0f; break;
        case fxEQ:
            parameters[0] = 0.14f; parameters[1] = 0.50f; parameters[2] = 0.34f; parameters[3] = 0.50f;
            parameters[4] = 0.58f; parameters[5] = 0.50f; parameters[6] = 0.82f; parameters[7] = 0.50f;
            parameters[8] = 0.22f; parameters[9] = 0.22f; parameters[10] = 0.22f; parameters[11] = 0.22f;
            break;
        case fxMultiband:
            parameters[0] = 0.42f; parameters[1] = 0.30f; parameters[2] = 0.22f;
            parameters[3] = 0.58f; parameters[4] = 0.44f; parameters[5] = 0.30f;
            parameters[6] = 0.72f; parameters[7] = 0.34f; parameters[8] = 0.50f; parameters[9] = 1.0f;
            break;
        case fxBitcrusher: parameters[0] = 0.52f; parameters[1] = 0.36f; parameters[2] = 0.12f; parameters[3] = 0.62f; parameters[4] = 1.0f; break;
        case fxImager:
            parameters[0] = 0.50f; parameters[1] = 0.50f; parameters[2] = 0.50f; parameters[3] = 0.50f;
            parameters[4] = 0.28f; parameters[5] = 0.53f; parameters[6] = 0.78f;
            break;
        case fxShift: parameters[0] = 0.24f; parameters[1] = 0.62f; parameters[2] = 0.18f; parameters[3] = 0.14f; parameters[4] = 1.0f; break;
        case fxTremolo: parameters[0] = 0.18f; parameters[1] = 0.70f; parameters[2] = 0.28f; parameters[3] = 0.52f; parameters[4] = 1.0f; break;
        case fxGate:
            parameters[0] = 0.46f; parameters[1] = 0.08f; parameters[2] = 0.18f; parameters[3] = 0.20f;
            parameters[4] = gateRateIndexToNormal(2); parameters[5] = 1.0f;
            initialVariant = getDefaultFXGateVariant();
            break;
        default: break;
    }

    for (int parameterIndex = 0; parameterIndex < fxSlotParameterCount; ++parameterIndex)
        setFloatParam(apvts, getFXSlotFloatParamID(slotIndex, parameterIndex), parameters[(size_t) parameterIndex]);
    setFloatParam(apvts, getFXSlotVariantParamID(slotIndex), (float) initialVariant);
}

void PluginProcessor::prepareEffectProcessors(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32) maxInternalBlockSize, 2 };
    const int delaySize = juce::jmax(2048, (int) std::ceil(sampleRate * 2.5));
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

        delayStates[(size_t) slot].left.assign((size_t) delaySize, 0.0f);
        delayStates[(size_t) slot].right.assign((size_t) delaySize, 0.0f);
        shiftStates[(size_t) slot].left.assign((size_t) shiftSize, 0.0f);
        shiftStates[(size_t) slot].right.assign((size_t) shiftSize, 0.0f);
        resetShiftState(slot);
        resetPhaserState(slot);
        clearFXEQSpectrum(slot);
        clearFXImagerScope(slot);
    }

    resetEffectState();
}

void PluginProcessor::resetEffectState()
{
    for (int slot = 0; slot < maxFXSlots; ++slot)
    {
        driveToneStates[(size_t) slot] = {};
        compressorStates[(size_t) slot] = {};
        bitcrusherStates[(size_t) slot] = {};
        imagerStates[(size_t) slot] = {};
        gateStates[(size_t) slot] = {};
        tremoloPhases[(size_t) slot] = 0.0f;
        compressorGainReductionMeters[(size_t) slot].store(0.0f);

        for (int band = 0; band < fxMultibandBandCount; ++band)
        {
            multibandUpwardMeters[(size_t) slot][(size_t) band].store(0.0f);
            multibandDownwardMeters[(size_t) slot][(size_t) band].store(0.0f);
        }

        clearFXEQSpectrum(slot);
        clearFXImagerScope(slot);
        resetShiftState(slot);
        resetPhaserState(slot);

        fxFilterSlotsL[(size_t) slot].setCoefficients(makeIdentityCoefficients());
        fxFilterSlotsR[(size_t) slot].setCoefficients(makeIdentityCoefficients());
        fxFilterStage2SlotsL[(size_t) slot].setCoefficients(makeIdentityCoefficients());
        fxFilterStage2SlotsR[(size_t) slot].setCoefficients(makeIdentityCoefficients());

        for (int band = 0; band < fxEQBandCount; ++band)
        {
            fxEQSlotsL[(size_t) slot][(size_t) band].setCoefficients(makeIdentityCoefficients());
            fxEQSlotsR[(size_t) slot][(size_t) band].setCoefficients(makeIdentityCoefficients());
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
    }
}

void PluginProcessor::resetPhaserState(int slotIndex)
{
    if (juce::isPositiveAndBelow(slotIndex, maxFXSlots))
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

    auto magnitudeForBin = [this](int bin)
    {
        const float real = eqSpectrumFFTData[(size_t) bin * 2];
        const float imag = eqSpectrumFFTData[(size_t) bin * 2 + 1];
        return std::sqrt(real * real + imag * imag) / (float) fftSize;
    };

    for (int i = 0; i < fxEQSpectrumBinCount; ++i)
    {
        const float norm = (float) i / juce::jmax(1.0f, (float) fxEQSpectrumBinCount - 1.0f);
        const float frequency = minFrequency * std::pow(maxFrequency / minFrequency, norm);
        const float fractionalBin = juce::jlimit(1.0f, (float) fftSize * 0.5f - 2.0f,
                                                 (frequency / juce::jmax(20.0f, nyquist)) * (float) (fftSize * 0.5f));
        const int lowerBin = juce::jlimit(1, fftSize / 2 - 2, (int) std::floor(fractionalBin));
        const int upperBin = lowerBin + 1;
        const float alpha = fractionalBin - (float) lowerBin;
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

void PluginProcessor::applyEffects(juce::AudioBuffer<float>& buffer, int slotStart, int slotEnd)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0)
        return;

    auto copyBufferToScratch = [this, &buffer, numChannels, numSamples]()
    {
        for (int channel = 0; channel < numChannels; ++channel)
            effectScratch.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    };

    for (int slot = slotStart; slot < slotEnd; ++slot)
    {
        const int moduleType = getFXOrderSlot(slot);
        if (! getFXSlotOnValue(slot))
            continue;

        if (moduleType == fxDrive)
        {
            const float a = getFXSlotFloatValue(slot, 0);
            const float b = getFXSlotFloatValue(slot, 1);
            const float c = getFXSlotFloatValue(slot, 2);
            const int driveType = juce::jlimit(0, getFXDriveTypeNames().size() - 1, getFXSlotVariant(slot));
            if (c <= 0.0001f)
                continue;

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
                        case 1: driven = juce::jlimit(-1.0f, 1.0f, input * 0.72f); break;
                        case 2: driven = std::tanh(input + 0.32f * input * input) * 0.92f; break;
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
                        case 4: driven = std::tanh(input * 0.70f + std::sin(input * (1.8f + a * 2.4f)) * 0.45f); break;
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
                        default: break;
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
                case 0: stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 6.5f, 0.55f + p1 * 5.95f)); break;
                case 1: stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, juce::jlimit(0.65f, 4.0f, 0.65f + p1 * 3.35f)); stage2Coefficients = stage1Coefficients; useStage2 = true; break;
                case 2: stage1Coefficients = juce::IIRCoefficients::makeHighPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 6.5f, 0.55f + p1 * 5.95f)); break;
                case 3: stage1Coefficients = juce::IIRCoefficients::makeHighPass(currentSampleRate, cutoff, juce::jlimit(0.65f, 4.0f, 0.65f + p1 * 3.35f)); stage2Coefficients = stage1Coefficients; useStage2 = true; break;
                case 4: stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate, cutoff, juce::jlimit(0.45f, 10.0f, 0.85f + p1 * 6.5f)); break;
                case 5: stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate, cutoff, juce::jlimit(0.55f, 8.0f, 0.85f + p1 * 5.0f)); stage2Coefficients = stage1Coefficients; useStage2 = true; break;
                case 6: stage1Coefficients = juce::IIRCoefficients::makeNotchFilter(currentSampleRate, cutoff, juce::jlimit(0.75f, 10.0f, 0.75f + p1 * 9.25f)); break;
                case 7: driveParam = p3; stage1Coefficients = juce::IIRCoefficients::makePeakFilter(currentSampleRate, cutoff, juce::jlimit(0.45f, 8.0f, 0.8f + p1 * 5.5f), juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2))); break;
                case 8: driveParam = p3; stage1Coefficients = juce::IIRCoefficients::makeLowShelf(currentSampleRate, cutoff, juce::jlimit(0.5f, 1.5f, 0.65f + p1 * 0.8f), juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2))); break;
                case 9: driveParam = p3; stage1Coefficients = juce::IIRCoefficients::makeHighShelf(currentSampleRate, cutoff, juce::jlimit(0.5f, 1.5f, 0.65f + p1 * 0.8f), juce::Decibels::decibelsToGain(normalToFXFilterGainDb(p2))); break;
                case 10:
                    driveParam = p3;
                    stage1Coefficients = juce::IIRCoefficients::makeAllPass(currentSampleRate, cutoff, juce::jlimit(0.45f, 1.4f, 0.55f + p1 * 0.9f));
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
                    stage1Coefficients = juce::IIRCoefficients::makeBandPass(currentSampleRate, centreA, juce::jlimit(1.0f, 12.0f, 1.4f + p1 * 7.0f));
                    stage2Coefficients = juce::IIRCoefficients::makePeakFilter(currentSampleRate, centreB, juce::jlimit(0.7f, 7.0f, 1.0f + p1 * 4.8f), juce::Decibels::decibelsToGain(4.0f + p1 * 10.0f));
                    useStage2 = true;
                    break;
                }
                case 19:
                    useLadder = true;
                    driveParam = p2;
                    ladderBias = juce::jmap(p3, -0.28f, 0.28f);
                    stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, juce::jlimit(0.75f, 3.0f, 0.75f + p1 * 2.25f));
                    stage2Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, juce::jlimit(24.0f, (float) currentSampleRate * 0.45f, cutoff * (0.78f + p3 * 0.34f)), juce::jlimit(0.70f, 1.8f, 0.72f + p1 * 1.08f));
                    useStage2 = true;
                    break;
                default: stage1Coefficients = juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff); break;
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

                const float tunedFrequency = juce::jlimit(18.0f, (float) currentSampleRate * 0.25f, normalToFXFilterFrequency(p0) * 0.42f);
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
            chorus.setRate(syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm) : normalToFXChorusRateHz(rate));
            chorus.setDepth(juce::jlimit(0.06f, 1.0f, 0.08f + depth * 0.90f));
            chorus.setCentreDelay(normalToFXChorusDelayMs(delay));
            chorus.setFeedback(normalToFXChorusFeedback(feedback));
            chorus.setMix(1.0f);

            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            chorus.process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.setSample(channel, sample, buffer.getSample(channel, sample) * (1.0f - mix) + effectScratch.getSample(channel, sample) * mix);

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
                    detector = juce::jmax(detector, std::abs(buffer.getSample(channel, sample) * inputGain));

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
            const std::array<float, 3> downwardAmounts{ getFXSlotFloatValue(slot, 0), getFXSlotFloatValue(slot, 1), getFXSlotFloatValue(slot, 2) };
            const std::array<float, 3> upwardAmounts{ getFXSlotFloatValue(slot, 3), getFXSlotFloatValue(slot, 4), getFXSlotFloatValue(slot, 5) };
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
                        state.downwardEnvelopes[(size_t) band] = targetDownDb + coeffDown * (state.downwardEnvelopes[(size_t) band] - targetDownDb);

                        const float targetUpDb = juce::jlimit(0.0f, 18.0f,
                                                              juce::jmax(0.0f, upThresholdDb[(size_t) band] - bandLevelDb)
                                                                  * (0.18f + depth * 0.32f)
                                                                  * upwardAmounts[(size_t) band]);
                        const float coeffUp = targetUpDb > state.upwardEnvelopes[(size_t) band] ? attackCoeff : releaseCoeff;
                        state.upwardEnvelopes[(size_t) band] = targetUpDb + coeffUp * (state.upwardEnvelopes[(size_t) band] - targetUpDb);

                        bands[(size_t) band] *= juce::Decibels::decibelsToGain(state.upwardEnvelopes[(size_t) band] - state.downwardEnvelopes[(size_t) band]);
                    }

                    const float recombined = (bands[0] + bands[1] + bands[2]) * outputGain;
                    const float wet = std::tanh(recombined * 1.06f);
                    data[sample] = dry * (1.0f - mix) + wet * mix;
                }
            }

            for (int band = 0; band < fxMultibandBandCount; ++band)
            {
                multibandUpwardMeters[(size_t) slot][(size_t) band].store(juce::jlimit(0.0f, 1.0f, state.upwardEnvelopes[(size_t) band] / 18.0f));
                multibandDownwardMeters[(size_t) slot][(size_t) band].store(juce::jlimit(0.0f, 1.0f, state.downwardEnvelopes[(size_t) band] / 24.0f));
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

            const int bitDepth = normalToFXBitcrusherBitDepth(juce::jlimit(0.0f, 1.0f, bits + mode.bitBias));
            const float quantisationSteps = (float) (1 << bitDepth);
            const int baseHoldSamples = normalToFXBitcrusherHoldSamples(juce::jlimit(0.0f, 1.0f, rate + mode.rateBias));
            const float jitterAmount = juce::jlimit(0.0f, 1.0f, jitter + mode.jitterBias) * 0.65f;
            const float toneHz = normalToFXBitcrusherToneHz(juce::jlimit(0.0f, 1.0f, tone + mode.toneBias));
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
                        state.heldSample[(size_t) channel] = juce::jlimit(-1.0f, 1.0f, std::round(dry * quantisationSteps) / quantisationSteps);
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

            const float coeff1 = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * normalToFXImagerFrequency(crossovers[0]) / (float) currentSampleRate));
            const float coeff2 = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * normalToFXImagerFrequency(crossovers[1]) / (float) currentSampleRate));
            const float coeff3 = juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * normalToFXImagerFrequency(crossovers[2]) / (float) currentSampleRate));

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

                std::array<float, 4> bandsL{ state.lowState1[0], state.lowState2[0] - state.lowState1[0], state.lowState3[0] - state.lowState2[0], leftIn - state.lowState3[0] };
                std::array<float, 4> bandsR{ state.lowState1[1], state.lowState2[1] - state.lowState1[1], state.lowState3[1] - state.lowState2[1], rightIn - state.lowState3[1] };

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
                    imagerScopeX[(size_t) slot][(size_t) writeIndex].store(juce::jlimit(-1.0f, 1.0f, (leftOut + rightOut) * 0.5f * scopeScale));
                    imagerScopeY[(size_t) slot][(size_t) writeIndex].store(juce::jlimit(-1.0f, 1.0f, (leftOut - rightOut) * 0.5f * scopeScale));
                }
            }

            continue;
        }

        if (moduleType == fxShift)
        {
            const float amount = getFXSlotFloatValue(slot, 0);
            const float width = getFXSlotFloatValue(slot, 1);
            const float delay = getFXSlotFloatValue(slot, 2);
            const float feedback = getFXSlotFloatValue(slot, 3);
            const float mix = getFXSlotFloatValue(slot, 4);
            const int mode = juce::jlimit(0, getFXShiftTypeNames().size() - 1, getFXSlotVariant(slot));
            if (mix <= 0.0001f)
                continue;

            auto& state = shiftStates[(size_t) slot];
            const int delayBufferSize = juce::jmin((int) state.left.size(), (int) state.right.size());
            if (delayBufferSize <= 0)
                continue;

            const float baseDelayMs = mode <= 1 ? normalToFXShiftDelayMs(delay) : normalToFXShiftPreDelayMs(delay);
            const int delaySamples = juce::jlimit(1, delayBufferSize - 1, (int) std::round(baseDelayMs * (float) currentSampleRate / 1000.0f));
            const float detuneCents = normalToFXShiftDetuneCents(amount);
            const float ringRateHz = normalToFXShiftRateHz(amount);
            const float stereoSpread = juce::jlimit(0.0f, 1.0f, width);
            const float feedbackAmount = juce::jmap(feedback, 0.0f, 0.8f);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const int readPosition = (state.writePosition - delaySamples + delayBufferSize) % delayBufferSize;
                const float dryLeft = buffer.getSample(0, sample);
                const float dryRight = numChannels > 1 ? buffer.getSample(1, sample) : dryLeft;
                float wetLeft = state.left[(size_t) readPosition];
                float wetRight = state.right[(size_t) readPosition];

                switch (mode)
                {
                    case 0:
                    case 1:
                    {
                        const float centsLeft = detuneCents * (0.8f + stereoSpread * 0.6f);
                        const float centsRight = detuneCents * (1.2f + stereoSpread * 0.8f);
                        wetLeft = dryLeft * std::sin(state.modPhaseL) + wetLeft * std::cos(state.modPhaseL) * juce::Decibels::decibelsToGain(centsLeft * 0.02f);
                        wetRight = dryRight * std::sin(state.modPhaseR) + wetRight * std::cos(state.modPhaseR) * juce::Decibels::decibelsToGain((mode == 0 ? -centsRight : centsRight) * 0.02f);
                        state.modPhaseL += juce::MathConstants<float>::twoPi * (0.12f + stereoSpread * 0.25f) / (float) currentSampleRate;
                        state.modPhaseR += juce::MathConstants<float>::twoPi * (0.16f + stereoSpread * 0.31f) / (float) currentSampleRate;
                        break;
                    }
                    case 2:
                    {
                        state.carrierPhaseL += juce::MathConstants<float>::twoPi * ringRateHz / (float) currentSampleRate;
                        state.carrierPhaseR += juce::MathConstants<float>::twoPi * ringRateHz * (1.0f + stereoSpread * 0.16f) / (float) currentSampleRate;
                        wetLeft = dryLeft * std::sin(state.carrierPhaseL);
                        wetRight = dryRight * std::sin(state.carrierPhaseR);
                        break;
                    }
                    case 3:
                    default:
                    {
                        state.carrierPhaseL += juce::MathConstants<float>::twoPi * ringRateHz / (float) currentSampleRate;
                        state.carrierPhaseR += juce::MathConstants<float>::twoPi * ringRateHz * (1.0f + stereoSpread * 0.12f) / (float) currentSampleRate;
                        wetLeft = std::sin(state.carrierPhaseL + dryLeft * 3.2f) * 0.7f;
                        wetRight = std::sin(state.carrierPhaseR + dryRight * 3.2f) * 0.7f;
                        break;
                    }
                }

                buffer.setSample(0, sample, dryLeft * (1.0f - mix) + wetLeft * mix);
                if (numChannels > 1)
                    buffer.setSample(1, sample, dryRight * (1.0f - mix) + wetRight * mix);

                state.left[(size_t) state.writePosition] = juce::jlimit(-1.0f, 1.0f, dryLeft + wetLeft * feedbackAmount);
                state.right[(size_t) state.writePosition] = juce::jlimit(-1.0f, 1.0f, dryRight + wetRight * feedbackAmount);
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

            float& phase = tremoloPhases[(size_t) slot];
            const float rateHz = syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm) : normalToFXChorusRateHz(rate);
            const float phaseOffset = normalToFXTremoloStereoPhase(stereo);

            auto tremoloShape = [shape](float value)
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
                            target = stepFrac / juce::jmax(0.001f, attackFrac);
                        else
                            target = sustainLevel + (1.0f - sustainLevel) * std::exp(-(stepFrac - attackFrac) / juce::jmax(0.001f, 1.0f - attackFrac) * decayCurve);
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
            flanger.setRate(syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm) : normalToFXChorusRateHz(rate));
            flanger.setDepth(juce::jlimit(0.05f, 1.0f, 0.08f + depth * 0.92f));
            flanger.setCentreDelay(normalToFXFlangerDelayMs(delay));
            flanger.setFeedback(normalToFXFlangerFeedback(feedback));
            flanger.setMix(1.0f);

            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            flanger.process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.setSample(channel, sample, buffer.getSample(channel, sample) * (1.0f - mix) + effectScratch.getSample(channel, sample) * mix);

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
            const float rateHz = syncEnabled ? normalToFXChorusSyncedRateHz(rate, currentHostBpm) : normalToFXPhaserRateHz(rate);
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
                        const float wet = processPhaserSample(dry, state.left, leftCoeff, state.feedbackLeft);
                        state.feedbackLeft = wet * feedbackAmount;
                        data[sample] = dry * (1.0f - mix) + wet * mix;
                    }
                    else
                    {
                        const float wet = processPhaserSample(dry, state.right, rightCoeff, state.feedbackRight);
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
            const float leftDelayMs = syncEnabled ? normalToFXDelaySyncedMs(leftTime, currentHostBpm) : normalToFXDelayFreeMs(leftTime);
            const float rightDelayMs = syncEnabled ? normalToFXDelaySyncedMs(effectiveRightTime, currentHostBpm) : normalToFXDelayFreeMs(effectiveRightTime);
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
            const float lowPassCoeff = lowPassEnabled ? juce::jlimit(0.001f, 1.0f, 1.0f - std::exp(-juce::MathConstants<float>::twoPi * lowPassHz / (float) currentSampleRate)) : 1.0f;
            const int delayType = juce::jlimit(0, getFXDelayTypeNames().size() - 1, getFXSlotVariant(slot));

            auto processDelayFilter = [&delayState, highPassEnabled, lowPassEnabled, highPassCoeff, lowPassCoeff](float input, int channel) mutable
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
                    case 1: writeLeft += wetRight * feedbackGain; writeRight += wetLeft * feedbackGain; break;
                    case 2: writeLeft += std::tanh(wetLeft * (0.9f + feedback * 0.7f)) * feedbackGain; writeRight += std::tanh(wetRight * (0.9f + feedback * 0.7f)) * feedbackGain; break;
                    case 3: writeLeft += (wetLeft * 0.74f + wetRight * 0.26f) * feedbackGain; writeRight += (wetRight * 0.74f + wetLeft * 0.26f) * feedbackGain; break;
                    default: writeLeft += wetLeft * feedbackGain; writeRight += wetRight * feedbackGain; break;
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
            const int delaySamples = delayBufferSize > 0 ? juce::jlimit(0, delayBufferSize - 1, (int) std::round(preDelayMs * (float) currentSampleRate / 1000.0f)) : 0;

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
                case 1: parameters.roomSize = juce::jlimit(0.12f, 0.92f, parameters.roomSize * 0.82f); parameters.damping = juce::jlimit(0.08f, 0.90f, parameters.damping * 0.74f); break;
                case 2: parameters.roomSize = juce::jlimit(0.10f, 0.78f, parameters.roomSize * 0.60f); parameters.damping = juce::jlimit(0.18f, 0.90f, 0.48f + damp * 0.32f); parameters.width = juce::jlimit(0.0f, 1.0f, parameters.width * 0.86f); break;
                case 3: parameters.roomSize = juce::jlimit(0.20f, 0.99f, parameters.roomSize + 0.12f); parameters.damping = juce::jlimit(0.05f, 0.84f, parameters.damping * 0.58f); parameters.width = 1.0f; break;
                case 4: parameters.roomSize = juce::jlimit(0.10f, 0.88f, parameters.roomSize * 0.72f); parameters.damping = juce::jlimit(0.40f, 0.98f, 0.72f + damp * 0.22f); parameters.width = juce::jlimit(0.0f, 1.0f, parameters.width * 0.64f); break;
                default: break;
            }

            reverbSlots[(size_t) slot].setParameters(parameters);
            auto wetBlock = juce::dsp::AudioBlock<float>(effectScratch).getSubsetChannelBlock(0, (size_t) numChannels).getSubBlock(0, (size_t) numSamples);
            auto wetContext = juce::dsp::ProcessContextReplacing<float>(wetBlock);
            reverbSlots[(size_t) slot].process(wetContext);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    float wet = effectScratch.getSample(channel, sample);
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

                    const float dry = buffer.getSample(channel, sample);
                    buffer.setSample(channel, sample, dry * (1.0f - mix) + wet * mix);
                }
            }
        }
    }
}

void PluginProcessor::updateMeterState(MeterState& meter, const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples, bool clipLatch)
{
    auto updateOne = [numSamples](std::atomic<float>& peakState, std::atomic<float>& rmsState, const float* data)
    {
        float peak = 0.0f;
        double sumSquares = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            peak = juce::jmax(peak, std::abs(sample));
            sumSquares += sample * sample;
        }

        const float rms = numSamples > 0 ? std::sqrt((float) (sumSquares / (double) numSamples)) : 0.0f;
        peakState.store(juce::jmax(peak * 0.72f, peakState.load() * 0.86f));
        rmsState.store(rmsState.load() * 0.72f + rms * 0.28f);
    };

    updateOne(meter.leftPeak, meter.leftRms, buffer.getReadPointer(0));
    if (numChannels > 1)
        updateOne(meter.rightPeak, meter.rightRms, buffer.getReadPointer(1));
    else
    {
        meter.rightPeak.store(meter.leftPeak.load());
        meter.rightRms.store(meter.leftRms.load());
    }

    const bool clipped = meter.leftPeak.load() > 1.0f || meter.rightPeak.load() > 1.0f;
    if (clipLatch && clipped)
        outputClipLatched.store(true);
    meter.clipped.store(clipLatch ? outputClipLatched.load() : clipped);
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::makeMeterSnapshot(const MeterState& state) const noexcept
{
    return { state.leftPeak.load(), state.rightPeak.load(), state.leftRms.load(), state.rightRms.load(), state.clipped.load() };
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::getInputMeterSnapshot() const noexcept
{
    return makeMeterSnapshot(inputMeter);
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::getTransientMeterSnapshot() const noexcept
{
    return makeMeterSnapshot(transientMeter);
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::getBodyMeterSnapshot() const noexcept
{
    return makeMeterSnapshot(bodyMeter);
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::getOutputMeterSnapshot() const noexcept
{
    return makeMeterSnapshot(outputMeter);
}

float PluginProcessor::getDetectionActivity() const noexcept
{
    return detectionActivity.load();
}

void PluginProcessor::clearOutputClipLatch() noexcept
{
    outputClipLatched.store(false);
    outputMeter.clipped.store(false);
}

void PluginProcessor::pushVisualizerSample(float waveform, float transientEnv, float bodyEnv) noexcept
{
    const int writePosition = visualizerWritePosition.load();
    waveformHistory[(size_t) writePosition].store(waveform);
    transientHistory[(size_t) writePosition].store(transientEnv);
    bodyHistory[(size_t) writePosition].store(bodyEnv);
    visualizerWritePosition.store((writePosition + 1) % visualizerHistorySize);
}

void PluginProcessor::pushBeforeAfterVisualizerSample(float before, float after) noexcept
{
    const int writePosition = beforeAfterVisualizerWritePosition.load();
    beforeSignalHistory[(size_t) writePosition].store(before);
    afterSignalHistory[(size_t) writePosition].store(after);
    beforeAfterVisualizerWritePosition.store((writePosition + 1) % visualizerHistorySize);
}

void PluginProcessor::getVisualizerData(std::array<float, visualizerHistorySize>& waveform,
                                        std::array<float, visualizerHistorySize>& transientEnvelope,
                                        std::array<float, visualizerHistorySize>& bodyEnvelope,
                                        int& writePosition) const
{
    for (int i = 0; i < visualizerHistorySize; ++i)
    {
        waveform[(size_t) i] = waveformHistory[(size_t) i].load();
        transientEnvelope[(size_t) i] = transientHistory[(size_t) i].load();
        bodyEnvelope[(size_t) i] = bodyHistory[(size_t) i].load();
    }
    writePosition = visualizerWritePosition.load();
}

void PluginProcessor::getBeforeAfterVisualizerData(std::array<float, visualizerHistorySize>& before,
                                                   std::array<float, visualizerHistorySize>& after,
                                                   int& writePosition) const
{
    for (int i = 0; i < visualizerHistorySize; ++i)
    {
        before[(size_t) i] = beforeSignalHistory[(size_t) i].load();
        after[(size_t) i] = afterSignalHistory[(size_t) i].load();
    }
    writePosition = beforeAfterVisualizerWritePosition.load();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
