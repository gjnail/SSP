#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
enum class SaturationStyle
{
    gentle = 0,
    cleanTube,
    warmTape,
    softClip,
    hardClip,
    amp,
    transformer,
    fuzz,
    rectify,
    brokenSpeaker
};

enum class StereoMode
{
    stereo = 0,
    midSide
};

constexpr std::array<const char*, PluginProcessor::maxBands> bandNames{
    "Sub", "Low", "Low Mid", "Upper Mid", "Presence", "Air"
};

float meterDecay(float current, float target) noexcept
{
    return juce::jmax(target, current * 0.90f);
}

juce::StringArray saturationStyleNames()
{
    return {"Gentle", "Clean Tube", "Warm Tape", "Soft Clip", "Hard Clip",
            "Amp", "Transformer", "Fuzz", "Rectify", "Broken Speaker"};
}

struct FactoryPresetInfo
{
    const char* name;
    const char* category;
    const char* tags;
};

constexpr std::array<FactoryPresetInfo, 18> factoryPresetInfo{{
    {"Gentle Warmth", "Clean", "subtle  mastering  warm"},
    {"Mastering Glow", "Clean", "polished  glue  open"},
    {"Soft Console", "Clean", "console  smooth  balanced"},
    {"Tube Kiss", "Clean", "tube  velvet  soft"},
    {"Transformer Glue", "Color", "transformer  weight  mixbus"},
    {"Presence Shine", "Color", "presence  sheen  excite"},
    {"Tube Push", "Drive", "tube  midrange  driven"},
    {"Hot Bus", "Drive", "bus  crunchy  energetic"},
    {"Edge Crunch", "Drive", "edge  clip  bite"},
    {"Amp Stack", "Guitar", "amp  stack  forward"},
    {"Broken Combo", "Guitar", "combo  speaker  character"},
    {"Tape Lift", "Color", "tape  low-end  lift"},
    {"Parallel Air", "FX", "air  parallel  shimmer"},
    {"Cassette Dust", "Lo-fi", "cassette  dusty  narrow"},
    {"Broken Bus", "Lo-fi", "broken  aggressive  speaker"},
    {"Speaker Crush", "Lo-fi", "speaker  crushed  gritty"},
    {"Fuzz Motion", "FX", "fuzz  motion  sound design"},
    {"Rectifier Bloom", "FX", "rectifier  bloom  edge"}
}};

juce::StringArray factoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : factoryPresetInfo)
        names.add(preset.name);
    return names;
}

const FactoryPresetInfo& getFactoryPresetInfoForIndex(int presetIndex)
{
    return factoryPresetInfo[(size_t) juce::jlimit(0, (int) factoryPresetInfo.size() - 1, presetIndex)];
}

float applyDynamics(float sample, float amount) noexcept
{
    const float clippedAmount = juce::jlimit(-1.0f, 1.0f, amount);

    if (clippedAmount >= 0.0f)
    {
        const float strength = 1.0f + clippedAmount * 2.4f;
        return std::tanh(sample * strength) / std::max(0.5f, strength * 0.72f);
    }

    const float expand = 1.0f + (-clippedAmount) * 1.6f;
    return juce::jlimit(-3.0f, 3.0f, sample * (0.85f + std::abs(sample) * expand));
}

float shapeSignal(float sample, float tone, float dynamics, SaturationStyle style) noexcept
{
    const float clippedTone = juce::jlimit(0.0f, 1.0f, tone);
    const float spectralTilt = juce::jmap(clippedTone, 0.0f, 1.0f, 0.82f, 1.42f);
    const float dynamicSample = applyDynamics(sample, dynamics);

    switch (style)
    {
        case SaturationStyle::gentle:
            return std::tanh(dynamicSample * juce::jmap(clippedTone, 0.0f, 1.0f, 0.9f, 1.6f)) * 0.92f;

        case SaturationStyle::cleanTube:
        {
            const float even = dynamicSample / (1.0f + std::abs(dynamicSample) * (0.55f + clippedTone * 0.8f));
            const float soft = std::tanh(dynamicSample * (1.1f + clippedTone * 1.3f));
            return juce::jmap(0.36f + clippedTone * 0.34f, even, soft);
        }

        case SaturationStyle::warmTape:
        {
            const float compressed = std::atan(dynamicSample * (1.2f + clippedTone * 2.0f)) / 1.3f;
            const float rounded = std::tanh((dynamicSample - 0.08f * dynamicSample * dynamicSample * dynamicSample) * 0.94f);
            return juce::jmap(clippedTone * 0.65f, rounded, compressed);
        }

        case SaturationStyle::softClip:
            return juce::jlimit(-1.25f, 1.25f, dynamicSample - 0.18f * std::pow(dynamicSample, 3.0f));

        case SaturationStyle::hardClip:
            return juce::jlimit(-0.94f, 0.94f, dynamicSample * juce::jmap(clippedTone, 0.0f, 1.0f, 1.2f, 2.6f));

        case SaturationStyle::amp:
        {
            const float pre = juce::jlimit(-3.0f, 3.0f, dynamicSample * (1.25f + clippedTone * 2.5f));
            const float odd = std::tanh(pre) + 0.18f * std::sin(pre * 2.5f);
            return juce::jlimit(-1.25f, 1.25f, odd * (0.86f + clippedTone * 0.18f));
        }

        case SaturationStyle::transformer:
        {
            const float asym = dynamicSample + 0.12f * std::abs(dynamicSample);
            const float fold = asym / (1.0f + std::abs(asym) * (1.1f + clippedTone * 1.7f));
            return juce::jlimit(-1.15f, 1.15f, fold * spectralTilt);
        }

        case SaturationStyle::fuzz:
        {
            const float squareish = std::tanh(dynamicSample * (2.4f + clippedTone * 3.2f));
            return juce::jlimit(-1.2f, 1.2f, squareish + 0.12f * std::sin(squareish * 10.0f));
        }

        case SaturationStyle::rectify:
        {
            const float rectified = std::copysign(std::pow(std::abs(dynamicSample), 0.82f), dynamicSample);
            const float diode = std::max(0.0f, rectified) - 0.58f * std::max(0.0f, -rectified);
            return juce::jlimit(-1.1f, 1.1f, std::tanh(diode * (1.8f + clippedTone)));
        }

        case SaturationStyle::brokenSpeaker:
        {
            const float crushed = juce::jlimit(-1.0f, 1.0f, dynamicSample * (1.6f + clippedTone * 2.6f));
            const float splat = crushed - 0.34f * std::pow(crushed, 3.0f) + 0.05f * std::sin(crushed * 22.0f);
            return juce::jlimit(-1.15f, 1.15f, splat);
        }
    }

    return std::tanh(dynamicSample);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("bands",
                                                                  "Bands",
                                                                  juce::StringArray{"1 Band", "2 Bands", "3 Bands", "4 Bands", "5 Bands", "6 Bands"},
                                                                  2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("globalDrive",
                                                                 "Master Drive",
                                                                 juce::NormalisableRange<float>(-6.0f, 24.0f, 0.1f),
                                                                 -1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("globalMix",
                                                                 "Master Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                 88.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("globalOutput",
                                                                 "Master Output",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("crossoverMode",
                                                                  "Crossover Mode",
                                                                  juce::StringArray{"Minimum Phase", "Linear Phase"},
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("oversampling",
                                                                  "Oversampling",
                                                                  juce::StringArray{"1x", "2x", "4x", "8x"},
                                                                  1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("stereoMode",
                                                                  "Stereo Mode",
                                                                  juce::StringArray{"Stereo", "Mid/Side"},
                                                                  0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("hqRender", "HQ Render", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoGain", "Auto Gain", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("soloBand",
                                                                  "Solo Band",
                                                                  juce::StringArray{"Off", "Band 1", "Band 2", "Band 3", "Band 4", "Band 5", "Band 6"},
                                                                  0));

    constexpr std::array<float, maxCrossovers> defaults{160.0f, 1800.0f, 5600.0f, 9000.0f, 14000.0f};
    for (int i = 0; i < maxCrossovers; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(crossoverParamId(i),
                                                                     "Crossover " + juce::String(i + 1),
                                                                     juce::NormalisableRange<float>(40.0f, 18000.0f, 1.0f, 0.32f),
                                                                     defaults[(size_t) i]));
    }

    for (int band = 0; band < maxBands; ++band)
    {
        const auto prefix = juce::String(bandNames[(size_t) band]) + " ";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandParamId(band, "style"),
                                                                      prefix + "Style",
                                                                      saturationStyleNames(),
                                                                      band == 0 ? 6 : (band == 1 ? 2 : 0)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "drive"),
                                                                     prefix + "Drive",
                                                                     juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f),
                                                                     band == 0 ? 2.5f : (band == 1 ? 1.8f : (band == 2 ? 1.2f : 0.0f))));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "tone"),
                                                                     prefix + "Tone",
                                                                     juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                     band == 0 ? -12.0f : (band == 1 ? -4.0f : (band == 2 ? 8.0f : 0.0f))));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "dynamics"),
                                                                     prefix + "Dynamics",
                                                                     juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f),
                                                                     0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "feedback"),
                                                                     prefix + "Feedback",
                                                                     juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f),
                                                                     0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "mix"),
                                                                     prefix + "Mix",
                                                                     juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                                                                     100.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(bandParamId(band, "level"),
                                                                     prefix + "Level",
                                                                     juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f),
                                                                     0.0f));
    }

    return {params.begin(), params.end()};
}

juce::String PluginProcessor::bandParamId(int bandIndex, const juce::String& suffix)
{
    return "band" + juce::String(bandIndex + 1) + suffix;
}

juce::String PluginProcessor::crossoverParamId(int crossoverIndex)
{
    return "crossover" + juce::String(crossoverIndex + 1);
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto& meter : bandMeters)
        meter.store(0.0f);
    for (auto& bin : spectrumBins)
        bin.store(0.0f);

    applyFactoryPreset((int) FactoryPreset::gentleWarmth);
}

PluginProcessor::~PluginProcessor() = default;

float PluginProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

float PluginProcessor::getHeatMeterLevel() const noexcept
{
    return heatMeter.load();
}

float PluginProcessor::getBandMeterLevel(int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(bandIndex, maxBands))
        return 0.0f;

    return bandMeters[(size_t) bandIndex].load();
}

std::array<float, PluginProcessor::analyzerBinCount> PluginProcessor::getSpectrumData() const noexcept
{
    std::array<float, analyzerBinCount> data{};
    for (int i = 0; i < analyzerBinCount; ++i)
        data[(size_t) i] = spectrumBins[(size_t) i].load();
    return data;
}

int PluginProcessor::getActiveBandCount() const noexcept
{
    return juce::jlimit(1, maxBands, 1 + juce::roundToInt(apvts.getRawParameterValue("bands")->load()));
}

int PluginProcessor::getSoloBand() const noexcept
{
    return juce::jlimit(-1, maxBands - 1, juce::roundToInt(apvts.getRawParameterValue("soloBand")->load()) - 1);
}

void PluginProcessor::setSoloBand(int bandIndex)
{
    setParameterValue("soloBand", (float) juce::jlimit(0, maxBands, bandIndex + 1));
}

juce::StringArray PluginProcessor::getFactoryPresetNames()
{
    return factoryPresetNames();
}

juce::String PluginProcessor::getFactoryPresetName(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).name;
}

juce::String PluginProcessor::getFactoryPresetCategory(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).category;
}

juce::String PluginProcessor::getFactoryPresetTags(int presetIndex)
{
    return getFactoryPresetInfoForIndex(presetIndex).tags;
}

int PluginProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentFactoryPreset;
}

void PluginProcessor::setParameterValue(const juce::String& paramId, float plainValue)
{
    if (auto* parameter = apvts.getParameter(paramId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

void PluginProcessor::applyFactoryPreset(int presetIndex)
{
    const auto preset = (FactoryPreset) juce::jlimit(0, (int) factoryPresetInfo.size() - 1, presetIndex);
    currentFactoryPreset = (int) preset;

    setParameterValue("bands", 3.0f);
    setParameterValue("globalDrive", 0.0f);
    setParameterValue("globalMix", 100.0f);
    setParameterValue("globalOutput", 0.0f);
    setParameterValue("crossoverMode", 0.0f);
    setParameterValue("oversampling", 1.0f);
    setParameterValue("stereoMode", 0.0f);
    setParameterValue("hqRender", 0.0f);
    setParameterValue("autoGain", 1.0f);
    setParameterValue("soloBand", 0.0f);
    setParameterValue("crossover1", 120.0f);
    setParameterValue("crossover2", 450.0f);
    setParameterValue("crossover3", 1800.0f);
    setParameterValue("crossover4", 5600.0f);
    setParameterValue("crossover5", 11000.0f);

    for (int band = 0; band < maxBands; ++band)
    {
        setParameterValue(bandParamId(band, "style"), 0.0f);
        setParameterValue(bandParamId(band, "drive"), 0.0f);
        setParameterValue(bandParamId(band, "tone"), 0.0f);
        setParameterValue(bandParamId(band, "dynamics"), 0.0f);
        setParameterValue(bandParamId(band, "feedback"), 0.0f);
        setParameterValue(bandParamId(band, "mix"), 100.0f);
        setParameterValue(bandParamId(band, "level"), 0.0f);
    }

    switch (preset)
    {
        case FactoryPreset::gentleWarmth:
            setParameterValue("bands", 2.0f);
            setParameterValue("globalDrive", -1.0f);
            setParameterValue("globalMix", 88.0f);
            setParameterValue("crossover1", 160.0f);
            setParameterValue("crossover2", 1800.0f);
            setParameterValue(bandParamId(0, "style"), 6.0f);
            setParameterValue(bandParamId(0, "drive"), 2.5f);
            setParameterValue(bandParamId(0, "tone"), -12.0f);
            setParameterValue(bandParamId(1, "style"), 2.0f);
            setParameterValue(bandParamId(1, "drive"), 1.8f);
            setParameterValue(bandParamId(1, "tone"), -4.0f);
            setParameterValue(bandParamId(2, "style"), 0.0f);
            setParameterValue(bandParamId(2, "drive"), 1.2f);
            setParameterValue(bandParamId(2, "tone"), 8.0f);
            break;

        case FactoryPreset::masteringGlow:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", -2.0f);
            setParameterValue("globalMix", 76.0f);
            setParameterValue("crossoverMode", 1.0f);
            setParameterValue("hqRender", 1.0f);
            setParameterValue(bandParamId(0, "style"), 2.0f);
            setParameterValue(bandParamId(0, "drive"), 1.8f);
            setParameterValue(bandParamId(1, "style"), 0.0f);
            setParameterValue(bandParamId(1, "drive"), 1.4f);
            setParameterValue(bandParamId(2, "style"), 1.0f);
            setParameterValue(bandParamId(2, "drive"), 1.1f);
            setParameterValue(bandParamId(3, "style"), 0.0f);
            setParameterValue(bandParamId(3, "drive"), 0.7f);
            break;

        case FactoryPreset::softConsole:
            setParameterValue("bands", 2.0f);
            setParameterValue("globalDrive", -0.5f);
            setParameterValue("globalMix", 92.0f);
            setParameterValue(bandParamId(0, "style"), 2.0f);
            setParameterValue(bandParamId(0, "drive"), 1.6f);
            setParameterValue(bandParamId(1, "style"), 6.0f);
            setParameterValue(bandParamId(1, "drive"), 1.4f);
            setParameterValue(bandParamId(2, "style"), 1.0f);
            setParameterValue(bandParamId(2, "drive"), 0.9f);
            break;

        case FactoryPreset::tubeKiss:
            setParameterValue("bands", 2.0f);
            setParameterValue("globalDrive", 0.5f);
            setParameterValue("globalMix", 82.0f);
            setParameterValue(bandParamId(1, "style"), 1.0f);
            setParameterValue(bandParamId(1, "drive"), 2.6f);
            setParameterValue(bandParamId(1, "tone"), 6.0f);
            setParameterValue(bandParamId(2, "style"), 1.0f);
            setParameterValue(bandParamId(2, "drive"), 1.7f);
            setParameterValue(bandParamId(2, "tone"), 10.0f);
            break;

        case FactoryPreset::transformerGlue:
            setParameterValue("bands", 2.0f);
            setParameterValue("globalDrive", 1.0f);
            setParameterValue("globalMix", 90.0f);
            setParameterValue(bandParamId(0, "style"), 6.0f);
            setParameterValue(bandParamId(0, "drive"), 3.2f);
            setParameterValue(bandParamId(0, "tone"), -14.0f);
            setParameterValue(bandParamId(1, "style"), 6.0f);
            setParameterValue(bandParamId(1, "drive"), 2.2f);
            setParameterValue(bandParamId(2, "style"), 2.0f);
            setParameterValue(bandParamId(2, "drive"), 1.2f);
            break;

        case FactoryPreset::presenceShine:
            setParameterValue("bands", 4.0f);
            setParameterValue("globalDrive", 0.0f);
            setParameterValue("globalMix", 74.0f);
            setParameterValue("crossover3", 3200.0f);
            setParameterValue("crossover4", 7600.0f);
            setParameterValue(bandParamId(3, "style"), 3.0f);
            setParameterValue(bandParamId(3, "drive"), 3.2f);
            setParameterValue(bandParamId(3, "tone"), 18.0f);
            setParameterValue(bandParamId(4, "style"), 6.0f);
            setParameterValue(bandParamId(4, "drive"), 2.8f);
            setParameterValue(bandParamId(4, "mix"), 68.0f);
            break;

        case FactoryPreset::tubePush:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 3.0f);
            setParameterValue(bandParamId(1, "style"), 1.0f);
            setParameterValue(bandParamId(1, "drive"), 6.0f);
            setParameterValue(bandParamId(1, "dynamics"), 24.0f);
            setParameterValue(bandParamId(2, "style"), 5.0f);
            setParameterValue(bandParamId(2, "drive"), 4.0f);
            setParameterValue(bandParamId(2, "tone"), 18.0f);
            setParameterValue(bandParamId(3, "style"), 1.0f);
            setParameterValue(bandParamId(3, "drive"), 3.0f);
            break;

        case FactoryPreset::hotBus:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 3.8f);
            setParameterValue("globalMix", 86.0f);
            setParameterValue(bandParamId(0, "style"), 2.0f);
            setParameterValue(bandParamId(0, "drive"), 4.2f);
            setParameterValue(bandParamId(1, "style"), 4.0f);
            setParameterValue(bandParamId(1, "drive"), 5.2f);
            setParameterValue(bandParamId(2, "style"), 3.0f);
            setParameterValue(bandParamId(2, "drive"), 4.0f);
            break;

        case FactoryPreset::edgeCrunch:
            setParameterValue("bands", 4.0f);
            setParameterValue("globalDrive", 4.4f);
            setParameterValue("globalMix", 88.0f);
            setParameterValue(bandParamId(2, "style"), 4.0f);
            setParameterValue(bandParamId(2, "drive"), 5.8f);
            setParameterValue(bandParamId(2, "tone"), 14.0f);
            setParameterValue(bandParamId(3, "style"), 4.0f);
            setParameterValue(bandParamId(3, "drive"), 4.8f);
            setParameterValue(bandParamId(4, "style"), 3.0f);
            setParameterValue(bandParamId(4, "drive"), 3.6f);
            break;

        case FactoryPreset::ampStack:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 2.8f);
            setParameterValue("globalMix", 100.0f);
            setParameterValue(bandParamId(1, "style"), 5.0f);
            setParameterValue(bandParamId(1, "drive"), 6.0f);
            setParameterValue(bandParamId(1, "tone"), 8.0f);
            setParameterValue(bandParamId(2, "style"), 5.0f);
            setParameterValue(bandParamId(2, "drive"), 5.6f);
            setParameterValue(bandParamId(2, "tone"), 18.0f);
            setParameterValue(bandParamId(3, "style"), 4.0f);
            setParameterValue(bandParamId(3, "drive"), 3.8f);
            break;

        case FactoryPreset::brokenCombo:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 3.0f);
            setParameterValue("globalMix", 92.0f);
            setParameterValue(bandParamId(1, "style"), 9.0f);
            setParameterValue(bandParamId(1, "drive"), 5.2f);
            setParameterValue(bandParamId(2, "style"), 9.0f);
            setParameterValue(bandParamId(2, "drive"), 5.8f);
            setParameterValue(bandParamId(2, "tone"), -10.0f);
            setParameterValue(bandParamId(3, "style"), 5.0f);
            setParameterValue(bandParamId(3, "drive"), 3.0f);
            break;

        case FactoryPreset::tapeLift:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 1.5f);
            setParameterValue("globalMix", 92.0f);
            setParameterValue(bandParamId(0, "style"), 2.0f);
            setParameterValue(bandParamId(0, "drive"), 4.0f);
            setParameterValue(bandParamId(0, "tone"), -18.0f);
            setParameterValue(bandParamId(1, "style"), 2.0f);
            setParameterValue(bandParamId(1, "drive"), 3.4f);
            setParameterValue(bandParamId(1, "dynamics"), 12.0f);
            setParameterValue(bandParamId(2, "style"), 6.0f);
            setParameterValue(bandParamId(2, "drive"), 2.0f);
            break;

        case FactoryPreset::parallelAir:
            setParameterValue("bands", 4.0f);
            setParameterValue("globalDrive", 0.5f);
            setParameterValue("globalMix", 70.0f);
            setParameterValue("crossover4", 7800.0f);
            setParameterValue(bandParamId(4, "style"), 3.0f);
            setParameterValue(bandParamId(4, "drive"), 5.0f);
            setParameterValue(bandParamId(4, "tone"), 32.0f);
            setParameterValue(bandParamId(4, "mix"), 46.0f);
            setParameterValue(bandParamId(5, "style"), 8.0f);
            setParameterValue(bandParamId(5, "drive"), 7.0f);
            setParameterValue(bandParamId(5, "tone"), 40.0f);
            setParameterValue(bandParamId(5, "mix"), 28.0f);
            break;

        case FactoryPreset::cassetteDust:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 1.2f);
            setParameterValue("globalMix", 82.0f);
            setParameterValue("globalOutput", -1.0f);
            setParameterValue(bandParamId(0, "style"), 2.0f);
            setParameterValue(bandParamId(0, "drive"), 2.4f);
            setParameterValue(bandParamId(1, "style"), 9.0f);
            setParameterValue(bandParamId(1, "drive"), 2.8f);
            setParameterValue(bandParamId(1, "tone"), -16.0f);
            setParameterValue(bandParamId(2, "style"), 2.0f);
            setParameterValue(bandParamId(2, "drive"), 1.2f);
            setParameterValue(bandParamId(2, "tone"), -20.0f);
            break;

        case FactoryPreset::brokenBus:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 5.0f);
            setParameterValue("globalMix", 84.0f);
            setParameterValue(bandParamId(1, "style"), 9.0f);
            setParameterValue(bandParamId(1, "drive"), 8.5f);
            setParameterValue(bandParamId(1, "feedback"), 8.0f);
            setParameterValue(bandParamId(2, "style"), 4.0f);
            setParameterValue(bandParamId(2, "drive"), 7.5f);
            setParameterValue(bandParamId(2, "dynamics"), 18.0f);
            setParameterValue(bandParamId(3, "style"), 9.0f);
            setParameterValue(bandParamId(3, "drive"), 6.5f);
            break;

        case FactoryPreset::speakerCrush:
            setParameterValue("bands", 3.0f);
            setParameterValue("globalDrive", 4.0f);
            setParameterValue("globalMix", 89.0f);
            setParameterValue(bandParamId(1, "style"), 9.0f);
            setParameterValue(bandParamId(1, "drive"), 6.0f);
            setParameterValue(bandParamId(1, "tone"), -18.0f);
            setParameterValue(bandParamId(2, "style"), 9.0f);
            setParameterValue(bandParamId(2, "drive"), 5.0f);
            setParameterValue(bandParamId(2, "dynamics"), 10.0f);
            break;

        case FactoryPreset::fuzzMotion:
            setParameterValue("bands", 5.0f);
            setParameterValue("globalDrive", 4.0f);
            setParameterValue("globalMix", 90.0f);
            setParameterValue("stereoMode", 1.0f);
            setParameterValue(bandParamId(2, "style"), 7.0f);
            setParameterValue(bandParamId(2, "drive"), 9.0f);
            setParameterValue(bandParamId(2, "feedback"), 6.0f);
            setParameterValue(bandParamId(3, "style"), 8.0f);
            setParameterValue(bandParamId(3, "drive"), 7.5f);
            setParameterValue(bandParamId(4, "style"), 7.0f);
            setParameterValue(bandParamId(4, "drive"), 6.0f);
            setParameterValue(bandParamId(4, "mix"), 68.0f);
            break;

        case FactoryPreset::rectifierBloom:
            setParameterValue("bands", 4.0f);
            setParameterValue("globalDrive", 2.6f);
            setParameterValue("globalMix", 80.0f);
            setParameterValue(bandParamId(2, "style"), 8.0f);
            setParameterValue(bandParamId(2, "drive"), 5.8f);
            setParameterValue(bandParamId(2, "tone"), 12.0f);
            setParameterValue(bandParamId(3, "style"), 8.0f);
            setParameterValue(bandParamId(3, "drive"), 4.5f);
            setParameterValue(bandParamId(4, "style"), 6.0f);
            setParameterValue(bandParamId(4, "drive"), 2.2f);
            break;
    }
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32) juce::jmax(samplesPerBlock, 1), 1};
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < maxCrossovers; ++i)
        {
            lowpassFilters[(size_t) channel][(size_t) i].reset();
            highpassFilters[(size_t) channel][(size_t) i].reset();
            lowpassFilters[(size_t) channel][(size_t) i].prepare(spec);
            highpassFilters[(size_t) channel][(size_t) i].prepare(spec);
            lowpassFilters[(size_t) channel][(size_t) i].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
            highpassFilters[(size_t) channel][(size_t) i].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        }
    }

    for (auto& bandState : bandStates)
        bandState = {};

    inputMeter.store(0.0f);
    outputMeter.store(0.0f);
    heatMeter.store(0.0f);

    for (auto& meter : bandMeters)
        meter.store(0.0f);
    for (auto& bin : spectrumBins)
        bin.store(0.0f);
    analyzerFifo.fill(0.0f);
    analyzerData.fill(0.0f);
    analyzerWritePosition = 0;
    analyzerBlockReady = false;

    updateCrossoverFilters();
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void PluginProcessor::updateMeters(float inputPeak, float outputPeak, float heatAmount,
                                   const std::array<float, maxBands>& bandPeaks) noexcept
{
    inputMeter.store(meterDecay(inputMeter.load(), inputPeak));
    outputMeter.store(meterDecay(outputMeter.load(), outputPeak));
    heatMeter.store(meterDecay(heatMeter.load(), heatAmount));

    for (int i = 0; i < maxBands; ++i)
        bandMeters[(size_t) i].store(meterDecay(bandMeters[(size_t) i].load(), bandPeaks[(size_t) i]));
}

void PluginProcessor::updateCrossoverFilters()
{
    for (int i = 0; i < maxCrossovers; ++i)
    {
        const float cutoff = juce::jlimit(30.0f, (float) currentSampleRate * 0.45f,
                                          apvts.getRawParameterValue(crossoverParamId(i))->load());

        for (int channel = 0; channel < 2; ++channel)
        {
            lowpassFilters[(size_t) channel][(size_t) i].setCutoffFrequency(cutoff);
            highpassFilters[(size_t) channel][(size_t) i].setCutoffFrequency(cutoff);
        }
    }
}

void PluginProcessor::pushNextAnalyzerSample(float sample) noexcept
{
    if (analyzerWritePosition == analyzerSize)
    {
        if (! analyzerBlockReady)
        {
            std::fill(analyzerData.begin(), analyzerData.end(), 0.0f);
            std::copy(analyzerFifo.begin(), analyzerFifo.end(), analyzerData.begin());
            analyzerBlockReady = true;
        }

        analyzerWritePosition = 0;
    }

    analyzerFifo[(size_t) analyzerWritePosition++] = sample;
}

void PluginProcessor::updateSpectrumData() noexcept
{
    if (! analyzerBlockReady)
        return;

    analyzerWindow.multiplyWithWindowingTable(analyzerData.data(), analyzerSize);
    analyzerFft.performFrequencyOnlyForwardTransform(analyzerData.data());

    const auto mindB = -72.0f;
    const auto maxdB = 6.0f;
    for (int i = 0; i < analyzerBinCount; ++i)
    {
        const float proportion = (float) i / (float) (analyzerBinCount - 1);
        const float skewed = std::pow(proportion, 2.1f);
        const int fftIndex = juce::jlimit(1, analyzerSize / 2 - 1, (int) std::round(skewed * (float) (analyzerSize / 2 - 1)));
        const float level = juce::Decibels::gainToDecibels(analyzerData[(size_t) fftIndex] / (float) analyzerSize, mindB);
        const float normalized = juce::jlimit(0.0f, 1.0f, juce::jmap(level, mindB, maxdB, 0.0f, 1.0f));
        spectrumBins[(size_t) i].store(juce::jmax(normalized, spectrumBins[(size_t) i].load() * 0.82f));
    }

    analyzerBlockReady = false;
}

std::array<float, PluginProcessor::maxBands> PluginProcessor::splitBands(float inputSample, int channel, int activeBands)
{
    std::array<float, maxBands> result{};
    float remainder = inputSample;

    for (int i = 0; i < activeBands - 1; ++i)
    {
        const auto low = lowpassFilters[(size_t) channel][(size_t) i].processSample(0, remainder);
        const auto high = highpassFilters[(size_t) channel][(size_t) i].processSample(0, remainder);
        result[(size_t) i] = low;
        remainder = high;
    }

    result[(size_t) (activeBands - 1)] = remainder;
    return result;
}

float PluginProcessor::processBandSample(float sample, int bandIndex, int channel,
                                         float globalDriveDb, bool autoGainEnabled) noexcept
{
    auto& state = bandStates[(size_t) bandIndex];
    const float driveDb = globalDriveDb + apvts.getRawParameterValue(bandParamId(bandIndex, "drive"))->load();
    const float tone = apvts.getRawParameterValue(bandParamId(bandIndex, "tone"))->load() / 100.0f;
    const float dynamics = apvts.getRawParameterValue(bandParamId(bandIndex, "dynamics"))->load() / 100.0f;
    const float feedback = apvts.getRawParameterValue(bandParamId(bandIndex, "feedback"))->load() / 100.0f;
    const float mix = apvts.getRawParameterValue(bandParamId(bandIndex, "mix"))->load() / 100.0f;
    const float levelDb = apvts.getRawParameterValue(bandParamId(bandIndex, "level"))->load();
    const auto style = (SaturationStyle) juce::jlimit(0, (int) saturationStyleNames().size() - 1,
                                                      juce::roundToInt(apvts.getRawParameterValue(bandParamId(bandIndex, "style"))->load()));

    const float driveGain = juce::Decibels::decibelsToGain(driveDb);
    const float levelGain = juce::Decibels::decibelsToGain(levelDb);
    const float compensation = autoGainEnabled ? juce::jmap(driveDb, 0.0f, 24.0f, 1.0f, 0.34f) : 1.0f;
    const float safeFeedback = juce::jlimit(0.0f, 0.36f, feedback * juce::jmap(juce::jlimit(0.0f, 24.0f, driveDb), 0.0f, 24.0f, 0.42f, 0.22f));
    const float feedbackSample = std::tanh(state.feedback[(size_t) channel] * 0.8f) * safeFeedback;
    const float fedInput = juce::jlimit(-2.0f, 2.0f, sample + feedbackSample);
    float wet = shapeSignal(fedInput * driveGain, 0.5f + tone * 0.5f, dynamics, style);

    const float cutoffHz = juce::jmap(tone, -1.0f, 1.0f, 1200.0f, 14000.0f);
    const float lowCoeff = std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) currentSampleRate);
    state.toneLowpass[(size_t) channel] = wet + lowCoeff * (state.toneLowpass[(size_t) channel] - wet);
    const float low = state.toneLowpass[(size_t) channel];
    const float high = wet - low;
    wet = low * juce::jmap(tone, -1.0f, 1.0f, 1.2f, 0.84f)
        + high * juce::jmap(tone, -1.0f, 1.0f, 0.62f, 1.5f);

    const float dcBlocked = wet - state.dcIn[(size_t) channel] + 0.995f * state.dcOut[(size_t) channel];
    state.dcIn[(size_t) channel] = wet;
    state.dcOut[(size_t) channel] = dcBlocked;
    wet = dcBlocked * levelGain * compensation;

    state.feedback[(size_t) channel] = juce::jlimit(-0.95f, 0.95f,
                                                    state.feedback[(size_t) channel] * 0.58f
                                                        + std::tanh(wet * 0.75f) * 0.42f);
    return sample * (1.0f - mix) + wet * mix;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numSamples == 0 || numChannels == 0)
        return;

    updateCrossoverFilters();

    const int activeBands = getActiveBandCount();
    const float globalDriveDb = apvts.getRawParameterValue("globalDrive")->load();
    const float globalMix = apvts.getRawParameterValue("globalMix")->load() / 100.0f;
    const float outputGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("globalOutput")->load());
    const bool autoGainEnabled = apvts.getRawParameterValue("autoGain")->load() > 0.5f;
    const int soloBand = getSoloBand();
    const auto stereoMode = (StereoMode) juce::jlimit(0, 1, juce::roundToInt(apvts.getRawParameterValue("stereoMode")->load()));

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float heatPeak = 0.0f;
    std::array<float, maxBands> bandPeaks{};

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float frameL = buffer.getSample(0, sample);
        float frameR = numChannels > 1 ? buffer.getSample(1, sample) : frameL;
        inputPeak = juce::jmax(inputPeak, juce::jmax(std::abs(frameL), std::abs(frameR)));

        if (stereoMode == StereoMode::midSide && numChannels > 1)
        {
            const float mid = 0.5f * (frameL + frameR);
            const float side = 0.5f * (frameL - frameR);
            frameL = mid;
            frameR = side;
        }

        std::array<float, 2> processedFrame{frameL, frameR};
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float input = channel == 0 ? frameL : frameR;
            const auto split = splitBands(input, channel, activeBands);

            float combined = 0.0f;
            for (int band = 0; band < activeBands; ++band)
            {
                const float processed = processBandSample(split[(size_t) band], band, channel, globalDriveDb, autoGainEnabled);
                if (soloBand < 0 || soloBand == band)
                    combined += processed;
                bandPeaks[(size_t) band] = juce::jmax(bandPeaks[(size_t) band], std::abs(processed));
                heatPeak = juce::jmax(heatPeak, juce::jlimit(0.0f, 1.0f, std::abs(processed - split[(size_t) band]) * 0.9f));
            }

            const float mixed = ((soloBand >= 0 ? 0.0f : input * (1.0f - globalMix)) + combined * globalMix) * outputGain;
            processedFrame[(size_t) channel] = juce::jlimit(-1.5f, 1.5f, mixed);
            outputPeak = juce::jmax(outputPeak, std::abs(processedFrame[(size_t) channel]));
        }

        if (stereoMode == StereoMode::midSide && numChannels > 1)
        {
            const float left = processedFrame[0] + processedFrame[1];
            const float right = processedFrame[0] - processedFrame[1];
            processedFrame[0] = left;
            processedFrame[1] = right;
        }

        buffer.setSample(0, sample, processedFrame[0]);
        if (numChannels > 1)
            buffer.setSample(1, sample, processedFrame[1]);

        const float analyzerSample = numChannels > 1 ? 0.5f * (processedFrame[0] + processedFrame[1]) : processedFrame[0];
        pushNextAnalyzerSample(analyzerSample);
    }

    updateSpectrumData();
    updateMeters(inputPeak, outputPeak, heatPeak, bandPeaks);

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
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

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
