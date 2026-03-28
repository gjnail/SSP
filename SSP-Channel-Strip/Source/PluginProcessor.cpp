#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGainDb = -24.0f;
constexpr float maxGainDb = 24.0f;
constexpr float sqrtHalf = 0.70710678f;
constexpr float meterFloorDb = -60.0f;

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
        param->beginChangeGesture();
        param->setValueNotifyingHost(normalised);
        param->endChangeGesture();
    }
}

void setFloatParamFast(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(value));
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

float gainToDb(float gain)
{
    return juce::Decibels::gainToDecibels(gain + 1.0e-6f, meterFloorDb);
}

float dbToGain(float db)
{
    return juce::Decibels::decibelsToGain(db);
}

float msToCoeff(float ms, double sampleRate)
{
    return std::exp(-1.0f / juce::jmax(1.0f, (float) sampleRate * juce::jmax(0.0001f, ms * 0.001f)));
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
            pushStage(makeTiltLowShelf(sampleRate, frequency, q, gainDb));
            pushStage(makeTiltHighShelf(sampleRate, frequency, q, gainDb));
            break;
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

PluginProcessor::ModuleOrder sanitiseModuleOrder(PluginProcessor::ModuleOrder order)
{
    PluginProcessor::ModuleOrder result{{PluginProcessor::moduleGate, PluginProcessor::moduleEq,
                                         PluginProcessor::moduleCompressor, PluginProcessor::moduleSaturation}};
    std::array<bool, PluginProcessor::moduleCount> seen{};
    int writeIndex = 0;

    for (int value : order)
    {
        const bool valid = value >= PluginProcessor::moduleGate && value <= PluginProcessor::moduleSaturation;
        if (!valid || seen[(size_t) value])
            continue;

        seen[(size_t) value] = true;
        result[(size_t) writeIndex++] = value;
    }

    for (int fallback : {PluginProcessor::moduleGate, PluginProcessor::moduleEq, PluginProcessor::moduleCompressor, PluginProcessor::moduleSaturation})
    {
        if (!seen[(size_t) fallback] && writeIndex < PluginProcessor::reorderableModuleCount)
            result[(size_t) writeIndex++] = fallback;
    }

    return result;
}

PluginProcessor::ModuleOrder buildSslModuleOrder(juce::AudioProcessorValueTreeState& apvts)
{
    const bool eqPreDynamics = getRawBoolParam(apvts, "eqPreDynamics");
    const bool compFirst = getRawBoolParam(apvts, "compGateOrder");
    const int satPosition = getChoiceIndex(apvts, "satPosition");

    std::vector<int> order;
    auto appendDynamics = [&]()
    {
        if (compFirst)
        {
            order.push_back(PluginProcessor::moduleCompressor);
            order.push_back(PluginProcessor::moduleGate);
        }
        else
        {
            order.push_back(PluginProcessor::moduleGate);
            order.push_back(PluginProcessor::moduleCompressor);
        }
    };

    if (eqPreDynamics)
    {
        if (satPosition == 0)
            order.push_back(PluginProcessor::moduleSaturation);

        order.push_back(PluginProcessor::moduleEq);

        if (satPosition == 1)
            order.push_back(PluginProcessor::moduleSaturation);

        appendDynamics();

        if (satPosition == 2)
            order.push_back(PluginProcessor::moduleSaturation);
    }
    else
    {
        if (satPosition == 0)
            order.push_back(PluginProcessor::moduleSaturation);

        appendDynamics();

        if (satPosition == 2)
            order.push_back(PluginProcessor::moduleSaturation);

        order.push_back(PluginProcessor::moduleEq);

        if (satPosition == 1)
            order.push_back(PluginProcessor::moduleSaturation);
    }

    PluginProcessor::ModuleOrder result{{PluginProcessor::moduleGate, PluginProcessor::moduleCompressor,
                                         PluginProcessor::moduleEq, PluginProcessor::moduleSaturation}};
    for (int i = 0; i < PluginProcessor::reorderableModuleCount && i < (int) order.size(); ++i)
        result[(size_t) i] = order[(size_t) i];

    return sanitiseModuleOrder(result);
}

enum class GateMode
{
    gate = 0,
    expander
};

enum class CompressorCharacter
{
    clean = 0,
    punch,
    glue
};

enum class SaturationCharacter
{
    warm = 0,
    edge,
    crisp
};

float shapeSaturationSample(float sample, float color, float asymmetry, SaturationCharacter character) noexcept
{
    switch (character)
    {
        case SaturationCharacter::warm:
        {
            const float even = std::tanh(sample + asymmetry);
            const float tape = std::atan((sample + asymmetry * 0.55f) * juce::jmap(color, 0.0f, 1.0f, 1.0f, 2.6f)) / 1.2f;
            return juce::jlimit(-1.2f, 1.2f, juce::jmap(0.55f + color * 0.25f, even, tape));
        }

        case SaturationCharacter::edge:
        {
            const float odd = sample - 0.15f * sample * sample * sample;
            const float tube = std::tanh((sample + asymmetry * 1.2f) * juce::jmap(color, 0.0f, 1.0f, 1.2f, 2.4f));
            return juce::jlimit(-1.25f, 1.25f, juce::jmap(0.35f + color * 0.4f, odd, tube));
        }

        case SaturationCharacter::crisp:
        default:
        {
            const float clipped = juce::jlimit(-1.0f, 1.0f, (sample + asymmetry * 0.35f) * juce::jmap(color, 0.0f, 1.0f, 1.4f, 3.0f));
            const float transistor = (sample + asymmetry * 0.2f) / (1.0f + std::abs(sample + asymmetry * 0.2f) * juce::jmap(color, 0.0f, 1.0f, 0.6f, 2.4f));
            return juce::jlimit(-1.15f, 1.15f, juce::jmap(0.4f + color * 0.35f, transistor, clipped));
        }
    }
}

float applyCompressorCurve(float detectorDb, float thresholdDb, float ratio, float kneeDb)
{
    const float overshoot = detectorDb - thresholdDb;

    if (kneeDb > 0.0f && std::abs(overshoot) <= kneeDb * 0.5f)
    {
        const float kneeStart = thresholdDb - kneeDb * 0.5f;
        const float delta = detectorDb - kneeStart;
        const float compression = (1.0f / ratio - 1.0f) * delta * delta / (2.0f * kneeDb);
        return -compression;
    }

    if (overshoot <= 0.0f)
        return 0.0f;

    const float compressedDb = overshoot / juce::jmax(1.0f, ratio);
    return -(overshoot - compressedDb);
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
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>("processingMode", "processingMode", getProcessingModeNames(), zeroLatency));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("analyzerMode", "analyzerMode", getAnalyzerModeNames(), analyzerPrePost));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("analyzerResolution", "analyzerResolution", getAnalyzerResolutionNames(), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("analyzerDecay", "analyzerDecay", juce::NormalisableRange<float>(0.05f, 0.98f, 0.01f), 0.72f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("globalBypass", "globalBypass", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("inputBypass", "inputBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputTrim", "inputTrim", juce::NormalisableRange<float>(-20.0f, 20.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("inputPhase", "inputPhase", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("inputHpfOn", "inputHpfOn", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputHpfFreq", "inputHpfFreq", juce::NormalisableRange<float>(20.0f, 600.0f, 0.01f, 0.25f), 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("inputLpfOn", "inputLpfOn", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputLpfFreq", "inputLpfFreq", juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.01f, 0.2f), 18000.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("dynamicsBypass", "dynamicsBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("gateBypass", "gateBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateThreshold", "gateThreshold", juce::NormalisableRange<float>(-72.0f, 0.0f, 0.01f), -42.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateRange", "gateRange", juce::NormalisableRange<float>(0.0f, 60.0f, 0.01f), 24.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateAttack", "gateAttack", juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f, 0.3f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateHold", "gateHold", juce::NormalisableRange<float>(0.0f, 300.0f, 0.01f, 0.35f), 32.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateRelease", "gateRelease", juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f, 0.3f), 110.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("gateMode", "gateMode", getGateModeNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateScHpFreq", "gateScHpFreq", juce::NormalisableRange<float>(20.0f, 2000.0f, 0.01f, 0.25f), 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateScLpFreq", "gateScLpFreq", juce::NormalisableRange<float>(200.0f, 20000.0f, 0.01f, 0.25f), 12000.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("gateScListen", "gateScListen", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("gateKeyInput", "gateKeyInput", juce::StringArray{"Internal", "External"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>("eqBypass", "eqBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("eqMode", "eqMode", juce::StringArray{"Modern", "Classic"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("eqPreDynamics", "eqPreDynamics", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("compBypass", "compBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compThreshold", "compThreshold", juce::NormalisableRange<float>(-30.0f, 10.0f, 0.01f), -18.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compRatio", "compRatio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.35f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compAttack", "compAttack", juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f, 0.3f), 15.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("compAttackMode", "compAttackMode", juce::StringArray{"Fast", "Slow"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compRelease", "compRelease", juce::NormalisableRange<float>(100.0f, 4000.0f, 0.01f, 0.35f), 250.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("compAutoRelease", "compAutoRelease", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compMakeup", "compMakeup", juce::NormalisableRange<float>(-12.0f, 18.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compKnee", "compKnee", juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("compCharacter", "compCharacter", getCompressorCharacterNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compScHpFreq", "compScHpFreq", juce::NormalisableRange<float>(20.0f, 2000.0f, 0.01f, 0.25f), 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compScLpFreq", "compScLpFreq", juce::NormalisableRange<float>(200.0f, 20000.0f, 0.01f, 0.25f), 18000.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("compScListen", "compScListen", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("compMix", "compMix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("compGateOrder", "compGateOrder", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("dynamicsStereoLink", "dynamicsStereoLink", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>("satBypass", "satBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satDrive", "satDrive", juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("satCharacter", "satCharacter", getSaturationCharacterNames(), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satMix", "satMix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 65.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("satOutput", "satOutput", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("satPosition", "satPosition", juce::StringArray{"Pre EQ", "Post EQ", "Post Dyn"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterBool>("outputBypass", "outputBypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "outputGain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputWidth", "outputWidth", juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputBalance", "outputBalance", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("outputLimiter", "outputLimiter", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputCeiling", "outputCeiling", juce::NormalisableRange<float>(-12.0f, 0.0f, 0.01f), -0.1f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("outputMeterMode", "outputMeterMode", juce::StringArray{"Peak", "VU"}, 0));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    analyzerFrame.pre.fill(-96.0f);
    analyzerFrame.post.fill(-96.0f);
    analyzerFrame.sidechain.fill(-96.0f);

    for (auto& enabled : compareEnabled)
        enabled.store(false);

    syncPointCacheFromParameters();
}

PluginProcessor::~PluginProcessor() = default;

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

const juce::StringArray& PluginProcessor::getModuleNames()
{
    static const juce::StringArray names{"Input", "Gate", "EQ", "Compressor", "Saturation", "Output"};
    return names;
}

const juce::StringArray& PluginProcessor::getGateModeNames()
{
    static const juce::StringArray names{"Gate", "Expander"};
    return names;
}

const juce::StringArray& PluginProcessor::getCompressorCharacterNames()
{
    static const juce::StringArray names{"Clean", "Punch", "Glue"};
    return names;
}

const juce::StringArray& PluginProcessor::getSaturationCharacterNames()
{
    static const juce::StringArray names{"Warm", "Edge", "Crisp"};
    return names;
}

const juce::StringArray& PluginProcessor::getFactoryPresetNames()
{
    static const juce::StringArray names{
        "Vocals / Male Vocal Chain",
        "Vocals / Female Vocal Chain",
        "Vocals / Vocal Bus",
        "Vocals / Podcast Voice",
        "Vocals / Vocal Aggressive",
        "Vocals / Vocal Intimate",
        "Drums / Kick Inside",
        "Drums / Snare Top",
        "Drums / Overheads",
        "Drums / Drum Bus",
        "Bass / Bass DI",
        "Bass / Bass Amp",
        "Bass / Sub Bass",
        "Guitars / Acoustic Strumming",
        "Guitars / Electric Clean",
        "Guitars / Electric Crunch",
        "Keys / Piano Bright",
        "Keys / Rhodes",
        "Keys / Synth Pad",
        "Mix / Mix Bus Glue",
        "Mix / Mix Bus Punch",
        "Mix / Mastering Gentle",
        "Utility / Clean Pass",
        "Utility / Gain Stage Only",
        "Utility / HPF + Compression Only"
    };
    return names;
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

    setBoolParam(apvts, pointParamId(index, "Enabled"), point.enabled);
    setFloatParam(apvts, pointParamId(index, "Freq"), juce::jlimit(minFrequency, maxFrequency, point.frequency));
    setFloatParam(apvts, pointParamId(index, "Gain"), juce::jlimit(minGainDb, maxGainDb, point.gainDb));
    setFloatParam(apvts, pointParamId(index, "Q"), juce::jlimit(0.2f, 18.0f, point.q));
    setChoiceParam(apvts, pointParamId(index, "Type"), juce::jlimit(0, getPointTypeNames().size() - 1, point.type));
    setChoiceParam(apvts, pointParamId(index, "Slope"), juce::jlimit(0, getSlopeNames().size() - 1, point.slopeIndex));
    setChoiceParam(apvts, pointParamId(index, "StereoMode"), juce::jlimit(0, getStereoModeNames().size() - 1, point.stereoMode));
    syncPointCacheFromParameters();
}

void PluginProcessor::setPointPosition(int index, float frequency, float gainDb)
{
    if (!juce::isPositiveAndBelow(index, maxPoints))
        return;

    setFloatParamFast(apvts, pointParamId(index, "Freq"), juce::jlimit(minFrequency, maxFrequency, frequency));
    setFloatParamFast(apvts, pointParamId(index, "Gain"), juce::jlimit(minGainDb, maxGainDb, gainDb));
    syncPointCacheFromParameters();
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
        for (int stage = 0; stage < pointSetup.numStages; ++stage)
            magnitude *= getMagnitudeForFrequency(pointSetup.coeffs[(size_t) stage], frequency, currentSampleRate);

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
}

float PluginProcessor::getResponseForFrequencyByStereoMode(float frequency, int stereoMode) const
{
    double magnitude = 1.0;
    PointArray points;
    std::array<PointFilterSetup, maxPoints> coeffs;

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        points = pointCache;
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
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        setup = coefficientCache[(size_t) index];
    }

    double magnitude = 1.0;
    for (int stage = 0; stage < setup.numStages; ++stage)
        magnitude *= getMagnitudeForFrequency(setup.coeffs[(size_t) stage], frequency, currentSampleRate);

    return juce::Decibels::gainToDecibels((float) magnitude, -48.0f);
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

PluginProcessor::StageMeterSnapshot PluginProcessor::getStageMeterSnapshot(int moduleIndex) const
{
    StageMeterSnapshot snapshot;
    if (!juce::isPositiveAndBelow(moduleIndex, moduleCount))
        return snapshot;

    const juce::SpinLock::ScopedLockType lock(meterLock);
    snapshot.peakDb = stagePeakDb[(size_t) moduleIndex];
    snapshot.rmsDb = stageRmsDb[(size_t) moduleIndex];
    snapshot.auxDb = stageAuxDb[(size_t) moduleIndex];
    snapshot.clipped = stageClipLatched[(size_t) moduleIndex];
    return snapshot;
}

void PluginProcessor::clearClipLatch(int moduleIndex)
{
    if (!juce::isPositiveAndBelow(moduleIndex, moduleCount))
        return;

    const juce::SpinLock::ScopedLockType lock(meterLock);
    stageClipLatched[(size_t) moduleIndex] = false;
}

PluginProcessor::ModuleOrder PluginProcessor::getModuleOrder() const
{
    return buildSslModuleOrder(const_cast<juce::AudioProcessorValueTreeState&>(apvts));
}

void PluginProcessor::setModuleOrder(const ModuleOrder& newOrder)
{
    const auto sanitised = sanitiseModuleOrder(newOrder);

    int eqIndex = reorderableModuleCount;
    int gateIndex = reorderableModuleCount;
    int compIndex = reorderableModuleCount;
    int satIndex = reorderableModuleCount;

    for (int i = 0; i < reorderableModuleCount; ++i)
    {
        const int module = sanitised[(size_t) i];
        if (module == moduleEq) eqIndex = i;
        else if (module == moduleGate) gateIndex = i;
        else if (module == moduleCompressor) compIndex = i;
        else if (module == moduleSaturation) satIndex = i;
    }

    setBoolParam(apvts, "eqPreDynamics", eqIndex < juce::jmin(gateIndex, compIndex));
    setBoolParam(apvts, "compGateOrder", compIndex < gateIndex);

    int satPosition = 1;
    if (satIndex < eqIndex)
        satPosition = 0;
    else if (satIndex < juce::jmax(gateIndex, compIndex))
        satPosition = 1;
    else
        satPosition = 2;

    setChoiceParam(apvts, "satPosition", satPosition);

    const juce::SpinLock::ScopedLockType lock(stateLock);
    moduleOrder = sanitised;
}

int PluginProcessor::getSoloModule() const noexcept
{
    return soloModule.load();
}

void PluginProcessor::setSoloModule(int moduleIndex) noexcept
{
    soloModule.store((moduleIndex >= 0 && moduleIndex < moduleCount) ? moduleIndex : -1);
}

bool PluginProcessor::isModuleCompareEnabled(int moduleIndex) const noexcept
{
    return juce::isPositiveAndBelow(moduleIndex, moduleCount) ? compareEnabled[(size_t) moduleIndex].load() : false;
}

void PluginProcessor::setModuleCompareEnabled(int moduleIndex, bool enabled) noexcept
{
    if (juce::isPositiveAndBelow(moduleIndex, moduleCount))
        compareEnabled[(size_t) moduleIndex].store(enabled);
}

float PluginProcessor::getModuleGainDeltaDb(int moduleIndex) const
{
    if (!juce::isPositiveAndBelow(moduleIndex, moduleCount))
        return 0.0f;

    const juce::SpinLock::ScopedLockType lock(meterLock);
    return moduleGainDeltaDb[(size_t) moduleIndex];
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    syncPointCacheFromParameters();

    for (auto& domainFilters : filters)
        for (auto& pointFilters : domainFilters)
            for (auto& filter : pointFilters)
                filter.reset();

    for (auto& filter : inputHighPassFilters)
        filter.reset();
    for (auto& filter : inputLowPassFilters)
        filter.reset();

    gateSidechainHighPass.reset();
    gateSidechainLowPass.reset();
    compSidechainHighPass.reset();
    compSidechainLowPass.reset();

    satToneState = {0.0f, 0.0f};
    satDcInputState = {0.0f, 0.0f};
    satDcOutputState = {0.0f, 0.0f};
    gateEnvelope = 0.0f;
    gateGainState = 1.0f;
    gateHoldSamplesRemaining = 0;
    compEnvelope = 0.0f;
    compGainReductionDb = 0.0f;

    inputTrimSmoothed.reset(sampleRate, 0.03);
    outputTrimSmoothed.reset(sampleRate, 0.03);
    outputWidthSmoothed.reset(sampleRate, 0.03);
    outputBalanceSmoothed.reset(sampleRate, 0.03);
    gateRangeSmoothed.reset(sampleRate, 0.03);
    compMakeupSmoothed.reset(sampleRate, 0.03);
    compMixSmoothed.reset(sampleRate, 0.03);
    satDriveSmoothed.reset(sampleRate, 0.03);
    satMixSmoothed.reset(sampleRate, 0.03);
    satOutputSmoothed.reset(sampleRate, 0.03);

    inputTrimSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "inputTrim"));
    outputTrimSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "outputGain"));
    outputWidthSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "outputWidth"));
    outputBalanceSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "outputBalance"));
    gateRangeSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "gateRange"));
    compMakeupSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "compMakeup"));
    compMixSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "compMix") * 0.01f);
    satDriveSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "satDrive"));
    satMixSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "satMix") * 0.01f);
    satOutputSmoothed.setCurrentAndTargetValue(getRawFloatParam(apvts, "satOutput"));

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
    updateFilterStates();
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

float PluginProcessor::processPointForDomain(int domainIndex, int pointIndex, float sample)
{
    PointFilterSetup setup;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        setup = coefficientCache[(size_t) pointIndex];
    }

    float value = sample;
    for (int stage = 0; stage < setup.numStages; ++stage)
        value = filters[(size_t) domainIndex][(size_t) pointIndex][(size_t) stage].processSingleSampleRaw(value);
    return value;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    syncPointCacheFromParameters();

    auto mainBuffer = getBusBuffer(buffer, true, 0);
    auto sidechainBuffer = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<float>();
    const auto points = getPointsSnapshot();
    const auto order = getModuleOrder();

    const bool globalBypassed = isGlobalBypassed();
    const bool inputBypass = getRawBoolParam(apvts, "inputBypass");
    const bool dynamicsBypass = getRawBoolParam(apvts, "dynamicsBypass");
    const bool gateBypass = getRawBoolParam(apvts, "gateBypass");
    const bool eqBypass = getRawBoolParam(apvts, "eqBypass");
    const bool compBypass = getRawBoolParam(apvts, "compBypass");
    const bool satBypass = getRawBoolParam(apvts, "satBypass");
    const bool outputBypass = getRawBoolParam(apvts, "outputBypass");
    const bool inputPhase = getRawBoolParam(apvts, "inputPhase");
    const bool inputHpfOn = getRawBoolParam(apvts, "inputHpfOn");
    const bool inputLpfOn = getRawBoolParam(apvts, "inputLpfOn");
    const bool gateListen = getRawBoolParam(apvts, "gateScListen");
    const bool compListen = getRawBoolParam(apvts, "compScListen");
    const bool limiterEnabled = getRawBoolParam(apvts, "outputLimiter");
    const bool gateUsesExternalKey = getChoiceIndex(apvts, "gateKeyInput") == 1;
    const bool compAutoRelease = getRawBoolParam(apvts, "compAutoRelease");
    const bool compFastMode = getChoiceIndex(apvts, "compAttackMode") == 0;

    const int numSamples = mainBuffer.getNumSamples();
    const int numChannels = juce::jmin(2, mainBuffer.getNumChannels());
    if (numSamples == 0 || numChannels < 2)
        return;

    inputTrimSmoothed.setTargetValue(getRawFloatParam(apvts, "inputTrim"));
    outputTrimSmoothed.setTargetValue(getRawFloatParam(apvts, "outputGain"));
    outputWidthSmoothed.setTargetValue(getRawFloatParam(apvts, "outputWidth"));
    outputBalanceSmoothed.setTargetValue(getRawFloatParam(apvts, "outputBalance"));
    gateRangeSmoothed.setTargetValue(getRawFloatParam(apvts, "gateRange"));
    compMakeupSmoothed.setTargetValue(getRawFloatParam(apvts, "compMakeup"));
    compMixSmoothed.setTargetValue(getRawFloatParam(apvts, "compMix") * 0.01f);
    satDriveSmoothed.setTargetValue(getRawFloatParam(apvts, "satDrive"));
    satMixSmoothed.setTargetValue(getRawFloatParam(apvts, "satMix") * 0.01f);
    satOutputSmoothed.setTargetValue(getRawFloatParam(apvts, "satOutput"));

    const float inputHpfFreq = getRawFloatParam(apvts, "inputHpfFreq");
    const float inputLpfFreq = getRawFloatParam(apvts, "inputLpfFreq");
    const float gateThresholdDb = getRawFloatParam(apvts, "gateThreshold");
    const float gateAttackMs = getRawFloatParam(apvts, "gateAttack");
    const float gateHoldMs = getRawFloatParam(apvts, "gateHold");
    const float gateReleaseMs = getRawFloatParam(apvts, "gateRelease");
    const float gateScHpFreq = getRawFloatParam(apvts, "gateScHpFreq");
    const float gateScLpFreq = getRawFloatParam(apvts, "gateScLpFreq");
    const auto gateMode = (GateMode) juce::jlimit(0, 1, getChoiceIndex(apvts, "gateMode"));

    const float compThresholdDb = getRawFloatParam(apvts, "compThreshold");
    const float compRatio = getRawFloatParam(apvts, "compRatio");
    const float compAttackBase = getRawFloatParam(apvts, "compAttack");
    const float compReleaseBase = getRawFloatParam(apvts, "compRelease");
    const float compKneeBase = getRawFloatParam(apvts, "compKnee");
    const float compScHpFreq = getRawFloatParam(apvts, "compScHpFreq");
    const float compScLpFreq = getRawFloatParam(apvts, "compScLpFreq");
    const auto compCharacter = (CompressorCharacter) juce::jlimit(0, 2, getChoiceIndex(apvts, "compCharacter"));

    const auto satCharacter = (SaturationCharacter) juce::jlimit(0, 2, getChoiceIndex(apvts, "satCharacter"));
    const float ceilingGain = dbToGain(getRawFloatParam(apvts, "outputCeiling"));

    for (auto& filter : inputHighPassFilters)
        filter.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, juce::jlimit(20.0f, 600.0f, inputHpfFreq), sqrtHalf));
    for (auto& filter : inputLowPassFilters)
        filter.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, juce::jlimit(1000.0f, 20000.0f, inputLpfFreq), sqrtHalf));

    gateSidechainHighPass.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, juce::jlimit(20.0f, 2000.0f, gateScHpFreq), sqrtHalf));
    gateSidechainLowPass.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, juce::jlimit(200.0f, 20000.0f, gateScLpFreq), sqrtHalf));
    compSidechainHighPass.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, juce::jlimit(20.0f, 2000.0f, compScHpFreq), sqrtHalf));
    compSidechainLowPass.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, juce::jlimit(200.0f, 20000.0f, compScLpFreq), sqrtHalf));

    const int solo = soloModule.load();
    auto moduleAllowedBySolo = [solo](int module)
    {
        if (solo < 0)
            return true;
        if (module == moduleInput || module == moduleOutput)
            return true;
        return module == solo;
    };

    auto stageCompareGain = [this](int moduleIndex)
    {
        return dbToGain(getModuleGainDeltaDb(moduleIndex));
    };

    std::array<float, moduleCount> stagePeakLinear{};
    std::array<double, moduleCount> stageRmsAccum{};
    std::array<float, moduleCount> stageAuxDbBlock{};
    std::array<bool, moduleCount> stageClippedBlock{};
    std::array<float, moduleCount> stageInputRmsAccum{};
    std::array<float, moduleCount> stageOutputRmsAccum{};
    stageAuxDbBlock.fill(meterFloorDb);

    auto recordStage = [&](int moduleIndex, float inL, float inR, float outL, float outR, float auxDb)
    {
        const float peak = juce::jmax(std::abs(outL), std::abs(outR));
        stagePeakLinear[(size_t) moduleIndex] = juce::jmax(stagePeakLinear[(size_t) moduleIndex], peak);
        stageRmsAccum[(size_t) moduleIndex] += 0.5 * ((double) outL * outL + (double) outR * outR);
        stageInputRmsAccum[(size_t) moduleIndex] += 0.5f * (inL * inL + inR * inR);
        stageOutputRmsAccum[(size_t) moduleIndex] += 0.5f * (outL * outL + outR * outR);
        stageAuxDbBlock[(size_t) moduleIndex] = juce::jmax(stageAuxDbBlock[(size_t) moduleIndex], auxDb);
        stageClippedBlock[(size_t) moduleIndex] = stageClippedBlock[(size_t) moduleIndex] || peak >= 0.999f;
    };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float left = mainBuffer.getSample(0, sample);
        float right = mainBuffer.getSample(1, sample);
        const float internalKey = 0.5f * (left + right);
        const float sidechainIn = sidechainBuffer.getNumChannels() >= 2 && sidechainBuffer.getNumSamples() > sample
                                      ? 0.5f * (sidechainBuffer.getSample(0, sample) + sidechainBuffer.getSample(1, sample))
                                      : internalKey;

        pushPreSpectrumSample(0.5f * (left + right));
        if (sidechainBuffer.getNumChannels() > 0 && sidechainBuffer.getNumSamples() > sample)
            pushSidechainSpectrumSample(sidechainIn);

        {
            const float inL = left;
            const float inR = right;
            const bool compare = isModuleCompareEnabled(moduleInput);
            const bool active = !globalBypassed && !inputBypass && moduleAllowedBySolo(moduleInput) && !compare;

            if (active)
            {
                const float trimGain = dbToGain(inputTrimSmoothed.getNextValue());
                left *= trimGain;
                right *= trimGain;
                if (inputPhase)
                {
                    left = -left;
                    right = -right;
                }
                if (inputHpfOn)
                {
                    left = inputHighPassFilters[0].processSingleSampleRaw(left);
                    right = inputHighPassFilters[1].processSingleSampleRaw(right);
                }
                if (inputLpfOn)
                {
                    left = inputLowPassFilters[0].processSingleSampleRaw(left);
                    right = inputLowPassFilters[1].processSingleSampleRaw(right);
                }
            }
            else if (compare)
            {
                const float gain = stageCompareGain(moduleInput);
                left *= gain;
                right *= gain;
            }

            recordStage(moduleInput, inL, inR, left, right, gainToDb(std::sqrt((left * left + right * right) * 0.5f)));
        }

        for (int orderedModule : order)
        {
            const float inL = left;
            const float inR = right;
            const bool compare = isModuleCompareEnabled(orderedModule);
            const bool activeByBypass = (orderedModule == moduleGate ? !gateBypass && !dynamicsBypass
                                        : orderedModule == moduleEq ? !eqBypass
                                        : orderedModule == moduleCompressor ? !compBypass && !dynamicsBypass
                                        : !satBypass);
            const bool active = !globalBypassed && activeByBypass && moduleAllowedBySolo(orderedModule) && !compare;

            if (orderedModule == moduleGate)
            {
                float auxDb = meterFloorDb;
                if (active)
                {
                    const float keySignal = gateUsesExternalKey ? sidechainIn : internalKey;
                    float detector = gateSidechainHighPass.processSingleSampleRaw(keySignal);
                    detector = gateSidechainLowPass.processSingleSampleRaw(detector);
                    const float detectorAbs = std::abs(detector) + 1.0e-6f;
                    const float thresholdGain = dbToGain(gateThresholdDb);
                    const float attackCoeff = msToCoeff(gateAttackMs, currentSampleRate);
                    const float releaseCoeff = msToCoeff(gateReleaseMs, currentSampleRate);
                    const int holdSamples = (int) std::round((gateHoldMs * 0.001) * currentSampleRate);

                    float targetGain = 1.0f;
                    if (detectorAbs >= thresholdGain)
                    {
                        gateHoldSamplesRemaining = holdSamples;
                    }
                    else if (gateHoldSamplesRemaining > 0)
                    {
                        --gateHoldSamplesRemaining;
                    }
                    else
                    {
                        const float rangeDb = gateRangeSmoothed.getNextValue();
                        if (gateMode == GateMode::gate)
                        {
                            targetGain = dbToGain(-rangeDb);
                        }
                        else
                        {
                            const float detectorDb = gainToDb(detectorAbs);
                            const float normalised = juce::jlimit(0.0f, 1.0f, (gateThresholdDb - detectorDb) / juce::jmax(1.0f, rangeDb));
                            targetGain = dbToGain(-rangeDb * normalised);
                        }
                    }

                    const float coeff = targetGain > gateGainState ? attackCoeff : releaseCoeff;
                    gateGainState = coeff * gateGainState + (1.0f - coeff) * targetGain;
                    auxDb = juce::jlimit(meterFloorDb, 18.0f, -gainToDb(gateGainState));

                    if (gateListen)
                    {
                        left = detector;
                        right = detector;
                    }
                    else
                    {
                        left *= gateGainState;
                        right *= gateGainState;
                    }
                }
                else if (compare)
                {
                    const float gain = stageCompareGain(moduleGate);
                    left *= gain;
                    right *= gain;
                }

                recordStage(moduleGate, inL, inR, left, right, auxDb);
            }
            else if (orderedModule == moduleEq)
            {
                if (active)
                {
                    for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
                    {
                        const auto& point = points[(size_t) pointIndex];
                        if (!point.enabled)
                            continue;

                        switch (point.stereoMode)
                        {
                            case PluginProcessor::left:
                                left = processPointForDomain(0, pointIndex, left);
                                break;
                            case PluginProcessor::right:
                                right = processPointForDomain(1, pointIndex, right);
                                break;
                            case PluginProcessor::mid:
                            {
                                float midSample = (left + right) * sqrtHalf;
                                float sideSample = (left - right) * sqrtHalf;
                                midSample = processPointForDomain(2, pointIndex, midSample);
                                left = (midSample + sideSample) * sqrtHalf;
                                right = (midSample - sideSample) * sqrtHalf;
                                break;
                            }
                            case PluginProcessor::side:
                            {
                                float midSample = (left + right) * sqrtHalf;
                                float sideSample = (left - right) * sqrtHalf;
                                sideSample = processPointForDomain(3, pointIndex, sideSample);
                                left = (midSample + sideSample) * sqrtHalf;
                                right = (midSample - sideSample) * sqrtHalf;
                                break;
                            }
                            case PluginProcessor::stereo:
                            default:
                                left = processPointForDomain(0, pointIndex, left);
                                right = processPointForDomain(1, pointIndex, right);
                                break;
                        }
                    }
                }
                else if (compare)
                {
                    const float gain = stageCompareGain(moduleEq);
                    left *= gain;
                    right *= gain;
                }

                recordStage(moduleEq, inL, inR, left, right, gainToDb(std::sqrt((left * left + right * right) * 0.5f)));
            }
            else if (orderedModule == moduleCompressor)
            {
                float auxDb = meterFloorDb;
                if (active)
                {
                    float attackMs = compAttackBase;
                    float releaseMs = compReleaseBase;
                    float kneeDb = compKneeBase;
                    float ratio = compRatio;

                    if (compFastMode)
                        attackMs = juce::jmin(attackMs, 3.0f);

                    if (compCharacter == CompressorCharacter::punch)
                    {
                        attackMs *= 0.45f;
                        releaseMs *= 0.75f;
                        kneeDb *= 0.55f;
                        ratio *= 1.25f;
                    }
                    else if (compCharacter == CompressorCharacter::glue)
                    {
                        attackMs *= 1.8f;
                        releaseMs *= 1.7f;
                        kneeDb = juce::jmax(kneeDb, 10.0f);
                        ratio *= 0.85f;
                    }

                    float detector = compSidechainHighPass.processSingleSampleRaw(sidechainIn);
                    detector = compSidechainLowPass.processSingleSampleRaw(detector);
                    const float detectorAbs = std::abs(detector) + 1.0e-6f;
                    const float attackCoeff = msToCoeff(attackMs, currentSampleRate);
                    if (compAutoRelease)
                        releaseMs = juce::jmap(juce::jlimit(0.0f, 1.0f, detectorAbs), juce::jmax(100.0f, releaseMs * 0.35f), releaseMs);
                    const float releaseCoeff = msToCoeff(releaseMs, currentSampleRate);

                    if (detectorAbs > compEnvelope)
                        compEnvelope = attackCoeff * compEnvelope + (1.0f - attackCoeff) * detectorAbs;
                    else
                        compEnvelope = releaseCoeff * compEnvelope + (1.0f - releaseCoeff) * detectorAbs;

                    const float detectorDb = gainToDb(compEnvelope);
                    const float gainReductionDb = -applyCompressorCurve(detectorDb, compThresholdDb, juce::jmax(1.0f, ratio), kneeDb);
                    compGainReductionDb += 0.18f * (gainReductionDb - compGainReductionDb);
                    auxDb = juce::jlimit(meterFloorDb, 24.0f, compGainReductionDb);

                    if (compListen)
                    {
                        left = detector;
                        right = detector;
                    }
                    else
                    {
                        const float wetGain = dbToGain(-compGainReductionDb + compMakeupSmoothed.getNextValue());
                        const float mix = compMixSmoothed.getNextValue();
                        float wetLeft = left * wetGain;
                        float wetRight = right * wetGain;

                        if (compCharacter == CompressorCharacter::punch)
                        {
                            wetLeft = juce::jlimit(-1.5f, 1.5f, wetLeft + (inL - wetLeft) * 0.16f);
                            wetRight = juce::jlimit(-1.5f, 1.5f, wetRight + (inR - wetRight) * 0.16f);
                        }
                        else if (compCharacter == CompressorCharacter::glue)
                        {
                            wetLeft = std::tanh(wetLeft * 1.08f);
                            wetRight = std::tanh(wetRight * 1.08f);
                        }

                        left = juce::jlimit(-1.5f, 1.5f, inL + mix * (wetLeft - inL));
                        right = juce::jlimit(-1.5f, 1.5f, inR + mix * (wetRight - inR));
                    }
                }
                else if (compare)
                {
                    const float gain = stageCompareGain(moduleCompressor);
                    left *= gain;
                    right *= gain;
                }

                recordStage(moduleCompressor, inL, inR, left, right, auxDb);
            }
            else if (orderedModule == moduleSaturation)
            {
                float auxDb = meterFloorDb;
                if (active)
                {
                    const float driveDb = satDriveSmoothed.getNextValue();
                    const float mix = satMixSmoothed.getNextValue();
                    const float outputDb = satOutputSmoothed.getNextValue();
                    const float driveGain = dbToGain(driveDb);
                    const float outputGain = dbToGain(outputDb);
                    const float color = satCharacter == SaturationCharacter::warm ? 0.42f : (satCharacter == SaturationCharacter::edge ? 0.72f : 0.88f);
                    const float asymmetry = satCharacter == SaturationCharacter::warm ? 0.16f : (satCharacter == SaturationCharacter::edge ? 0.28f : 0.08f);
                    const float cutoffHz = satCharacter == SaturationCharacter::crisp ? 14000.0f : 6200.0f;
                    const float toneCoeff = std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) currentSampleRate);
                    const float highBlend = satCharacter == SaturationCharacter::warm ? 0.92f : (satCharacter == SaturationCharacter::edge ? 1.18f : 1.36f);

                    float peakHeat = 0.0f;
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        float dry = channel == 0 ? inL : inR;
                        float wet = shapeSaturationSample(dry * driveGain, color, asymmetry, satCharacter);
                        const float dcBlocked = wet - satDcInputState[(size_t) channel] + 0.995f * satDcOutputState[(size_t) channel];
                        satDcInputState[(size_t) channel] = wet;
                        satDcOutputState[(size_t) channel] = dcBlocked;
                        wet = dcBlocked;

                        satToneState[(size_t) channel] = wet + toneCoeff * (satToneState[(size_t) channel] - wet);
                        const float low = satToneState[(size_t) channel];
                        const float high = wet - low;
                        wet = (low + high * highBlend) * outputGain;

                        const float result = juce::jlimit(-1.5f, 1.5f, dry + mix * (wet - dry));
                        peakHeat = juce::jmax(peakHeat, juce::jlimit(0.0f, 1.0f, std::abs(wet - dry)));

                        if (channel == 0)
                            left = result;
                        else
                            right = result;
                    }

                    auxDb = juce::jmap(peakHeat, 0.0f, 1.0f, meterFloorDb, 0.0f);
                }
                else if (compare)
                {
                    const float gain = stageCompareGain(moduleSaturation);
                    left *= gain;
                    right *= gain;
                }

                recordStage(moduleSaturation, inL, inR, left, right, auxDb);
            }
        }

        {
            const float inL = left;
            const float inR = right;
            const bool compare = isModuleCompareEnabled(moduleOutput);
            const bool active = !globalBypassed && !outputBypass && moduleAllowedBySolo(moduleOutput) && !compare;
            float auxDb = meterFloorDb;

            if (active)
            {
                const float width = outputWidthSmoothed.getNextValue();
                const float balance = outputBalanceSmoothed.getNextValue();
                const float outGain = dbToGain(outputTrimSmoothed.getNextValue());

                float mid = (left + right) * sqrtHalf;
                float side = (left - right) * sqrtHalf * juce::jlimit(0.0f, 2.0f, width);
                left = (mid + side) * sqrtHalf;
                right = (mid - side) * sqrtHalf;

                const float pan = juce::jlimit(-1.0f, 1.0f, balance / 100.0f);
                const float panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                left *= std::cos(panAngle);
                right *= std::sin(panAngle);

                left *= outGain;
                right *= outGain;

                if (limiterEnabled)
                {
                    left = juce::jlimit(-ceilingGain, ceilingGain, left);
                    right = juce::jlimit(-ceilingGain, ceilingGain, right);
                }

                auxDb = gainToDb(std::sqrt((left * left + right * right) * 0.5f));
            }
            else if (compare)
            {
                const float gain = stageCompareGain(moduleOutput);
                left *= gain;
                right *= gain;
            }

            recordStage(moduleOutput, inL, inR, left, right, auxDb);
        }

        mainBuffer.setSample(0, sample, left);
        mainBuffer.setSample(1, sample, right);
        pushPostSpectrumSample(0.5f * (left + right));
    }

    for (int moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
    {
        setStageMeterSnapshot(moduleIndex,
                              stagePeakLinear[(size_t) moduleIndex],
                              stageRmsAccum[(size_t) moduleIndex],
                              numSamples,
                              stageAuxDbBlock[(size_t) moduleIndex],
                              stageClippedBlock[(size_t) moduleIndex],
                              stageInputRmsAccum[(size_t) moduleIndex],
                              stageOutputRmsAccum[(size_t) moduleIndex]);
    }
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
    {
        const auto order = getModuleOrder();
        for (int i = 0; i < reorderableModuleCount; ++i)
            state->setAttribute("moduleOrder" + juce::String(i), order[(size_t) i]);

        copyXmlToBinary(*state, destData);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        ModuleOrder restoredOrder{{moduleGate, moduleEq, moduleCompressor, moduleSaturation}};
        for (int i = 0; i < reorderableModuleCount; ++i)
            restoredOrder[(size_t) i] = xmlState->getIntAttribute("moduleOrder" + juce::String(i), restoredOrder[(size_t) i]);
        setModuleOrder(restoredOrder);
    }

    syncPointCacheFromParameters();
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
    const auto coeffs = buildCoefficientArray(points, currentSampleRate);

    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        pointCache = points;
        coefficientCache = coeffs;
    }

    updateFilterStates();
}

void PluginProcessor::updateFilterStates()
{
    std::array<PointFilterSetup, maxPoints> coeffs;
    {
        const juce::SpinLock::ScopedLockType lock(stateLock);
        coeffs = coefficientCache;
    }

    for (auto& domainFilters : filters)
    {
        for (int pointIndex = 0; pointIndex < maxPoints; ++pointIndex)
        {
            const auto& setup = coeffs[(size_t) pointIndex];
            for (int stage = 0; stage < maxStagesPerPoint; ++stage)
            {
                auto& filter = domainFilters[(size_t) pointIndex][(size_t) stage];
                filter.setCoefficients(stage < setup.numStages ? setup.coeffs[(size_t) stage] : makeIdentity());
            }
        }
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

void PluginProcessor::setStageMeterSnapshot(int moduleIndex, float peakLinear, double rmsAccum, int sampleCount, float auxDb, bool clipped, float inputRmsAccum, float outputRmsAccum)
{
    if (!juce::isPositiveAndBelow(moduleIndex, moduleCount))
        return;

    const float rms = std::sqrt((float) (rmsAccum / (double) juce::jmax(1, sampleCount)));
    const float inputRms = std::sqrt(inputRmsAccum / (float) juce::jmax(1, sampleCount));
    const float outputRms = std::sqrt(outputRmsAccum / (float) juce::jmax(1, sampleCount));

    const juce::SpinLock::ScopedLockType lock(meterLock);
    stagePeakDb[(size_t) moduleIndex] = gainToDb(peakLinear);
    stageRmsDb[(size_t) moduleIndex] = gainToDb(rms);
    stageAuxDb[(size_t) moduleIndex] = auxDb;
    stageClipLatched[(size_t) moduleIndex] = stageClipLatched[(size_t) moduleIndex] || clipped;

    if (inputRms > 0.0f && outputRms > 0.0f)
        moduleGainDeltaDb[(size_t) moduleIndex] = gainToDb(outputRms) - gainToDb(inputRms);
}

void PluginProcessor::applyFactoryPreset(const juce::String& presetName)
{
    for (int i = 0; i < maxPoints; ++i)
        removePoint(i);

    auto resetDefaults = [&]()
    {
        setBoolParam(apvts, "globalBypass", false);
        setBoolParam(apvts, "inputBypass", false);
        setFloatParam(apvts, "inputTrim", 0.0f);
        setBoolParam(apvts, "inputPhase", false);
        setBoolParam(apvts, "inputHpfOn", false);
        setFloatParam(apvts, "inputHpfFreq", 80.0f);
        setBoolParam(apvts, "inputLpfOn", false);
        setFloatParam(apvts, "inputLpfFreq", 18000.0f);

        setBoolParam(apvts, "dynamicsBypass", false);
        setBoolParam(apvts, "gateBypass", false);
        setFloatParam(apvts, "gateThreshold", -42.0f);
        setFloatParam(apvts, "gateRange", 24.0f);
        setFloatParam(apvts, "gateAttack", 5.0f);
        setFloatParam(apvts, "gateHold", 35.0f);
        setFloatParam(apvts, "gateRelease", 120.0f);
        setChoiceParam(apvts, "gateMode", 0);
        setFloatParam(apvts, "gateScHpFreq", 60.0f);
        setFloatParam(apvts, "gateScLpFreq", 12000.0f);
        setBoolParam(apvts, "gateScListen", false);
        setChoiceParam(apvts, "gateKeyInput", 0);

        setBoolParam(apvts, "eqBypass", false);
        setChoiceParam(apvts, "eqMode", 0);
        setBoolParam(apvts, "eqPreDynamics", false);

        setBoolParam(apvts, "compBypass", false);
        setFloatParam(apvts, "compThreshold", -18.0f);
        setFloatParam(apvts, "compRatio", 4.0f);
        setFloatParam(apvts, "compAttack", 15.0f);
        setChoiceParam(apvts, "compAttackMode", 1);
        setFloatParam(apvts, "compRelease", 250.0f);
        setBoolParam(apvts, "compAutoRelease", false);
        setFloatParam(apvts, "compMakeup", 0.0f);
        setFloatParam(apvts, "compKnee", 6.0f);
        setChoiceParam(apvts, "compCharacter", 0);
        setFloatParam(apvts, "compScHpFreq", 80.0f);
        setFloatParam(apvts, "compScLpFreq", 18000.0f);
        setBoolParam(apvts, "compScListen", false);
        setFloatParam(apvts, "compMix", 100.0f);
        setBoolParam(apvts, "compGateOrder", false);
        setBoolParam(apvts, "dynamicsStereoLink", true);

        setBoolParam(apvts, "satBypass", false);
        setFloatParam(apvts, "satDrive", 5.0f);
        setChoiceParam(apvts, "satCharacter", 0);
        setFloatParam(apvts, "satMix", 40.0f);
        setFloatParam(apvts, "satOutput", 0.0f);
        setChoiceParam(apvts, "satPosition", 1);

        setBoolParam(apvts, "outputBypass", false);
        setFloatParam(apvts, "outputGain", 0.0f);
        setFloatParam(apvts, "outputWidth", 1.0f);
        setFloatParam(apvts, "outputBalance", 0.0f);
        setBoolParam(apvts, "outputLimiter", true);
        setFloatParam(apvts, "outputCeiling", -0.1f);
        setChoiceParam(apvts, "outputMeterMode", 0);
    };

    auto configureBand = [this](int index, float frequency, float gain, float q, int type)
    {
        if (index < 0 || index >= maxPoints)
            return;

        EqPoint point;
        point.enabled = true;
        point.frequency = frequency;
        point.gainDb = gain;
        point.q = q;
        point.type = type;
        point.slopeIndex = 1;
        point.stereoMode = stereo;
        setPoint(index, point);
    };

    resetDefaults();

    if (presetName.containsIgnoreCase("Clean Pass"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqBypass", true);
        setBoolParam(apvts, "compBypass", true);
        setBoolParam(apvts, "satBypass", true);
        setBoolParam(apvts, "outputLimiter", false);
    }
    else if (presetName.containsIgnoreCase("Gain Stage Only"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqBypass", true);
        setBoolParam(apvts, "compBypass", true);
        setBoolParam(apvts, "satBypass", true);
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 40.0f);
    }
    else if (presetName.containsIgnoreCase("HPF + Compression Only"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqBypass", true);
        setBoolParam(apvts, "satBypass", true);
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 90.0f);
        setFloatParam(apvts, "compThreshold", -20.0f);
        setFloatParam(apvts, "compRatio", 3.5f);
        setFloatParam(apvts, "compAttack", 12.0f);
        setFloatParam(apvts, "compRelease", 220.0f);
    }
    else if (presetName.containsIgnoreCase("Male Vocal"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 80.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -19.0f);
        setFloatParam(apvts, "compRatio", 3.2f);
        setFloatParam(apvts, "compAttack", 10.0f);
        setFloatParam(apvts, "compRelease", 210.0f);
        setChoiceParam(apvts, "compCharacter", 0);
        setFloatParam(apvts, "satDrive", 3.0f);
        setChoiceParam(apvts, "satCharacter", 0);
        configureBand(0, 220.0f, -1.2f, 1.0f, bell);
        configureBand(1, 4200.0f, 2.4f, 0.8f, bell);
        configureBand(2, 11000.0f, 1.6f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Female Vocal"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 110.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -18.0f);
        setFloatParam(apvts, "compRatio", 3.0f);
        setChoiceParam(apvts, "compCharacter", 0);
        setFloatParam(apvts, "satDrive", 2.5f);
        configureBand(0, 300.0f, -1.0f, 1.0f, bell);
        configureBand(1, 5200.0f, 2.8f, 0.85f, bell);
        configureBand(2, 14000.0f, 1.8f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Podcast"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 75.0f);
        setBoolParam(apvts, "gateBypass", false);
        setFloatParam(apvts, "gateThreshold", -48.0f);
        setFloatParam(apvts, "compThreshold", -22.0f);
        setFloatParam(apvts, "compRatio", 4.0f);
        setBoolParam(apvts, "compAutoRelease", true);
        configureBand(0, 180.0f, -1.0f, 1.2f, bell);
        configureBand(1, 3500.0f, 2.0f, 1.0f, bell);
    }
    else if (presetName.containsIgnoreCase("Vocal Bus"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqPreDynamics", true);
        setFloatParam(apvts, "compThreshold", -12.0f);
        setFloatParam(apvts, "compRatio", 2.2f);
        setChoiceParam(apvts, "compCharacter", 2);
        setFloatParam(apvts, "satDrive", 2.2f);
        configureBand(0, 16000.0f, 1.2f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Aggressive"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 100.0f);
        setFloatParam(apvts, "compThreshold", -24.0f);
        setFloatParam(apvts, "compRatio", 6.0f);
        setChoiceParam(apvts, "compCharacter", 1);
        setChoiceParam(apvts, "satCharacter", 1);
        setFloatParam(apvts, "satDrive", 7.5f);
        setFloatParam(apvts, "satMix", 55.0f);
        configureBand(0, 2500.0f, 3.0f, 1.1f, bell);
        configureBand(1, 9000.0f, 2.2f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Intimate"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 70.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -16.0f);
        setFloatParam(apvts, "compRatio", 2.5f);
        setChoiceParam(apvts, "compCharacter", 2);
        setFloatParam(apvts, "satDrive", 1.8f);
        configureBand(0, 220.0f, 1.0f, 1.0f, bell);
        configureBand(1, 5200.0f, 1.6f, 0.9f, bell);
    }
    else if (presetName.containsIgnoreCase("Kick Inside"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 28.0f);
        setFloatParam(apvts, "gateThreshold", -36.0f);
        setFloatParam(apvts, "compThreshold", -14.0f);
        setFloatParam(apvts, "compRatio", 5.0f);
        setChoiceParam(apvts, "compCharacter", 1);
        configureBand(0, 65.0f, 2.4f, 0.8f, bell);
        configureBand(1, 3500.0f, 2.0f, 1.2f, bell);
    }
    else if (presetName.containsIgnoreCase("Snare Top"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 90.0f);
        setFloatParam(apvts, "gateThreshold", -34.0f);
        setFloatParam(apvts, "compThreshold", -16.0f);
        setFloatParam(apvts, "compRatio", 4.8f);
        setChoiceParam(apvts, "satCharacter", 1);
        configureBand(0, 180.0f, 1.8f, 1.0f, bell);
        configureBand(1, 7000.0f, 2.2f, 1.1f, bell);
    }
    else if (presetName.containsIgnoreCase("Overheads"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 150.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -10.0f);
        setFloatParam(apvts, "compRatio", 2.0f);
        configureBand(0, 10000.0f, 1.8f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Drum Bus"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -14.0f);
        setFloatParam(apvts, "compRatio", 4.0f);
        setChoiceParam(apvts, "compCharacter", 2);
        setFloatParam(apvts, "compAttack", 20.0f);
        setFloatParam(apvts, "compRelease", 260.0f);
        setChoiceParam(apvts, "satCharacter", 1);
        setFloatParam(apvts, "satDrive", 6.5f);
        configureBand(0, 400.0f, -2.4f, 1.0f, bell);
        configureBand(1, 9000.0f, 1.8f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Bass DI"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 30.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -18.0f);
        setFloatParam(apvts, "compRatio", 4.8f);
        setChoiceParam(apvts, "compCharacter", 1);
        setFloatParam(apvts, "satDrive", 4.8f);
        configureBand(0, 90.0f, 1.8f, 0.75f, lowShelf);
        configureBand(1, 800.0f, -1.6f, 1.3f, bell);
    }
    else if (presetName.containsIgnoreCase("Bass Amp"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 35.0f);
        setFloatParam(apvts, "compThreshold", -16.0f);
        setChoiceParam(apvts, "satCharacter", 1);
        setFloatParam(apvts, "satDrive", 6.5f);
        configureBand(0, 120.0f, 1.5f, 0.8f, bell);
        configureBand(1, 2400.0f, 1.8f, 1.1f, bell);
    }
    else if (presetName.containsIgnoreCase("Sub Bass"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 24.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -20.0f);
        setFloatParam(apvts, "compRatio", 3.0f);
        setChoiceParam(apvts, "satCharacter", 0);
        setFloatParam(apvts, "satDrive", 2.8f);
        configureBand(0, 55.0f, 1.4f, 0.8f, lowShelf);
    }
    else if (presetName.containsIgnoreCase("Acoustic Strumming"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 100.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -14.0f);
        setFloatParam(apvts, "compRatio", 2.5f);
        configureBand(0, 250.0f, -1.4f, 1.0f, bell);
        configureBand(1, 8500.0f, 1.8f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Electric Clean"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 80.0f);
        setBoolParam(apvts, "gateBypass", false);
        setFloatParam(apvts, "gateThreshold", -46.0f);
        setFloatParam(apvts, "compThreshold", -12.0f);
        configureBand(0, 3000.0f, 1.6f, 1.0f, bell);
    }
    else if (presetName.containsIgnoreCase("Electric Crunch"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 90.0f);
        setBoolParam(apvts, "gateBypass", false);
        setFloatParam(apvts, "gateThreshold", -40.0f);
        setChoiceParam(apvts, "satCharacter", 1);
        setFloatParam(apvts, "satDrive", 8.5f);
        configureBand(0, 4200.0f, 2.4f, 1.1f, bell);
    }
    else if (presetName.containsIgnoreCase("Piano Bright"))
    {
        setBoolParam(apvts, "inputHpfOn", true);
        setFloatParam(apvts, "inputHpfFreq", 45.0f);
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -12.0f);
        setFloatParam(apvts, "compRatio", 2.2f);
        configureBand(0, 6000.0f, 1.8f, 0.8f, bell);
        configureBand(1, 12000.0f, 1.4f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Rhodes"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -14.0f);
        setFloatParam(apvts, "compRatio", 2.5f);
        setChoiceParam(apvts, "satCharacter", 0);
        setFloatParam(apvts, "satDrive", 3.5f);
        configureBand(0, 3200.0f, 1.2f, 0.95f, bell);
    }
    else if (presetName.containsIgnoreCase("Synth Pad"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -10.0f);
        setFloatParam(apvts, "compRatio", 2.0f);
        setFloatParam(apvts, "outputWidth", 1.35f);
        configureBand(0, 180.0f, -1.2f, 0.9f, bell);
        configureBand(1, 9000.0f, 1.5f, 0.75f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Mix Bus Glue"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqPreDynamics", true);
        setFloatParam(apvts, "compThreshold", -12.0f);
        setFloatParam(apvts, "compRatio", 2.0f);
        setFloatParam(apvts, "compAttack", 30.0f);
        setFloatParam(apvts, "compRelease", 320.0f);
        setChoiceParam(apvts, "compCharacter", 2);
        setFloatParam(apvts, "satDrive", 2.5f);
        setBoolParam(apvts, "outputLimiter", true);
        configureBand(0, 140.0f, 0.8f, 0.7f, lowShelf);
        configureBand(1, 8500.0f, 0.9f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Mix Bus Punch"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setFloatParam(apvts, "compThreshold", -10.0f);
        setFloatParam(apvts, "compRatio", 3.5f);
        setChoiceParam(apvts, "compCharacter", 1);
        setChoiceParam(apvts, "satCharacter", 1);
        setFloatParam(apvts, "satDrive", 4.0f);
        configureBand(0, 60.0f, 0.8f, 0.8f, lowShelf);
        configureBand(1, 11000.0f, 1.0f, 0.7f, highShelf);
    }
    else if (presetName.containsIgnoreCase("Mastering Gentle"))
    {
        setBoolParam(apvts, "gateBypass", true);
        setBoolParam(apvts, "eqPreDynamics", true);
        setFloatParam(apvts, "compThreshold", -8.0f);
        setFloatParam(apvts, "compRatio", 1.8f);
        setChoiceParam(apvts, "compCharacter", 2);
        setFloatParam(apvts, "satDrive", 1.4f);
        setFloatParam(apvts, "satMix", 18.0f);
        setBoolParam(apvts, "outputLimiter", true);
        configureBand(0, 12500.0f, 0.8f, 0.7f, highShelf);
    }
}
