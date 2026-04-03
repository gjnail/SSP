#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const auto directRange = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    const auto crossRange = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("leftFromLeft", "Left From Left", directRange, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("leftFromRight", "Left From Right", crossRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("rightFromLeft", "Right From Left", crossRange, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("rightFromRight", "Right From Right", directRange, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputTrimDb", "Output Trim",
                                                                 juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
                                                                 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("swap", "Swap Channels", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("mono", "Mono Sum", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("invertLeft", "Invert Left", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("invertRight", "Invert Right", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoCompensate", "Auto Compensate", false));

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

juce::String PluginProcessor::getDescription() const
{
    return "Simple stereo channel mixer with four direct routing amounts.";
}

void PluginProcessor::setParameterValue(const juce::String& parameterID, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(parameterID)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void PluginProcessor::resetToDefault()
{
    applyQuickPreset(QuickPreset::defaultStereo);
    setParameterValue("outputTrimDb", 0.0f);
    setParameterValue("autoCompensate", 0.0f);
}

void PluginProcessor::applyQuickPreset(QuickPreset preset)
{
    switch (preset)
    {
        case QuickPreset::defaultStereo:
            setParameterValue("leftFromLeft", 1.0f);
            setParameterValue("leftFromRight", 0.0f);
            setParameterValue("rightFromLeft", 0.0f);
            setParameterValue("rightFromRight", 1.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::swap:
            setParameterValue("leftFromLeft", 0.0f);
            setParameterValue("leftFromRight", 1.0f);
            setParameterValue("rightFromLeft", 1.0f);
            setParameterValue("rightFromRight", 0.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::mono:
            setParameterValue("leftFromLeft", 0.5f);
            setParameterValue("leftFromRight", 0.5f);
            setParameterValue("rightFromLeft", 0.5f);
            setParameterValue("rightFromRight", 0.5f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::dualMono:
            setParameterValue("leftFromLeft", 1.0f);
            setParameterValue("leftFromRight", 0.0f);
            setParameterValue("rightFromLeft", 0.0f);
            setParameterValue("rightFromRight", 1.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 1.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::cross:
            setParameterValue("leftFromLeft", 1.0f);
            setParameterValue("leftFromRight", 1.0f);
            setParameterValue("rightFromLeft", 1.0f);
            setParameterValue("rightFromRight", 1.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::leftOnly:
            setParameterValue("leftFromLeft", 1.0f);
            setParameterValue("leftFromRight", 0.0f);
            setParameterValue("rightFromLeft", 1.0f);
            setParameterValue("rightFromRight", 0.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
        case QuickPreset::rightOnly:
            setParameterValue("leftFromLeft", 0.0f);
            setParameterValue("leftFromRight", 1.0f);
            setParameterValue("rightFromLeft", 0.0f);
            setParameterValue("rightFromRight", 1.0f);
            setParameterValue("swap", 0.0f);
            setParameterValue("mono", 0.0f);
            setParameterValue("invertLeft", 0.0f);
            setParameterValue("invertRight", 0.0f);
            break;
    }
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    outputTrimSmoothed.reset(sampleRate, 0.03);
    outputTrimSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outputTrimDb")->load()));

    const char* matrixIDs[4] = { "leftFromLeft", "leftFromRight", "rightFromLeft", "rightFromRight" };
    for (int i = 0; i < 4; ++i)
    {
        matrixSmoothed[(size_t) i].reset(sampleRate, 0.025);
        matrixSmoothed[(size_t) i].setCurrentAndTargetValue(apvts.getRawParameterValue(matrixIDs[i])->load());
    }
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (numChannels < 2 || numSamples == 0)
        return;

    outputTrimSmoothed.setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("outputTrimDb")->load()));

    const char* matrixIDs[4] = { "leftFromLeft", "leftFromRight", "rightFromLeft", "rightFromRight" };
    for (int i = 0; i < 4; ++i)
        matrixSmoothed[(size_t) i].setTargetValue(apvts.getRawParameterValue(matrixIDs[i])->load());

    const bool swapChannels = apvts.getRawParameterValue("swap")->load() >= 0.5f;
    const bool monoSum = apvts.getRawParameterValue("mono")->load() >= 0.5f;
    const bool invertLeft = apvts.getRawParameterValue("invertLeft")->load() >= 0.5f;
    const bool invertRight = apvts.getRawParameterValue("invertRight")->load() >= 0.5f;
    const bool autoCompensate = apvts.getRawParameterValue("autoCompensate")->load() >= 0.5f;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputL = left[sample];
        float inputR = right[sample];

        if (swapChannels)
            std::swap(inputL, inputR);

        if (monoSum)
        {
            const float mono = 0.5f * (inputL + inputR);
            inputL = mono;
            inputR = mono;
        }

        const float ll = matrixSmoothed[0].getNextValue();
        const float lr = matrixSmoothed[1].getNextValue();
        const float rl = matrixSmoothed[2].getNextValue();
        const float rr = matrixSmoothed[3].getNextValue();

        float autoGain = 1.0f;
        if (autoCompensate)
        {
            const float energy = std::sqrt(juce::jmax(0.5f, 0.5f * (ll * ll + lr * lr + rl * rl + rr * rr)));
            autoGain = 1.0f / juce::jmax(0.35f, energy);
        }

        float wetL = inputL * ll + inputR * lr;
        float wetR = inputL * rl + inputR * rr;

        if (invertLeft)
            wetL = -wetL;
        if (invertRight)
            wetR = -wetR;

        const float outputTrim = outputTrimSmoothed.getNextValue();
        left[sample] = wetL * autoGain * outputTrim;
        right[sample] = wetR * autoGain * outputTrim;
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
