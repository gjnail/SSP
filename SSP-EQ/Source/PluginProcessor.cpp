#include "PluginProcessor.h"
#include "EQPresetLibrary.h"
#include "PluginEditor.h"
#include <complex>

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGainDb = -24.0f;
constexpr float maxGainDb = 24.0f;
constexpr float sqrtHalf = 0.70710678f;
constexpr auto presetStateWrapperTag = "SSPEQ_STATE";
constexpr auto presetMetaTag = "PRESET_META";

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

    slug = slug.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return slug.isNotEmpty() ? slug : "preset";
}

juce::String sanitisePresetText(const juce::String& text)
{
    return text.trim().retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_/&+.,()[]");
}

juce::String sanitisePresetCategory(const juce::String& text)
{
    auto category = sanitisePresetText(text).replaceCharacter('\\', '/');
    while (category.contains("//"))
        category = category.replace("//", "/");
    return category.trimCharactersAtStart(" /").trimCharactersAtEnd(" /");
}

juce::String sanitisePresetName(const juce::String& text)
{
    return sanitisePresetText(text).trimCharactersAtStart(" ").trimCharactersAtEnd(" ");
}

juce::String makePresetKey(bool isFactory, const juce::String& category, const juce::String& name)
{
    return (isFactory ? "factory:" : "user:") + slugify(category + "/" + name);
}

juce::String makePresetFilenameStem(const juce::String& category, const juce::String& name)
{
    auto categorySlug = slugify(category);
    if (categorySlug.isEmpty())
        categorySlug = "root";
    return categorySlug + "__" + slugify(name);
}

juce::String pointParamId(int index, const juce::String& suffix)
{
    return "p" + juce::String(index) + suffix;
}

float getRawFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = apvts.getRawParameterValue(id))
        return param->load();

    return 0.0f;
}

bool getRawBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return getRawFloatParam(apvts, id) >= 0.5f;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return param->getIndex();

    return 0;
}

void setBoolParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* param = apvts.getParameter(id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(value ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
}

void setFloatParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1(value);
        if (auto* raw = apvts.getRawParameterValue(id); raw != nullptr && std::abs(raw->load() - normalised) < 0.0001f)
            return;

        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

void setFloatParamFast(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1(value);
        if (auto* raw = apvts.getRawParameterValue(id); raw != nullptr && std::abs(raw->load() - normalised) < 0.0001f)
            return;

        param->setValueNotifyingHost(normalised);
    }
}

void setChoiceParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
    {
        const auto normalised = param->getNormalisableRange().convertTo0to1((float) index);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

int slopeIndexToDbPerOct(int slopeIndex)
{
    static constexpr int slopes[] = {6, 12, 18, 24, 36, 48, 72, 96};
    return slopes[(size_t) juce::jlimit(0, 7, slopeIndex)];
}

int stageCountForSlope(int slopeIndex)
{
    const int slopeDb = slopeIndexToDbPerOct(slopeIndex);
    return juce::jlimit(1, PluginProcessor::maxStagesPerPoint, (int) std::ceil((double) slopeDb / 12.0));
}

juce::IIRCoefficients makeIdentity()
{
    return {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
}

bool pointTypeUsesGain(int type)
{
    return type == PluginProcessor::bell
        || type == PluginProcessor::lowShelf
        || type == PluginProcessor::highShelf
        || type == PluginProcessor::tiltShelf;
}

bool pointTypeCanBlendDynamically(int type)
{
    return ! pointTypeUsesGain(type);
}

bool presetTypeUsesSlope(const juce::String& type)
{
    const auto lower = type.toLowerCase();
    return lower == "highpass" || lower == "lowcut" || lower == "lowpass" || lower == "highcut"
        || lower == "lowshelf" || lower == "highshelf";
}

juce::String pointTypeToPresetType(int type)
{
    switch (type)
    {
        case PluginProcessor::lowShelf: return "lowshelf";
        case PluginProcessor::highShelf: return "highshelf";
        case PluginProcessor::lowCut: return "highpass";
        case PluginProcessor::highCut: return "lowpass";
        case PluginProcessor::notch: return "notch";
        case PluginProcessor::bandPass: return "bandpass";
        case PluginProcessor::tiltShelf: return "tiltshelf";
        case PluginProcessor::bell:
        default: return "bell";
    }
}

int presetTypeToPointType(const juce::String& type)
{
    const auto lower = type.toLowerCase();
    if (lower == "lowshelf")
        return PluginProcessor::lowShelf;
    if (lower == "highshelf")
        return PluginProcessor::highShelf;
    if (lower == "highpass" || lower == "lowcut")
        return PluginProcessor::lowCut;
    if (lower == "lowpass" || lower == "highcut")
        return PluginProcessor::highCut;
    if (lower == "notch")
        return PluginProcessor::notch;
    if (lower == "bandpass")
        return PluginProcessor::bandPass;
    if (lower == "tiltshelf")
        return PluginProcessor::tiltShelf;
    return PluginProcessor::bell;
}

juce::String stereoModeToPresetType(int mode)
{
    switch (mode)
    {
        case PluginProcessor::left: return "left";
        case PluginProcessor::right: return "right";
        case PluginProcessor::mid: return "mid";
        case PluginProcessor::side: return "side";
        case PluginProcessor::stereo:
        default: return "stereo";
    }
}

int presetTypeToStereoMode(const juce::String& mode)
{
    const auto lower = mode.toLowerCase();
    if (lower == "left")
        return PluginProcessor::left;
    if (lower == "right")
        return PluginProcessor::right;
    if (lower == "mid")
        return PluginProcessor::mid;
    if (lower == "side")
        return PluginProcessor::side;
    return PluginProcessor::stereo;
}

juce::String dynamicDirectionToPresetType(int direction)
{
    return direction == PluginProcessor::dynamicBelow ? "below" : "above";
}

int presetTypeToDynamicDirection(const juce::String& direction)
{
    return direction.equalsIgnoreCase("below") ? PluginProcessor::dynamicBelow : PluginProcessor::dynamicAbove;
}

juce::String sidechainSourceToPresetType(int source)
{
    switch (source)
    {
        case PluginProcessor::sidechainExternal: return "external";
        case PluginProcessor::sidechainBand:     return "band";
        case PluginProcessor::sidechainInternal:
        default:                                 return "internal";
    }
}

int presetTypeToSidechainSource(const juce::String& source)
{
    if (source.equalsIgnoreCase("external"))
        return PluginProcessor::sidechainExternal;
    if (source.equalsIgnoreCase("band"))
        return PluginProcessor::sidechainBand;
    return PluginProcessor::sidechainInternal;
}

juce::IIRCoefficients makeTiltLowShelf(double sampleRate, float frequency, float q, float gainDb)
{
    return juce::IIRCoefficients::makeLowShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(-gainDb * 0.5f));
}

juce::IIRCoefficients makeTiltHighShelf(double sampleRate, float frequency, float q, float gainDb)
{
    return juce::IIRCoefficients::makeHighShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb * 0.5f));
}

PluginProcessor::PointFilterSetup buildPointSetup(const PluginProcessor::EqPoint& point, double sampleRate)
{
    PluginProcessor::PointFilterSetup setup{};

    if (!point.enabled || sampleRate <= 0.0)
        return setup;

    const int type = juce::jlimit(0, PluginProcessor::getPointTypeNames().size() - 1, point.type);
    const float frequency = juce::jlimit(minFrequency, maxFrequency, point.frequency);
    const float q = juce::jlimit(0.2f, 18.0f, point.q);
    const float gainDb = pointTypeUsesGain(type) ? juce::jlimit(minGainDb, maxGainDb, point.gainDb) : 0.0f;
    const int repeatedStageCount = stageCountForSlope(point.slopeIndex);

    auto pushStage = [&](const juce::IIRCoefficients& coeffs)
    {
        if (setup.numStages < PluginProcessor::maxStagesPerPoint)
            setup.coeffs[(size_t) setup.numStages++] = coeffs;
    };

    switch (type)
    {
        case PluginProcessor::lowShelf:
        {
            const float perStageGain = gainDb / (float) repeatedStageCount;
            for (int stage = 0; stage < repeatedStageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeLowShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(perStageGain)));
            break;
        }
        case PluginProcessor::highShelf:
        {
            const float perStageGain = gainDb / (float) repeatedStageCount;
            for (int stage = 0; stage < repeatedStageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeHighShelf(sampleRate, frequency, q, juce::Decibels::decibelsToGain(perStageGain)));
            break;
        }
        case PluginProcessor::lowCut:
            for (int stage = 0; stage < repeatedStageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeHighPass(sampleRate, frequency, sqrtHalf));
            break;
        case PluginProcessor::highCut:
            for (int stage = 0; stage < repeatedStageCount; ++stage)
                pushStage(juce::IIRCoefficients::makeLowPass(sampleRate, frequency, sqrtHalf));
            break;
        case PluginProcessor::notch:
            pushStage(juce::IIRCoefficients::makeNotchFilter(sampleRate, frequency, q));
            break;
        case PluginProcessor::bandPass:
            pushStage(juce::IIRCoefficients::makeBandPass(sampleRate, frequency, q));
            break;
        case PluginProcessor::tiltShelf:
        {
            pushStage(makeTiltLowShelf(sampleRate, frequency, q, gainDb));
            pushStage(makeTiltHighShelf(sampleRate, frequency, q, gainDb));
            break;
        }
        case PluginProcessor::bell:
        default:
            pushStage(juce::IIRCoefficients::makePeakFilter(sampleRate, frequency, q, juce::Decibels::decibelsToGain(gainDb)));
            break;
    }

    return setup;
}

PluginProcessor::PointArray readPointsFromState(juce::AudioProcessorValueTreeState& apvts)
{
    PluginProcessor::PointArray points{};
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        auto& point = points[(size_t) i];
        point.enabled = getRawBoolParam(apvts, pointParamId(i, "Enabled"));
        point.frequency = getRawFloatParam(apvts, pointParamId(i, "Freq"));
        point.gainDb = getRawFloatParam(apvts, pointParamId(i, "Gain"));
        point.q = getRawFloatParam(apvts, pointParamId(i, "Q"));
        point.type = getChoiceIndex(apvts, pointParamId(i, "Type"));
        point.slopeIndex = getChoiceIndex(apvts, pointParamId(i, "Slope"));
        point.stereoMode = getChoiceIndex(apvts, pointParamId(i, "StereoMode"));
        point.dynamicEnabled = getRawBoolParam(apvts, pointParamId(i, "DynEnabled"));
        point.dynamicThresholdDb = getRawFloatParam(apvts, pointParamId(i, "DynThreshold"));
        point.dynamicRatio = getRawFloatParam(apvts, pointParamId(i, "DynRatio"));
        point.dynamicAttackMs = getRawFloatParam(apvts, pointParamId(i, "DynAttack"));
        point.dynamicReleaseMs = getRawFloatParam(apvts, pointParamId(i, "DynRelease"));
        point.dynamicKneeDb = getRawFloatParam(apvts, pointParamId(i, "DynKnee"));
        point.dynamicDirection = getChoiceIndex(apvts, pointParamId(i, "DynDirection"));
        point.dynamicRangeDb = getRawFloatParam(apvts, pointParamId(i, "DynRange"));
        point.sidechainSource = getChoiceIndex(apvts, pointParamId(i, "ScSource"));
        point.sidechainBandIndex = juce::roundToInt(getRawFloatParam(apvts, pointParamId(i, "ScBand")));
        point.sidechainHighPassHz = getRawFloatParam(apvts, pointParamId(i, "ScHP"));
        point.sidechainLowPassHz = getRawFloatParam(apvts, pointParamId(i, "ScLP"));
    }

    return points;
}

std::array<PluginProcessor::PointFilterSetup, PluginProcessor::maxPoints> buildCoefficientArray(const PluginProcessor::PointArray& points, double sampleRate)
{
    std::array<PluginProcessor::PointFilterSetup, PluginProcessor::maxPoints> coeffs{};
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
        coeffs[(size_t) i] = buildPointSetup(points[(size_t) i], sampleRate);
    return coeffs;
}

double getMagnitudeForFrequency(const juce::IIRCoefficients& coefficients, double frequency, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto* c = coefficients.coefficients;
    const double cosW = std::cos(w);
    const double sinW = std::sin(w);
    const double cos2W = std::cos(2.0 * w);
    const double sin2W = std::sin(2.0 * w);

    const double numReal = (double) c[0] + (double) c[1] * cosW + (double) c[2] * cos2W;
    const double numImag = -((double) c[1] * sinW + (double) c[2] * sin2W);
    const double denReal = 1.0 + (double) c[3] * cosW + (double) c[4] * cos2W;
    const double denImag = -((double) c[3] * sinW + (double) c[4] * sin2W);

    const double numeratorMagnitude = std::sqrt(numReal * numReal + numImag * numImag);
    const double denominatorMagnitude = std::sqrt(denReal * denReal + denImag * denImag);
    const double magnitude = denominatorMagnitude > 0.0 ? numeratorMagnitude / denominatorMagnitude : 1.0;
    return std::isfinite(magnitude) ? magnitude : 1.0;
}

float collapseSignalToMono(const juce::AudioBuffer<float>& buffer, int sampleIndex)
{
    const int numChannels = juce::jmax(1, buffer.getNumChannels());
    float mono = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        mono += buffer.getReadPointer(channel)[sampleIndex];
    return mono / (float) numChannels;
}

float getMaximumDynamicGainDb(const PluginProcessor::EqPoint& point)
{
    if (! point.dynamicEnabled || ! pointTypeUsesGain(point.type))
        return point.gainDb;

    const float limitedRange = juce::jlimit(0.0f, 24.0f, point.dynamicRangeDb);
    const float magnitude = juce::jmin(std::abs(point.gainDb), limitedRange);
    return point.gainDb < 0.0f ? -magnitude : magnitude;
}

PluginProcessor::EqPoint makeSetPointForVisual(const PluginProcessor::EqPoint& point)
{
    auto result = point;
    if (point.dynamicEnabled && pointTypeUsesGain(point.type))
        result.gainDb = getMaximumDynamicGainDb(point);
    return result;
}

PluginProcessor::EqPoint makeActivePointForVisual(const PluginProcessor::EqPoint& point, float effectiveGainDb)
{
    auto result = point;
    if (point.dynamicEnabled && pointTypeUsesGain(point.type))
        result.gainDb = effectiveGainDb;
    return result;
}

float scaleFilterResponseByActivity(float responseDb, float activity)
{
    return responseDb * juce::jlimit(0.0f, 1.0f, activity);
}

juce::AudioBuffer<float> createUnitImpulseBuffer()
{
    juce::AudioBuffer<float> impulse(1, 1);
    impulse.clear();
    impulse.setSample(0, 0, 1.0f);
    return impulse;
}

bool pointArraysDiffer(const PluginProcessor::PointArray& lhs, const PluginProcessor::PointArray& rhs)
{
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        const auto& a = lhs[(size_t) i];
        const auto& b = rhs[(size_t) i];
        if (a.enabled != b.enabled
            || std::abs(a.frequency - b.frequency) > 0.001f
            || std::abs(a.gainDb - b.gainDb) > 0.001f
            || std::abs(a.q - b.q) > 0.001f
            || a.type != b.type
            || a.slopeIndex != b.slopeIndex
            || a.stereoMode != b.stereoMode
            || a.dynamicEnabled != b.dynamicEnabled
            || std::abs(a.dynamicThresholdDb - b.dynamicThresholdDb) > 0.001f
            || std::abs(a.dynamicRatio - b.dynamicRatio) > 0.001f
            || std::abs(a.dynamicAttackMs - b.dynamicAttackMs) > 0.001f
            || std::abs(a.dynamicReleaseMs - b.dynamicReleaseMs) > 0.001f
            || std::abs(a.dynamicKneeDb - b.dynamicKneeDb) > 0.001f
            || a.dynamicDirection != b.dynamicDirection
            || std::abs(a.dynamicRangeDb - b.dynamicRangeDb) > 0.001f
            || a.sidechainSource != b.sidechainSource
            || a.sidechainBandIndex != b.sidechainBandIndex
            || std::abs(a.sidechainHighPassHz - b.sidechainHighPassHz) > 0.001f
            || std::abs(a.sidechainLowPassHz - b.sidechainLowPassHz) > 0.001f)
            return true;
    }

    return false;
}

PluginProcessor::PointArray makeEmptyPointArray()
{
    PluginProcessor::PointArray points{};
    for (int i = 0; i < PluginProcessor::maxPoints; ++i)
    {
        auto& point = points[(size_t) i];
        point.enabled = false;
        point.frequency = juce::jmap((float) i, 0.0f, (float) juce::jmax(1, PluginProcessor::maxPoints - 1), 80.0f, 12000.0f);
        point.gainDb = 0.0f;
        point.q = 1.0f;
        point.type = PluginProcessor::bell;
        point.slopeIndex = 1;
        point.stereoMode = PluginProcessor::stereo;
    }

    return points;
}

int slopeDbToIndex(int slopeDb)
{
    static constexpr int slopes[] = {6, 12, 18, 24, 36, 48, 72, 96};
    for (int i = 0; i < (int) std::size(slopes); ++i)
        if (slopes[(size_t) i] == slopeDb)
            return i;
    return 1;
}

PluginProcessor::EqPoint presetBandToPoint(const PluginProcessor::PresetBand& bandData)
{
    PluginProcessor::EqPoint point;
    point.enabled = bandData.enabled;
    point.type = presetTypeToPointType(bandData.type);
    point.frequency = juce::jlimit(minFrequency, maxFrequency, bandData.frequency);
    point.gainDb = juce::jlimit(minGainDb, maxGainDb, bandData.gain);
    point.q = juce::jlimit(0.2f, 18.0f, bandData.q);
    point.slopeIndex = slopeDbToIndex(bandData.slope);
    point.stereoMode = presetTypeToStereoMode(bandData.stereoMode);
    point.dynamicEnabled = bandData.dynamicEnabled;
    point.dynamicThresholdDb = juce::jlimit(-60.0f, 0.0f, bandData.dynamicThresholdDb);
    point.dynamicRatio = juce::jlimit(1.0f, 20.0f, bandData.dynamicRatio);
    point.dynamicAttackMs = juce::jlimit(0.1f, 200.0f, bandData.dynamicAttackMs);
    point.dynamicReleaseMs = juce::jlimit(10.0f, 2000.0f, bandData.dynamicReleaseMs);
    point.dynamicKneeDb = juce::jlimit(0.0f, 12.0f, bandData.dynamicKneeDb);
    point.dynamicDirection = presetTypeToDynamicDirection(bandData.dynamicDirection);
    point.dynamicRangeDb = juce::jlimit(0.0f, 24.0f, bandData.dynamicRangeDb);
    point.sidechainSource = presetTypeToSidechainSource(bandData.sidechainSource);
    point.sidechainBandIndex = juce::jlimit(0, PluginProcessor::maxPoints - 1, bandData.sidechainBandIndex);
    point.sidechainHighPassHz = juce::jlimit(minFrequency, maxFrequency, bandData.sidechainHighPassHz);
    point.sidechainLowPassHz = juce::jlimit(minFrequency, maxFrequency, bandData.sidechainLowPassHz);
    return point;
}

PluginProcessor::PresetBand pointToPresetBand(const PluginProcessor::EqPoint& point)
{
    PluginProcessor::PresetBand bandData;
    bandData.enabled = point.enabled;
    bandData.type = pointTypeToPresetType(point.type);
    bandData.frequency = point.frequency;
    bandData.gain = point.gainDb;
    bandData.q = point.q;
    bandData.slope = slopeIndexToDbPerOct(point.slopeIndex);
    bandData.stereoMode = stereoModeToPresetType(point.stereoMode);
    bandData.dynamicEnabled = point.dynamicEnabled;
    bandData.dynamicThresholdDb = point.dynamicThresholdDb;
    bandData.dynamicRatio = point.dynamicRatio;
    bandData.dynamicAttackMs = point.dynamicAttackMs;
    bandData.dynamicReleaseMs = point.dynamicReleaseMs;
    bandData.dynamicKneeDb = point.dynamicKneeDb;
    bandData.dynamicDirection = dynamicDirectionToPresetType(point.dynamicDirection);
    bandData.dynamicRangeDb = point.dynamicRangeDb;
    bandData.sidechainSource = sidechainSourceToPresetType(point.sidechainSource);
    bandData.sidechainBandIndex = point.sidechainBandIndex;
    bandData.sidechainHighPassHz = point.sidechainHighPassHz;
    bandData.sidechainLowPassHz = point.sidechainLowPassHz;
    return bandData;
}

juce::var presetBandToVar(const PluginProcessor::PresetBand& bandData)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("enabled", bandData.enabled);
    object->setProperty("type", bandData.type);
    object->setProperty("frequency", bandData.frequency);
    object->setProperty("gain", bandData.gain);
    object->setProperty("q", bandData.q);
    object->setProperty("slope", presetTypeUsesSlope(bandData.type) ? juce::var(bandData.slope) : juce::var());
    object->setProperty("stereoMode", bandData.stereoMode);
    object->setProperty("dynamicEnabled", bandData.dynamicEnabled);
    object->setProperty("dynamicThresholdDb", bandData.dynamicThresholdDb);
    object->setProperty("dynamicRatio", bandData.dynamicRatio);
    object->setProperty("dynamicAttackMs", bandData.dynamicAttackMs);
    object->setProperty("dynamicReleaseMs", bandData.dynamicReleaseMs);
    object->setProperty("dynamicKneeDb", bandData.dynamicKneeDb);
    object->setProperty("dynamicDirection", bandData.dynamicDirection);
    object->setProperty("dynamicRangeDb", bandData.dynamicRangeDb);
    object->setProperty("sidechainSource", bandData.sidechainSource);
    object->setProperty("sidechainBandIndex", bandData.sidechainBandIndex);
    object->setProperty("sidechainHighPassHz", bandData.sidechainHighPassHz);
    object->setProperty("sidechainLowPassHz", bandData.sidechainLowPassHz);
    return juce::var(object.release());
}

bool readPresetBandFromVar(const juce::var& value, PluginProcessor::PresetBand& bandData)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    bandData.enabled = (bool) object->getProperty("enabled");
    bandData.type = object->getProperty("type").toString();
    bandData.frequency = (float) object->getProperty("frequency");
    bandData.gain = (float) object->getProperty("gain");
    bandData.q = (float) object->getProperty("q");
    bandData.slope = object->hasProperty("slope") && ! object->getProperty("slope").isVoid()
        ? (int) object->getProperty("slope")
        : 12;
    bandData.stereoMode = object->getProperty("stereoMode").toString();
    bandData.dynamicEnabled = object->hasProperty("dynamicEnabled") ? (bool) object->getProperty("dynamicEnabled") : false;
    bandData.dynamicThresholdDb = object->hasProperty("dynamicThresholdDb") ? (float) object->getProperty("dynamicThresholdDb") : -24.0f;
    bandData.dynamicRatio = object->hasProperty("dynamicRatio") ? (float) object->getProperty("dynamicRatio") : 4.0f;
    bandData.dynamicAttackMs = object->hasProperty("dynamicAttackMs") ? (float) object->getProperty("dynamicAttackMs") : 10.0f;
    bandData.dynamicReleaseMs = object->hasProperty("dynamicReleaseMs") ? (float) object->getProperty("dynamicReleaseMs") : 100.0f;
    bandData.dynamicKneeDb = object->hasProperty("dynamicKneeDb") ? (float) object->getProperty("dynamicKneeDb") : 6.0f;
    bandData.dynamicDirection = object->hasProperty("dynamicDirection") ? object->getProperty("dynamicDirection").toString() : "above";
    bandData.dynamicRangeDb = object->hasProperty("dynamicRangeDb") ? (float) object->getProperty("dynamicRangeDb") : 24.0f;
    bandData.sidechainSource = object->hasProperty("sidechainSource") ? object->getProperty("sidechainSource").toString() : "internal";
    bandData.sidechainBandIndex = object->hasProperty("sidechainBandIndex") ? (int) object->getProperty("sidechainBandIndex") : 0;
    bandData.sidechainHighPassHz = object->hasProperty("sidechainHighPassHz") ? (float) object->getProperty("sidechainHighPassHz") : minFrequency;
    bandData.sidechainLowPassHz = object->hasProperty("sidechainLowPassHz") ? (float) object->getProperty("sidechainLowPassHz") : maxFrequency;

    if (bandData.type.isEmpty())
        bandData.type = "bell";
    if (bandData.stereoMode.isEmpty())
        bandData.stereoMode = "stereo";
    if (bandData.dynamicDirection.isEmpty())
        bandData.dynamicDirection = "above";
    if (bandData.sidechainSource.isEmpty())
        bandData.sidechainSource = "internal";
    if (bandData.frequency <= 0.0f)
        bandData.frequency = 1000.0f;
    if (bandData.q <= 0.0f)
        bandData.q = 1.0f;

    return true;
}

juce::String serialisePointArrayToJson(const PluginProcessor::PointArray& points)
{
    juce::Array<juce::var> bands;
    for (const auto& point : points)
        if (point.enabled)
            bands.add(presetBandToVar(pointToPresetBand(point)));

    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("bands", juce::var(bands));
    return juce::JSON::toString(juce::var(object.release()));
}

bool deserialisePointArrayFromJson(const juce::String& json, PluginProcessor::PointArray& points)
{
    const auto parsed = juce::JSON::parse(json);
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return false;

    const auto* array = object->getProperty("bands").getArray();
    if (array == nullptr)
        return false;

    points = makeEmptyPointArray();
    const int count = juce::jmin((int) array->size(), PluginProcessor::maxPoints);
    for (int i = 0; i < count; ++i)
    {
        PluginProcessor::PresetBand bandData;
        if (readPresetBandFromVar(array->getReference(i), bandData))
            points[(size_t) i] = presetBandToPoint(bandData);
    }

    return true;
}

juce::String serialisePresetRecordToJson(const PluginProcessor::PresetRecord& preset)
{
    juce::Array<juce::var> bands;
    for (const auto& bandData : preset.bands)
        bands.add(presetBandToVar(bandData));

    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("name", preset.name);
    object->setProperty("category", preset.category);
    object->setProperty("author", preset.author);
    object->setProperty("isFactory", preset.isFactory);
    object->setProperty("bands", juce::var(bands));
    object->setProperty("moduleOrder", juce::var());
    object->setProperty("version", preset.version);
    return juce::JSON::toString(juce::var(object.release()), true);
}

bool readPresetRecordFromJson(const juce::String& json, PluginProcessor::PresetRecord& preset)
{
    const auto parsed = juce::JSON::parse(json);
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return false;

    preset.name = sanitisePresetName(object->getProperty("name").toString());
    preset.category = sanitisePresetCategory(object->getProperty("category").toString());
    preset.author = object->getProperty("author").toString();
    preset.isFactory = (bool) object->getProperty("isFactory");
    preset.version = object->getProperty("version").toString();
    if (preset.version.isEmpty())
        preset.version = "1.0";
    if (preset.author.isEmpty())
        preset.author = "Imported";
    if (preset.name.isEmpty())
        return false;

    preset.bands.clearQuick();
    if (const auto* bands = object->getProperty("bands").getArray())
    {
        for (const auto& entry : *bands)
        {
            PluginProcessor::PresetBand bandData;
            if (readPresetBandFromVar(entry, bandData))
                preset.bands.add(bandData);
        }
    }

    preset.id = makePresetKey(preset.isFactory, preset.category, preset.name);
    return true;
}

float randomFrequencyLog(juce::Random& random, float minHz, float maxHz)
{
    const auto minLog = std::log(minHz);
    const auto maxLog = std::log(maxHz);
    return std::exp(minLog + random.nextFloat() * (maxLog - minLog));
}

PluginProcessor::EqPoint randomPointForMode(juce::Random& random, PluginProcessor::RandomizeMode mode)
{
    PluginProcessor::EqPoint point;
    point.enabled = true;
    point.stereoMode = PluginProcessor::stereo;

    struct WeightedType
    {
        int type;
        int weight;
    };

    static constexpr std::array<WeightedType, 7> weightedTypes
    {{
        { PluginProcessor::bell, 50 },
        { PluginProcessor::lowShelf, 15 },
        { PluginProcessor::highShelf, 15 },
        { PluginProcessor::notch, 5 },
        { PluginProcessor::lowCut, 5 },
        { PluginProcessor::highCut, 5 },
        { PluginProcessor::bandPass, 5 }
    }};

    int roll = random.nextInt(100);
    point.type = PluginProcessor::bell;
    for (const auto& entry : weightedTypes)
    {
        roll -= entry.weight;
        if (roll < 0)
        {
            point.type = entry.type;
            break;
        }
    }

    point.frequency = randomFrequencyLog(random, minFrequency, maxFrequency);

    switch (mode)
    {
        case PluginProcessor::RandomizeMode::subtle:
            point.gainDb = juce::jmap(random.nextFloat(), -4.0f, 4.0f);
            point.q = juce::jmap(random.nextFloat(), 0.45f, 3.5f);
            break;
        case PluginProcessor::RandomizeMode::extreme:
            point.gainDb = juce::jmap(random.nextFloat(), minGainDb, maxGainDb);
            point.q = juce::jmap(random.nextFloat(), 0.2f, 18.0f);
            break;
        case PluginProcessor::RandomizeMode::standard:
        default:
            point.gainDb = juce::jmap(random.nextFloat(), -12.0f, 12.0f);
            point.q = juce::jmap(random.nextFloat(), 0.3f, 8.0f);
            break;
    }

    if (point.type == PluginProcessor::lowCut || point.type == PluginProcessor::highCut)
    {
        point.slopeIndex = mode == PluginProcessor::RandomizeMode::extreme ? random.nextInt(8) : random.nextInt(6);
        point.gainDb = 0.0f;
        point.q = sqrtHalf;
    }
    else if (point.type == PluginProcessor::lowShelf || point.type == PluginProcessor::highShelf)
    {
        point.slopeIndex = mode == PluginProcessor::RandomizeMode::extreme ? random.nextInt(8) : random.nextInt(4);
    }
    else
    {
        point.slopeIndex = 1;
    }

    return point;
}

std::pair<float, float> soloMonitoringRangeForPoint(const PluginProcessor::EqPoint& point)
{
    const float q = juce::jlimit(0.2f, 18.0f, point.q);
    const float octaveSpread = juce::jlimit(0.35f, 3.2f, 2.6f / q);
    const float spreadMultiplier = std::pow(2.0f, octaveSpread * 0.5f);

    switch (point.type)
    {
        case PluginProcessor::lowShelf:
        case PluginProcessor::lowCut:
            return { minFrequency, juce::jlimit(minFrequency, maxFrequency, point.frequency * spreadMultiplier) };
        case PluginProcessor::highShelf:
        case PluginProcessor::highCut:
            return { juce::jlimit(minFrequency, maxFrequency, point.frequency / spreadMultiplier), maxFrequency };
        case PluginProcessor::tiltShelf:
            return { juce::jlimit(minFrequency, maxFrequency, point.frequency / spreadMultiplier),
                     juce::jlimit(minFrequency, maxFrequency, point.frequency * spreadMultiplier) };
        case PluginProcessor::bell:
        case PluginProcessor::notch:
        case PluginProcessor::bandPass:
        default:
            return { juce::jlimit(minFrequency, maxFrequency, point.frequency / spreadMultiplier),
                     juce::jlimit(minFrequency, maxFrequency, point.frequency * spreadMultiplier) };
    }
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int i = 0; i < maxPoints; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(pointParamId(i, "Enabled"), pointParamId(i, "Enabled"), false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Freq"),
            pointParamId(i, "Freq"),
            juce::NormalisableRange<float>(minFrequency, maxFrequency, 0.0f, 0.25f),
            juce::jmap((float) i, 0.0f, (float) juce::jmax(1, maxPoints - 1), 80.0f, 12000.0f)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Gain"),
            pointParamId(i, "Gain"),
            juce::NormalisableRange<float>(minGainDb, maxGainDb, 0.01f),
            0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "Q"),
            pointParamId(i, "Q"),
            juce::NormalisableRange<float>(0.2f, 18.0f, 0.01f, 0.35f),
            1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "Type"),
            pointParamId(i, "Type"),
            getPointTypeNames(),
            bell));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "Slope"),
            pointParamId(i, "Slope"),
            getSlopeNames(),
            1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "StereoMode"),
            pointParamId(i, "StereoMode"),
            getStereoModeNames(),
            stereo));
        params.push_back(std::make_unique<juce::AudioParameterBool>(pointParamId(i, "DynEnabled"), pointParamId(i, "DynEnabled"), false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynThreshold"),
            pointParamId(i, "DynThreshold"),
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
            -24.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynRatio"),
            pointParamId(i, "DynRatio"),
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.35f),
            4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynAttack"),
            pointParamId(i, "DynAttack"),
            juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.3f),
            10.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynRelease"),
            pointParamId(i, "DynRelease"),
            juce::NormalisableRange<float>(10.0f, 2000.0f, 0.1f, 0.3f),
            100.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynKnee"),
            pointParamId(i, "DynKnee"),
            juce::NormalisableRange<float>(0.0f, 12.0f, 0.01f),
            6.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "DynDirection"),
            pointParamId(i, "DynDirection"),
            getDynamicDirectionNames(),
            dynamicAbove));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "DynRange"),
            pointParamId(i, "DynRange"),
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f),
            24.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pointParamId(i, "ScSource"),
            pointParamId(i, "ScSource"),
            getSidechainSourceNames(),
            sidechainInternal));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "ScBand"),
            pointParamId(i, "ScBand"),
            juce::NormalisableRange<float>(0.0f, (float) juce::jmax(0, maxPoints - 1), 1.0f),
            0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "ScHP"),
            pointParamId(i, "ScHP"),
            juce::NormalisableRange<float>(minFrequency, maxFrequency, 0.0f, 0.25f),
            minFrequency));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pointParamId(i, "ScLP"),
            pointParamId(i, "ScLP"),
            juce::NormalisableRange<float>(minFrequency, maxFrequency, 0.0f, 0.25f),
            maxFrequency));
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>("processingMode", "processingMode", getProcessingModeNames(), zeroLatency));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("analyzerMode", "analyzerMode", getAnalyzerModeNames(), analyzerPrePost));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("analyzerResolution", "analyzerResolution", getAnalyzerResolutionNames(), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("analyzerDecay", "analyzerDecay", juce::NormalisableRange<float>(0.05f, 0.98f, 0.01f), 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "outputGain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("globalBypass", "globalBypass", false));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto& convolver : linearPhaseConvolvers)
        convolver = std::make_unique<juce::dsp::Convolution>(juce::dsp::Convolution::Latency{ 0 }, convolutionQueue);

    analyzerFrame.pre.fill(-96.0f);
    analyzerFrame.post.fill(-96.0f);
    analyzerFrame.sidechain.fill(-96.0f);
    linearIrRequestedPoints = makeEmptyPointArray();
    linearIrWorker = std::thread([this] { linearFirWorkerLoop(); });
    syncPointCacheFromParameters();
    initialisePresetTracking();
    refreshUserPresets();
    int defaultPresetIndex = 0;
    for (int i = 0; i < getFactoryPresets().size(); ++i)
    {
        if (getFactoryPresets().getReference(i).name == "Default Setting")
        {
            defaultPresetIndex = i;
            break;
        }
    }

    loadFactoryPreset(defaultPresetIndex);
    compareSlots[0] = captureCurrentCompareSlotState();
    compareSlots[1] = compareSlots[0];
}

PluginProcessor::~PluginProcessor()
{
    {
        const std::lock_guard<std::mutex> lock(linearIrMutex);
        linearIrExit = true;
        linearIrDirty = true;
    }
    linearIrCondition.notify_all();
    if (linearIrWorker.joinable())
        linearIrWorker.join();

    for (const auto& parameterID : parameterIDs)
        apvts.removeParameterListener(parameterID, this);
}

const juce::StringArray& PluginProcessor::getPointTypeNames()
{
    static const juce::StringArray names{"Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf"};
    return names;
}

const juce::StringArray& PluginProcessor::getPointTypeDisplayNames()
{
    static const juce::StringArray names{
        "Bell     /\\",
        "Low Shelf  _/",
        "High Shelf \\_",
        "Low Cut   /|",
        "High Cut  |\\",
        "Notch     \\/",
        "Band Pass /\\/",
        "Tilt Shelf //"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getSlopeNames()
{
    static const juce::StringArray names{"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct"};
    return names;
}

const juce::StringArray& PluginProcessor::getStereoModeNames()
{
    static const juce::StringArray names{"Stereo", "Left", "Right", "Mid", "Side"};
    return names;
}

const juce::StringArray& PluginProcessor::getAnalyzerModeNames()
{
    static const juce::StringArray names{"Pre", "Post", "Pre+Post", "Sidechain"};
    return names;
}

const juce::StringArray& PluginProcessor::getProcessingModeNames()
{
    static const juce::StringArray names{"Zero Latency", "Natural Phase", "Linear Phase"};
    return names;
}

const juce::StringArray& PluginProcessor::getAnalyzerResolutionNames()
{
    static const juce::StringArray names{"Low", "Medium", "High"};
    return names;
}

const juce::StringArray& PluginProcessor::getDynamicDirectionNames()
{
    static const juce::StringArray names{"Above", "Below"};
    return names;
}

const juce::StringArray& PluginProcessor::getSidechainSourceNames()
{
    static const juce::StringArray names{"Internal", "External SC", "Band Trigger"};
    return names;
}

const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets()
{
    return sspeq::presets::getFactoryPresetLibrary();
}

PluginProcessor::EqPoint PluginProcessor::getPoint(int index) const
{
    const auto points = getPointsSnapshot();
    return juce::isPositiveAndBelow(index, maxPoints) ? points[(size_t) index] : EqPoint{};
}

PluginProcessor::PointArray PluginProcessor::getPointsSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return pointCache;
}

void PluginProcessor::setPoint(int index, const EqPoint& point)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    if (! point.enabled && soloPointIndex == index)
        setSoloPointIndex(-1);

    setBoolParam(apvts, pointParamId(index, "Enabled"), point.enabled);
    setFloatParam(apvts, pointParamId(index, "Freq"), juce::jlimit(minFrequency, maxFrequency, point.frequency));
    setFloatParam(apvts, pointParamId(index, "Gain"), juce::jlimit(minGainDb, maxGainDb, point.gainDb));
    setFloatParam(apvts, pointParamId(index, "Q"), juce::jlimit(0.2f, 18.0f, point.q));
    setChoiceParam(apvts, pointParamId(index, "Type"), juce::jlimit(0, getPointTypeNames().size() - 1, point.type));
    setChoiceParam(apvts, pointParamId(index, "Slope"), juce::jlimit(0, getSlopeNames().size() - 1, point.slopeIndex));
    setChoiceParam(apvts, pointParamId(index, "StereoMode"), juce::jlimit(0, getStereoModeNames().size() - 1, point.stereoMode));
    setBoolParam(apvts, pointParamId(index, "DynEnabled"), point.dynamicEnabled);
    setFloatParam(apvts, pointParamId(index, "DynThreshold"), juce::jlimit(-60.0f, 0.0f, point.dynamicThresholdDb));
    setFloatParam(apvts, pointParamId(index, "DynRatio"), juce::jlimit(1.0f, 20.0f, point.dynamicRatio));
    setFloatParam(apvts, pointParamId(index, "DynAttack"), juce::jlimit(0.1f, 200.0f, point.dynamicAttackMs));
    setFloatParam(apvts, pointParamId(index, "DynRelease"), juce::jlimit(10.0f, 2000.0f, point.dynamicReleaseMs));
    setFloatParam(apvts, pointParamId(index, "DynKnee"), juce::jlimit(0.0f, 12.0f, point.dynamicKneeDb));
    setChoiceParam(apvts, pointParamId(index, "DynDirection"), juce::jlimit(0, getDynamicDirectionNames().size() - 1, point.dynamicDirection));
    setFloatParam(apvts, pointParamId(index, "DynRange"), juce::jlimit(0.0f, 24.0f, point.dynamicRangeDb));
    setChoiceParam(apvts, pointParamId(index, "ScSource"), juce::jlimit(0, getSidechainSourceNames().size() - 1, point.sidechainSource));
    setFloatParam(apvts, pointParamId(index, "ScBand"), (float) juce::jlimit(0, maxPoints - 1, point.sidechainBandIndex));
    setFloatParam(apvts, pointParamId(index, "ScHP"), juce::jlimit(minFrequency, maxFrequency, point.sidechainHighPassHz));
    setFloatParam(apvts, pointParamId(index, "ScLP"), juce::jlimit(minFrequency, maxFrequency, point.sidechainLowPassHz));
    syncPointCacheFromParameters();
}

void PluginProcessor::setPointPosition(int index, float frequency, float gainDb)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    const float clampedFrequency = juce::jlimit(minFrequency, maxFrequency, frequency);
    const float clampedGain = juce::jlimit(minGainDb, maxGainDb, gainDb);
    setFloatParamFast(apvts, pointParamId(index, "Freq"), clampedFrequency);
    setFloatParamFast(apvts, pointParamId(index, "Gain"), clampedGain);

    auto points = getPointsSnapshot();
    points[(size_t) index].frequency = clampedFrequency;
    points[(size_t) index].gainDb = clampedGain;
    auto visualPoints = points;
    visualPoints[(size_t) index] = makeActivePointForVisual(points[(size_t) index], dynamicRuntime[(size_t) index].lastEffectiveGainDb);
    const auto coeffs = buildCoefficientArray(visualPoints, currentSampleRate);
    const auto setPoints = [points]
    {
        auto result = points;
        for (auto& point : result)
            point = makeSetPointForVisual(point);
        return result;
    }();
    const auto setCoeffs = buildCoefficientArray(setPoints, currentSampleRate);

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        pointCache = points;
        visualPointCache = visualPoints;
        coefficientCache = coeffs;
        setCoefficientCache = setCoeffs;
    }

    updateFilterStates();
}

int PluginProcessor::addPoint(float frequency, float gainDb)
{
    for (int i = 0; i < maxPoints; ++i)
    {
        if (!getPoint(i).enabled)
        {
            EqPoint point;
            point.enabled = true;
            point.frequency = frequency;
            point.gainDb = gainDb;
            point.q = 1.0f;
            point.type = bell;
            point.slopeIndex = 1;
            point.stereoMode = stereo;
            setPoint(i, point);
            return i;
        }
    }

    return -1;
}

void PluginProcessor::removePoint(int index)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    auto point = getPoint(index);
    point.enabled = false;
    setPoint(index, point);
}

float PluginProcessor::getResponseForFrequency(float frequency) const
{
    double magnitude = 1.0;
    std::array<PointFilterSetup, maxPoints> coeffs;

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        coeffs = coefficientCache;
    }

    for (const auto& pointSetup : coeffs)
    {
        for (int stage = 0; stage < pointSetup.numStages; ++stage)
            magnitude *= getMagnitudeForFrequency(pointSetup.coeffs[(size_t) stage], frequency, currentSampleRate);
    }

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

float PluginProcessor::getResponseForFrequencyByStereoMode(float frequency, int stereoMode) const
{
    double magnitude = 1.0;
    PointArray points;
    std::array<PointFilterSetup, maxPoints> coeffs;

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        points = visualPointCache;
        coeffs = coefficientCache;
    }

    bool anyMatched = false;
    for (int i = 0; i < maxPoints; ++i)
    {
        if (!points[(size_t) i].enabled || points[(size_t) i].stereoMode != stereoMode)
            continue;

        anyMatched = true;
        for (int stage = 0; stage < coeffs[(size_t) i].numStages; ++stage)
            magnitude *= getMagnitudeForFrequency(coeffs[(size_t) i].coeffs[(size_t) stage], frequency, currentSampleRate);
    }

    return anyMatched ? juce::Decibels::gainToDecibels((float) magnitude, -48.0f) : 0.0f;
}

float PluginProcessor::getBandResponseForFrequency(int index, float frequency) const
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return 0.0f;

    PointFilterSetup setup;
    DynamicVisualState visualState;
    EqPoint point;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        point = pointCache[(size_t) index];
        setup = coefficientCache[(size_t) index];
        visualState = dynamicVisualStates[(size_t) index];
    }

    double magnitude = 1.0;
    for (int stage = 0; stage < setup.numStages; ++stage)
        magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, currentSampleRate);

    const auto responseDb = juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
    return point.dynamicEnabled && pointTypeCanBlendDynamically(point.type)
        ? scaleFilterResponseByActivity(responseDb, visualState.activity)
        : responseDb;
}

float PluginProcessor::getBandSetResponseForFrequency(int index, float frequency) const
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return 0.0f;

    PointFilterSetup setup;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        setup = setCoefficientCache[(size_t) index];
    }

    double magnitude = 1.0;
    for (int stage = 0; stage < setup.numStages; ++stage)
        magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, currentSampleRate);

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

PluginProcessor::DynamicVisualState PluginProcessor::getDynamicVisualState(int index) const
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return {};

    const juce::SpinLock::ScopedLockType lock(stateLock);
    return dynamicVisualStates[(size_t) index];
}

int PluginProcessor::getEnabledPointCount() const
{
    int count = 0;
    const auto points = getPointsSnapshot();
    for (const auto& point : points)
        if (point.enabled)
            ++count;
    return count;
}

PluginProcessor::AnalyzerFrame PluginProcessor::getAnalyzerFrameCopy() const
{
    const juce::SpinLock::ScopedLockType lock(stateLock);
    return analyzerFrame;
}

float PluginProcessor::getOutputGainDb() const
{
    return getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "outputGain");
}

bool PluginProcessor::isGlobalBypassed() const
{
    return getRawBoolParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "globalBypass");
}

int PluginProcessor::getAnalyzerMode() const
{
    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "analyzerMode");
}

int PluginProcessor::getProcessingMode() const
{
    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "processingMode");
}

int PluginProcessor::getAnalyzerResolution() const
{
    return getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "analyzerResolution");
}

float PluginProcessor::getAnalyzerDecay() const
{
    return getRawFloatParam(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "analyzerDecay");
}

int PluginProcessor::getCurrentLatencySamples() const noexcept
{
    return currentLatencySamples.load();
}

double PluginProcessor::getCurrentLatencyMs() const noexcept
{
    return currentSampleRate > 0.0 ? (double) getCurrentLatencySamples() * 1000.0 / currentSampleRate : 0.0;
}

void PluginProcessor::setBandSidechainListen(int pointIndex, bool shouldListen)
{
    sidechainListenPointIndex = shouldListen && juce::isPositiveAndBelow(pointIndex, maxPoints) ? pointIndex : -1;
}

bool PluginProcessor::isBandSidechainListening(int pointIndex) const noexcept
{
    return sidechainListenPointIndex == pointIndex;
}

bool PluginProcessor::hasDynamicPointEnabled() const
{
    const auto points = getPointsSnapshot();
    for (const auto& point : points)
        if (point.enabled && point.dynamicEnabled)
            return true;
    return false;
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("SSP")
        .getChildFile("SSP EQ Presets");
}

juce::String PluginProcessor::getCurrentPresetName() const noexcept
{
    return currentPresetName;
}

juce::String PluginProcessor::getCurrentPresetCategory() const noexcept
{
    return currentPresetCategory;
}

juce::String PluginProcessor::getCurrentPresetAuthor() const noexcept
{
    return currentPresetAuthor;
}

juce::String PluginProcessor::getCurrentPresetKey() const noexcept
{
    return currentPresetKey;
}

bool PluginProcessor::isCurrentPresetFactory() const noexcept
{
    return currentPresetIsFactory;
}

bool PluginProcessor::isCurrentPresetDirty() const noexcept
{
    return currentPresetDirty.load();
}

bool PluginProcessor::matchesCurrentPreset(const PresetRecord& preset) const noexcept
{
    return preset.id.isNotEmpty() && preset.id == currentPresetKey;
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

void PluginProcessor::refreshUserPresets()
{
    userPresets.clearQuick();
    userPresetFiles.clearQuick();

    auto directory = getUserPresetDirectory();
    directory.createDirectory();

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.sspeqpreset");
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });

    for (const auto& file : files)
    {
        PresetRecord preset;
        if (! readPresetRecordFromJson(file.loadFileAsString(), preset))
            continue;

        preset.isFactory = false;
        preset.id = makePresetKey(false, preset.category, preset.name);
        userPresets.add(preset);
        userPresetFiles.add(file);
    }
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, getFactoryPresets().size()))
        return false;

    syncCurrentStateIntoActiveCompareSlot();
    requestVisualTransitionMs(100);
    applyPresetRecord(getFactoryPresets().getReference(index), true);
    return true;
}

bool PluginProcessor::loadUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresets.size()))
        return false;

    syncCurrentStateIntoActiveCompareSlot();
    requestVisualTransitionMs(100);
    applyPresetRecord(userPresets.getReference(index), true);
    return true;
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName, const juce::String& category)
{
    const auto cleanName = sanitisePresetName(presetName);
    const auto cleanCategory = sanitisePresetCategory(category);
    if (cleanName.isEmpty())
        return false;

    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    auto preset = buildCurrentPresetRecord(cleanName, cleanCategory,
                                           juce::SystemStats::getFullUserName().isNotEmpty() ? juce::SystemStats::getFullUserName() : "SSP User",
                                           false);
    preset.id = makePresetKey(false, preset.category, preset.name);

    const auto file = directory.getChildFile(makePresetFilenameStem(preset.category, preset.name) + ".sspeqpreset");
    if (! file.replaceWithText(serialisePresetRecordToJson(preset)))
        return false;

    refreshUserPresets();
    setCurrentPresetMetadata(preset.id, preset.name, preset.category, preset.author, false);
    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
    compareSlots[(size_t) activeCompareSlot] = captureCurrentCompareSlotState();
    clearABState();
    return true;
}

bool PluginProcessor::deleteUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresetFiles.size()))
        return false;

    const auto preset = userPresets.getReference(index);
    if (! userPresetFiles.getReference(index).deleteFile())
        return false;

    refreshUserPresets();
    if (preset.id == currentPresetKey)
    {
        currentPresetKey.clear();
        currentPresetDirty.store(true);
    }

    return true;
}

bool PluginProcessor::importPresetFromFile(const juce::File& file, bool loadAfterImport)
{
    if (! file.existsAsFile())
        return false;

    PresetRecord preset;
    if (! readPresetRecordFromJson(file.loadFileAsString(), preset))
        return false;

    preset.isFactory = false;
    preset.id = makePresetKey(false, preset.category, preset.name);

    auto directory = getUserPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    const auto destination = directory.getChildFile(makePresetFilenameStem(preset.category, preset.name) + ".sspeqpreset");
    if (! destination.replaceWithText(serialisePresetRecordToJson(preset)))
        return false;

    refreshUserPresets();

    if (loadAfterImport)
    {
        for (int i = 0; i < userPresets.size(); ++i)
            if (userPresets.getReference(i).id == preset.id)
                return loadUserPreset(i);
    }

    return true;
}

bool PluginProcessor::exportCurrentPresetToFile(const juce::File& file) const
{
    const auto presetName = currentPresetName.isNotEmpty() ? currentPresetName : "Current Settings";
    const auto preset = buildCurrentPresetRecord(presetName, currentPresetCategory,
                                                 currentPresetAuthor.isNotEmpty() ? currentPresetAuthor : "SSP User",
                                                 currentPresetIsFactory);
    return file.replaceWithText(serialisePresetRecordToJson(preset));
}

bool PluginProcessor::resetUserPresetsToFactoryDefaults()
{
    refreshUserPresets();

    bool removedAny = false;
    for (const auto& file : userPresetFiles)
        removedAny = file.deleteFile() || removedAny;

    refreshUserPresets();
    if (! currentPresetIsFactory)
    {
        currentPresetKey.clear();
        currentPresetDirty.store(true);
    }
    return removedAny || userPresetFiles.isEmpty();
}

bool PluginProcessor::hasLoadedPresetState() const noexcept
{
    return lastLoadedPresetSnapshot.valid;
}

bool PluginProcessor::hasABComparisonTarget() const noexcept
{
    return compareSlots[0].snapshot.valid || compareSlots[1].snapshot.valid;
}

bool PluginProcessor::isABComparisonActive() const noexcept
{
    return activeCompareSlot == 1;
}

bool PluginProcessor::toggleABComparison()
{
    return setActiveCompareSlot(activeCompareSlot == 0 ? 1 : 0);
}

bool PluginProcessor::beginPresetPreview(const PresetRecord& preset)
{
    if (preset.id.isEmpty())
        return false;

    if (! previewActive)
    {
        previewRestoreSnapshot = captureCurrentPresetSnapshot();
        previewActive = true;
    }

    if (previewPresetKey == preset.id)
        return true;

    previewPresetKey = preset.id;

    PresetStateSnapshot snapshot;
    snapshot.points = makeEmptyPointArray();
    snapshot.valid = true;
    for (int i = 0; i < juce::jmin(preset.bands.size(), maxPoints); ++i)
        snapshot.points[(size_t) i] = presetBandToPoint(preset.bands.getReference(i));

    requestVisualTransitionMs(100);
    applyPresetSnapshot(snapshot);
    return true;
}

void PluginProcessor::endPresetPreview()
{
    if (! previewActive)
        return;

    if (previewRestoreSnapshot.valid)
    {
        requestVisualTransitionMs(100);
        applyPresetSnapshot(previewRestoreSnapshot);
    }

    previewActive = false;
    previewPresetKey.clear();
    previewRestoreSnapshot = {};
}

bool PluginProcessor::isPresetPreviewActive() const noexcept
{
    return previewActive;
}

PluginProcessor::PointArray PluginProcessor::getPreviewReferencePointsSnapshot() const
{
    return previewRestoreSnapshot.valid ? previewRestoreSnapshot.points : getPointsSnapshot();
}

int PluginProcessor::getActiveCompareSlot() const noexcept
{
    return activeCompareSlot;
}

bool PluginProcessor::setActiveCompareSlot(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) compareSlots.size()))
        return false;

    if (slotIndex == activeCompareSlot)
        return true;

    endPresetPreview();
    syncCurrentStateIntoActiveCompareSlot();

    if (! compareSlots[(size_t) slotIndex].snapshot.valid)
        compareSlots[(size_t) slotIndex] = compareSlots[(size_t) activeCompareSlot];

    requestVisualTransitionMs(90);
    activeCompareSlot = slotIndex;
    applyCompareSlotState(slotIndex);
    return true;
}

bool PluginProcessor::copyActiveCompareSlotToOther()
{
    const int targetSlot = activeCompareSlot == 0 ? 1 : 0;
    syncCurrentStateIntoActiveCompareSlot();
    compareSlots[(size_t) targetSlot] = compareSlots[(size_t) activeCompareSlot];
    return true;
}

int PluginProcessor::getSoloPointIndex() const noexcept
{
    return soloPointIndex;
}

void PluginProcessor::setSoloPointIndex(int pointIndex)
{
    const int targetPoint = juce::isPositiveAndBelow(pointIndex, maxPoints) && getPoint(pointIndex).enabled ? pointIndex : -1;
    if (soloPointIndex == targetPoint)
        return;

    soloPointIndex = targetPoint;
    updateSoloMonitoringSetup();
}

void PluginProcessor::toggleSoloPoint(int pointIndex)
{
    setSoloPointIndex(soloPointIndex == pointIndex ? -1 : pointIndex);
}

juce::Array<PluginProcessor::PresetRecord> PluginProcessor::getSequentialPresetList() const
{
    juce::Array<PresetRecord> ordered;
    for (const auto& preset : getFactoryPresets())
        ordered.add(preset);
    for (const auto& preset : userPresets)
        ordered.add(preset);
    return ordered;
}

bool PluginProcessor::loadPresetByKey(const juce::String& presetKey)
{
    if (presetKey.isEmpty())
        return false;

    for (int i = 0; i < getFactoryPresets().size(); ++i)
        if (getFactoryPresets().getReference(i).id == presetKey)
            return loadFactoryPreset(i);

    refreshUserPresets();
    for (int i = 0; i < userPresets.size(); ++i)
        if (userPresets.getReference(i).id == presetKey)
            return loadUserPreset(i);

    return false;
}

bool PluginProcessor::stepPreset(int delta)
{
    const auto ordered = getSequentialPresetList();
    if (ordered.isEmpty())
        return false;

    int currentIndex = -1;
    for (int i = 0; i < ordered.size(); ++i)
    {
        if (ordered.getReference(i).id == currentPresetKey)
        {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0)
        currentIndex = delta >= 0 ? -1 : 0;

    const int nextIndex = (currentIndex + delta + ordered.size()) % ordered.size();
    return loadPresetByKey(ordered.getReference(nextIndex).id);
}

void PluginProcessor::randomizeCurrentSlot(RandomizeMode mode)
{
    endPresetPreview();
    syncCurrentStateIntoActiveCompareSlot();
    randomizeUndoState = captureCurrentCompareSlotState();
    hasRandomizeUndoState = true;

    PresetStateSnapshot snapshot;
    snapshot.points = makeEmptyPointArray();
    snapshot.valid = true;

    juce::Random random{ (int) juce::Time::getMillisecondCounter() };
    int bandCount = 0;
    switch (mode)
    {
        case RandomizeMode::subtle: bandCount = 3 + random.nextInt(3); break;
        case RandomizeMode::extreme: bandCount = 2 + random.nextInt(11); break;
        case RandomizeMode::standard:
        default: bandCount = 2 + random.nextInt(7); break;
    }

    const float forcedLow = randomFrequencyLog(random, 25.0f, 450.0f);
    const float forcedHigh = randomFrequencyLog(random, 2200.0f, 18000.0f);

    for (int i = 0; i < bandCount && i < maxPoints; ++i)
    {
        auto point = randomPointForMode(random, mode);
        if (i == 0)
            point.frequency = forcedLow;
        else if (i == 1)
            point.frequency = forcedHigh;
        snapshot.points[(size_t) i] = point;
    }

    requestVisualTransitionMs(mode == RandomizeMode::extreme ? 220 : 200);
    applyPresetSnapshot(snapshot);
    currentPresetDirty.store(true);
    compareSlots[(size_t) activeCompareSlot].dirty = true;
    compareSlots[(size_t) activeCompareSlot].snapshot = snapshot;
}

bool PluginProcessor::undoRandomizeCurrentSlot()
{
    if (! hasRandomizeUndoState)
        return false;

    requestVisualTransitionMs(120);
    compareSlots[(size_t) activeCompareSlot] = randomizeUndoState;
    applyCompareSlotState(activeCompareSlot);
    hasRandomizeUndoState = false;
    return true;
}

bool PluginProcessor::hasRandomizeUndo() const noexcept
{
    return hasRandomizeUndoState;
}

int PluginProcessor::consumePendingVisualTransitionMs(int fallbackMs)
{
    const int requested = pendingVisualTransitionMs.exchange(fallbackMs);
    return requested > 0 ? requested : fallbackMs;
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    if (dirtyTrackingSuppressionDepth.load() <= 0)
    {
        currentPresetDirty.store(true);
        compareSlots[(size_t) activeCompareSlot].dirty = true;
    }
}

void PluginProcessor::initialisePresetTracking()
{
    for (auto* parameter : getParameters())
    {
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
        {
            parameterIDs.addIfNotAlreadyThere(withID->paramID);
            apvts.addParameterListener(withID->paramID, this);
        }
    }
}

void PluginProcessor::restorePresetMetadataFromState()
{
    if (currentPresetName.isEmpty())
        currentPresetName = "Current Settings";
    if (currentPresetAuthor.isEmpty())
        currentPresetAuthor = currentPresetIsFactory ? "SSP Factory" : "SSP User";
    if (currentPresetKey.isEmpty() && currentPresetName.isNotEmpty())
        currentPresetKey = makePresetKey(currentPresetIsFactory, currentPresetCategory, currentPresetName);
    if (! lastLoadedPresetSnapshot.valid)
        lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    if (! compareSlots[(size_t) activeCompareSlot].snapshot.valid)
    {
        compareSlots[(size_t) activeCompareSlot] = captureCurrentCompareSlotState();
        compareSlots[(size_t) (activeCompareSlot == 0 ? 1 : 0)] = compareSlots[(size_t) activeCompareSlot];
    }
}

void PluginProcessor::applyPresetRecord(const PresetRecord& preset, bool updateLoadedPresetReference)
{
    endPresetPreview();

    PresetStateSnapshot snapshot;
    snapshot.points = makeEmptyPointArray();
    snapshot.valid = true;
    for (int i = 0; i < juce::jmin(preset.bands.size(), maxPoints); ++i)
        snapshot.points[(size_t) i] = presetBandToPoint(preset.bands.getReference(i));

    applyPresetSnapshot(snapshot);
    setCurrentPresetMetadata(makePresetKey(preset.isFactory, preset.category, preset.name), preset.name, preset.category, preset.author, preset.isFactory);
    currentPresetDirty.store(false);

    if (updateLoadedPresetReference)
    {
        lastLoadedPresetSnapshot = snapshot;
        activeCompareSlot = 0;
        compareSlots[0].snapshot = snapshot;
        compareSlots[0].baselineSnapshot = snapshot;
        compareSlots[0].presetKey = currentPresetKey;
        compareSlots[0].presetName = currentPresetName;
        compareSlots[0].presetCategory = currentPresetCategory;
        compareSlots[0].presetAuthor = currentPresetAuthor;
        compareSlots[0].isFactory = currentPresetIsFactory;
        compareSlots[0].dirty = false;
        clearABState();
    }
}

void PluginProcessor::applyPresetSnapshot(const PresetStateSnapshot& snapshot)
{
    if (! snapshot.valid)
        return;

    dirtyTrackingSuppressionDepth.fetch_add(1);
    for (int i = 0; i < maxPoints; ++i)
    {
        const auto& point = snapshot.points[(size_t) i];
        setBoolParam(apvts, pointParamId(i, "Enabled"), point.enabled);
        setFloatParam(apvts, pointParamId(i, "Freq"), juce::jlimit(minFrequency, maxFrequency, point.frequency));
        setFloatParam(apvts, pointParamId(i, "Gain"), juce::jlimit(minGainDb, maxGainDb, point.gainDb));
        setFloatParam(apvts, pointParamId(i, "Q"), juce::jlimit(0.2f, 18.0f, point.q));
        setChoiceParam(apvts, pointParamId(i, "Type"), juce::jlimit(0, getPointTypeNames().size() - 1, point.type));
        setChoiceParam(apvts, pointParamId(i, "Slope"), juce::jlimit(0, getSlopeNames().size() - 1, point.slopeIndex));
        setChoiceParam(apvts, pointParamId(i, "StereoMode"), juce::jlimit(0, getStereoModeNames().size() - 1, point.stereoMode));
        setBoolParam(apvts, pointParamId(i, "DynEnabled"), point.dynamicEnabled);
        setFloatParam(apvts, pointParamId(i, "DynThreshold"), juce::jlimit(-60.0f, 0.0f, point.dynamicThresholdDb));
        setFloatParam(apvts, pointParamId(i, "DynRatio"), juce::jlimit(1.0f, 20.0f, point.dynamicRatio));
        setFloatParam(apvts, pointParamId(i, "DynAttack"), juce::jlimit(0.1f, 200.0f, point.dynamicAttackMs));
        setFloatParam(apvts, pointParamId(i, "DynRelease"), juce::jlimit(10.0f, 2000.0f, point.dynamicReleaseMs));
        setFloatParam(apvts, pointParamId(i, "DynKnee"), juce::jlimit(0.0f, 12.0f, point.dynamicKneeDb));
        setChoiceParam(apvts, pointParamId(i, "DynDirection"), juce::jlimit(0, getDynamicDirectionNames().size() - 1, point.dynamicDirection));
        setFloatParam(apvts, pointParamId(i, "DynRange"), juce::jlimit(0.0f, 24.0f, point.dynamicRangeDb));
        setChoiceParam(apvts, pointParamId(i, "ScSource"), juce::jlimit(0, getSidechainSourceNames().size() - 1, point.sidechainSource));
        setFloatParam(apvts, pointParamId(i, "ScBand"), (float) juce::jlimit(0, maxPoints - 1, point.sidechainBandIndex));
        setFloatParam(apvts, pointParamId(i, "ScHP"), juce::jlimit(minFrequency, maxFrequency, point.sidechainHighPassHz));
        setFloatParam(apvts, pointParamId(i, "ScLP"), juce::jlimit(minFrequency, maxFrequency, point.sidechainLowPassHz));
    }

    dirtyTrackingSuppressionDepth.fetch_sub(1);
    syncPointCacheFromParameters();
    updateSoloMonitoringSetup();
}

PluginProcessor::PresetStateSnapshot PluginProcessor::captureCurrentPresetSnapshot() const
{
    PresetStateSnapshot snapshot;
    snapshot.points = getPointsSnapshot();
    snapshot.valid = true;
    return snapshot;
}

void PluginProcessor::syncCurrentStateIntoActiveCompareSlot()
{
    if (previewActive)
        return;

    compareSlots[(size_t) activeCompareSlot] = captureCurrentCompareSlotState();
}

void PluginProcessor::applyCompareSlotState(int slotIndex)
{
    if (! juce::isPositiveAndBelow(slotIndex, (int) compareSlots.size()))
        return;

    const auto& slot = compareSlots[(size_t) slotIndex];
    if (slot.snapshot.valid)
        applyPresetSnapshot(slot.snapshot);

    currentPresetKey = slot.presetKey;
    currentPresetName = slot.presetName;
    currentPresetCategory = slot.presetCategory;
    currentPresetAuthor = slot.presetAuthor;
    currentPresetIsFactory = slot.isFactory;
    currentPresetDirty.store(slot.dirty);
    lastLoadedPresetSnapshot = slot.baselineSnapshot.valid ? slot.baselineSnapshot : slot.snapshot;
}

void PluginProcessor::setCompareSlotState(int slotIndex, const CompareSlotState& state)
{
    if (juce::isPositiveAndBelow(slotIndex, (int) compareSlots.size()))
        compareSlots[(size_t) slotIndex] = state;
}

PluginProcessor::CompareSlotState PluginProcessor::captureCurrentCompareSlotState() const
{
    CompareSlotState slot;
    slot.snapshot = captureCurrentPresetSnapshot();
    slot.baselineSnapshot = lastLoadedPresetSnapshot.valid ? lastLoadedPresetSnapshot : slot.snapshot;
    slot.presetKey = currentPresetKey;
    slot.presetName = currentPresetName;
    slot.presetCategory = currentPresetCategory;
    slot.presetAuthor = currentPresetAuthor;
    slot.isFactory = currentPresetIsFactory;
    slot.dirty = currentPresetDirty.load();
    return slot;
}

void PluginProcessor::setCurrentPresetMetadata(juce::String presetKey, juce::String presetName, juce::String category, juce::String author, bool isFactory)
{
    currentPresetKey = std::move(presetKey);
    currentPresetName = std::move(presetName);
    currentPresetCategory = std::move(category);
    currentPresetAuthor = std::move(author);
    currentPresetIsFactory = isFactory;
}

PluginProcessor::PresetRecord PluginProcessor::buildCurrentPresetRecord(juce::String presetName, juce::String category, juce::String author, bool isFactory) const
{
    PresetRecord preset;
    preset.name = sanitisePresetName(presetName);
    preset.category = sanitisePresetCategory(category);
    preset.author = author.isNotEmpty() ? author : (isFactory ? "SSP Factory" : "SSP User");
    preset.isFactory = isFactory;
    preset.id = makePresetKey(isFactory, preset.category, preset.name);

    const auto snapshot = captureCurrentPresetSnapshot();
    for (const auto& point : snapshot.points)
        if (point.enabled)
            preset.bands.add(pointToPresetBand(point));

    return preset;
}

void PluginProcessor::clearABState()
{
    abComparisonActive = false;
    abComparisonSnapshot = {};
    compareSlots[(size_t) activeCompareSlot].baselineSnapshot = lastLoadedPresetSnapshot;
    compareSlots[(size_t) activeCompareSlot].dirty = currentPresetDirty.load();
}

void PluginProcessor::requestVisualTransitionMs(int durationMs)
{
    pendingVisualTransitionMs.store(durationMs);
}

void PluginProcessor::updateSoloMonitoringSetup()
{
    if (! juce::isPositiveAndBelow(soloPointIndex, maxPoints))
        return;

    const auto point = getPoint(soloPointIndex);
    if (! point.enabled || currentSampleRate <= 0.0)
        return;

    const auto [lowHz, highHz] = soloMonitoringRangeForPoint(point);
    const auto lowPassFreq = juce::jlimit(minFrequency + 10.0f, maxFrequency, highHz);
    const auto highPassFreq = juce::jlimit(minFrequency, maxFrequency - 10.0f, lowHz);

    const auto lowPassCoeffs = juce::IIRCoefficients::makeLowPass(currentSampleRate, lowPassFreq, sqrtHalf);
    const auto highPassCoeffs = juce::IIRCoefficients::makeHighPass(currentSampleRate, highPassFreq, sqrtHalf);
    soloLowPassLeft.setCoefficients(lowPassCoeffs);
    soloLowPassRight.setCoefficients(lowPassCoeffs);
    soloHighPassLeft.setCoefficients(highPassCoeffs);
    soloHighPassRight.setCoefficients(highPassCoeffs);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    syncPointCacheFromParameters();

    for (auto& modeFilters : filters)
        for (auto& domainFilters : modeFilters)
            for (auto& pointFilters : domainFilters)
                for (auto& filter : pointFilters)
                    filter.reset();

    for (auto& domainFilters : naturalPhaseCompensationFilters)
        for (auto& pointFilters : domainFilters)
            for (auto& filter : pointFilters)
                filter.reset();

    soloHighPassLeft.reset();
    soloHighPassRight.reset();
    soloLowPassLeft.reset();
    soloLowPassRight.reset();
    naturalPhaseDelayLines[0].prepare({ sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1 });
    naturalPhaseDelayLines[1].prepare({ sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1 });
    naturalPhaseDelayLines[0].setMaximumDelayInSamples(1024);
    naturalPhaseDelayLines[1].setMaximumDelayInSamples(1024);
    naturalPhaseDelayLines[0].reset();
    naturalPhaseDelayLines[1].reset();
    resetDynamicState();

    auto unitImpulse = createUnitImpulseBuffer();
    for (auto& convolver : linearPhaseConvolvers)
        convolver->loadImpulseResponse(juce::AudioBuffer<float>(unitImpulse),
                                       sampleRate,
                                       juce::dsp::Convolution::Stereo::no,
                                       juce::dsp::Convolution::Trim::no,
                                       juce::dsp::Convolution::Normalise::no);

    juce::dsp::ProcessSpec convolverSpec{ sampleRate, (juce::uint32) juce::jmax(1, samplesPerBlock), 1 };
    for (auto& convolver : linearPhaseConvolvers)
    {
        convolver->prepare(convolverSpec);
        convolver->reset();
    }

    preSpectrumFifo.fill(0.0f);
    postSpectrumFifo.fill(0.0f);
    sidechainSpectrumFifo.fill(0.0f);
    fftData.fill(0.0f);
    analyzerFrame.pre.fill(-96.0f);
    analyzerFrame.post.fill(-96.0f);
    analyzerFrame.sidechain.fill(-96.0f);
    preSpectrumFifoIndex = 0;
    postSpectrumFifoIndex = 0;
    sidechainSpectrumFifoIndex = 0;
    processingModeFadeRemainingSamples = 0;
    activeProcessingMode = getProcessingMode();
    previousProcessingMode = activeProcessingMode;
    updateFilterStates();
    updateNaturalPhaseCompensation();
    scheduleLinearFirRebuild();
    loadPendingLinearImpulseResponses();
    updateLatencyState();
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo() || mainIn != mainOut)
        return false;

    const auto sidechain = layouts.getChannelSet(true, 1);
    return sidechain.isDisabled() || sidechain == juce::AudioChannelSet::stereo();
}

void PluginProcessor::ensureScratchBuffers(int numSamples)
{
    scratchInputBuffer.setSize(2, numSamples, false, false, true);
    scratchRenderBufferA.setSize(2, numSamples, false, false, true);
    scratchRenderBufferB.setSize(2, numSamples, false, false, true);
    scratchRenderBufferC.setSize(2, numSamples, false, false, true);
    linearMidBuffer.setSize(1, numSamples, false, false, true);
    linearSideBuffer.setSize(1, numSamples, false, false, true);
    linearTempMonoBuffer.setSize(1, numSamples, false, false, true);

    blockSidechainListen.resize((size_t) numSamples, 0.0f);
    for (int i = 0; i < maxPoints; ++i)
    {
        blockEffectiveGainDb[(size_t) i].resize((size_t) numSamples, 0.0f);
        blockActivity[(size_t) i].resize((size_t) numSamples, 0.0f);
    }
}

void PluginProcessor::computeDynamicControlSignals(const juce::AudioBuffer<float>& mainBuffer,
                                                   const juce::AudioBuffer<float>& sidechainBuffer)
{
    const auto points = getPointsSnapshot();
    const int numSamples = mainBuffer.getNumSamples();
    const auto detectorSampleRate = juce::jmax(1.0, currentSampleRate);

    auto sourceSampleForPoint = [&](const EqPoint& point, int sampleIndex) -> float
    {
        auto sampleFromBuffer = [&](const juce::AudioBuffer<float>& source) -> float
        {
            const int channels = source.getNumChannels();
            if (channels <= 0 || source.getNumSamples() <= sampleIndex)
                return 0.0f;

            const float leftSample = source.getReadPointer(0)[sampleIndex];
            const float rightSample = channels > 1 ? source.getReadPointer(1)[sampleIndex] : leftSample;

            switch (point.stereoMode)
            {
                case left:  return leftSample;
                case right: return rightSample;
                case mid:   return (leftSample + rightSample) * sqrtHalf;
                case side:  return (leftSample - rightSample) * sqrtHalf;
                case stereo:
                default:    return 0.5f * (leftSample + rightSample);
            }
        };

        if (point.sidechainSource == sidechainExternal && sidechainBuffer.getNumChannels() > 0)
            return sampleFromBuffer(sidechainBuffer);

        return sampleFromBuffer(mainBuffer);
    };

    auto configureDetector = [&](int pointIndex)
    {
        const auto& point = points[(size_t) pointIndex];
        auto referencePoint = point;
        if (point.sidechainSource == sidechainBand
            && juce::isPositiveAndBelow(point.sidechainBandIndex, maxPoints)
            && points[(size_t) point.sidechainBandIndex].enabled)
            referencePoint = points[(size_t) point.sidechainBandIndex];

        const float detectorFrequency = juce::jlimit(minFrequency, maxFrequency, referencePoint.frequency);
        const float detectorQ = juce::jlimit(0.2f, 18.0f, referencePoint.q);
        const float detectorHP = juce::jlimit(minFrequency, maxFrequency, juce::jmin(point.sidechainHighPassHz, point.sidechainLowPassHz - 1.0f));
        const float detectorLP = juce::jlimit(minFrequency, maxFrequency, juce::jmax(point.sidechainLowPassHz, point.sidechainHighPassHz + 1.0f));

        juce::IIRCoefficients primary;
        switch (referencePoint.type)
        {
            case lowShelf:
            case lowCut:   primary = juce::IIRCoefficients::makeLowPass(detectorSampleRate, detectorFrequency, sqrtHalf); break;
            case highShelf:
            case highCut:  primary = juce::IIRCoefficients::makeHighPass(detectorSampleRate, detectorFrequency, sqrtHalf); break;
            case bell:
            case notch:
            case bandPass:
            case tiltShelf:
            default:       primary = juce::IIRCoefficients::makeBandPass(detectorSampleRate, detectorFrequency, detectorQ); break;
        }

        dynamicRuntime[(size_t) pointIndex].detectorPrimary.setCoefficients(primary);
        dynamicRuntime[(size_t) pointIndex].detectorHighPass.setCoefficients(juce::IIRCoefficients::makeHighPass(detectorSampleRate, detectorHP, sqrtHalf));
        dynamicRuntime[(size_t) pointIndex].detectorLowPass.setCoefficients(juce::IIRCoefficients::makeLowPass(detectorSampleRate, detectorLP, sqrtHalf));
    };

    for (int i = 0; i < maxPoints; ++i)
        configureDetector(i);

    std::array<DynamicVisualState, maxPoints> updatedVisuals{};

    for (int sample = 0; sample < numSamples; ++sample)
    {
        blockSidechainListen[(size_t) sample] = 0.0f;

        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& point = points[(size_t) pointIndex];
            auto& runtime = dynamicRuntime[(size_t) pointIndex];
            auto& visual = updatedVisuals[(size_t) pointIndex];
            visual.enabled = point.enabled && point.dynamicEnabled;

            if (! point.enabled)
            {
                blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample] = 0.0f;
                blockActivity[(size_t) pointIndex][(size_t) sample] = 0.0f;
                continue;
            }

            if (! point.dynamicEnabled)
            {
                blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample] = point.gainDb;
                blockActivity[(size_t) pointIndex][(size_t) sample] = 1.0f;
                visual.effectiveGainDb = point.gainDb;
                visual.maxGainDb = getMaximumDynamicGainDb(point);
                visual.activity = 1.0f;
                continue;
            }

            const float rawSample = sourceSampleForPoint(point, sample);
            const float filteredA = runtime.detectorPrimary.processSingleSampleRaw(rawSample);
            const float filteredB = runtime.detectorHighPass.processSingleSampleRaw(filteredA);
            const float detectorSample = runtime.detectorLowPass.processSingleSampleRaw(filteredB);
            runtime.lastMonitorSample = detectorSample;

            if (pointIndex == sidechainListenPointIndex)
                blockSidechainListen[(size_t) sample] = detectorSample;

            const float targetPower = detectorSample * detectorSample;
            const float timeMs = targetPower > runtime.smoothedPower ? point.dynamicAttackMs : point.dynamicReleaseMs;
            const float coeff = (float) std::exp(-1.0 / juce::jmax(1.0, detectorSampleRate * (double) juce::jmax(0.1f, timeMs) * 0.001));
            runtime.smoothedPower = coeff * runtime.smoothedPower + (1.0f - coeff) * targetPower;

            const float levelDb = juce::Decibels::gainToDecibels(std::sqrt(juce::jmax(runtime.smoothedPower, 1.0e-12f)), -96.0f);
            const float threshold = juce::jlimit(-60.0f, 0.0f, point.dynamicThresholdDb);
            const float knee = juce::jlimit(0.0f, 12.0f, point.dynamicKneeDb);
            const float signedDistance = point.dynamicDirection == dynamicAbove ? (levelDb - threshold)
                                                                                : (threshold - levelDb);

            float overDb = 0.0f;
            if (knee > 0.0f)
            {
                if (signedDistance > -0.5f * knee)
                {
                    if (signedDistance >= 0.5f * knee)
                        overDb = signedDistance;
                    else
                    {
                        const float x = (signedDistance + 0.5f * knee) / knee;
                        overDb = 0.5f * knee * x * x;
                    }
                }
            }
            else
            {
                overDb = juce::jmax(0.0f, signedDistance);
            }

            const float ratio = juce::jmax(1.0f, point.dynamicRatio);
            const float compressionDb = juce::jmax(0.0f, overDb * (1.0f - 1.0f / ratio));
            const float maxDynamicGainDb = pointTypeUsesGain(point.type)
                ? std::abs(getMaximumDynamicGainDb(point))
                : juce::jmax(1.0f, juce::jlimit(0.0f, 24.0f, point.dynamicRangeDb));
            const float activity = juce::jlimit(0.0f, 1.0f, compressionDb / juce::jmax(0.01f, maxDynamicGainDb));
            const float effectiveGainDb = pointTypeUsesGain(point.type)
                ? getMaximumDynamicGainDb(point) * activity
                : point.gainDb;

            runtime.lastEffectiveGainDb = effectiveGainDb;
            runtime.lastActivity = activity;

            blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample] = effectiveGainDb;
            blockActivity[(size_t) pointIndex][(size_t) sample] = activity;

            visual.detectorLevelDb = levelDb;
            visual.activity = activity;
            visual.effectiveGainDb = effectiveGainDb;
            visual.maxGainDb = pointTypeUsesGain(point.type) ? getMaximumDynamicGainDb(point) : point.gainDb;
            visual.activeResponseDb = effectiveGainDb;
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        dynamicVisualStates = updatedVisuals;
    }

    auto visualPoints = points;
    for (int i = 0; i < maxPoints; ++i)
        visualPoints[(size_t) i] = makeActivePointForVisual(points[(size_t) i], dynamicRuntime[(size_t) i].lastEffectiveGainDb);

    const auto visualCoeffs = buildCoefficientArray(visualPoints, currentSampleRate);
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        visualPointCache = visualPoints;
        coefficientCache = visualCoeffs;
    }
}

float PluginProcessor::processPointForDomain(int modeIndex, int domainIndex, int pointIndex, float sample, float wetMix)
{
    PointFilterSetup setup;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        setup = coefficientCache[(size_t) pointIndex];
    }

    const float dry = sample;
    float value = sample;
    for (int stage = 0; stage < setup.numStages; ++stage)
        value = filters[(size_t) modeIndex][(size_t) domainIndex][(size_t) pointIndex][(size_t) stage].processSingleSampleRaw(value);

    return juce::jmap(juce::jlimit(0.0f, 1.0f, wetMix), dry, value);
}

void PluginProcessor::renderZeroLatencyMode(const juce::AudioBuffer<float>& source,
                                            juce::AudioBuffer<float>& destination)
{
    destination.makeCopyOf(source, true);
    const auto points = getPointsSnapshot();
    auto* leftData = destination.getWritePointer(0);
    auto* rightData = destination.getWritePointer(1);

    for (int sample = 0; sample < destination.getNumSamples(); ++sample)
    {
        float leftSample = leftData[sample];
        float rightSample = rightData[sample];

        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& point = points[(size_t) pointIndex];
            if (! point.enabled)
                continue;

            if (point.dynamicEnabled && pointTypeUsesGain(point.type))
                updateDynamicBandSetup(pointIndex, blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample]);

            const float wetMix = (point.dynamicEnabled && pointTypeCanBlendDynamically(point.type))
                ? blockActivity[(size_t) pointIndex][(size_t) sample]
                : 1.0f;

            switch (point.stereoMode)
            {
                case left:
                    leftSample = processPointForDomain(zeroLatency, 0, pointIndex, leftSample, wetMix);
                    break;
                case right:
                    rightSample = processPointForDomain(zeroLatency, 1, pointIndex, rightSample, wetMix);
                    break;
                case mid:
                {
                    float mid = (leftSample + rightSample) * sqrtHalf;
                    const float side = (leftSample - rightSample) * sqrtHalf;
                    mid = processPointForDomain(zeroLatency, 2, pointIndex, mid, wetMix);
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case side:
                {
                    const float mid = (leftSample + rightSample) * sqrtHalf;
                    float side = (leftSample - rightSample) * sqrtHalf;
                    side = processPointForDomain(zeroLatency, 3, pointIndex, side, wetMix);
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case stereo:
                default:
                    leftSample = processPointForDomain(zeroLatency, 0, pointIndex, leftSample, wetMix);
                    rightSample = processPointForDomain(zeroLatency, 1, pointIndex, rightSample, wetMix);
                    break;
            }
        }

        leftData[sample] = leftSample;
        rightData[sample] = rightSample;
    }
}

void PluginProcessor::renderNaturalPhaseMode(const juce::AudioBuffer<float>& source,
                                             juce::AudioBuffer<float>& destination)
{
    destination.makeCopyOf(source, true);
    const auto points = getPointsSnapshot();
    auto* leftData = destination.getWritePointer(0);
    auto* rightData = destination.getWritePointer(1);

    auto processCompensation = [this](int domain, int pointIndex, float sampleValue)
    {
        float result = sampleValue;
        PointFilterSetup setup;
        {
            const juce::SpinLock::ScopedLockType lock(stateLock);
            setup = setCoefficientCache[(size_t) pointIndex];
        }

        for (int stage = 0; stage < setup.numStages; ++stage)
            result = naturalPhaseCompensationFilters[(size_t) domain][(size_t) pointIndex][(size_t) stage].processSingleSampleRaw(result);
        return result;
    };

    for (int sample = 0; sample < destination.getNumSamples(); ++sample)
    {
        float leftSample = leftData[sample];
        float rightSample = rightData[sample];

        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& point = points[(size_t) pointIndex];
            if (! point.enabled)
                continue;

            if (point.dynamicEnabled && pointTypeUsesGain(point.type))
                updateDynamicBandSetup(pointIndex, blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample]);

            const float wetMix = (point.dynamicEnabled && pointTypeCanBlendDynamically(point.type))
                ? blockActivity[(size_t) pointIndex][(size_t) sample]
                : 1.0f;

            switch (point.stereoMode)
            {
                case left:
                    leftSample = processCompensation(0, pointIndex, processPointForDomain(naturalPhase, 0, pointIndex, leftSample, wetMix));
                    break;
                case right:
                    rightSample = processCompensation(1, pointIndex, processPointForDomain(naturalPhase, 1, pointIndex, rightSample, wetMix));
                    break;
                case mid:
                {
                    float mid = (leftSample + rightSample) * sqrtHalf;
                    const float side = (leftSample - rightSample) * sqrtHalf;
                    mid = processCompensation(2, pointIndex, processPointForDomain(naturalPhase, 2, pointIndex, mid, wetMix));
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case side:
                {
                    const float mid = (leftSample + rightSample) * sqrtHalf;
                    float side = (leftSample - rightSample) * sqrtHalf;
                    side = processCompensation(3, pointIndex, processPointForDomain(naturalPhase, 3, pointIndex, side, wetMix));
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case stereo:
                default:
                    leftSample = processCompensation(0, pointIndex, processPointForDomain(naturalPhase, 0, pointIndex, leftSample, wetMix));
                    rightSample = processCompensation(1, pointIndex, processPointForDomain(naturalPhase, 1, pointIndex, rightSample, wetMix));
                    break;
            }
        }

        naturalPhaseDelayLines[0].pushSample(0, leftSample);
        naturalPhaseDelayLines[1].pushSample(0, rightSample);
        leftData[sample] = naturalPhaseDelayLines[0].popSample(0);
        rightData[sample] = naturalPhaseDelayLines[1].popSample(0);
    }
}

void PluginProcessor::renderDynamicIirOverlay(int modeIndex,
                                              const juce::AudioBuffer<float>& source,
                                              juce::AudioBuffer<float>& destination)
{
    const auto points = getPointsSnapshot();
    auto* leftData = destination.getWritePointer(0);
    auto* rightData = destination.getWritePointer(1);
    juce::ignoreUnused(source);

    for (int sample = 0; sample < destination.getNumSamples(); ++sample)
    {
        float leftSample = leftData[sample];
        float rightSample = rightData[sample];

        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& point = points[(size_t) pointIndex];
            if (! point.enabled || ! point.dynamicEnabled)
                continue;

            updateDynamicBandSetup(pointIndex, blockEffectiveGainDb[(size_t) pointIndex][(size_t) sample]);
            const float wetMix = pointTypeCanBlendDynamically(point.type)
                ? blockActivity[(size_t) pointIndex][(size_t) sample]
                : 1.0f;

            switch (point.stereoMode)
            {
                case left:
                    leftSample = processPointForDomain(modeIndex, 0, pointIndex, leftSample, wetMix);
                    break;
                case right:
                    rightSample = processPointForDomain(modeIndex, 1, pointIndex, rightSample, wetMix);
                    break;
                case mid:
                {
                    float mid = (leftSample + rightSample) * sqrtHalf;
                    const float side = (leftSample - rightSample) * sqrtHalf;
                    mid = processPointForDomain(modeIndex, 2, pointIndex, mid, wetMix);
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case side:
                {
                    const float mid = (leftSample + rightSample) * sqrtHalf;
                    float side = (leftSample - rightSample) * sqrtHalf;
                    side = processPointForDomain(modeIndex, 3, pointIndex, side, wetMix);
                    leftSample = (mid + side) * sqrtHalf;
                    rightSample = (mid - side) * sqrtHalf;
                    break;
                }
                case stereo:
                default:
                    leftSample = processPointForDomain(modeIndex, 0, pointIndex, leftSample, wetMix);
                    rightSample = processPointForDomain(modeIndex, 1, pointIndex, rightSample, wetMix);
                    break;
            }
        }

        leftData[sample] = leftSample;
        rightData[sample] = rightSample;
    }
}

void PluginProcessor::renderLinearPhaseMode(const juce::AudioBuffer<float>& source,
                                            juce::AudioBuffer<float>& destination)
{
    if (linearPhaseConvolvers[0] == nullptr || linearPhaseConvolvers[0]->getCurrentIRSize() <= 0)
    {
        destination.makeCopyOf(source, true);
        return;
    }

    destination.makeCopyOf(source, true);
    linearTempMonoBuffer.setSize(1, source.getNumSamples(), false, false, true);

    auto processMonoConvolver = [&](int convolverIndex, const float* inputData, float* outputData)
    {
        linearTempMonoBuffer.copyFrom(0, 0, inputData, source.getNumSamples());
        auto block = juce::dsp::AudioBlock<float>(linearTempMonoBuffer);
        linearPhaseConvolvers[(size_t) convolverIndex]->process(juce::dsp::ProcessContextReplacing<float>(block));
        juce::FloatVectorOperations::copy(outputData, linearTempMonoBuffer.getReadPointer(0), source.getNumSamples());
    };

    processMonoConvolver(0, source.getReadPointer(0), destination.getWritePointer(0));
    processMonoConvolver(1, source.getReadPointer(1), destination.getWritePointer(1));
    processMonoConvolver(2, destination.getReadPointer(0), destination.getWritePointer(0));
    processMonoConvolver(3, destination.getReadPointer(1), destination.getWritePointer(1));

    auto* midWrite = linearMidBuffer.getWritePointer(0);
    auto* sideWrite = linearSideBuffer.getWritePointer(0);
    for (int sample = 0; sample < source.getNumSamples(); ++sample)
    {
        midWrite[sample] = (destination.getReadPointer(0)[sample] + destination.getReadPointer(1)[sample]) * sqrtHalf;
        sideWrite[sample] = (destination.getReadPointer(0)[sample] - destination.getReadPointer(1)[sample]) * sqrtHalf;
    }

    {
        auto midBlock = juce::dsp::AudioBlock<float>(linearMidBuffer);
        linearPhaseConvolvers[4]->process(juce::dsp::ProcessContextReplacing<float>(midBlock));
        auto sideBlock = juce::dsp::AudioBlock<float>(linearSideBuffer);
        linearPhaseConvolvers[5]->process(juce::dsp::ProcessContextReplacing<float>(sideBlock));
    }

    auto* left = destination.getWritePointer(0);
    auto* right = destination.getWritePointer(1);
    for (int sample = 0; sample < source.getNumSamples(); ++sample)
    {
        const float mid = linearMidBuffer.getReadPointer(0)[sample];
        const float side = linearSideBuffer.getReadPointer(0)[sample];
        left[sample] = (mid + side) * sqrtHalf;
        right[sample] = (mid - side) * sqrtHalf;
    }

    renderDynamicIirOverlay(linearPhase, destination, destination);
}

void PluginProcessor::renderProcessingMode(int processingMode,
                                           const juce::AudioBuffer<float>& source,
                                           juce::AudioBuffer<float>& destination)
{
    switch (processingMode)
    {
        case naturalPhase: renderNaturalPhaseMode(source, destination); break;
        case linearPhase:  renderLinearPhaseMode(source, destination); break;
        case zeroLatency:
        default:           renderZeroLatencyMode(source, destination); break;
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    syncPointCacheFromParameters();
    loadPendingLinearImpulseResponses();

    auto mainBuffer = getBusBuffer(buffer, true, 0);
    auto sidechainBuffer = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<float>();
    const int numSamples = mainBuffer.getNumSamples();
    const int numChannels = juce::jmin(2, mainBuffer.getNumChannels());
    ensureScratchBuffers(numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
        scratchInputBuffer.copyFrom(channel, 0, mainBuffer, channel, 0, numSamples);
    if (numChannels == 1)
        scratchInputBuffer.copyFrom(1, 0, mainBuffer, 0, 0, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        pushPreSpectrumSample(collapseSignalToMono(scratchInputBuffer, sample));
        if (sidechainBuffer.getNumChannels() > 0 && sidechainBuffer.getNumSamples() > sample)
            pushSidechainSpectrumSample(collapseSignalToMono(sidechainBuffer, sample));
    }

    computeDynamicControlSignals(scratchInputBuffer, sidechainBuffer);

    const int requestedMode = getProcessingMode();
    if (requestedMode != activeProcessingMode)
    {
        previousProcessingMode = activeProcessingMode;
        activeProcessingMode = requestedMode;
        processingModeFadeTotalSamples = juce::jmax(1, (int) std::round(currentSampleRate * 0.08));
        processingModeFadeRemainingSamples = processingModeFadeTotalSamples;
        updateLatencyState();
    }

    if (isGlobalBypassed())
    {
        scratchRenderBufferA.makeCopyOf(scratchInputBuffer, true);
    }
    else if (processingModeFadeRemainingSamples > 0)
    {
        renderProcessingMode(previousProcessingMode, scratchInputBuffer, scratchRenderBufferA);
        renderProcessingMode(activeProcessingMode, scratchInputBuffer, scratchRenderBufferB);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const int fadeSamplesUsed = processingModeFadeTotalSamples - processingModeFadeRemainingSamples + sample;
            const float mix = juce::jlimit(0.0f, 1.0f, (float) fadeSamplesUsed / (float) juce::jmax(1, processingModeFadeTotalSamples));
            for (int channel = 0; channel < 2; ++channel)
            {
                const float a = scratchRenderBufferA.getReadPointer(channel)[sample];
                const float b = scratchRenderBufferB.getReadPointer(channel)[sample];
                scratchRenderBufferA.getWritePointer(channel)[sample] = juce::jmap(mix, a, b);
            }
        }

        processingModeFadeRemainingSamples = juce::jmax(0, processingModeFadeRemainingSamples - numSamples);
        if (processingModeFadeRemainingSamples == 0)
            previousProcessingMode = activeProcessingMode;
    }
    else
    {
        renderProcessingMode(activeProcessingMode, scratchInputBuffer, scratchRenderBufferA);
    }

    const float outputGain = juce::Decibels::decibelsToGain(getOutputGainDb());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float leftSample = sidechainListenPointIndex >= 0 ? blockSidechainListen[(size_t) sample]
                                                          : scratchRenderBufferA.getReadPointer(0)[sample] * outputGain;
        float rightSample = sidechainListenPointIndex >= 0 ? blockSidechainListen[(size_t) sample]
                                                           : scratchRenderBufferA.getReadPointer(1)[sample] * outputGain;

        if (soloPointIndex >= 0)
        {
            leftSample = soloLowPassLeft.processSingleSampleRaw(soloHighPassLeft.processSingleSampleRaw(leftSample));
            rightSample = soloLowPassRight.processSingleSampleRaw(soloHighPassRight.processSingleSampleRaw(rightSample));
        }

        if (numChannels > 0)
            mainBuffer.getWritePointer(0)[sample] = leftSample;
        if (numChannels > 1)
            mainBuffer.getWritePointer(1)[sample] = rightSample;

        pushPostSpectrumSample(numChannels > 1 ? 0.5f * (leftSample + rightSample) : leftSample);
    }
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
    {
        juce::XmlElement wrapper(presetStateWrapperTag);
        wrapper.addChildElement(state.release());

        auto* meta = wrapper.createNewChildElement(presetMetaTag);
        meta->setAttribute("currentPresetKey", currentPresetKey);
        meta->setAttribute("currentPresetName", currentPresetName);
        meta->setAttribute("currentPresetCategory", currentPresetCategory);
        meta->setAttribute("currentPresetAuthor", currentPresetAuthor);
        meta->setAttribute("currentPresetIsFactory", currentPresetIsFactory);
        meta->setAttribute("currentPresetDirty", currentPresetDirty.load());
        meta->setAttribute("lastLoadedPresetSnapshot", lastLoadedPresetSnapshot.valid ? serialisePointArrayToJson(lastLoadedPresetSnapshot.points) : juce::String());
        meta->setAttribute("activeCompareSlot", activeCompareSlot);
        for (int slot = 0; slot < (int) compareSlots.size(); ++slot)
        {
            const auto& compare = compareSlots[(size_t) slot];
            meta->setAttribute("compare" + juce::String(slot) + "Snapshot",
                               compare.snapshot.valid ? serialisePointArrayToJson(compare.snapshot.points) : juce::String());
            meta->setAttribute("compare" + juce::String(slot) + "Baseline",
                               compare.baselineSnapshot.valid ? serialisePointArrayToJson(compare.baselineSnapshot.points) : juce::String());
            meta->setAttribute("compare" + juce::String(slot) + "PresetKey", compare.presetKey);
            meta->setAttribute("compare" + juce::String(slot) + "PresetName", compare.presetName);
            meta->setAttribute("compare" + juce::String(slot) + "PresetCategory", compare.presetCategory);
            meta->setAttribute("compare" + juce::String(slot) + "PresetAuthor", compare.presetAuthor);
            meta->setAttribute("compare" + juce::String(slot) + "IsFactory", compare.isFactory);
            meta->setAttribute("compare" + juce::String(slot) + "Dirty", compare.dirty);
        }

        copyXmlToBinary(wrapper, destData);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        const juce::XmlElement* stateXml = nullptr;
        const juce::XmlElement* metaXml = nullptr;

        if (xmlState->hasTagName(presetStateWrapperTag))
        {
            stateXml = xmlState->getChildByName(apvts.state.getType());
            metaXml = xmlState->getChildByName(presetMetaTag);
        }
        else if (xmlState->hasTagName(apvts.state.getType()))
        {
            stateXml = xmlState.get();
        }

        if (stateXml != nullptr)
            apvts.replaceState(juce::ValueTree::fromXml(*stateXml));

        currentPresetKey.clear();
        currentPresetName.clear();
        currentPresetCategory.clear();
        currentPresetAuthor.clear();
        currentPresetIsFactory = false;
        currentPresetDirty.store(false);
        lastLoadedPresetSnapshot = {};
        for (auto& compareSlot : compareSlots)
            compareSlot = {};
        activeCompareSlot = 0;

        if (metaXml != nullptr)
        {
            currentPresetKey = metaXml->getStringAttribute("currentPresetKey");
            currentPresetName = metaXml->getStringAttribute("currentPresetName");
            currentPresetCategory = metaXml->getStringAttribute("currentPresetCategory");
            currentPresetAuthor = metaXml->getStringAttribute("currentPresetAuthor");
            currentPresetIsFactory = metaXml->getBoolAttribute("currentPresetIsFactory", false);
            currentPresetDirty.store(metaXml->getBoolAttribute("currentPresetDirty", false));

            PointArray lastLoadedPoints{};
            if (deserialisePointArrayFromJson(metaXml->getStringAttribute("lastLoadedPresetSnapshot"), lastLoadedPoints))
            {
                lastLoadedPresetSnapshot.points = lastLoadedPoints;
                lastLoadedPresetSnapshot.valid = true;
            }

            activeCompareSlot = juce::jlimit(0, 1, metaXml->getIntAttribute("activeCompareSlot", 0));
            for (int slot = 0; slot < (int) compareSlots.size(); ++slot)
            {
                auto& compare = compareSlots[(size_t) slot];
                PointArray comparePoints{};
                if (deserialisePointArrayFromJson(metaXml->getStringAttribute("compare" + juce::String(slot) + "Snapshot"), comparePoints))
                {
                    compare.snapshot.points = comparePoints;
                    compare.snapshot.valid = true;
                }

                PointArray baselinePoints{};
                if (deserialisePointArrayFromJson(metaXml->getStringAttribute("compare" + juce::String(slot) + "Baseline"), baselinePoints))
                {
                    compare.baselineSnapshot.points = baselinePoints;
                    compare.baselineSnapshot.valid = true;
                }

                compare.presetKey = metaXml->getStringAttribute("compare" + juce::String(slot) + "PresetKey");
                compare.presetName = metaXml->getStringAttribute("compare" + juce::String(slot) + "PresetName");
                compare.presetCategory = metaXml->getStringAttribute("compare" + juce::String(slot) + "PresetCategory");
                compare.presetAuthor = metaXml->getStringAttribute("compare" + juce::String(slot) + "PresetAuthor");
                compare.isFactory = metaXml->getBoolAttribute("compare" + juce::String(slot) + "IsFactory", false);
                compare.dirty = metaXml->getBoolAttribute("compare" + juce::String(slot) + "Dirty", false);
            }
        }
    }

    syncPointCacheFromParameters();
    restorePresetMetadataFromState();
    if (compareSlots[(size_t) activeCompareSlot].snapshot.valid)
        applyCompareSlotState(activeCompareSlot);
    clearABState();
    previewActive = false;
    previewPresetKey.clear();
    previewRestoreSnapshot = {};
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

void PluginProcessor::syncPointCacheFromParameters()
{
    const auto points = readPointsFromState(apvts);
    bool changed = true;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        changed = pointArraysDiffer(pointCache, points);
        pointCache = points;
    }

    if (changed)
    {
        updateVisualPointCache();
        updateSoloMonitoringSetup();
        updateLatencyState();
    }
}

void PluginProcessor::updateVisualPointCache()
{
    PointArray points;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        points = pointCache;
    }

    auto visualPoints = points;
    auto setPoints = points;
    for (int i = 0; i < maxPoints; ++i)
    {
        visualPoints[(size_t) i] = makeActivePointForVisual(points[(size_t) i], dynamicRuntime[(size_t) i].lastEffectiveGainDb);
        setPoints[(size_t) i] = makeSetPointForVisual(points[(size_t) i]);
    }

    const auto coeffs = buildCoefficientArray(visualPoints, currentSampleRate);
    const auto setCoeffs = buildCoefficientArray(setPoints, currentSampleRate);

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        visualPointCache = visualPoints;
        coefficientCache = coeffs;
        setCoefficientCache = setCoeffs;
        for (int i = 0; i < maxPoints; ++i)
        {
            dynamicVisualStates[(size_t) i].enabled = points[(size_t) i].enabled && points[(size_t) i].dynamicEnabled;
            dynamicVisualStates[(size_t) i].maxGainDb = getMaximumDynamicGainDb(points[(size_t) i]);
        }
    }

    updateFilterStates();
    updateNaturalPhaseCompensation();
    scheduleLinearFirRebuild();
}

void PluginProcessor::updateFilterStates()
{
    std::array<PointFilterSetup, maxPoints> coeffs;
    PointArray points;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        points = pointCache;
        coeffs = coefficientCache;
    }

    for (int modeIndex = 0; modeIndex < 3; ++modeIndex)
    {
        for (int domainIndex = 0; domainIndex < 4; ++domainIndex)
        {
            for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
            {
                const auto& point = points[(size_t) pointIndex];
                const auto& setup = coeffs[(size_t) pointIndex];
                const bool allowSetup = modeIndex == linearPhase ? (point.enabled && point.dynamicEnabled) : point.enabled;

                for (int stage = 0; stage < maxStagesPerPoint; ++stage)
                {
                    auto& filter = filters[(size_t) modeIndex][(size_t) domainIndex][(size_t) pointIndex][(size_t) stage];
                    filter.setCoefficients(allowSetup && stage < setup.numStages ? setup.coeffs[(size_t) stage] : makeIdentity());
                }
            }
        }
    }
}

void PluginProcessor::updateDynamicBandSetup(int pointIndex, float effectiveGainDb)
{
    if (! juce::isPositiveAndBelow(pointIndex, maxPoints))
        return;

    EqPoint point;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        point = pointCache[(size_t) pointIndex];
    }

    if (! point.enabled || ! point.dynamicEnabled)
        return;

    const auto activePoint = makeActivePointForVisual(point, effectiveGainDb);
    const auto setup = buildPointSetup(activePoint, currentSampleRate);

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        visualPointCache[(size_t) pointIndex] = activePoint;
        coefficientCache[(size_t) pointIndex] = setup;
        dynamicVisualStates[(size_t) pointIndex].effectiveGainDb = effectiveGainDb;
        dynamicVisualStates[(size_t) pointIndex].activity = dynamicRuntime[(size_t) pointIndex].lastActivity;
    }

    for (int domainIndex = 0; domainIndex < 4; ++domainIndex)
    {
        for (int stage = 0; stage < maxStagesPerPoint; ++stage)
        {
            const auto coeffs = stage < setup.numStages ? setup.coeffs[(size_t) stage] : makeIdentity();
            filters[(size_t) zeroLatency][(size_t) domainIndex][(size_t) pointIndex][(size_t) stage].setCoefficients(coeffs);
            filters[(size_t) naturalPhase][(size_t) domainIndex][(size_t) pointIndex][(size_t) stage].setCoefficients(coeffs);
            filters[(size_t) linearPhase][(size_t) domainIndex][(size_t) pointIndex][(size_t) stage].setCoefficients(coeffs);
        }
    }
}

void PluginProcessor::updateNaturalPhaseCompensation()
{
    PointArray points;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        points = pointCache;
    }

    int activeCount = 0;
    for (int domainIndex = 0; domainIndex < 4; ++domainIndex)
    {
        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& point = points[(size_t) pointIndex];
            if (domainIndex == 0 && point.enabled)
                ++activeCount;

            const auto compensationCoefficients = point.enabled
                ? juce::IIRCoefficients::makeAllPass(juce::jmax(1.0, currentSampleRate),
                                                     juce::jlimit(minFrequency, maxFrequency, point.frequency),
                                                     juce::jlimit(0.3f, 12.0f, point.q * 0.85f))
                : makeIdentity();

            for (int stage = 0; stage < maxStagesPerPoint; ++stage)
                naturalPhaseCompensationFilters[(size_t) domainIndex][(size_t) pointIndex][(size_t) stage]
                    .setCoefficients(stage == 0 ? compensationCoefficients : makeIdentity());
        }
    }

    naturalPhaseLatencySamples = activeCount > 12 ? 512 : (activeCount > 0 ? 256 : 0);
    naturalPhaseDelayLines[0].setDelay((float) naturalPhaseLatencySamples);
    naturalPhaseDelayLines[1].setDelay((float) naturalPhaseLatencySamples);
}

void PluginProcessor::updateLatencyState()
{
    int latencySamples = 0;
    switch (getProcessingMode())
    {
        case naturalPhase:
            latencySamples = naturalPhaseLatencySamples;
            break;
        case linearPhase:
            latencySamples = linearPhaseGroupDelaySamples;
            break;
        case zeroLatency:
        default:
            latencySamples = 0;
            break;
    }

    currentLatencySamples.store(latencySamples);
    setLatencySamples(latencySamples);
}

void PluginProcessor::scheduleLinearFirRebuild()
{
    PointArray points;
    {
        const juce::SpinLock::ScopedLockType stateGuard(stateLock);
        points = pointCache;
    }

    {
        const std::lock_guard<std::mutex> lock(linearIrMutex);
        linearIrRequestedPoints = points;
        linearIrDirty = true;
    }
    linearIrCondition.notify_one();
}

void PluginProcessor::linearFirWorkerLoop()
{
    auto buildImpulseResponseForMode = [](const PointArray& points, double sampleRate, int stereoMode, int kernelSize)
    {
        juce::AudioBuffer<float> buffer(1, kernelSize);
        buffer.clear();

        auto responseForFrequency = [&](double frequency)
        {
            double magnitude = 1.0;
            for (const auto& point : points)
            {
                if (! point.enabled || point.dynamicEnabled || point.stereoMode != stereoMode)
                    continue;

                const auto setup = buildPointSetup(makeSetPointForVisual(point), sampleRate);
                for (int stage = 0; stage < setup.numStages; ++stage)
                    magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, sampleRate);
            }
            return magnitude;
        };

        const int order = juce::roundToInt(std::log2((double) kernelSize));
        juce::dsp::FFT localFft(order);
        std::vector<juce::dsp::Complex<float>> spectrum((size_t) kernelSize);
        std::vector<juce::dsp::Complex<float>> impulse((size_t) kernelSize);

        for (int bin = 0; bin <= kernelSize / 2; ++bin)
        {
            const double rawFrequency = (double) bin * sampleRate / (double) kernelSize;
            const double safeFrequency = juce::jlimit(1.0, sampleRate * 0.5 - 1.0, rawFrequency);
            const float magnitude = (float) responseForFrequency(safeFrequency);
            spectrum[(size_t) bin] = { magnitude, 0.0f };
            if (bin > 0 && bin < kernelSize / 2)
                spectrum[(size_t) (kernelSize - bin)] = { magnitude, 0.0f };
        }

        localFft.perform(spectrum.data(), impulse.data(), true);
        auto* write = buffer.getWritePointer(0);
        for (int sample = 0; sample < kernelSize; ++sample)
        {
            const int shifted = (sample + kernelSize / 2) % kernelSize;
            // JUCE's inverse FFT already returns scaled time-domain data, so dividing again
            // collapses the IR toward silence and makes the linear-phase path effectively mute.
            write[sample] = impulse[(size_t) shifted].real();
        }

        return buffer;
    };

    while (true)
    {
        PointArray points;
        double sampleRate = 44100.0;

        {
            std::unique_lock<std::mutex> lock(linearIrMutex);
            linearIrCondition.wait(lock, [this] { return linearIrDirty || linearIrExit; });
            if (linearIrExit)
                break;

            linearIrDirty = false;
            points = linearIrRequestedPoints;
            sampleRate = currentSampleRate;
        }

        LinearImpulseSet builtSet;
        builtSet.kernelSize = sampleRate <= 48000.0 ? 4096 : 8192;
        builtSet.groupDelaySamples = builtSet.kernelSize / 2;
        builtSet.responses[0] = buildImpulseResponseForMode(points, sampleRate, stereo, builtSet.kernelSize);
        builtSet.responses[1].makeCopyOf(builtSet.responses[0], true);
        builtSet.responses[2] = buildImpulseResponseForMode(points, sampleRate, left, builtSet.kernelSize);
        builtSet.responses[3] = buildImpulseResponseForMode(points, sampleRate, right, builtSet.kernelSize);
        builtSet.responses[4] = buildImpulseResponseForMode(points, sampleRate, mid, builtSet.kernelSize);
        builtSet.responses[5] = buildImpulseResponseForMode(points, sampleRate, side, builtSet.kernelSize);
        builtSet.valid = true;

        {
            const std::lock_guard<std::mutex> lock(linearIrMutex);
            linearIrReadySet = std::move(builtSet);
            linearIrReady = true;
        }
    }
}

void PluginProcessor::loadPendingLinearImpulseResponses()
{
    LinearImpulseSet readySet;
    {
        const std::lock_guard<std::mutex> lock(linearIrMutex);
        if (! linearIrReady || ! linearIrReadySet.valid)
            return;

        readySet = std::move(linearIrReadySet);
        linearIrReady = false;
        linearIrReadySet = {};
    }

    for (int i = 0; i < (int) linearPhaseConvolvers.size(); ++i)
        linearPhaseConvolvers[(size_t) i]->loadImpulseResponse(std::move(readySet.responses[(size_t) i]),
                                                               currentSampleRate,
                                                               juce::dsp::Convolution::Stereo::no,
                                                               juce::dsp::Convolution::Trim::no,
                                                               juce::dsp::Convolution::Normalise::no);

    linearPhaseKernelSize = readySet.kernelSize;
    linearPhaseGroupDelaySamples = readySet.groupDelaySamples;
    updateLatencyState();
}

void PluginProcessor::resetDynamicState()
{
    for (auto& runtime : dynamicRuntime)
    {
        runtime.detectorPrimary.reset();
        runtime.detectorHighPass.reset();
        runtime.detectorLowPass.reset();
        runtime.smoothedPower = 0.0f;
        runtime.lastEffectiveGainDb = 0.0f;
        runtime.lastActivity = 0.0f;
        runtime.lastMonitorSample = 0.0f;
    }
}

void PluginProcessor::pushSpectrumSample(float) noexcept {}

void PluginProcessor::pushPreSpectrumSample(float sample) noexcept
{
    preSpectrumFifo[(size_t) preSpectrumFifoIndex++] = sample;
    if (preSpectrumFifoIndex >= fftSize)
    {
        performSpectrumAnalysis(preSpectrumFifo, analyzerFrame.pre, getAnalyzerDecay());
        preSpectrumFifoIndex = 0;
    }
}

void PluginProcessor::pushPostSpectrumSample(float sample) noexcept
{
    postSpectrumFifo[(size_t) postSpectrumFifoIndex++] = sample;
    if (postSpectrumFifoIndex >= fftSize)
    {
        performSpectrumAnalysis(postSpectrumFifo, analyzerFrame.post, getAnalyzerDecay());
        postSpectrumFifoIndex = 0;
    }
}

void PluginProcessor::pushSidechainSpectrumSample(float sample) noexcept
{
    sidechainSpectrumFifo[(size_t) sidechainSpectrumFifoIndex++] = sample;
    if (sidechainSpectrumFifoIndex >= fftSize)
    {
        performSpectrumAnalysis(sidechainSpectrumFifo, analyzerFrame.sidechain, getAnalyzerDecay());
        sidechainSpectrumFifoIndex = 0;
    }
}

void PluginProcessor::performSpectrumAnalysis(const std::array<float, fftSize>& source, SpectrumArray& destination, float decayAmount)
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::copy(source.begin(), source.end(), fftData.begin());

    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    SpectrumArray latestSpectrum;
    latestSpectrum.fill(-96.0f);

    const int resolution = getAnalyzerResolution();
    const int binStride = resolution == 0 ? 4 : (resolution == 1 ? 2 : 1);

    for (int i = 1; i < fftSize / 2; i += binStride)
    {
        float accumulated = 0.0f;
        int samples = 0;
        for (int bin = i; bin < juce::jmin(i + binStride, fftSize / 2); ++bin)
        {
            accumulated += fftData[(size_t) bin];
            ++samples;
        }

        const float level = juce::jmax((accumulated / (float) juce::jmax(1, samples)) / (float) fftSize, 1.0e-5f);
        const float db = juce::Decibels::gainToDecibels(level, -96.0f);
        for (int bin = i; bin < juce::jmin(i + binStride, fftSize / 2); ++bin)
            latestSpectrum[(size_t) bin] = db;
    }

    const juce::SpinLock::ScopedLockType lock(stateLock);
    for (size_t i = 0; i < destination.size(); ++i)
        destination[i] = juce::jmax(latestSpectrum[i], juce::jmap(decayAmount, -96.0f, destination[i]));
}
