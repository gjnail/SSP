#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Parameter = juce::AudioProcessorValueTreeState::Parameter;
using Attributes = juce::AudioProcessorValueTreeStateParameterAttributes;

constexpr float maxPredelayMs = 250.0f;
constexpr float maxEarlyDelayMs = 110.0f;

juce::NormalisableRange<float> makeLogRange(float minValue, float maxValue, float interval = 0.0f)
{
    juce::NormalisableRange<float> range(minValue, maxValue, interval);
    range.setSkewForCentre(std::sqrt(minValue * maxValue));
    return range;
}

juce::String formatHz(float value)
{
    if (value >= 1000.0f)
        return juce::String(value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz";

    return juce::String(juce::roundToInt(value)) + " Hz";
}

juce::String formatPercent(float value)
{
    return juce::String(juce::roundToInt(value)) + "%";
}

juce::String formatSeconds(float value)
{
    return juce::String(value, 2) + " s";
}

juce::String formatMilliseconds(float value)
{
    return juce::String(value, value < 10.0f ? 2 : 1) + " ms";
}

juce::String formatRate(float value)
{
    return juce::String(value, value < 1.0f ? 2 : 1) + " Hz";
}

float parseNumeric(const juce::String& text)
{
    juce::String cleaned;
    cleaned.preallocateBytes(text.getNumBytesAsUTF8());

    for (juce::juce_wchar ch : text)
    {
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
            cleaned << ch;
    }

    return cleaned.getFloatValue();
}

std::unique_ptr<juce::RangedAudioParameter> makeFloatParameter(const juce::String& id,
                                                               const juce::String& name,
                                                               juce::NormalisableRange<float> range,
                                                               float defaultValue,
                                                               juce::String (*formatter)(float),
                                                               const juce::String& label = {})
{
    return std::make_unique<Parameter>(juce::ParameterID { id, 1 },
                                       name,
                                       range,
                                       defaultValue,
                                       Attributes().withStringFromValueFunction([formatter](float value, int) { return formatter(value); })
                                                   .withValueFromStringFunction([](const juce::String& text) { return parseNumeric(text); })
                                                   .withLabel(label));
}

std::unique_ptr<juce::RangedAudioParameter> makeToggleParameter(const juce::String& id,
                                                                const juce::String& name,
                                                                bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 },
                                                      name,
                                                      defaultValue,
                                                      juce::AudioParameterBoolAttributes()
                                                          .withStringFromValueFunction([](bool enabled, int) { return enabled ? "On" : "Off"; })
                                                          .withLabel("state"));
}

float remap01(float value, float start, float end)
{
    return juce::jlimit(0.0f, 1.0f, (value - start) / (end - start));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(makeFloatParameter("loCutHz", "Lo Cut", makeLogRange(20.0f, 1600.0f), 140.0f, formatHz, "Hz"));
    params.push_back(makeFloatParameter("hiCutHz", "Hi Cut", makeLogRange(1200.0f, 20000.0f), 14000.0f, formatHz, "Hz"));

    params.push_back(makeFloatParameter("earlyAmount", "Early Amount", { 0.0f, 100.0f, 0.1f }, 28.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("earlyRate", "Early Rate", { 0.05f, 6.0f, 0.01f }, 0.8f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("earlyShape", "Early Shape", { 0.0f, 100.0f, 0.1f }, 52.0f, formatPercent, "%"));
    params.push_back(makeToggleParameter("earlySpin", "Spin", false));

    params.push_back(makeFloatParameter("diffCrossoverHz", "Diffusion Crossover", makeLogRange(180.0f, 6000.0f), 1200.0f, formatHz, "Hz"));
    params.push_back(makeFloatParameter("lowDiffAmount", "Low Diffusion", { 0.0f, 100.0f, 0.1f }, 56.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("lowDiffScale", "Low Scale", { 25.0f, 200.0f, 0.1f }, 100.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("lowDiffRate", "Low Rate", { 0.05f, 4.0f, 0.01f }, 0.55f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("highDiffAmount", "High Diffusion", { 0.0f, 100.0f, 0.1f }, 72.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("highDiffScale", "High Scale", { 25.0f, 200.0f, 0.1f }, 84.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("highDiffRate", "High Rate", { 0.05f, 8.0f, 0.01f }, 1.35f, formatRate, "Hz"));

    params.push_back(makeFloatParameter("size", "Size", { 20.0f, 200.0f, 0.1f }, 100.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("decaySeconds", "Decay", { 0.20f, 12.0f, 0.01f }, 2.40f, formatSeconds, "s"));
    params.push_back(makeFloatParameter("predelayMs", "Predelay", { 0.0f, maxPredelayMs, 0.1f }, 24.0f, formatMilliseconds, "ms"));
    params.push_back(makeFloatParameter("stereoWidth", "Stereo Width", { 0.0f, 200.0f, 0.1f }, 118.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("density", "Density", { 0.0f, 100.0f, 0.1f }, 68.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("dryWet", "Dry/Wet", { 0.0f, 100.0f, 0.1f }, 28.0f, formatPercent, "%"));

    params.push_back(makeFloatParameter("chorusAmount", "Chorus Amount", { 0.0f, 100.0f, 0.1f }, 12.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("chorusRate", "Chorus Rate", { 0.05f, 8.0f, 0.01f }, 0.75f, formatRate, "Hz"));
    params.push_back(makeToggleParameter("freeze", "Freeze", false));
    params.push_back(makeToggleParameter("flatCut", "Flat/Cut", false));
    params.push_back(makeFloatParameter("reflect", "Reflect", { 0.0f, 100.0f, 0.1f }, 45.0f, formatPercent, "%"));

    return { params.begin(), params.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec stereoSpec;
    stereoSpec.sampleRate = sampleRate;
    stereoSpec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    stereoSpec.numChannels = 2;

    juce::dsp::ProcessSpec monoSpec = stereoSpec;
    monoSpec.numChannels = 1;

    auto prepareFilterArray = [monoSpec](auto& filters, juce::dsp::StateVariableTPTFilterType type)
    {
        for (auto& filter : filters)
        {
            filter.reset();
            filter.prepare(monoSpec);
            filter.setType(type);
        }
    };

    prepareFilterArray(inputLowPassFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(inputHighPassFilters, juce::dsp::StateVariableTPTFilterType::highpass);
    prepareFilterArray(outputLowPassFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(outputHighPassFilters, juce::dsp::StateVariableTPTFilterType::highpass);
    prepareFilterArray(crossoverLowFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(crossoverHighFilters, juce::dsp::StateVariableTPTFilterType::highpass);

    for (auto& line : preDelayLines)
    {
        line.setMaximumDelayInSamples((int) std::ceil(maxPredelayMs * 0.001 * sampleRate) + 2);
        line.prepare(monoSpec);
        line.reset();
    }

    for (auto& line : earlyReflectionLines)
    {
        line.setMaximumDelayInSamples((int) std::ceil(maxEarlyDelayMs * 0.001 * sampleRate) + 2);
        line.prepare(monoSpec);
        line.reset();
    }

    lowDiffusionChorus.prepare(stereoSpec);
    highDiffusionChorus.prepare(stereoSpec);
    wetChorus.prepare(stereoSpec);
    lowDiffusionChorus.reset();
    highDiffusionChorus.reset();
    wetChorus.reset();

    reverb.reset();
    mixSmoothed.reset(sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue(getValue("dryWet") / 100.0f);
    earlyPhase = 0.0f;

    updateScratchBuffers(2, juce::jmax(1, samplesPerBlock));
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::updateScratchBuffers(int numChannels, int numSamples)
{
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    filteredInputBuffer.setSize(numChannels, numSamples, false, false, true);
    wetBuffer.setSize(numChannels, numSamples, false, false, true);
    lowBandBuffer.setSize(numChannels, numSamples, false, false, true);
    highBandBuffer.setSize(numChannels, numSamples, false, false, true);
    earlyBuffer.setSize(numChannels, numSamples, false, false, true);
}

void PluginProcessor::updateFilterCutoffs(float loCutHz, float hiCutHz, float crossoverHz)
{
    const auto shapedLowCut = juce::jlimit(20.0f, 16000.0f, loCutHz);
    const auto shapedHighCut = juce::jlimit(shapedLowCut + 40.0f, 20000.0f, hiCutHz);

    for (auto& filter : inputHighPassFilters)
        filter.setCutoffFrequency(shapedLowCut);

    for (auto& filter : inputLowPassFilters)
        filter.setCutoffFrequency(shapedHighCut);

    for (auto& filter : outputHighPassFilters)
        filter.setCutoffFrequency(juce::jlimit(20.0f, 18000.0f, shapedLowCut * 1.15f));

    for (auto& filter : outputLowPassFilters)
        filter.setCutoffFrequency(juce::jlimit(600.0f, 20000.0f, shapedHighCut * 0.80f));

    for (auto& filter : crossoverLowFilters)
        filter.setCutoffFrequency(crossoverHz);

    for (auto& filter : crossoverHighFilters)
        filter.setCutoffFrequency(crossoverHz);
}

void PluginProcessor::applyInputFilters(juce::AudioBuffer<float>& buffer)
{
    const auto channels = juce::jmin(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            auto filtered = inputHighPassFilters[(size_t) channel].processSample(0, data[sample]);
            filtered = inputLowPassFilters[(size_t) channel].processSample(0, filtered);
            data[sample] = filtered;
        }
    }
}

void PluginProcessor::applyFlatCutFilters(juce::AudioBuffer<float>& buffer)
{
    const auto channels = juce::jmin(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            auto filtered = outputHighPassFilters[(size_t) channel].processSample(0, data[sample]);
            filtered = outputLowPassFilters[(size_t) channel].processSample(0, filtered);
            data[sample] = filtered;
        }
    }
}

void PluginProcessor::buildPredelayedInput(const juce::AudioBuffer<float>& source,
                                           juce::AudioBuffer<float>& destination,
                                           float predelaySamples)
{
    destination.clear();

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* input = source.getReadPointer(channel);
        auto* output = destination.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            preDelayLines[(size_t) channel].pushSample(0, input[sample]);
            output[sample] = preDelayLines[(size_t) channel].popSample(0, predelaySamples);
        }
    }
}

void PluginProcessor::buildEarlyReflections(const juce::AudioBuffer<float>& source,
                                            juce::AudioBuffer<float>& destination,
                                            float amount,
                                            float reflect,
                                            float shape,
                                            float rate,
                                            bool spinEnabled)
{
    destination.clear();

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();
    const float amountNorm = juce::jlimit(0.0f, 1.0f, amount);
    const float reflectNorm = juce::jlimit(0.0f, 1.0f, reflect);
    const float shapeNorm = juce::jlimit(0.0f, 1.0f, shape);
    const float basePhaseStep = juce::MathConstants<float>::twoPi * juce::jlimit(0.01f, 8.0f, rate) / (float) currentSampleRate;
    const std::array<float, 4> baseDelaysMs {
        6.0f + shapeNorm * 9.0f,
        15.0f + shapeNorm * 13.0f,
        28.0f + shapeNorm * 17.0f,
        46.0f + shapeNorm * 26.0f
    };
    const std::array<float, 4> tapGains {
        0.42f,
        0.28f + reflectNorm * 0.08f,
        0.20f + reflectNorm * 0.12f,
        0.12f + reflectNorm * 0.10f
    };

    for (int sample = 0; sample < samples; ++sample)
    {
        const float baseMod = std::sin(earlyPhase);
        const float spinMod = std::sin(earlyPhase + juce::MathConstants<float>::halfPi);

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto* input = source.getReadPointer(channel);
            auto* output = destination.getWritePointer(channel);
            const float channelSkew = spinEnabled ? ((channel == 0 ? -1.0f : 1.0f) * spinMod) : 0.0f;
            const float inputSample = input[sample];
            earlyReflectionLines[(size_t) channel].pushSample(0, inputSample);

            float earlySample = 0.0f;

            for (size_t tap = 0; tap < baseDelaysMs.size(); ++tap)
            {
                const float modDepth = 0.45f + 0.90f * shapeNorm + 0.15f * (float) tap;
                const float delayMs = juce::jlimit(1.0f,
                                                   maxEarlyDelayMs,
                                                   baseDelaysMs[tap] + baseMod * modDepth + channelSkew * (0.25f + 0.1f * (float) tap));
                const float tapDelaySamples = delayMs * 0.001f * (float) currentSampleRate;
                earlySample += tapGains[tap] * earlyReflectionLines[(size_t) channel].popSample(0, tapDelaySamples, false);
            }

            output[sample] = earlySample * amountNorm;
        }

        earlyPhase += basePhaseStep;
        if (earlyPhase > juce::MathConstants<float>::twoPi)
            earlyPhase -= juce::MathConstants<float>::twoPi;
    }
}

void PluginProcessor::splitIntoDiffusionBands(const juce::AudioBuffer<float>& source,
                                              juce::AudioBuffer<float>& lowBand,
                                              juce::AudioBuffer<float>& highBand)
{
    lowBand.makeCopyOf(source, true);
    highBand.makeCopyOf(source, true);

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* lowData = lowBand.getWritePointer(channel);
        auto* highData = highBand.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            lowData[sample] = crossoverLowFilters[(size_t) channel].processSample(0, lowData[sample]);
            highData[sample] = crossoverHighFilters[(size_t) channel].processSample(0, highData[sample]);
        }
    }
}

void PluginProcessor::processChorus(juce::AudioBuffer<float>& buffer, juce::dsp::Chorus<float>& chorus)
{
    juce::dsp::AudioBlock<float> block(buffer);
    chorus.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void PluginProcessor::applyWidth(juce::AudioBuffer<float>& buffer, float widthScale)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const auto samples = buffer.getNumSamples();

    for (int sample = 0; sample < samples; ++sample)
    {
        const float mid = 0.5f * (left[sample] + right[sample]);
        const float side = 0.5f * (left[sample] - right[sample]) * widthScale;
        left[sample] = mid + side;
        right[sample] = mid - side;
    }
}

float PluginProcessor::getValue(const juce::String& parameterID) const
{
    if (auto* value = apvts.getRawParameterValue(parameterID))
        return value->load();

    jassertfalse;
    return 0.0f;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    updateScratchBuffers(numChannels, numSamples);
    dryBuffer.makeCopyOf(buffer, true);

    const float loCutHz = getValue("loCutHz");
    const float hiCutHz = getValue("hiCutHz");
    const float earlyAmount = getValue("earlyAmount") / 100.0f;
    const float earlyRate = getValue("earlyRate");
    const float earlyShape = getValue("earlyShape") / 100.0f;
    const bool earlySpin = getValue("earlySpin") > 0.5f;
    const float crossoverHz = getValue("diffCrossoverHz");
    const float lowDiffAmount = getValue("lowDiffAmount") / 100.0f;
    const float lowDiffScale = getValue("lowDiffScale") / 100.0f;
    const float lowDiffRate = getValue("lowDiffRate");
    const float highDiffAmount = getValue("highDiffAmount") / 100.0f;
    const float highDiffScale = getValue("highDiffScale") / 100.0f;
    const float highDiffRate = getValue("highDiffRate");
    const float sizePercent = getValue("size");
    const float decaySeconds = getValue("decaySeconds");
    const float predelayMs = getValue("predelayMs");
    const float stereoWidth = getValue("stereoWidth");
    const float density = getValue("density") / 100.0f;
    const float dryWet = getValue("dryWet") / 100.0f;
    const float chorusAmount = getValue("chorusAmount") / 100.0f;
    const float chorusRate = getValue("chorusRate");
    const bool freeze = getValue("freeze") > 0.5f;
    const bool flatCut = getValue("flatCut") > 0.5f;
    const float reflect = getValue("reflect") / 100.0f;

    mixSmoothed.setTargetValue(dryWet);
    if (dryWet <= 0.0001f && ! freeze)
    {
        mixSmoothed.setCurrentAndTargetValue(dryWet);
        reverb.reset();
        return;
    }

    updateFilterCutoffs(loCutHz, hiCutHz, crossoverHz);

    filteredInputBuffer.makeCopyOf(buffer, true);
    applyInputFilters(filteredInputBuffer);

    buildPredelayedInput(filteredInputBuffer,
                         wetBuffer,
                         predelayMs * 0.001f * (float) currentSampleRate);

    splitIntoDiffusionBands(wetBuffer, lowBandBuffer, highBandBuffer);

    lowDiffusionChorus.setRate(lowDiffRate);
    lowDiffusionChorus.setDepth(juce::jlimit(0.0f, 1.0f, 0.18f + lowDiffScale * 0.42f));
    lowDiffusionChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, 8.0f + lowDiffScale * 14.0f));
    lowDiffusionChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.10f + density * 0.32f));
    lowDiffusionChorus.setMix(lowDiffAmount);

    highDiffusionChorus.setRate(highDiffRate);
    highDiffusionChorus.setDepth(juce::jlimit(0.0f, 1.0f, 0.12f + highDiffScale * 0.48f));
    highDiffusionChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, 4.5f + highDiffScale * 10.0f));
    highDiffusionChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.04f + density * 0.24f));
    highDiffusionChorus.setMix(highDiffAmount);

    processChorus(lowBandBuffer, lowDiffusionChorus);
    processChorus(highBandBuffer, highDiffusionChorus);

    wetBuffer.makeCopyOf(lowBandBuffer, true);
    wetBuffer.addFrom(0, 0, highBandBuffer, 0, 0, numSamples);
    if (numChannels > 1)
        wetBuffer.addFrom(1, 0, highBandBuffer, 1, 0, numSamples);

    buildEarlyReflections(filteredInputBuffer, earlyBuffer, earlyAmount, reflect, earlyShape, earlyRate, earlySpin);

    wetBuffer.addFrom(0, 0, earlyBuffer, 0, 0, numSamples, 0.45f + reflect * 0.35f);
    if (numChannels > 1)
        wetBuffer.addFrom(1, 0, earlyBuffer, 1, 0, numSamples, 0.45f + reflect * 0.35f);

    const float decayNorm = remap01(std::log(decaySeconds), std::log(0.20f), std::log(12.0f));
    const float sizeNorm = remap01(sizePercent, 20.0f, 200.0f);
    const float widthNorm = juce::jlimit(0.0f, 2.0f, stereoWidth / 100.0f);
    const float toneBias = remap01(hiCutHz, 1200.0f, 20000.0f);
    const float lowBias = remap01(loCutHz, 20.0f, 1600.0f);

    juce::Reverb::Parameters params;
    params.roomSize = juce::jlimit(0.08f, 0.99f, 0.12f + sizeNorm * 0.54f + decayNorm * 0.24f + density * 0.10f);
    params.damping = juce::jlimit(0.08f, 0.96f, 0.84f - toneBias * 0.30f + (1.0f - density) * 0.18f + (flatCut ? 0.08f : -0.03f));
    params.wetLevel = juce::jlimit(0.12f, 1.0f, 0.30f + density * 0.22f + reflect * 0.18f + (lowDiffAmount + highDiffAmount) * 0.10f);
    params.dryLevel = 0.0f;
    params.width = juce::jlimit(0.0f, 1.0f, 0.18f + widthNorm * 0.50f);
    params.freezeMode = freeze ? 0.97f : 0.0f;
    reverb.setParameters(params);

    if (numChannels >= 2)
        reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), numSamples);
    else if (numChannels == 1)
        reverb.processMono(wetBuffer.getWritePointer(0), numSamples);

    wetBuffer.addFrom(0, 0, earlyBuffer, 0, 0, numSamples, 0.55f + reflect * 0.45f);
    if (numChannels > 1)
        wetBuffer.addFrom(1, 0, earlyBuffer, 1, 0, numSamples, 0.55f + reflect * 0.45f);

    wetChorus.setRate(chorusRate);
    wetChorus.setDepth(juce::jlimit(0.0f, 1.0f, chorusAmount * 0.78f));
    wetChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, 6.0f + chorusAmount * 18.0f));
    wetChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.08f + chorusAmount * 0.18f));
    wetChorus.setMix(chorusAmount * 0.55f);
    processChorus(wetBuffer, wetChorus);

    if (flatCut)
        applyFlatCutFilters(wetBuffer);

    const float spectralTrim = juce::jlimit(0.55f, 1.15f, 1.02f - lowBias * 0.10f + toneBias * 0.08f);
    wetBuffer.applyGain(spectralTrim);
    applyWidth(wetBuffer, juce::jlimit(0.0f, 2.0f, widthNorm));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* out = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            const auto* wet = wetBuffer.getReadPointer(channel);
            out[sample] = dry[sample] * (1.0f - mix) + wet[sample] * mix;
        }
    }

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
