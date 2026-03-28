#include "FactoryPresetLoader.h"

#include "PluginProcessor.h"

#include <array>
#include <cmath>

namespace factorypresetloader
{
namespace
{
bool setParameterPlain(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plainValue)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
    {
        ranged->setValueNotifyingHost(ranged->convertTo0to1(plainValue));
        return true;
    }

    return false;
}

float readParameterPlain(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();

    return fallback;
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    setParameterPlain(apvts, id, value ? 1.0f : 0.0f);
}

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
    fxDimension
};

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
    setParameterPlain(apvts, prefix + "Octave", (float) octave);
    setParameterPlain(apvts, prefix + "SampleRoot", 60.0f);
    setParameterPlain(apvts, prefix + "Level", level);
    setParameterPlain(apvts, prefix + "Detune", detune);
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

void applyInitPreset(juce::AudioProcessorValueTreeState& apvts)
{
    setOscillator(apvts, 1, 1, 2, 0.88f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 2, 0, 2, 0.00f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setOscillator(apvts, 3, 3, 2, 0.00f, 0.0f, true, false, 3, 0.22f, 0.0f, 0.0f, 0.0f);
    setParameterPlain(apvts, "osc1WarpFMSource", 1.0f);
    setParameterPlain(apvts, "osc2WarpFMSource", 2.0f);
    setParameterPlain(apvts, "osc3WarpFMSource", 0.0f);
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
}

void setOscillatorFMSource(juce::AudioProcessorValueTreeState& apvts, int osc, int sourceIndex)
{
    setParameterPlain(apvts, "osc" + juce::String(osc) + "WarpFMSource", (float) juce::jlimit(0, 6, sourceIndex));
}

void setFilterSection(juce::AudioProcessorValueTreeState& apvts, bool enabled, int mode, float cutoff, float resonance, float drive, float envAmount, float attack, float decay, float sustain, float release)
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

void setVoiceSection(juce::AudioProcessorValueTreeState& apvts, bool mono, bool legato, bool portamento, float glide, float bendUp, float bendDown)
{
    setParameterPlain(apvts, "voiceMode", mono ? 1.0f : 0.0f);
    setBoolParam(apvts, "voiceLegato", legato);
    setBoolParam(apvts, "voicePortamento", portamento);
    setParameterPlain(apvts, "voiceGlide", juce::jlimit(0.0f, 2.0f, glide));
    setParameterPlain(apvts, "pitchBendUp", juce::jlimit(0.0f, 24.0f, bendUp));
    setParameterPlain(apvts, "pitchBendDown", juce::jlimit(0.0f, 24.0f, bendDown));
}

void setModSection(juce::AudioProcessorValueTreeState& apvts, float lfoRate, float lfoDepth, float modWheelCutoff, float modWheelVibrato)
{
    setParameterPlain(apvts, "lfoRate", juce::jlimit(0.05f, 20.0f, lfoRate));
    setParameterPlain(apvts, "lfoDepth", juce::jlimit(0.0f, 1.0f, lfoDepth));
    setParameterPlain(apvts, "modWheelCutoff", juce::jlimit(0.0f, 1.0f, modWheelCutoff));
    setParameterPlain(apvts, "modWheelVibrato", juce::jlimit(0.0f, 1.0f, modWheelVibrato));
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
        {
            float defaultValue = 0.5f;
            if (parameterIndex == 2)
                defaultValue = 0.3f;
            else if (parameterIndex == 3)
                defaultValue = 0.0f;
            else if (parameterIndex == 4 || parameterIndex == 5)
                defaultValue = 0.3f;
            else if (parameterIndex == 6 || parameterIndex == 7)
                defaultValue = 0.0f;

            setParameterPlain(apvts, getFXSlotFloatParamID(slot, parameterIndex), defaultValue);
        }
        setParameterPlain(apvts, getFXSlotVariantParamID(slot), 0.0f);
    }
}

void setFXSlot(juce::AudioProcessorValueTreeState& apvts, int slot, int type, int variant, float a, float b, float c, bool enabled = true)
{
    if (! juce::isPositiveAndBelow(slot, PluginProcessor::maxFXSlots))
        return;

    std::array<float, PluginProcessor::fxSlotParameterCount> parameters{};
    for (int parameterIndex = 0; parameterIndex < PluginProcessor::fxSlotParameterCount; ++parameterIndex)
        parameters[(size_t) parameterIndex] = parameterIndex == 2 ? 0.3f
                                        : parameterIndex == 3 ? 0.0f
                                        : (parameterIndex == 4 || parameterIndex == 5) ? 0.3f
                                        : (parameterIndex == 6 || parameterIndex == 7) ? 0.0f
                                        : 0.5f;

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
        parameters[2] = 0.52f;
        parameters[3] = 0.46f;
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

int getPresetLane(const factorypresetbank::PresetSeed& seed)
{
    return juce::jlimit(0, 4, seed.info.index % 5);
}
}

void applyPreset(juce::AudioProcessorValueTreeState& apvts, const factorypresetbank::PresetSeed& seed)
{
    using PackStyle = factorypresetbank::PackStyle;
    using SoundClass = factorypresetbank::SoundClass;

    const auto style = seed.packStyle;
    const float bright = seed.brightness;
    const float movement = seed.movement;
    const float weight = seed.weight;
    const float width = seed.width;
    const float edge = seed.edge;
    const float air = seed.air;
    const float contour = seed.contour;
    const float special = seed.special;
    const int lane = getPresetLane(seed);

    applyInitPreset(apvts);
    resetLegacyFXParams(apvts);
    clearFXChain(apvts);

    switch (seed.soundClass)
    {
        case SoundClass::lead:
        {
            const auto subCategory = seed.info.subCategory;
            const bool futureRaveLike = subCategory == "Future Rave";
            const bool tranceLike = subCategory == "Trance";
            const bool melodicLike = subCategory == "Melodic Techno";
            const bool basslineLike = subCategory == "Bassline";
            const bool rollerLike = subCategory == "Roller";
            const bool breaksLike = subCategory == "Breaks";
            const bool digitalLike = subCategory == "Digital";
            const bool mono = futureRaveLike || melodicLike || basslineLike || rollerLike || digitalLike;
            const int filterMode = futureRaveLike ? selectChoice(std::array<int, 4>{ filterBand24, filterLadderDrive, filterPeak, filterResonator }, special)
                                   : tranceLike ? selectChoice(std::array<int, 4>{ filterLow24, filterPeak, filterVowelA, filterResonator }, special)
                                   : basslineLike ? selectChoice(std::array<int, 4>{ filterBand24, filterPeak, filterLadderDrive, filterMetalComb }, special)
                                   : rollerLike ? selectChoice(std::array<int, 4>{ filterBand24, filterPeak, filterResonator, filterPhaseWarp }, special)
                                   : digitalLike ? selectChoice(std::array<int, 4>{ filterPhaseWarp, filterBand24, filterResonator, filterMetalComb }, special)
                                   : breaksLike ? selectChoice(std::array<int, 4>{ filterBand24, filterPeak, filterHigh12, filterPhaseWarp }, special)
                                   : style == PackStyle::solar ? selectChoice(std::array<int, 4>{ filterLow24, filterPeak, filterLadderDrive, filterResonator }, special)
                                   : style == PackStyle::ground ? selectChoice(std::array<int, 4>{ filterLow24, filterTilt, filterPeak, filterLadderDrive }, special)
                                   : style == PackStyle::halo ? selectChoice(std::array<int, 4>{ filterPeak, filterFormant, filterVowelA, filterPhaseWarp }, special)
                                   : selectChoice(std::array<int, 4>{ filterBand24, filterResonator, filterMetalComb, filterPhaseWarp }, special);
            setVoiceSection(apvts, mono, true, mono || movement > 0.55f, mono ? scaleNormalised(movement, 0.03f, futureRaveLike ? 0.20f : 0.14f) : scaleNormalised(movement, 0.01f, tranceLike ? 0.06f : 0.08f), mono ? 7.0f : 12.0f, mono ? 2.0f : 7.0f);
            setOscillator(apvts, 1, basslineLike ? 2 : digitalLike ? 2 : style == PackStyle::ground ? (edge > 0.55f ? 2 : 1) : style == PackStyle::halo ? 3 : 1, 2, scaleNormalised(weight, 0.64f, 0.82f), mono ? scaleNormalised(edge, basslineLike ? -2.0f : -4.0f, basslineLike ? 0.0f : -1.0f) : scaleNormalised(edge, tranceLike ? -6.0f : -5.5f, tranceLike ? -2.4f : -2.0f), true, true, mono ? (width > 0.58f ? 5 : 3) : (width > 0.70f ? 7 : 5), mono ? scaleNormalised(width, basslineLike ? 0.08f : 0.12f, basslineLike ? 0.22f : 0.32f) : scaleNormalised(width, 0.28f, tranceLike ? 0.82f : 0.72f), digitalLike ? scaleNormalised(movement, 0.14f, 0.34f) : style == PackStyle::shattered ? scaleNormalised(movement, 0.10f, 0.34f) : scaleNormalised(movement, 0.00f, tranceLike ? 0.10f : 0.18f), digitalLike ? scaleNormalised(special, 0.16f, 0.56f) : style == PackStyle::shattered ? scaleNormalised(special, 0.10f, 0.52f) : scaleNormalised(special, 0.02f, futureRaveLike ? 0.30f : 0.22f), scaleNormalised(special, 0.00f, futureRaveLike ? 0.16f : 0.10f));
            setOscillator(apvts, 2, digitalLike ? 2 : basslineLike ? 1 : style == PackStyle::shattered ? 2 : bright > 0.58f ? 1 : 3, style == PackStyle::solar ? 2 : 3, scaleNormalised(width, 0.22f, basslineLike ? 0.36f : 0.50f), mono ? scaleNormalised(edge, 1.0f, basslineLike ? 3.2f : 4.0f) : scaleNormalised(edge, tranceLike ? 1.4f : 2.0f, tranceLike ? 4.4f : 6.0f), true, ! mono || width > 0.62f, mono ? 3 : (width > 0.72f ? 7 : 5), mono ? scaleNormalised(width, 0.10f, basslineLike ? 0.20f : 0.24f) : scaleNormalised(width, 0.20f, 0.50f), scaleNormalised(movement, 0.00f, breaksLike ? 0.24f : 0.18f), digitalLike ? scaleNormalised(special, 0.12f, 0.38f) : style == PackStyle::shattered ? scaleNormalised(special, 0.10f, 0.34f) : scaleNormalised(special, 0.00f, tranceLike ? 0.08f : 0.12f), scaleNormalised(air, 0.00f, tranceLike ? 0.12f : 0.08f));
            setOscillator(apvts, 3, style == PackStyle::halo ? 3 : style == PackStyle::ground ? 0 : 2, style == PackStyle::ground ? 1 : 2, scaleNormalised(air, 0.10f, basslineLike ? 0.18f : 0.26f), scaleNormalised(special, -2.0f, 2.0f), basslineLike || style == PackStyle::ground, false, 3, 0.10f, style == PackStyle::ground ? scaleNormalised(special, 0.00f, 0.08f) : 0.0f, tranceLike || style == PackStyle::halo ? scaleNormalised(air, 0.00f, 0.12f) : 0.0f, scaleNormalised(special, 0.00f, 0.08f));
            setOscillatorFMSource(apvts, 1, digitalLike ? 6 : style == PackStyle::shattered ? 6 : 1);
            setOscillatorFMSource(apvts, 2, tranceLike ? 5 : style == PackStyle::halo ? 5 : 0);
            setOscillatorFMSource(apvts, 3, basslineLike ? 1 : style == PackStyle::ground ? 4 : 2);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, mono ? (basslineLike ? 900.0f : 1800.0f) : 2600.0f, mono ? (basslineLike ? 4200.0f : 7600.0f) : tranceLike ? 14000.0f : 13000.0f), scaleNormalised(edge, 0.85f, mono ? (futureRaveLike || basslineLike ? 8.2f : 6.8f) : tranceLike ? 4.0f : 4.8f), scaleNormalised(edge, mono ? 0.14f : 0.12f, futureRaveLike || basslineLike ? 0.64f : 0.56f), scaleNormalised(contour, tranceLike ? 0.18f : 0.28f, breaksLike ? 0.88f : 0.78f), scaleNormalised(contour, 0.001f, tranceLike ? 0.020f : 0.040f), scaleNormalised(contour, 0.09f, tranceLike ? 0.36f : 0.46f), mono ? scaleNormalised(weight, basslineLike ? 0.06f : 0.10f, basslineLike ? 0.32f : 0.45f) : scaleNormalised(weight, 0.18f, tranceLike ? 0.46f : 0.52f), scaleNormalised(air, 0.08f, tranceLike ? 0.42f : 0.34f));
            setAmpSection(apvts, scaleNormalised(contour, 0.001f, mono ? 0.020f : 0.040f), scaleNormalised(contour, 0.10f, mono ? 0.36f : 0.48f), mono ? scaleNormalised(weight, 0.44f, 0.78f) : scaleNormalised(weight, 0.58f, 0.88f), scaleNormalised(air, 0.08f, mono ? 0.24f : 0.42f));
            setModSection(apvts, scaleNormalised(movement, 0.20f, basslineLike ? 4.2f : 7.5f), scaleNormalised(movement * special, digitalLike ? 0.08f : 0.02f, digitalLike ? 0.34f : breaksLike ? 0.30f : 0.26f), scaleNormalised(bright, 0.16f, basslineLike ? 0.40f : 0.70f), scaleNormalised(air, tranceLike ? 0.26f : 0.18f, tranceLike ? 0.90f : 0.82f));
            setWarpSection(apvts, scaleNormalised(edge, 0.06f, 0.36f), style == PackStyle::halo && air > 0.68f, scaleNormalised(special, 0.08f, 0.58f));
            setNoiseSection(apvts, scaleNormalised(style == PackStyle::halo ? air : special, 0.00f, 0.045f), pickNoiseType(style, special), true, true);
            setParameterPlain(apvts, "masterSpread", mono ? scaleNormalised(width, basslineLike ? 0.06f : 0.12f, basslineLike ? 0.18f : 0.26f) : scaleNormalised(width, tranceLike ? 0.64f : 0.56f, tranceLike ? 0.96f : 0.92f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, 0.68f, 0.86f));
            setFXSlot(apvts, 0, fxDrive, futureRaveLike ? 2 : basslineLike ? 4 : style == PackStyle::ground ? 2 : style == PackStyle::shattered ? 3 : 0, scaleNormalised(edge, 0.28f, basslineLike ? 0.84f : 0.78f), scaleNormalised(bright, 0.34f, basslineLike ? 0.56f : 0.68f), scaleNormalised(edge, mono ? 0.18f : 0.12f, mono ? 0.34f : 0.26f), edge > 0.18f || mono);
            setFXSlot(apvts, 1, breaksLike ? fxPhaser : style == PackStyle::halo ? fxDimension : fxChorus, 0, scaleNormalised(width, 0.20f, 0.52f), scaleNormalised(width, 0.30f, 0.78f), scaleNormalised(width, mono ? 0.08f : 0.14f, mono ? 0.18f : 0.28f), ! mono || style == PackStyle::halo || breaksLike);
            setFXSlot(apvts, 2, tranceLike ? fxDelay : fxDelay, tranceLike ? 1 : style == PackStyle::shattered ? 2 : mono ? 1 : 0, scaleNormalised(movement, 0.18f, 0.46f), scaleNormalised(air, 0.20f, tranceLike ? 0.58f : 0.52f), scaleNormalised(air, basslineLike ? 0.08f : 0.14f, basslineLike ? 0.18f : 0.30f));
            setFXSlot(apvts, 3, fxReverb, tranceLike || style == PackStyle::halo ? 3 : 0, scaleNormalised(air, basslineLike ? 0.12f : 0.22f, basslineLike ? 0.30f : 0.64f), scaleNormalised(air, 0.26f, 0.64f), scaleNormalised(air, mono ? 0.08f : 0.14f, basslineLike ? 0.14f : 0.28f));
            setFXSlot(apvts, 4, fxCompressor, futureRaveLike || basslineLike ? 1 : style == PackStyle::ground ? 1 : 0, scaleNormalised(edge, 0.26f, 0.68f), scaleNormalised(weight, 0.22f, 0.60f), scaleNormalised(edge, 0.18f, 0.34f), mono || edge > 0.42f);

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.5f) * (mono ? 0.82f : 0.90f)));
                    setParameterPlain(apvts, "noiseAmount",
                                      juce::jlimit(0.0f, 0.4f, readParameterPlain(apvts, "noiseAmount", 0.0f) * 0.60f));
                    setParameterPlain(apvts, getFXSlotFloatParamID(3, 2),
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, getFXSlotFloatParamID(3, 2), 0.18f) * 0.72f));
                    break;

                case 1:
                    if (! basslineLike && ! rollerLike)
                        setVoiceSection(apvts, false, true, false, tranceLike ? 0.03f : 0.05f, 12.0f, 7.0f);
                    setBoolParam(apvts, "osc1UnisonOn", true);
                    setBoolParam(apvts, "osc2UnisonOn", true);
                    setParameterPlain(apvts, "osc1UnisonVoices", 7.0f);
                    setParameterPlain(apvts, "osc2UnisonVoices", 7.0f);
                    setParameterPlain(apvts, "osc1Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1Unison", 0.32f) + 0.16f));
                    setParameterPlain(apvts, "osc2Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2Unison", 0.26f) + 0.12f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.6f) + 0.18f));
                    setFXSlot(apvts, 1, style == PackStyle::halo ? fxDimension : fxChorus, 0,
                              scaleNormalised(width, 0.30f, 0.56f),
                              scaleNormalised(width, 0.48f, 0.86f),
                              scaleNormalised(width, 0.16f, 0.28f),
                              true);
                    setFXSlot(apvts, 2, fxDelay, 1,
                              scaleNormalised(movement, 0.18f, 0.34f),
                              scaleNormalised(air, 0.32f, 0.60f),
                              scaleNormalised(air, 0.18f, 0.32f),
                              true);
                    setFXSlot(apvts, 3, fxReverb, tranceLike ? 3 : 0,
                              scaleNormalised(air, 0.36f, 0.82f),
                              scaleNormalised(air, 0.32f, 0.68f),
                              scaleNormalised(air, 0.18f, 0.34f),
                              true);
                    break;

                case 2:
                    setParameterPlain(apvts, "osc1WarpSync",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpSync", 0.0f) + 0.22f));
                    setParameterPlain(apvts, "osc2WarpSync",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2WarpSync", 0.0f) + 0.14f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (breaksLike ? filterHigh12 : futureRaveLike || basslineLike ? filterBand24 : filterPeak));
                    setParameterPlain(apvts, "filterResonance",
                                      juce::jlimit(0.2f, 18.0f, readParameterPlain(apvts, "filterResonance", 1.2f) + 1.5f));
                    setParameterPlain(apvts, "filterEnvAmount",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterEnvAmount", 0.4f) + 0.14f));
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.18f) * 0.82f));
                    if (breaksLike)
                        setFXSlot(apvts, 1, fxPhaser, 0, 0.34f, 0.72f, 0.22f, true);
                    break;

                case 3:
                    setVoiceSection(apvts, true, true, true, scaleNormalised(movement, 0.05f, 0.18f), 7.0f, 2.0f);
                    setParameterPlain(apvts, "osc3Level",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc3Level", 0.14f) + 0.12f));
                    setParameterPlain(apvts, "filterDrive",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterDrive", 0.20f) + 0.18f));
                    setParameterPlain(apvts, "warpSaturator",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpSaturator", 0.10f) + 0.16f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.18f) * 0.70f));
                    setFXSlot(apvts, 0, fxDrive, futureRaveLike ? 1 : 2, 0.74f, 0.42f, 0.30f, true);
                    setFXSlot(apvts, 4, fxCompressor, 1, 0.62f, 0.54f, 0.28f, true);
                    break;

                case 4:
                    setParameterPlain(apvts, "osc1Wave", (float) (digitalLike || rollerLike ? 2 : 3));
                    setParameterPlain(apvts, "osc2Wave", 2.0f);
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.10f) + 0.22f));
                    setParameterPlain(apvts, "osc2WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2WarpFM", 0.08f) + 0.18f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.12f) + 0.24f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (digitalLike || rollerLike ? filterPhaseWarp : filterMetalComb));
                    setFXSlot(apvts, 1, fxFlanger, 0,
                              scaleNormalised(movement, 0.18f, 0.54f),
                              scaleNormalised(width, 0.34f, 0.76f),
                              0.18f,
                              true);
                    setFXSlot(apvts, 2, fxDelay, 3,
                              scaleNormalised(movement, 0.24f, 0.46f),
                              scaleNormalised(air, 0.28f, 0.56f),
                              scaleNormalised(air, 0.14f, 0.28f),
                              true);
                    break;
            }
            break;
        }

        case SoundClass::pluck:
        {
            const auto subCategory = seed.info.subCategory;
            const bool gateLike = subCategory == "Gate";
            const bool progressiveLike = subCategory == "Progressive";
            const bool afroLike = subCategory == "Afro";
            const int filterMode = gateLike ? selectChoice(std::array<int, 4>{ filterBand12, filterPeak, filterLow24, filterResonator }, special)
                                   : progressiveLike ? selectChoice(std::array<int, 4>{ filterTilt, filterLow24, filterPeak, filterResonator }, special)
                                   : afroLike ? selectChoice(std::array<int, 4>{ filterResonator, filterPeak, filterFormant, filterBand12 }, special)
                                   : style == PackStyle::solar ? selectChoice(std::array<int, 4>{ filterPeak, filterLow24, filterResonator, filterBand12 }, special)
                                   : style == PackStyle::shattered ? selectChoice(std::array<int, 4>{ filterBand24, filterMetalComb, filterPhaseWarp, filterNotch }, special)
                                   : selectChoice(std::array<int, 4>{ filterLow24, filterTilt, filterPeak, filterResonator }, special);
            setVoiceSection(apvts, false, true, false, 0.04f, 12.0f, 7.0f);
            setOscillator(apvts, 1, 1, 2, scaleNormalised(weight, 0.58f, 0.76f), scaleNormalised(edge, -4.0f, -1.0f), true, true, width > 0.62f ? 7 : 5, scaleNormalised(width, 0.18f, 0.54f), scaleNormalised(movement, 0.00f, 0.14f), scaleNormalised(special, 0.02f, 0.18f), scaleNormalised(special, 0.02f, 0.12f));
            setOscillator(apvts, 2, style == PackStyle::shattered ? 2 : 3, 3, scaleNormalised(air, 0.16f, 0.34f), scaleNormalised(edge, 1.0f, 5.0f), true, width > 0.55f, width > 0.70f ? 5 : 3, scaleNormalised(width, 0.10f, 0.32f), scaleNormalised(special, 0.00f, 0.18f), scaleNormalised(special, 0.00f, 0.22f), scaleNormalised(special, 0.00f, 0.10f));
            setOscillator(apvts, 3, style == PackStyle::solar ? 2 : 0, 1, scaleNormalised(weight, 0.06f, 0.20f), scaleNormalised(special, -2.0f, 2.0f), style == PackStyle::shattered, false, 3, 0.10f, 0.0f, 0.0f, scaleNormalised(special, 0.00f, 0.08f));
            setOscillatorFMSource(apvts, 1, style == PackStyle::shattered ? 6 : 1);
            setOscillatorFMSource(apvts, 2, style == PackStyle::solar ? 5 : 3);
            setOscillatorFMSource(apvts, 3, 4);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, afroLike ? 700.0f : 900.0f, gateLike ? 6400.0f : 5600.0f), scaleNormalised(edge, 1.1f, gateLike ? 8.0f : 7.5f), scaleNormalised(edge, 0.10f, afroLike ? 0.36f : 0.46f), scaleNormalised(contour, progressiveLike ? 0.46f : 0.62f, gateLike ? 1.00f : 0.98f), scaleNormalised(contour, 0.001f, gateLike ? 0.014f : 0.018f), scaleNormalised(contour, progressiveLike ? 0.14f : 0.10f, progressiveLike ? 0.44f : 0.34f), scaleNormalised(weight, 0.00f, progressiveLike ? 0.28f : 0.20f), scaleNormalised(air, 0.05f, afroLike ? 0.26f : 0.20f));
            setAmpSection(apvts, scaleNormalised(contour, 0.001f, 0.012f), scaleNormalised(contour, 0.08f, 0.28f), scaleNormalised(weight, 0.04f, 0.24f), scaleNormalised(air, 0.05f, 0.18f));
            setModSection(apvts, scaleNormalised(movement, progressiveLike ? 0.22f : 0.35f, gateLike ? 8.8f : progressiveLike ? 5.4f : 8.0f), scaleNormalised(movement, 0.00f, gateLike ? 0.24f : afroLike ? 0.10f : 0.18f), scaleNormalised(bright, 0.10f, progressiveLike ? 0.42f : 0.54f), scaleNormalised(air, 0.04f, afroLike ? 0.18f : 0.24f));
            setWarpSection(apvts, scaleNormalised(edge, 0.00f, 0.24f), false, scaleNormalised(special, 0.04f, 0.42f));
            setNoiseSection(apvts, scaleNormalised(special, 0.00f, 0.035f), pickNoiseType(style, special), true, true);
            setParameterPlain(apvts, "masterSpread", scaleNormalised(width, afroLike ? 0.36f : 0.44f, progressiveLike ? 0.88f : 0.82f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, 0.62f, 0.82f));
            setFXSlot(apvts, 0, fxDrive, style == PackStyle::shattered ? 3 : 0, scaleNormalised(edge, 0.20f, 0.62f), scaleNormalised(bright, 0.34f, 0.72f), scaleNormalised(edge, 0.06f, 0.18f), edge > 0.26f);
            setFXSlot(apvts, 1, fxDimension, 0, scaleNormalised(width, 0.22f, 0.54f), scaleNormalised(width, 0.38f, 0.84f), scaleNormalised(width, 0.12f, 0.28f));
            setFXSlot(apvts, 2, fxDelay, gateLike ? 1 : progressiveLike ? 2 : style == PackStyle::shattered ? 2 : 0, scaleNormalised(movement, 0.14f, 0.42f), scaleNormalised(air, 0.18f, afroLike ? 0.54f : 0.46f), scaleNormalised(air, progressiveLike ? 0.12f : 0.18f, progressiveLike ? 0.28f : 0.36f));
            setFXSlot(apvts, 3, fxReverb, gateLike ? 3 : style == PackStyle::solar ? 1 : 0, scaleNormalised(air, 0.18f, progressiveLike ? 0.62f : 0.52f), scaleNormalised(air, 0.20f, 0.58f), scaleNormalised(air, afroLike ? 0.10f : 0.14f, afroLike ? 0.22f : 0.26f));

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "ampDecay",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampDecay", 0.18f) * 0.75f));
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.10f) * 0.70f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.5f) * 0.84f));
                    setParameterPlain(apvts, getFXSlotFloatParamID(3, 2),
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, getFXSlotFloatParamID(3, 2), 0.18f) * 0.68f));
                    break;

                case 1:
                    setBoolParam(apvts, "osc1UnisonOn", true);
                    setBoolParam(apvts, "osc2UnisonOn", true);
                    setParameterPlain(apvts, "osc1UnisonVoices", 7.0f);
                    setParameterPlain(apvts, "osc1Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1Unison", 0.30f) + 0.14f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.56f) + 0.18f));
                    setFXSlot(apvts, 1, fxDimension, 0, 0.42f, 0.88f, 0.26f, true);
                    setFXSlot(apvts, 2, fxDelay, 1, 0.26f, 0.42f, 0.24f, true);
                    setFXSlot(apvts, 3, fxReverb, progressiveLike ? 3 : 1, 0.58f, 0.44f, 0.22f, true);
                    break;

                case 2:
                    setParameterPlain(apvts, "filterMode", (float) (afroLike ? filterResonator : filterBand12));
                    setParameterPlain(apvts, "filterResonance",
                                      juce::jlimit(0.2f, 18.0f, readParameterPlain(apvts, "filterResonance", 2.0f) + 1.8f));
                    setParameterPlain(apvts, "filterEnvAmount",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterEnvAmount", 0.60f) + 0.12f));
                    setParameterPlain(apvts, "lfoRate",
                                      juce::jlimit(0.05f, 20.0f, readParameterPlain(apvts, "lfoRate", 2.0f) * 1.35f));
                    setFXSlot(apvts, 2, fxDelay, gateLike ? 1 : 0, 0.18f, 0.30f, 0.18f, true);
                    break;

                case 3:
                    setParameterPlain(apvts, "osc2Wave", 2.0f);
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.02f) + 0.16f));
                    setParameterPlain(apvts, "osc2WarpSync",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2WarpSync", 0.02f) + 0.14f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.08f) + 0.18f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (style == PackStyle::shattered ? filterPhaseWarp : filterMetalComb));
                    setFXSlot(apvts, 1, fxFlanger, 0, 0.24f, 0.56f, 0.18f, true);
                    break;

                case 4:
                    setParameterPlain(apvts, "osc1Wave", afroLike ? 3.0f : 1.0f);
                    setParameterPlain(apvts, "osc2Wave", afroLike ? 3.0f : 0.0f);
                    setParameterPlain(apvts, "filterCutoff",
                                      juce::jlimit(20.0f, 20000.0f, readParameterPlain(apvts, "filterCutoff", 2200.0f) * 0.86f));
                    setParameterPlain(apvts, "ampSustain",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "ampSustain", 0.12f) + 0.08f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.5f) * (afroLike ? 0.82f : 0.90f)));
                    setFXSlot(apvts, 2, fxDelay, progressiveLike ? 2 : 0, 0.18f, 0.28f, afroLike ? 0.12f : 0.18f, true);
                    break;
            }
            break;
        }

        case SoundClass::chord:
        {
            const auto subCategory = seed.info.subCategory;
            const bool brassLike = seed.info.category == "Brass";
            const bool techHouseLike = subCategory == "Tech House";
            const bool peakTechnoLike = subCategory == "Peak Techno";
            const bool afroLike = subCategory == "Afro";
            const bool ukgLike = subCategory == "UK Garage";
            const bool speedGarageLike = subCategory == "Speed Garage";
            const int filterMode = brassLike ? selectChoice(std::array<int, 4>{ filterPeak, filterLadderDrive, filterResonator, filterBand24 }, special)
                                             : techHouseLike ? selectChoice(std::array<int, 4>{ filterBand12, filterPeak, filterTilt, filterBand24 }, special)
                                             : peakTechnoLike ? selectChoice(std::array<int, 4>{ filterBand24, filterResonator, filterPhaseWarp, filterMetalComb }, special)
                                             : afroLike ? selectChoice(std::array<int, 4>{ filterResonator, filterTilt, filterFormant, filterPeak }, special)
                                             : ukgLike || speedGarageLike ? selectChoice(std::array<int, 4>{ filterBand12, filterPeak, filterTilt, filterResonator }, special)
                                                                         : selectChoice(std::array<int, 4>{ filterPeak, filterTilt, filterLow24, filterResonator }, special);
            setVoiceSection(apvts, false, true, false, 0.05f, 12.0f, 7.0f);
            setOscillator(apvts, 1, brassLike ? 2 : 1, 2, scaleNormalised(weight, 0.56f, 0.74f), scaleNormalised(edge, -3.0f, -1.0f), true, true, width > 0.68f ? 7 : 5, scaleNormalised(width, 0.16f, 0.46f), scaleNormalised(movement, 0.00f, 0.12f), scaleNormalised(special, 0.00f, brassLike ? 0.20f : 0.10f), scaleNormalised(special, 0.00f, 0.08f));
            setOscillator(apvts, 2, brassLike ? 1 : 3, brassLike ? 3 : 2, scaleNormalised(air, 0.18f, 0.40f), scaleNormalised(edge, 1.0f, 5.0f), true, width > 0.58f, width > 0.72f ? 5 : 3, scaleNormalised(width, 0.10f, 0.30f), scaleNormalised(special, 0.00f, 0.14f), 0.0f, scaleNormalised(special, 0.00f, 0.06f));
            setOscillator(apvts, 3, brassLike ? 3 : 0, 1, scaleNormalised(weight, 0.08f, 0.22f), scaleNormalised(special, -2.0f, 2.0f), style == PackStyle::ground, false, 3, 0.10f, 0.0f, 0.0f, 0.0f);
            setOscillatorFMSource(apvts, 1, brassLike ? 1 : 0);
            setOscillatorFMSource(apvts, 2, style == PackStyle::solar ? 5 : 4);
            setOscillatorFMSource(apvts, 3, 2);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, brassLike ? 900.0f : afroLike ? 900.0f : 1200.0f, brassLike ? 5200.0f : techHouseLike || ukgLike || speedGarageLike ? 4800.0f : 7600.0f), scaleNormalised(edge, brassLike ? 1.2f : 0.9f, peakTechnoLike ? 7.8f : brassLike ? 7.2f : 4.8f), scaleNormalised(edge, 0.08f, peakTechnoLike ? 0.40f : brassLike ? 0.52f : 0.32f), scaleNormalised(contour, brassLike ? 0.28f : techHouseLike || ukgLike || speedGarageLike ? 0.44f : 0.18f, techHouseLike || ukgLike || speedGarageLike ? 0.90f : brassLike ? 0.72f : 0.44f), scaleNormalised(contour, 0.002f, techHouseLike || ukgLike || speedGarageLike ? 0.016f : brassLike ? 0.020f : 0.040f), scaleNormalised(contour, techHouseLike || ukgLike || speedGarageLike ? 0.08f : 0.16f, techHouseLike || ukgLike || speedGarageLike ? 0.28f : brassLike ? 0.54f : 0.72f), scaleNormalised(weight, techHouseLike || ukgLike || speedGarageLike ? 0.00f : brassLike ? 0.16f : 0.28f, techHouseLike || ukgLike || speedGarageLike ? 0.16f : brassLike ? 0.54f : 0.72f), scaleNormalised(air, 0.10f, afroLike ? 0.62f : 0.54f));
            setAmpSection(apvts, scaleNormalised(contour, 0.002f, techHouseLike || ukgLike || speedGarageLike ? 0.016f : brassLike ? 0.020f : 0.060f), scaleNormalised(contour, techHouseLike || ukgLike || speedGarageLike ? 0.10f : 0.18f, techHouseLike || ukgLike || speedGarageLike ? 0.28f : brassLike ? 0.42f : 0.78f), scaleNormalised(weight, techHouseLike || ukgLike || speedGarageLike ? 0.00f : 0.40f, techHouseLike || ukgLike || speedGarageLike ? 0.18f : brassLike ? 0.76f : 0.88f), scaleNormalised(air, techHouseLike ? 0.06f : 0.16f, techHouseLike ? 0.18f : brassLike ? 0.38f : 0.68f));
            setModSection(apvts, scaleNormalised(movement, 0.18f, 5.2f), scaleNormalised(movement, 0.00f, brassLike ? 0.14f : 0.10f), scaleNormalised(bright, 0.10f, 0.46f), scaleNormalised(air, 0.04f, 0.24f));
            setWarpSection(apvts, scaleNormalised(edge, 0.00f, brassLike ? 0.34f : 0.20f), false, scaleNormalised(special, 0.02f, brassLike ? 0.32f : 0.22f));
            setNoiseSection(apvts, scaleNormalised(special, 0.00f, 0.030f), pickNoiseType(style, special), true, true);
            setParameterPlain(apvts, "masterSpread", scaleNormalised(width, techHouseLike || ukgLike || speedGarageLike ? 0.22f : brassLike ? 0.34f : 0.44f, techHouseLike || ukgLike || speedGarageLike ? 0.56f : brassLike ? 0.66f : 0.82f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, 0.62f, 0.84f));
            setFXSlot(apvts, 0, fxDrive, brassLike ? 2 : 0, scaleNormalised(edge, 0.18f, 0.60f), scaleNormalised(bright, 0.28f, 0.64f), scaleNormalised(edge, 0.08f, brassLike ? 0.22f : 0.16f), brassLike || edge > 0.26f);
            setFXSlot(apvts, 1, techHouseLike || ukgLike || speedGarageLike ? fxDelay : fxChorus, techHouseLike || ukgLike || speedGarageLike ? 1 : 0, scaleNormalised(width, 0.16f, 0.42f), scaleNormalised(width, 0.22f, 0.58f), scaleNormalised(width, 0.12f, 0.24f), ! brassLike || techHouseLike || ukgLike || speedGarageLike);
            setFXSlot(apvts, 2, fxDimension, 0, scaleNormalised(width, 0.24f, 0.54f), scaleNormalised(width, 0.38f, 0.82f), scaleNormalised(width, 0.14f, 0.28f));
            setFXSlot(apvts, 3, fxReverb, peakTechnoLike ? 2 : brassLike ? 1 : 0, scaleNormalised(air, techHouseLike || ukgLike || speedGarageLike ? 0.10f : 0.18f, techHouseLike || ukgLike || speedGarageLike ? 0.26f : afroLike ? 0.64f : 0.56f), scaleNormalised(air, 0.22f, 0.58f), scaleNormalised(air, techHouseLike || ukgLike || speedGarageLike ? 0.06f : 0.10f, techHouseLike || ukgLike || speedGarageLike ? 0.14f : brassLike ? 0.18f : 0.26f));
            setFXSlot(apvts, 4, techHouseLike ? fxCompressor : fxDelay, techHouseLike ? 0 : 0, scaleNormalised(movement, 0.16f, 0.36f), scaleNormalised(air, 0.16f, 0.40f), scaleNormalised(air, brassLike ? 0.06f : 0.10f, brassLike ? 0.16f : 0.22f), techHouseLike || ! brassLike || air > 0.56f);

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "ampDecay",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampDecay", 0.20f) * 0.72f));
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.16f) * 0.70f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.48f) * 0.82f));
                    setParameterPlain(apvts, getFXSlotFloatParamID(3, 2),
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, getFXSlotFloatParamID(3, 2), 0.14f) * 0.62f));
                    break;

                case 1:
                    setParameterPlain(apvts, "osc1Wave", (float) (ukgLike || speedGarageLike ? 2 : 1));
                    setParameterPlain(apvts, "osc2Wave", (float) (ukgLike || speedGarageLike ? 3 : 3));
                    setBoolParam(apvts, "osc1UnisonOn", true);
                    setParameterPlain(apvts, "osc1UnisonVoices", 7.0f);
                    setParameterPlain(apvts, "osc1Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1Unison", 0.28f) + 0.12f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.54f) + 0.14f));
                    setFXSlot(apvts, 1, ukgLike || speedGarageLike ? fxDelay : fxChorus,
                              ukgLike || speedGarageLike ? 1 : 0,
                              0.22f, 0.48f, 0.18f, true);
                    setFXSlot(apvts, 2, fxDimension, 0, 0.42f, 0.82f, 0.22f, true);
                    break;

                case 2:
                    setParameterPlain(apvts, "filterMode",
                                      (float) (peakTechnoLike ? filterBand24 : brassLike ? filterPeak : filterBand12));
                    setParameterPlain(apvts, "filterResonance",
                                      juce::jlimit(0.2f, 18.0f, readParameterPlain(apvts, "filterResonance", 1.5f) + 1.2f));
                    setParameterPlain(apvts, "filterDrive",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterDrive", 0.12f) + 0.12f));
                    setParameterPlain(apvts, "ampAttack",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampAttack", 0.01f) * 0.70f));
                    setFXSlot(apvts, 0, fxDrive, brassLike ? 2 : 1, 0.56f, 0.40f, 0.20f, true);
                    break;

                case 3:
                    setParameterPlain(apvts, "ampSustain",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "ampSustain", 0.30f) + 0.18f));
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.22f) * 1.45f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.58f) + 0.20f));
                    setFXSlot(apvts, 2, fxDimension, 0, 0.44f, 0.88f, 0.26f, true);
                    setFXSlot(apvts, 3, fxReverb, afroLike ? 0 : 3, 0.62f, 0.46f, 0.24f, true);
                    break;

                case 4:
                    setParameterPlain(apvts, "osc2Wave", 2.0f);
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.02f) + 0.14f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.04f) + 0.18f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (peakTechnoLike ? filterPhaseWarp : filterMetalComb));
                    setFXSlot(apvts, 1, fxFlanger, 0, 0.18f, 0.52f, 0.16f, true);
                    setFXSlot(apvts, 4, fxDelay, 3, 0.26f, 0.34f, 0.16f, true);
                    break;
            }
            break;
        }

        case SoundClass::bass:
        {
            const auto subCategory = seed.info.subCategory;
            const bool melodicLike = subCategory == "Melodic Techno";
            const bool techHouseLike = subCategory == "Tech House";
            const bool acidLike = subCategory == "Acid";
            const bool peakTechnoLike = subCategory == "Peak Techno";
            const bool ukgLike = subCategory == "UK Garage";
            const bool reeseLike = subCategory == "Reese";
            const bool neuroLike = subCategory == "Neuro";
            const bool driven = techHouseLike || acidLike || peakTechnoLike || neuroLike;
            const int filterMode = acidLike ? selectChoice(std::array<int, 4>{ filterLadderDrive, filterLow24, filterPeak, filterResonator }, special)
                              : peakTechnoLike ? selectChoice(std::array<int, 4>{ filterLadderDrive, filterMetalComb, filterBand24, filterResonator }, special)
                              : reeseLike ? selectChoice(std::array<int, 4>{ filterLow24, filterPeak, filterBand24, filterNotch }, special)
                              : neuroLike ? selectChoice(std::array<int, 4>{ filterPhaseWarp, filterMetalComb, filterBand24, filterCombPlus }, special)
                              : driven ? selectChoice(std::array<int, 4>{ filterLadderDrive, filterLow24, filterCombPlus, filterMetalComb }, special)
                                       : selectChoice(std::array<int, 4>{ filterLow24, filterLadderDrive, filterPeak, filterBand24 }, special);
            setVoiceSection(apvts, true, true, true, scaleNormalised(movement, 0.02f, acidLike ? 0.14f : driven ? 0.16f : 0.10f), 7.0f, 2.0f);
            setOscillator(apvts, 1, ukgLike ? 2 : reeseLike ? 1 : neuroLike ? 1 : driven ? 1 : 2, 1, scaleNormalised(weight, 0.72f, 0.90f), scaleNormalised(edge, reeseLike ? -1.0f : -2.0f, reeseLike ? 2.0f : 1.0f), true, (driven || reeseLike) && width > 0.55f, (driven || reeseLike) && width > 0.75f ? 5 : 3, scaleNormalised(width, 0.08f, reeseLike ? 0.30f : driven ? 0.24f : 0.16f), scaleNormalised(special, 0.00f, neuroLike ? 0.28f : driven ? 0.18f : 0.08f), scaleNormalised(special, 0.00f, neuroLike ? 0.18f : driven ? 0.12f : 0.04f), scaleNormalised(special, 0.00f, 0.06f));
            setOscillator(apvts, 2, neuroLike ? 2 : driven ? 2 : reeseLike ? 1 : 0, 0, scaleNormalised(weight, 0.24f, reeseLike ? 0.56f : 0.48f), scaleNormalised(edge, 0.0f, reeseLike ? 5.0f : 4.0f), false, false, 3, 0.10f, style == PackStyle::ground || melodicLike ? scaleNormalised(special, 0.00f, 0.10f) : 0.0f, 0.0f, 0.0f);
            setOscillator(apvts, 3, 0, 1, scaleNormalised(weight, 0.10f, 0.26f), scaleNormalised(special, -1.0f, 1.0f), true, false, 3, 0.08f, 0.0f, 0.0f, 0.0f);
            setOscillatorFMSource(apvts, 1, neuroLike ? 6 : reeseLike ? 1 : driven ? 1 : ukgLike ? 4 : 4);
            setOscillatorFMSource(apvts, 2, 0);
            setOscillatorFMSource(apvts, 3, 2);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, driven || reeseLike ? 140.0f : 80.0f, acidLike ? 2200.0f : reeseLike ? 2800.0f : driven ? 1600.0f : ukgLike ? 1400.0f : 900.0f), scaleNormalised(edge, driven || reeseLike ? 1.0f : 0.9f, peakTechnoLike ? 9.0f : neuroLike ? 8.8f : reeseLike ? 6.8f : driven ? 8.0f : 5.2f), scaleNormalised(edge, driven ? 0.22f : reeseLike ? 0.18f : 0.12f, neuroLike ? 0.78f : driven ? 0.70f : reeseLike ? 0.40f : 0.48f), scaleNormalised(contour, 0.22f, 0.72f), scaleNormalised(contour, 0.001f, acidLike ? 0.014f : 0.020f), scaleNormalised(contour, 0.08f, acidLike ? 0.24f : 0.30f), scaleNormalised(weight, reeseLike ? 0.06f : 0.10f, reeseLike ? 0.34f : 0.44f), scaleNormalised(air, 0.06f, ukgLike ? 0.26f : 0.20f));
            setAmpSection(apvts, scaleNormalised(contour, 0.001f, 0.018f), scaleNormalised(contour, 0.06f, 0.22f), scaleNormalised(weight, 0.52f, 0.82f), scaleNormalised(air, 0.05f, 0.18f));
            setModSection(apvts, scaleNormalised(movement, neuroLike ? 0.18f : 0.12f, neuroLike ? 6.0f : reeseLike ? 4.2f : 3.4f), scaleNormalised(movement, 0.00f, neuroLike ? 0.26f : driven ? 0.18f : reeseLike ? 0.12f : 0.10f), scaleNormalised(bright, 0.04f, ukgLike ? 0.32f : 0.26f), scaleNormalised(air, 0.00f, 0.12f));
            setWarpSection(apvts, scaleNormalised(edge, reeseLike ? 0.08f : driven ? 0.12f : 0.04f, neuroLike ? 0.48f : driven ? 0.42f : 0.24f), false, scaleNormalised(special, reeseLike ? 0.08f : 0.02f, neuroLike ? 0.56f : driven ? 0.30f : 0.18f));
            setNoiseSection(apvts, scaleNormalised(special, 0.00f, neuroLike ? 0.026f : driven ? 0.022f : 0.012f), pickNoiseType(style, special), true, true);
            setParameterPlain(apvts, "masterSpread", scaleNormalised(width, reeseLike ? 0.12f : driven ? 0.10f : 0.06f, reeseLike ? 0.34f : ukgLike ? 0.22f : driven ? 0.24f : 0.14f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, 0.74f, 0.92f));
            setFXSlot(apvts, 0, fxDrive, neuroLike ? 4 : reeseLike ? 2 : techHouseLike ? 1 : driven ? 4 : 2, scaleNormalised(edge, 0.30f, neuroLike ? 0.88f : 0.82f), scaleNormalised(bright, 0.24f, reeseLike ? 0.60f : 0.54f), scaleNormalised(edge, 0.16f, 0.34f));
            setFXSlot(apvts, 1, fxCompressor, style == PackStyle::ground || reeseLike || peakTechnoLike || neuroLike ? 1 : 0, scaleNormalised(edge, 0.30f, 0.72f), scaleNormalised(weight, 0.28f, 0.66f), scaleNormalised(edge, 0.18f, 0.36f));
            setFXSlot(apvts, 2, fxFilter, acidLike ? 0 : neuroLike ? 2 : reeseLike ? 3 : 0, scaleNormalised(bright, 0.20f, 0.48f), scaleNormalised(edge, 0.18f, 0.52f), scaleNormalised(edge, 0.08f, 0.20f), (driven || reeseLike) && special > 0.38f);
            setFXSlot(apvts, 3, fxDimension, 0, scaleNormalised(width, 0.10f, 0.30f), scaleNormalised(width, 0.18f, reeseLike ? 0.56f : 0.42f), scaleNormalised(width, 0.04f, reeseLike ? 0.18f : 0.12f), (driven || reeseLike || ukgLike) && width > 0.48f);

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "osc3Level",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc3Level", 0.16f) + 0.16f));
                    setBoolParam(apvts, "osc3ToFilter", false);
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.12f) * 0.72f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.12f) * 0.70f));
                    break;

                case 1:
                    setVoiceSection(apvts, true, true, true, scaleNormalised(movement, 0.08f, 0.24f), 7.0f, 2.0f);
                    setParameterPlain(apvts, "filterEnvAmount",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterEnvAmount", 0.36f) + 0.12f));
                    setParameterPlain(apvts, "filterDecay",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "filterDecay", 0.18f) * 0.84f));
                    if (ukgLike)
                        setParameterPlain(apvts, "masterSpread",
                                          juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.12f) + 0.06f));
                    break;

                case 2:
                    setParameterPlain(apvts, "filterMode",
                                      (float) (acidLike ? filterLadderDrive : peakTechnoLike ? filterPeak : filterLadderDrive));
                    setParameterPlain(apvts, "filterResonance",
                                      juce::jlimit(0.2f, 18.0f, readParameterPlain(apvts, "filterResonance", 1.8f) + 2.4f));
                    setParameterPlain(apvts, "filterEnvAmount",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterEnvAmount", 0.40f) + 0.16f));
                    setParameterPlain(apvts, "osc1WarpSync",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpSync", 0.00f) + 0.12f));
                    setFXSlot(apvts, 2, fxFilter, acidLike ? 0 : 2, 0.20f, 0.54f, 0.18f, true);
                    break;

                case 3:
                    setParameterPlain(apvts, "osc1Wave", 1.0f);
                    setParameterPlain(apvts, "osc2Wave", 1.0f);
                    setBoolParam(apvts, "osc1UnisonOn", true);
                    setBoolParam(apvts, "osc2UnisonOn", true);
                    setParameterPlain(apvts, "osc1UnisonVoices", 5.0f);
                    setParameterPlain(apvts, "osc2UnisonVoices", 5.0f);
                    setParameterPlain(apvts, "osc1Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1Unison", 0.16f) + 0.12f));
                    setParameterPlain(apvts, "osc2Detune",
                                      juce::jlimit(-24.0f, 24.0f, readParameterPlain(apvts, "osc2Detune", 2.0f) + 1.8f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.14f) + 0.12f));
                    setFXSlot(apvts, 3, fxDimension, 0, 0.20f, reeseLike ? 0.62f : 0.46f, reeseLike ? 0.18f : 0.12f, true);
                    break;

                case 4:
                    setParameterPlain(apvts, "osc2Wave", 2.0f);
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.02f) + 0.24f));
                    setParameterPlain(apvts, "osc2WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2WarpFM", 0.02f) + 0.18f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.08f) + 0.24f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (neuroLike ? filterPhaseWarp : filterMetalComb));
                    setFXSlot(apvts, 1, fxCompressor, 1, 0.66f, 0.60f, 0.28f, true);
                    setFXSlot(apvts, 2, fxFilter, neuroLike ? 4 : 2, 0.18f, 0.58f, 0.22f, true);
                    break;
            }
            break;
        }

        case SoundClass::key:
        case SoundClass::organ:
        case SoundClass::stringer:
        case SoundClass::pad:
        case SoundClass::drone:
        case SoundClass::texture:
        case SoundClass::vocal:
        {
            const bool organLike = seed.soundClass == SoundClass::organ;
            const bool stringLike = seed.soundClass == SoundClass::stringer;
            const bool padLike = seed.soundClass == SoundClass::pad;
            const bool droneLike = seed.soundClass == SoundClass::drone;
            const bool textureLike = seed.soundClass == SoundClass::texture;
            const bool vocalLike = seed.soundClass == SoundClass::vocal;
            const int filterMode = vocalLike ? selectChoice(std::array<int, 5>{ filterFormant, filterVowelA, filterVowelE, filterVowelI, filterVowelO }, special)
                               : organLike ? selectChoice(std::array<int, 4>{ filterTilt, filterPeak, filterBand12, filterFormant }, special)
                               : stringLike ? selectChoice(std::array<int, 4>{ filterResonator, filterLow24, filterPeak, filterFormant }, special)
                               : droneLike ? selectChoice(std::array<int, 4>{ filterLow24, filterResonator, filterPhaseWarp, filterCombMinus }, special)
                               : textureLike ? selectChoice(std::array<int, 4>{ filterPhaseWarp, filterFlangePlus, filterFormant, filterBand24 }, special)
                                             : selectChoice(std::array<int, 4>{ filterLow24, filterPhaseWarp, filterResonator, filterFormant }, special);
            setVoiceSection(apvts, false, true, false, 0.06f, 12.0f, 7.0f);
            setOscillator(apvts, 1, vocalLike ? 3 : organLike ? 2 : stringLike ? 1 : 1, 2, scaleNormalised(weight, 0.42f, 0.72f), scaleNormalised(edge, -3.0f, -1.0f), true, (padLike || textureLike || stringLike) && width > 0.44f, width > 0.70f ? 7 : 5, scaleNormalised(width, 0.16f, 0.48f), scaleNormalised(special, 0.00f, vocalLike ? 0.12f : 0.08f), scaleNormalised(special, 0.00f, textureLike ? 0.18f : 0.08f), scaleNormalised(special, 0.00f, 0.10f));
            setOscillator(apvts, 2, vocalLike ? 2 : organLike ? 3 : 3, 3, scaleNormalised(air, 0.18f, 0.40f), scaleNormalised(edge, 0.0f, 4.0f), width > 0.62f, width > 0.72f, width > 0.72f ? 7 : 5, scaleNormalised(width, 0.18f, 0.46f), scaleNormalised(movement, 0.00f, vocalLike ? 0.08f : 0.12f), scaleNormalised(special, 0.00f, 0.14f), scaleNormalised(special, 0.00f, 0.10f));
            setOscillator(apvts, 3, droneLike ? 0 : organLike ? 2 : 0, droneLike ? 1 : 2, scaleNormalised(weight, 0.10f, 0.28f), scaleNormalised(special, -3.0f, 3.0f), true, false, 3, 0.12f, style == PackStyle::halo ? scaleNormalised(special, 0.00f, 0.08f) : 0.0f, 0.0f, scaleNormalised(special, 0.00f, 0.10f));
            setOscillatorFMSource(apvts, 1, vocalLike ? 5 : 0);
            setOscillatorFMSource(apvts, 2, vocalLike ? 3 : 4);
            setOscillatorFMSource(apvts, 3, textureLike ? 6 : 2);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, droneLike ? 180.0f : 600.0f, droneLike ? 3200.0f : 8800.0f), scaleNormalised(edge, vocalLike ? 1.4f : 0.9f, vocalLike ? 7.0f : 5.8f), scaleNormalised(edge, droneLike ? 0.04f : 0.08f, droneLike ? 0.28f : 0.22f), scaleNormalised(contour, droneLike ? 0.10f : 0.14f, textureLike ? 0.58f : 0.42f), scaleNormalised(contour, organLike ? 0.001f : droneLike ? 0.04f : 0.02f, organLike ? 0.012f : droneLike ? 0.40f : 0.18f), scaleNormalised(contour, organLike ? 0.10f : droneLike ? 0.50f : 0.28f, organLike ? 0.28f : droneLike ? 1.80f : stringLike ? 0.90f : 0.90f), organLike ? scaleNormalised(weight, 0.60f, 0.86f) : droneLike ? scaleNormalised(weight, 0.24f, 0.70f) : scaleNormalised(weight, 0.42f, 0.84f), scaleNormalised(air, droneLike ? 0.80f : 0.40f, droneLike ? 2.80f : stringLike ? 1.80f : 2.40f));
            setAmpSection(apvts, scaleNormalised(contour, organLike ? 0.001f : droneLike ? 0.18f : 0.06f, organLike ? 0.012f : droneLike ? 1.40f : vocalLike ? 0.60f : 1.00f), scaleNormalised(contour, organLike ? 0.10f : droneLike ? 0.60f : 0.22f, organLike ? 0.28f : droneLike ? 2.20f : stringLike ? 1.20f : 1.60f), organLike ? scaleNormalised(weight, 0.72f, 0.92f) : scaleNormalised(weight, 0.52f, 0.94f), scaleNormalised(air, organLike ? 0.16f : droneLike ? 1.20f : 0.80f, organLike ? 0.36f : droneLike ? 4.00f : stringLike ? 1.80f : 3.20f));
            setModSection(apvts, scaleNormalised(movement, organLike ? 0.16f : 0.08f, organLike ? 3.0f : textureLike ? 4.80f : 2.60f), scaleNormalised(movement, organLike ? 0.04f : 0.08f, textureLike ? 0.46f : vocalLike ? 0.24f : 0.18f), scaleNormalised(bright, 0.12f, 0.56f), vocalLike ? scaleNormalised(air, 0.16f, 0.62f) : scaleNormalised(air, 0.00f, 0.20f));
            setWarpSection(apvts, scaleNormalised(edge, 0.00f, textureLike ? 0.20f : 0.12f), air > 0.70f, scaleNormalised(special, droneLike ? 0.10f : 0.06f, textureLike ? 0.42f : 0.26f));
            setNoiseSection(apvts, scaleNormalised(textureLike ? special : air, 0.01f, textureLike ? 0.14f : 0.06f), pickNoiseType(style, special), true, ! textureLike);
            setParameterPlain(apvts, "masterSpread", scaleNormalised(width, organLike ? 0.24f : 0.60f, organLike ? 0.46f : 0.96f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, organLike ? 0.60f : 0.54f, organLike ? 0.82f : 0.78f));
            setFXSlot(apvts, 0, vocalLike ? fxChorus : organLike ? fxChorus : fxDimension, 0, scaleNormalised(width, 0.22f, 0.56f), scaleNormalised(width, 0.40f, 0.88f), scaleNormalised(width, 0.14f, 0.34f));
            setFXSlot(apvts, 1, textureLike ? fxPhaser : organLike ? fxDelay : fxChorus, organLike ? 2 : 0, scaleNormalised(movement, 0.14f, 0.52f), scaleNormalised(movement, 0.28f, 0.72f), scaleNormalised(movement, 0.08f, 0.22f));
            setFXSlot(apvts, 2, fxDelay, textureLike ? 3 : vocalLike ? 1 : 0, scaleNormalised(movement, 0.18f, 0.52f), scaleNormalised(air, 0.20f, 0.58f), scaleNormalised(air, 0.14f, 0.28f));
            setFXSlot(apvts, 3, fxReverb, style == PackStyle::halo ? 3 : 0, scaleNormalised(air, 0.34f, 0.86f), scaleNormalised(air, 0.28f, 0.74f), scaleNormalised(air, 0.14f, 0.42f));
            setFXSlot(apvts, 4, fxDimension, 0, scaleNormalised(width, 0.30f, 0.60f), scaleNormalised(width, 0.48f, 0.92f), scaleNormalised(width, 0.14f, 0.28f), vocalLike || stringLike || padLike);

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.56f) * 0.84f));
                    setParameterPlain(apvts, getFXSlotFloatParamID(3, 2),
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, getFXSlotFloatParamID(3, 2), 0.20f) * 0.70f));
                    break;

                case 1:
                    setBoolParam(apvts, "osc1UnisonOn", padLike || stringLike || vocalLike);
                    setBoolParam(apvts, "osc2UnisonOn", true);
                    setParameterPlain(apvts, "osc2UnisonVoices", 7.0f);
                    setParameterPlain(apvts, "osc2Unison",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc2Unison", 0.22f) + 0.14f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.68f) + 0.16f));
                    setFXSlot(apvts, 3, fxReverb, style == PackStyle::halo ? 3 : 0, 0.72f, 0.54f, 0.30f, true);
                    break;

                case 2:
                    setParameterPlain(apvts, "lfoRate",
                                      juce::jlimit(0.05f, 20.0f, readParameterPlain(apvts, "lfoRate", 1.0f) * 1.50f));
                    setParameterPlain(apvts, "lfoDepth",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "lfoDepth", 0.10f) + 0.10f));
                    setFXSlot(apvts, 1, textureLike ? fxPhaser : fxChorus, 0, 0.26f, 0.64f, 0.22f, true);
                    setFXSlot(apvts, 2, fxDelay, textureLike ? 3 : 1, 0.28f, 0.40f, 0.20f, true);
                    break;

                case 3:
                    setParameterPlain(apvts, "filterCutoff",
                                      juce::jlimit(20.0f, 20000.0f, readParameterPlain(apvts, "filterCutoff", 2400.0f) * 0.74f));
                    setParameterPlain(apvts, "filterDrive",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterDrive", 0.10f) + 0.10f));
                    setParameterPlain(apvts, "warpSaturator",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpSaturator", 0.04f) + 0.08f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.42f) * 0.82f));
                    break;

                case 4:
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.00f) + 0.12f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.06f) + 0.18f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (textureLike ? filterPhaseWarp : vocalLike ? filterFormant : filterResonator));
                    setFXSlot(apvts, 1, fxFlanger, 0, 0.18f, 0.48f, 0.16f, true);
                    setFXSlot(apvts, 2, fxDelay, 3, 0.24f, 0.36f, 0.18f, true);
                    break;
            }
            break;
        }

        default:
        {
            const bool bellLike = seed.soundClass == SoundClass::bell;
            const bool hybridLike = seed.soundClass == SoundClass::hybrid || seed.recipe == factorypresetbank::Recipe::hybridDigitalLead;
            const bool fxLike = seed.soundClass == SoundClass::fx || seed.recipe == factorypresetbank::Recipe::glitchTexture;
            const bool sequenceLike = seed.soundClass == SoundClass::sequence;
            const bool percussionLike = seed.soundClass == SoundClass::percussion;
            const int filterMode = bellLike ? selectChoice(std::array<int, 4>{ filterBand24, filterPeak, filterResonator, filterFormant }, special)
                               : percussionLike ? selectChoice(std::array<int, 4>{ filterMetalComb, filterBand24, filterPeak, filterResonator }, special)
                               : fxLike ? selectChoice(std::array<int, 4>{ filterBand24, filterPhaseWarp, filterNotch, filterFlangeMinus }, special)
                               : hybridLike ? selectChoice(std::array<int, 4>{ filterPhaseWarp, filterResonator, filterMetalComb, filterBand24 }, special)
                                            : selectChoice(std::array<int, 4>{ filterResonator, filterBand24, filterMetalComb, filterPeak }, special);
            setVoiceSection(apvts, sequenceLike || percussionLike, true, sequenceLike || percussionLike, sequenceLike ? scaleNormalised(movement, 0.02f, 0.10f) : percussionLike ? 0.03f : bellLike ? 0.05f : 0.04f, 12.0f, 7.0f);
            setOscillator(apvts, 1, bellLike ? 3 : percussionLike ? 2 : hybridLike ? 1 : 0, sequenceLike ? 2 : bellLike ? 3 : 3, scaleNormalised(weight, bellLike ? 0.24f : 0.34f, bellLike ? 0.48f : 0.62f), scaleNormalised(edge, bellLike ? -3.0f : -2.0f, bellLike ? 0.0f : 2.0f), true, hybridLike && width > 0.55f, width > 0.74f ? 5 : 3, scaleNormalised(width, 0.10f, bellLike ? 0.28f : 0.34f), scaleNormalised(special, percussionLike ? 0.10f : bellLike ? 0.06f : 0.18f, percussionLike ? 0.34f : bellLike ? 0.18f : 0.56f), scaleNormalised(special, 0.00f, bellLike ? 0.10f : hybridLike || sequenceLike ? 0.38f : 0.12f), scaleNormalised(special, 0.00f, bellLike ? 0.06f : percussionLike ? 0.14f : 0.10f));
            setOscillator(apvts, 2, bellLike ? 0 : hybridLike || sequenceLike ? 2 : 3, bellLike ? 4 : 3, scaleNormalised(air, bellLike ? 0.10f : 0.16f, bellLike ? 0.24f : 0.34f), scaleNormalised(edge, bellLike ? -1.0f : 0.0f, bellLike ? 3.0f : 5.0f), hybridLike && width > 0.50f, width > 0.74f, width > 0.74f ? 5 : 3, scaleNormalised(width, 0.10f, bellLike ? 0.24f : 0.28f), scaleNormalised(special, hybridLike ? 0.08f : bellLike ? 0.04f : 0.00f, hybridLike ? 0.26f : bellLike ? 0.12f : 0.12f), scaleNormalised(special, sequenceLike ? 0.10f : bellLike ? 0.00f : 0.00f, sequenceLike ? 0.28f : bellLike ? 0.08f : 0.10f), scaleNormalised(special, 0.00f, bellLike ? 0.04f : 0.10f));
            setOscillator(apvts, 3, percussionLike ? 1 : 0, percussionLike ? 2 : 1, scaleNormalised(weight, 0.06f, 0.20f), scaleNormalised(special, -4.0f, 4.0f), true, false, 3, 0.10f, fxLike ? scaleNormalised(special, 0.00f, 0.10f) : 0.0f, 0.0f, scaleNormalised(special, 0.00f, 0.08f));
            setOscillatorFMSource(apvts, 1, bellLike ? 5 : hybridLike || fxLike ? 6 : 1);
            setOscillatorFMSource(apvts, 2, bellLike ? 4 : fxLike ? 3 : 5);
            setOscillatorFMSource(apvts, 3, 4);
            setFilterSection(apvts, true, filterMode, scaleNormalised(bright, bellLike ? 1200.0f : percussionLike ? 400.0f : 700.0f, bellLike ? 12000.0f : percussionLike ? 6400.0f : fxLike ? 9000.0f : 7600.0f), scaleNormalised(edge, bellLike ? 1.0f : 1.2f, bellLike ? 5.0f : percussionLike ? 8.4f : 7.2f), scaleNormalised(edge, bellLike ? 0.04f : 0.08f, bellLike ? 0.20f : percussionLike ? 0.52f : 0.34f), scaleNormalised(contour, bellLike ? 0.54f : sequenceLike || percussionLike ? 0.40f : 0.18f, bellLike ? 0.98f : sequenceLike || percussionLike ? 0.90f : 0.52f), scaleNormalised(contour, bellLike ? 0.001f : percussionLike ? 0.001f : 0.002f, bellLike ? 0.012f : percussionLike ? 0.012f : 0.030f), scaleNormalised(contour, bellLike ? 0.08f : percussionLike ? 0.06f : 0.20f, bellLike ? 0.24f : percussionLike ? 0.24f : 0.84f), bellLike ? scaleNormalised(weight, 0.00f, 0.16f) : percussionLike ? 0.0f : scaleNormalised(weight, 0.02f, sequenceLike ? 0.30f : 0.18f), scaleNormalised(air, bellLike ? 0.10f : percussionLike ? 0.05f : 0.18f, bellLike ? 0.50f : percussionLike ? 0.20f : 1.20f));
            setAmpSection(apvts, scaleNormalised(contour, bellLike ? 0.001f : percussionLike ? 0.001f : 0.002f, bellLike ? 0.010f : percussionLike ? 0.010f : sequenceLike ? 0.016f : 0.040f), scaleNormalised(contour, bellLike ? 0.06f : percussionLike ? 0.04f : 0.06f, bellLike ? 0.18f : percussionLike ? 0.20f : sequenceLike ? 0.26f : 1.20f), bellLike ? scaleNormalised(weight, 0.00f, 0.12f) : percussionLike ? 0.0f : scaleNormalised(weight, sequenceLike ? 0.04f : 0.00f, sequenceLike ? 0.22f : 0.18f), scaleNormalised(air, bellLike ? 0.08f : percussionLike ? 0.04f : 0.26f, bellLike ? 0.18f : percussionLike ? 0.16f : 1.80f));
            setModSection(apvts, scaleNormalised(movement, bellLike ? 0.40f : sequenceLike ? 0.30f : 0.14f, bellLike ? 10.0f : sequenceLike ? 10.0f : fxLike ? 8.0f : 5.0f), scaleNormalised(movement, bellLike ? 0.02f : percussionLike ? 0.00f : 0.10f, bellLike ? 0.16f : fxLike ? 0.62f : sequenceLike ? 0.26f : 0.18f), scaleNormalised(bright, 0.06f, bellLike ? 0.34f : fxLike ? 0.66f : 0.40f), scaleNormalised(air, 0.00f, bellLike ? 0.12f : 0.22f));
            setWarpSection(apvts, scaleNormalised(edge, bellLike ? 0.00f : 0.04f, hybridLike || percussionLike ? 0.34f : 0.20f), fxLike, scaleNormalised(special, bellLike ? 0.10f : hybridLike || fxLike ? 0.18f : 0.08f, bellLike ? 0.36f : hybridLike || fxLike ? 0.72f : 0.44f));
            setNoiseSection(apvts, scaleNormalised(fxLike ? special : air, bellLike ? 0.00f : percussionLike ? 0.00f : 0.01f, bellLike ? 0.02f : fxLike ? 0.16f : percussionLike ? 0.04f : 0.08f), pickNoiseType(style, special), true, ! fxLike);
            setParameterPlain(apvts, "masterSpread", scaleNormalised(width, bellLike ? 0.24f : percussionLike ? 0.12f : 0.24f, bellLike ? 0.56f : percussionLike ? 0.26f : 0.70f));
            setParameterPlain(apvts, "masterGain", scaleNormalised(weight, 0.58f, percussionLike ? 0.88f : 0.80f));
            setFXSlot(apvts, 0, bellLike ? fxDelay : fxLike ? fxFilter : fxDrive, bellLike ? 1 : fxLike ? quantiseIndex(special, 5) : style == PackStyle::shattered ? 3 : 0, scaleNormalised(edge, 0.20f, bellLike ? 0.42f : 0.70f), scaleNormalised(bright, 0.24f, 0.68f), scaleNormalised(edge, bellLike ? 0.10f : 0.12f, bellLike ? 0.20f : 0.24f), bellLike || ! fxLike || edge > 0.18f);
            setFXSlot(apvts, 1, bellLike ? fxReverb : fxLike ? fxPhaser : hybridLike ? fxFlanger : fxDelay, bellLike ? 1 : 0, scaleNormalised(movement, 0.16f, 0.52f), scaleNormalised(movement, 0.24f, 0.66f), scaleNormalised(movement, bellLike ? 0.12f : 0.10f, bellLike ? 0.24f : sequenceLike ? 0.26f : 0.20f));
            setFXSlot(apvts, 2, bellLike ? fxDimension : sequenceLike ? fxFilter : fxDelay, bellLike ? 0 : sequenceLike ? 0 : fxLike ? 3 : 1, scaleNormalised(movement, 0.18f, 0.52f), scaleNormalised(air, 0.18f, bellLike ? 0.42f : 0.54f), scaleNormalised(air, bellLike ? 0.12f : percussionLike ? 0.04f : 0.12f, bellLike ? 0.22f : percussionLike ? 0.12f : 0.28f));
            setFXSlot(apvts, 3, fxReverb, bellLike ? 3 : fxLike ? 4 : hybridLike ? 3 : 0, scaleNormalised(air, bellLike ? 0.28f : 0.18f, bellLike ? 0.56f : 0.72f), scaleNormalised(air, 0.20f, bellLike ? 0.46f : 0.64f), scaleNormalised(air, bellLike ? 0.12f : percussionLike ? 0.06f : 0.12f, bellLike ? 0.24f : percussionLike ? 0.16f : 0.30f));
            setFXSlot(apvts, 4, fxCompressor, 0, scaleNormalised(edge, 0.20f, 0.64f), scaleNormalised(weight, 0.22f, 0.60f), scaleNormalised(edge, percussionLike ? 0.12f : 0.08f, percussionLike ? 0.28f : 0.20f), sequenceLike || percussionLike);

            switch (lane)
            {
                case 0:
                    setParameterPlain(apvts, "ampRelease",
                                      juce::jlimit(0.001f, 5.0f, readParameterPlain(apvts, "ampRelease", 0.14f) * 0.76f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.30f) * 0.84f));
                    break;

                case 1:
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.38f) + 0.18f));
                    setFXSlot(apvts, 2, bellLike ? fxDimension : sequenceLike ? fxFilter : fxDelay,
                              bellLike ? 0 : sequenceLike ? 0 : 1,
                              0.28f, 0.44f, bellLike ? 0.18f : 0.24f, true);
                    setFXSlot(apvts, 3, fxReverb, bellLike ? 3 : hybridLike ? 3 : 0, 0.62f, 0.42f, 0.24f, true);
                    break;

                case 2:
                    setParameterPlain(apvts, "lfoRate",
                                      juce::jlimit(0.05f, 20.0f, readParameterPlain(apvts, "lfoRate", 2.0f) * 1.45f));
                    setParameterPlain(apvts, "lfoDepth",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "lfoDepth", 0.04f) + 0.10f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (sequenceLike ? filterBand24 : bellLike ? filterPeak : filterResonator));
                    break;

                case 3:
                    setParameterPlain(apvts, "filterDrive",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "filterDrive", 0.12f) + 0.14f));
                    setParameterPlain(apvts, "warpSaturator",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpSaturator", 0.06f) + 0.12f));
                    setParameterPlain(apvts, "masterSpread",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "masterSpread", 0.24f) * 0.82f));
                    setFXSlot(apvts, 0, bellLike ? fxDelay : fxLike ? fxFilter : fxDrive,
                              bellLike ? 1 : fxLike ? 3 : 2,
                              0.54f, 0.34f, 0.20f, true);
                    break;

                case 4:
                    setParameterPlain(apvts, "osc1WarpFM",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "osc1WarpFM", 0.02f) + 0.18f));
                    setParameterPlain(apvts, "warpMutate",
                                      juce::jlimit(0.0f, 1.0f, readParameterPlain(apvts, "warpMutate", 0.10f) + 0.24f));
                    setParameterPlain(apvts, "noiseAmount",
                                      juce::jlimit(0.0f, 0.4f, readParameterPlain(apvts, "noiseAmount", 0.02f) + 0.04f));
                    setParameterPlain(apvts, "filterMode",
                                      (float) (fxLike ? filterPhaseWarp : hybridLike ? filterMetalComb : filterFormant));
                    setFXSlot(apvts, 1, bellLike ? fxReverb : fxLike ? fxPhaser : fxFlanger, 0, 0.24f, 0.60f, 0.20f, true);
                    setFXSlot(apvts, 2, bellLike ? fxDimension : fxDelay, bellLike ? 0 : 3, 0.26f, 0.38f, 0.18f, true);
                    break;
            }
            break;
        }
    }
}
}
