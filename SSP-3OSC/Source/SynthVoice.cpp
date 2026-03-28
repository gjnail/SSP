#include "SynthVoice.h"
#include "WavetableLibrary.h"

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr int maxCombDelaySamples = 8192;
constexpr std::array<float, 5> octaveOffsets{{ -2.0f, -1.0f, 0.0f, 1.0f, 2.0f }};

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

using Coefficients = juce::dsp::IIR::Coefficients<float>;
using CoefficientsPtr = typename Coefficients::Ptr;

CoefficientsPtr makeNeutralCoefficients(double sampleRate)
{
    return Coefficients::makeAllPass(sampleRate, 19000.0f, 0.7071f);
}

float readInterpolatedSample(const juce::AudioBuffer<float>& buffer, double position)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0)
        return 0.0f;

    if (numSamples == 1)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mono += buffer.getSample(channel, 0);
        return mono / (float) numChannels;
    }

    double wrapped = std::fmod(position, (double) numSamples);
    if (wrapped < 0.0)
        wrapped += (double) numSamples;

    const int indexA = juce::jlimit(0, numSamples - 1, (int) wrapped);
    const int indexB = (indexA + 1) % numSamples;
    const float fraction = (float) (wrapped - (double) indexA);

    float sampleA = 0.0f;
    float sampleB = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        sampleA += buffer.getSample(channel, indexA);
        sampleB += buffer.getSample(channel, indexB);
    }

    sampleA /= (float) numChannels;
    sampleB /= (float) numChannels;
    return juce::jmap(fraction, sampleA, sampleB);
}

struct ModulationState
{
    float filterCutoffOctaves = 0.0f;
    float filterResonance = 0.0f;
    float filterDrive = 0.0f;
    float filterEnvAmount = 0.0f;
    float ampAttack = 0.0f;
    float ampDecay = 0.0f;
    float ampSustain = 0.0f;
    float ampRelease = 0.0f;
    float envAttack = 0.0f;
    float envDecay = 0.0f;
    float envSustain = 0.0f;
    float envRelease = 0.0f;
    float noiseAmount = 0.0f;
    float subLevel = 0.0f;
    float glide = 0.0f;
    float warpSaturator = 0.0f;
    float warpMutate = 0.0f;
    float masterSpread = 0.0f;
    float masterGain = 0.0f;
    std::array<float, 3> oscLevel{};
    std::array<float, 3> oscDetune{};
    std::array<float, 3> oscPan{};
    std::array<float, 3> oscWTPos{};
    std::array<float, 3> oscWarp1{};
    std::array<float, 3> oscWarp2{};
    std::array<float, 3> oscUnison{};
};

void applyDestination(ModulationState& state, reactormod::Destination destination, float value)
{
    switch (destination)
    {
        case reactormod::Destination::filterCutoff:      state.filterCutoffOctaves += value * 10.0f; break;
        case reactormod::Destination::filterResonance:   state.filterResonance += value * 17.8f; break;
        case reactormod::Destination::filterDrive:       state.filterDrive += value * 1.0f; break;
        case reactormod::Destination::filterEnvAmount:   state.filterEnvAmount += value * 1.0f; break;
        case reactormod::Destination::ampAttack:         state.ampAttack += value * 5.0f; break;
        case reactormod::Destination::ampDecay:          state.ampDecay += value * 5.0f; break;
        case reactormod::Destination::ampSustain:        state.ampSustain += value * 1.0f; break;
        case reactormod::Destination::ampRelease:        state.ampRelease += value * 5.0f; break;
        case reactormod::Destination::filterAttack:      state.envAttack += value * 5.0f; break;
        case reactormod::Destination::filterDecay:       state.envDecay += value * 5.0f; break;
        case reactormod::Destination::filterSustain:     state.envSustain += value * 1.0f; break;
        case reactormod::Destination::filterRelease:     state.envRelease += value * 5.0f; break;
        case reactormod::Destination::noiseAmount:       state.noiseAmount += value * 0.4f; break;
        case reactormod::Destination::subLevel:          state.subLevel += value * 1.0f; break;
        case reactormod::Destination::voiceGlide:        state.glide += value * 2.0f; break;
        case reactormod::Destination::warpSaturator:     state.warpSaturator += value * 1.0f; break;
        case reactormod::Destination::warpMutate:        state.warpMutate += value * 1.0f; break;
        case reactormod::Destination::masterSpread:      state.masterSpread += value * 1.0f; break;
        case reactormod::Destination::masterGain:        state.masterGain += value * 1.0f; break;
        case reactormod::Destination::osc1Level:         state.oscLevel[0] += value * 1.0f; break;
        case reactormod::Destination::osc1Detune:        state.oscDetune[0] += value * 48.0f; break;
        case reactormod::Destination::osc1Pan:           state.oscPan[0] += value * 2.0f; break;
        case reactormod::Destination::osc1WTPos:         state.oscWTPos[0] += value * 1.0f; break;
        case reactormod::Destination::osc1Warp1:         state.oscWarp1[0] += value * 1.0f; break;
        case reactormod::Destination::osc1Warp2:         state.oscWarp2[0] += value * 1.0f; break;
        case reactormod::Destination::osc1Unison:        state.oscUnison[0] += value * 1.0f; break;
        case reactormod::Destination::osc2Level:         state.oscLevel[1] += value * 1.0f; break;
        case reactormod::Destination::osc2Detune:        state.oscDetune[1] += value * 48.0f; break;
        case reactormod::Destination::osc2Pan:           state.oscPan[1] += value * 2.0f; break;
        case reactormod::Destination::osc2WTPos:         state.oscWTPos[1] += value * 1.0f; break;
        case reactormod::Destination::osc2Warp1:         state.oscWarp1[1] += value * 1.0f; break;
        case reactormod::Destination::osc2Warp2:         state.oscWarp2[1] += value * 1.0f; break;
        case reactormod::Destination::osc2Unison:        state.oscUnison[1] += value * 1.0f; break;
        case reactormod::Destination::osc3Level:         state.oscLevel[2] += value * 1.0f; break;
        case reactormod::Destination::osc3Detune:        state.oscDetune[2] += value * 48.0f; break;
        case reactormod::Destination::osc3Pan:           state.oscPan[2] += value * 2.0f; break;
        case reactormod::Destination::osc3WTPos:         state.oscWTPos[2] += value * 1.0f; break;
        case reactormod::Destination::osc3Warp1:         state.oscWarp1[2] += value * 1.0f; break;
        case reactormod::Destination::osc3Warp2:         state.oscWarp2[2] += value * 1.0f; break;
        case reactormod::Destination::osc3Unison:        state.oscUnison[2] += value * 1.0f; break;
        case reactormod::Destination::none:
        case reactormod::Destination::count:
        default:
            break;
    }
}
}

SynthVoice::SynthVoice(PluginProcessor& p)
    : processor(p)
{
    for (int i = 0; i < 3; ++i)
    {
        const auto prefix = "osc" + juce::String(i + 1);
        oscType[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Type");
        oscWave[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Wave");
        oscWavetable[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Wavetable");
        oscWTPosition[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "WTPos");
        oscLevel[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Level");
        oscOctave[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Octave");
        oscSampleRoot[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "SampleRoot");
        oscDetune[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Detune");
        oscPan[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Pan");
        oscWidth[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Width");
        oscToFilter[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "ToFilter");
        oscUnisonOn[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "UnisonOn");
        oscUnisonVoices[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "UnisonVoices");
        oscUnison[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "Unison");
        oscWarpFM[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "WarpFM");
        oscWarpFMSource[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "WarpFMSource");
        oscWarpMode[(size_t) i][0] = processor.apvts.getRawParameterValue(prefix + "Warp1Mode");
        oscWarpAmount[(size_t) i][0] = processor.apvts.getRawParameterValue(prefix + "Warp1Amount");
        oscWarpMode[(size_t) i][1] = processor.apvts.getRawParameterValue(prefix + "Warp2Mode");
        oscWarpAmount[(size_t) i][1] = processor.apvts.getRawParameterValue(prefix + "Warp2Amount");
        oscWarpSync[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "WarpSync");
        oscWarpBend[(size_t) i] = processor.apvts.getRawParameterValue(prefix + "WarpBend");
    }

    voiceMode = processor.apvts.getRawParameterValue("voiceMode");
    voiceLegato = processor.apvts.getRawParameterValue("voiceLegato");
    voicePortamento = processor.apvts.getRawParameterValue("voicePortamento");
    voiceGlide = processor.apvts.getRawParameterValue("voiceGlide");
    pitchBendUp = processor.apvts.getRawParameterValue("pitchBendUp");
    pitchBendDown = processor.apvts.getRawParameterValue("pitchBendDown");
    modWheelCutoff = processor.apvts.getRawParameterValue("modWheelCutoff");
    modWheelVibrato = processor.apvts.getRawParameterValue("modWheelVibrato");
    warpSaturator = processor.apvts.getRawParameterValue("warpSaturator");
    warpSaturatorPostFilter = processor.apvts.getRawParameterValue("warpSaturatorPostFilter");
    warpMutate = processor.apvts.getRawParameterValue("warpMutate");
    noiseAmount = processor.apvts.getRawParameterValue("noiseAmount");
    noiseType = processor.apvts.getRawParameterValue("noiseType");
    noiseToFilter = processor.apvts.getRawParameterValue("noiseToFilter");
    noiseToAmpEnv = processor.apvts.getRawParameterValue("noiseToAmpEnv");
    subWave = processor.apvts.getRawParameterValue("subWave");
    subLevel = processor.apvts.getRawParameterValue("subLevel");
    subOctave = processor.apvts.getRawParameterValue("subOctave");
    subDirectOut = processor.apvts.getRawParameterValue("subDirectOut");
    filterOn = processor.apvts.getRawParameterValue("filterOn");
    filterMode = processor.apvts.getRawParameterValue("filterMode");
    filterCutoff = processor.apvts.getRawParameterValue("filterCutoff");
    filterResonance = processor.apvts.getRawParameterValue("filterResonance");
    filterDrive = processor.apvts.getRawParameterValue("filterDrive");
    filterEnvAmount = processor.apvts.getRawParameterValue("filterEnvAmount");
    filterAttack = processor.apvts.getRawParameterValue("filterAttack");
    filterDecay = processor.apvts.getRawParameterValue("filterDecay");
    filterSustain = processor.apvts.getRawParameterValue("filterSustain");
    filterRelease = processor.apvts.getRawParameterValue("filterRelease");
    ampAttack = processor.apvts.getRawParameterValue("ampAttack");
    ampDecay = processor.apvts.getRawParameterValue("ampDecay");
    ampSustain = processor.apvts.getRawParameterValue("ampSustain");
    ampRelease = processor.apvts.getRawParameterValue("ampRelease");
    masterSpread = processor.apvts.getRawParameterValue("masterSpread");
    masterGain = processor.apvts.getRawParameterValue("masterGain");

    for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
    {
        modMatrixDestinations[(size_t) (slot - 1)] = processor.apvts.getRawParameterValue(reactormod::getMatrixDestinationParamID(slot));
        modMatrixAmounts[(size_t) (slot - 1)] = processor.apvts.getRawParameterValue(reactormod::getMatrixAmountParamID(slot));
    }
}

void SynthVoice::prepare(double newSampleRate, int, int newOutputChannels)
{
    sampleRate = newSampleRate;
    outputChannels = juce::jmax(1, newOutputChannels);
    ampEnvelope.setSampleRate(sampleRate);
    filterEnvelope.setSampleRate(sampleRate);
    currentNoteFrequency = 440.0f;
    targetNoteFrequency = 440.0f;
    pitchWheelNormalised = 0.0f;
    modWheelValue = 0.0f;
    lfoPhases.assign(processor.getModulationLfoCount(), 0.0f);
    lfoOneShotFinished.assign(processor.getModulationLfoCount(), false);

    const auto neutral = makeNeutralCoefficients(sampleRate);
    for (int channel = 0; channel < 2; ++channel)
    {
        filterA[(size_t) channel].reset();
        filterB[(size_t) channel].reset();
        filterA[(size_t) channel].coefficients = neutral;
        filterB[(size_t) channel].coefficients = neutral;
    }

    combBufferL.assign(maxCombDelaySamples, 0.0f);
    combBufferR.assign(maxCombDelaySamples, 0.0f);
    combWritePositionL = 0;
    combWritePositionR = 0;
    smoothedCutoff = 2200.0f;
    lastFilterCutoff = smoothedCutoff;
    lastFilterResonance = 0.75f;
    filterUpdateCountdown = 0;
    lastFilterModeIndex = -99;
    pinkNoiseState = 0.0f;
    brownNoiseState = 0.0f;
    digitalNoiseState = 0.0f;
    crackleNoiseState = 0.0f;
    radioNoisePhase = 0.0f;
    ventNoisePhase = 0.0f;
    lastWhiteNoise = 0.0f;
    previousWhiteNoise = 0.0f;
    digitalNoiseCounter = 0;
    subOscPhase = 0.0f;

    for (auto& phaseGroup : oscPhases)
        phaseGroup.fill(0.0f);
    for (auto& phaseGroup : oscSyncPhases)
        phaseGroup.fill(0.0f);
    for (auto& positionGroup : oscSamplePositions)
        positionGroup.fill(0.0);
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    noteFrequency = (float) juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    targetNoteFrequency = noteFrequency;
    noteVelocity = velocity;
    const auto snapshot = processor.getModulationSnapshot();
    if (lfoPhases.size() != snapshot.lfos.size())
    {
        lfoPhases.assign(snapshot.lfos.size(), 0.0f);
        lfoOneShotFinished.assign(snapshot.lfos.size(), false);
    }

    for (size_t i = 0; i < snapshot.lfos.size(); ++i)
    {
        if (snapshot.lfos[i].triggerMode != reactormod::TriggerMode::free)
        {
            lfoPhases[i] = 0.0f;
            lfoOneShotFinished[i] = false;
        }
    }
    smoothedCutoff = filterCutoff->load();
    lastFilterCutoff = smoothedCutoff;
    lastFilterResonance = filterResonance->load();
    filterUpdateCountdown = 0;
    lastFilterModeIndex = -99;
    combWritePositionL = 0;
    combWritePositionR = 0;
    pinkNoiseState = 0.0f;
    brownNoiseState = 0.0f;
    digitalNoiseState = 0.0f;
    crackleNoiseState = 0.0f;
    radioNoisePhase = random.nextFloat();
    ventNoisePhase = random.nextFloat();
    lastWhiteNoise = 0.0f;
    previousWhiteNoise = 0.0f;
    digitalNoiseCounter = 0;
    subOscPhase = random.nextFloat();

    std::fill(combBufferL.begin(), combBufferL.end(), 0.0f);
    std::fill(combBufferR.begin(), combBufferR.end(), 0.0f);

    for (auto& phaseGroup : oscPhases)
        for (auto& phase : phaseGroup)
            phase = random.nextFloat();
    for (auto& phaseGroup : oscSyncPhases)
        for (auto& phase : phaseGroup)
            phase = random.nextFloat();
    for (auto& positionGroup : oscSamplePositions)
        for (auto& position : positionGroup)
            position = 0.0;

    const bool hadActiveEnvelope = ampEnvelope.isActive();
    const bool glideEnabled = voicePortamento->load() >= 0.5f && voiceGlide->load() > 0.0001f;
    const bool legatoOnly = voiceLegato->load() >= 0.5f;

    if (!glideEnabled || (legatoOnly && !hadActiveEnvelope))
        currentNoteFrequency = targetNoteFrequency;
    else if (currentNoteFrequency <= 0.0f)
        currentNoteFrequency = targetNoteFrequency;

    ampEnvelope.noteOn();
    filterEnvelope.noteOn();
}

void SynthVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        filterEnvelope.reset();
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved(int newValue)
{
    pitchWheelNormalised = juce::jlimit(-1.0f, 1.0f, ((float) newValue - 8192.0f) / 8192.0f);
}

void SynthVoice::controllerMoved(int controllerNumber, int newValue)
{
    if (controllerNumber == 1)
        modWheelValue = juce::jlimit(0.0f, 1.0f, (float) newValue / 127.0f);
}

float SynthVoice::renderOscillatorSample(int oscIndex, float phase, const std::array<float, 3>& wavetablePositions) const
{
    const int oscTypeIndex = oscType[(size_t) oscIndex] != nullptr
        ? juce::roundToInt(oscType[(size_t) oscIndex]->load())
        : 0;

    if (oscTypeIndex == 2 && oscWavetable[(size_t) oscIndex] != nullptr)
    {
        const int tableIndex = juce::roundToInt(oscWavetable[(size_t) oscIndex]->load());
        const float position = juce::jlimit(0.0f, 1.0f, wavetablePositions[(size_t) oscIndex]);
        return wavetable::renderSample(tableIndex, position, phase);
    }

    const int waveIndex = juce::jlimit(0, 3, juce::roundToInt(oscWave[(size_t) oscIndex]->load()));

    switch (waveIndex)
    {
        case 0: return std::sin(twoPi * phase);
        case 1: return 2.0f * phase - 1.0f;
        case 2: return phase < 0.5f ? 1.0f : -1.0f;
        case 3: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        default: break;
    }

    return 0.0f;
}

float SynthVoice::wrapPhase(float phase)
{
    phase -= std::floor(phase);
    if (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

float SynthVoice::bendPhase(float phase, float amount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float wrappedPhase = wrapPhase(phase);

    if (clampedAmount <= 0.0001f)
        return wrappedPhase;

    const float pivot = juce::jmap(clampedAmount, 0.0f, 1.0f, 0.5f, 0.14f);
    if (wrappedPhase < pivot)
        return 0.5f * (wrappedPhase / juce::jmax(0.0001f, pivot));

    return 0.5f + 0.5f * ((wrappedPhase - pivot) / juce::jmax(0.0001f, 1.0f - pivot));
}

float SynthVoice::saturate(float sample, float drive)
{
    const float actualDrive = juce::jmax(1.0f, drive);
    return std::tanh(sample * actualDrive) / std::tanh(actualDrive);
}

float SynthVoice::applyMutate(int oscIndex, float phase, float sample, float amount,
                              const std::array<float, 3>& wavetablePositions) const
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (clampedAmount <= 0.0001f)
        return sample;

    const float harmonicPhase = wrapPhase(phase + 0.11f * clampedAmount * std::sin(twoPi * phase * (2.0f + (float) oscIndex)));
    const float harmonic = renderOscillatorSample(oscIndex, harmonicPhase, wavetablePositions);
    const float folded = std::sin((sample + harmonic * 0.45f) * (1.0f + clampedAmount * 4.5f));
    return juce::jlimit(-1.0f, 1.0f, juce::jmap(clampedAmount, sample, folded));
}

float SynthVoice::applyWarpMode(int oscIndex, int voiceIndex, float phase, int modeIndex, float amount,
                                float noiseSample, float lfo, float mutateAmount,
                                const std::array<float, 3>& wavetablePositions) const
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (clampedAmount <= 0.0001f || modeIndex == warpModeOff)
        return phase;

    switch (modeIndex)
    {
        case warpModeFM:
        {
            const int fmSourceIndex = juce::roundToInt(oscWarpFMSource[(size_t) oscIndex]->load());
            const float modulator = renderFMSourceSample(fmSourceIndex, oscIndex, voiceIndex, noiseSample, lfo, wavetablePositions);
            const float fmDepth = clampedAmount * (0.18f + clampedAmount * (0.34f + mutateAmount * 0.22f));
            return wrapPhase(phase + fmDepth * modulator);
        }

        case warpModeSync:
            return wrapPhase(phase * (1.0f + clampedAmount * 7.0f));

        case warpModeBendPlus:
            return bendPhase(phase, clampedAmount);

        case warpModeBendMinus:
            return 1.0f - bendPhase(1.0f - phase, clampedAmount);

        case warpModePhasePlus:
            return wrapPhase(phase + clampedAmount * 0.35f);

        case warpModePhaseMinus:
            return wrapPhase(phase - clampedAmount * 0.35f);

        case warpModeMirror:
        {
            const float mirrored = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
            return wrapPhase(juce::jmap(clampedAmount, phase, mirrored));
        }

        case warpModeWrap:
            return wrapPhase((phase - 0.5f) * (1.0f + clampedAmount * 6.0f) + 0.5f);

        case warpModePinch:
        {
            const float exponent = 1.0f + clampedAmount * 5.0f;
            if (phase < 0.5f)
                return 0.5f * std::pow(phase * 2.0f, exponent);

            return 1.0f - 0.5f * std::pow((1.0f - phase) * 2.0f, exponent);
        }

        default:
            break;
    }

    return phase;
}

float SynthVoice::renderNoiseSample(int noiseTypeIndex)
{
    const float white = random.nextFloat() * 2.0f - 1.0f;
    const float blue = juce::jlimit(-1.0f, 1.0f, white - lastWhiteNoise);
    const float violet = juce::jlimit(-1.0f, 1.0f, white - 2.0f * lastWhiteNoise + previousWhiteNoise);
    previousWhiteNoise = lastWhiteNoise;
    lastWhiteNoise = white;

    switch (noiseTypeIndex)
    {
        case 1: // Pink
            pinkNoiseState = 0.84f * pinkNoiseState + 0.16f * white;
            return pinkNoiseState;

        case 2: // Brown
            brownNoiseState = juce::jlimit(-1.0f, 1.0f, brownNoiseState + white * 0.08f);
            return brownNoiseState;

        case 3: // Digital
            if ((digitalNoiseCounter++ % 6) == 0)
                digitalNoiseState = white;
            return digitalNoiseState;

        case 4: // Dust
            return random.nextFloat() < 0.035f ? white * 1.8f : 0.0f;

        case 5: // Radio
            radioNoisePhase += 1100.0f / (float) sampleRate;
            if (radioNoisePhase >= 1.0f)
                radioNoisePhase -= 1.0f;
            return juce::jlimit(-1.0f, 1.0f, white * 0.35f + std::sin(twoPi * radioNoisePhase) * 0.18f
                                                     + (random.nextFloat() < 0.02f ? white * 0.95f : 0.0f));

        case 6: // Space Vent
            ventNoisePhase += 0.27f / (float) sampleRate;
            if (ventNoisePhase >= 1.0f)
                ventNoisePhase -= 1.0f;
            pinkNoiseState = 0.88f * pinkNoiseState + 0.12f * white;
            return juce::jlimit(-1.0f, 1.0f, pinkNoiseState * 0.75f + std::sin(twoPi * ventNoisePhase) * 0.32f);

        case 7: // Blue
            return blue;

        case 8: // Violet
            return violet;

        case 9: // Crackle
            crackleNoiseState *= 0.82f;
            if (random.nextFloat() < 0.028f)
                crackleNoiseState = white * 1.7f;
            return juce::jlimit(-1.0f, 1.0f, crackleNoiseState);

        case 10: // Wind Tunnel
            ventNoisePhase += 0.16f / (float) sampleRate;
            if (ventNoisePhase >= 1.0f)
                ventNoisePhase -= 1.0f;
            brownNoiseState = juce::jlimit(-1.0f, 1.0f, brownNoiseState + white * 0.05f);
            return juce::jlimit(-1.0f, 1.0f, brownNoiseState * 0.75f + std::sin(twoPi * ventNoisePhase) * 0.22f);

        case 11: // Arc Plasma
            if ((digitalNoiseCounter++ % 4) == 0)
                digitalNoiseState = white;
            radioNoisePhase += 2600.0f / (float) sampleRate;
            if (radioNoisePhase >= 1.0f)
                radioNoisePhase -= 1.0f;
            return juce::jlimit(-1.0f, 1.0f, digitalNoiseState * 0.65f + std::sin(twoPi * radioNoisePhase) * 0.28f + blue * 0.18f);

        default: // White
            return white;
    }
}

float SynthVoice::renderFMSourceSample(int sourceIndex, int targetOsc, int voiceIndex, float noiseSample, float lfo,
                                       const std::array<float, 3>& wavetablePositions) const
{
    const int safeSourceIndex = juce::jlimit(0, 6, sourceIndex);

    if (juce::isPositiveAndBelow(safeSourceIndex, 3))
    {
        const bool sourceUnisonEnabled = oscUnisonOn[(size_t) safeSourceIndex]->load() >= 0.5f;
        const int sourceVoiceCount = sourceUnisonEnabled
            ? juce::jlimit(2, maxUnisonVoices, juce::roundToInt(oscUnisonVoices[(size_t) safeSourceIndex]->load()))
            : 1;
        const int safeVoice = juce::jlimit(0, sourceVoiceCount - 1, voiceIndex);
        return renderOscillatorSample(safeSourceIndex, oscPhases[(size_t) safeSourceIndex][(size_t) safeVoice], wavetablePositions);
    }

    const float phase = oscPhases[(size_t) targetOsc][(size_t) juce::jlimit(0, maxUnisonVoices - 1, voiceIndex)];

    switch (safeSourceIndex)
    {
        case 3:  return noiseSample;
        case 4:  return std::sin(twoPi * wrapPhase(phase * 0.5f));
        case 5:  return 0.6f * lfo + 0.4f * std::sin(twoPi * wrapPhase(phase * 0.25f));
        case 6:  return std::sin(twoPi * wrapPhase(phase * 3.0f + noiseSample * 0.18f));
        default: return 0.0f;
    }
}

float SynthVoice::panLeft(float pan)
{
    return std::cos((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
}

float SynthVoice::panRight(float pan)
{
    return std::sin((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
}

float SynthVoice::resonanceToQ(float resonance)
{
    return juce::jmap(resonance, 0.2f, 18.0f, 0.45f, 12.0f);
}

float SynthVoice::resonanceToGainDb(float resonance)
{
    return juce::jmap(resonance, 0.2f, 18.0f, 0.0f, 18.0f);
}

float SynthVoice::resonanceToNormalised(float resonance)
{
    return juce::jlimit(0.0f, 1.0f, (resonance - 0.2f) / 17.8f);
}

void SynthVoice::updateFilterState(FilterMode mode, float cutoffHz, float resonance)
{
    const float clampedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffHz);
    const float q = resonanceToQ(resonance);
    const float resonanceNorm = resonanceToNormalised(resonance);
    const float cutoffNorm = (float) juce::mapFromLog10(clampedCutoff, 20.0f, 20000.0f);
    const float gainFactor = juce::Decibels::decibelsToGain(resonanceToGainDb(resonance));

    CoefficientsPtr first = makeNeutralCoefficients(sampleRate);
    CoefficientsPtr second = makeNeutralCoefficients(sampleRate);

    switch (mode)
    {
        case FilterMode::low12:
        case FilterMode::low24:
            first = Coefficients::makeLowPass(sampleRate, clampedCutoff, q);
            second = Coefficients::makeLowPass(sampleRate, clampedCutoff, q);
            break;
        case FilterMode::high12:
        case FilterMode::high24:
            first = Coefficients::makeHighPass(sampleRate, clampedCutoff, q);
            second = Coefficients::makeHighPass(sampleRate, clampedCutoff, q);
            break;
        case FilterMode::band12:
        case FilterMode::band24:
            first = Coefficients::makeBandPass(sampleRate, clampedCutoff, q);
            second = Coefficients::makeBandPass(sampleRate, clampedCutoff, q);
            break;
        case FilterMode::notch:
            first = Coefficients::makeNotch(sampleRate, clampedCutoff, q);
            second = makeNeutralCoefficients(sampleRate);
            break;
        case FilterMode::peak:
            first = Coefficients::makePeakFilter(sampleRate, clampedCutoff, juce::jlimit(0.35f, 2.5f, q * 0.16f), gainFactor);
            second = makeNeutralCoefficients(sampleRate);
            break;
        case FilterMode::lowShelf:
            first = Coefficients::makeLowShelf(sampleRate, clampedCutoff, 0.7071f, gainFactor);
            second = makeNeutralCoefficients(sampleRate);
            break;
        case FilterMode::highShelf:
            first = Coefficients::makeHighShelf(sampleRate, clampedCutoff, 0.7071f, gainFactor);
            second = makeNeutralCoefficients(sampleRate);
            break;
        case FilterMode::allPass:
            first = Coefficients::makeAllPass(sampleRate, clampedCutoff, q);
            second = makeNeutralCoefficients(sampleRate);
            break;
        case FilterMode::formant:
        {
            const float centre1 = juce::jlimit(120.0f, 3200.0f, clampedCutoff * 0.8f);
            const float centre2 = juce::jlimit(350.0f, 9000.0f, centre1 * (2.0f + resonanceNorm * 0.7f));
            first = Coefficients::makeBandPass(sampleRate, centre1, juce::jmap(resonanceNorm, 0.0f, 1.0f, 2.8f, 8.5f));
            second = Coefficients::makeBandPass(sampleRate, centre2, juce::jmap(resonanceNorm, 0.0f, 1.0f, 3.2f, 10.0f));
            break;
        }
        case FilterMode::vowelA:
        case FilterMode::vowelE:
        case FilterMode::vowelI:
        case FilterMode::vowelO:
        case FilterMode::vowelU:
        {
            float c1 = 650.0f, c2 = 1200.0f;
            switch (mode)
            {
                case FilterMode::vowelA: c1 = 800.0f; c2 = 1220.0f; break;
                case FilterMode::vowelE: c1 = 430.0f; c2 = 1880.0f; break;
                case FilterMode::vowelI: c1 = 310.0f; c2 = 2250.0f; break;
                case FilterMode::vowelO: c1 = 520.0f; c2 = 970.0f; break;
                case FilterMode::vowelU: c1 = 360.0f; c2 = 820.0f; break;
                default: break;
            }

            const float sweep = std::pow(2.0f, (cutoffNorm - 0.5f) * 1.4f);
            c1 = juce::jlimit(120.0f, 5000.0f, c1 * sweep);
            c2 = juce::jlimit(250.0f, 12000.0f, c2 * sweep);
            first = Coefficients::makeBandPass(sampleRate, c1, juce::jmap(resonanceNorm, 0.0f, 1.0f, 3.0f, 8.5f));
            second = Coefficients::makeBandPass(sampleRate, c2, juce::jmap(resonanceNorm, 0.0f, 1.0f, 3.6f, 10.5f));
            break;
        }
        case FilterMode::tilt:
        {
            const float tiltDb = juce::jmap(cutoffNorm, 0.0f, 1.0f, -7.5f, 7.5f) * (0.45f + resonanceNorm * 0.55f);
            first = Coefficients::makeLowShelf(sampleRate, juce::jlimit(80.0f, 6000.0f, clampedCutoff * 0.72f), 0.7071f,
                                               juce::Decibels::decibelsToGain(-tiltDb));
            second = Coefficients::makeHighShelf(sampleRate, juce::jlimit(180.0f, 14000.0f, clampedCutoff * 1.55f), 0.7071f,
                                                 juce::Decibels::decibelsToGain(tiltDb));
            break;
        }
        case FilterMode::resonator:
            first = Coefficients::makeBandPass(sampleRate, clampedCutoff, juce::jmap(resonanceNorm, 0.0f, 1.0f, 2.4f, 9.8f));
            second = Coefficients::makePeakFilter(sampleRate, juce::jlimit(80.0f, 18000.0f, clampedCutoff * 1.52f),
                                                  juce::jlimit(0.55f, 1.8f, q * 0.15f), gainFactor);
            break;
        case FilterMode::ladderDrive:
            first = Coefficients::makeLowPass(sampleRate, clampedCutoff, juce::jlimit(0.45f, 7.5f, q * 0.8f));
            second = Coefficients::makeLowPass(sampleRate, juce::jlimit(20.0f, 20000.0f, clampedCutoff * 0.96f),
                                               juce::jlimit(0.45f, 7.5f, q * 0.8f));
            break;
        case FilterMode::phaseWarp:
            first = Coefficients::makeAllPass(sampleRate, juce::jlimit(80.0f, 12000.0f, clampedCutoff * 0.72f), juce::jlimit(0.5f, 7.0f, q * 0.55f));
            second = Coefficients::makeAllPass(sampleRate, juce::jlimit(120.0f, 18000.0f, clampedCutoff * 1.38f), juce::jlimit(0.5f, 7.0f, q * 0.48f));
            break;
        case FilterMode::combPlus:
        case FilterMode::combMinus:
        case FilterMode::flangePlus:
        case FilterMode::flangeMinus:
        case FilterMode::metalComb:
        default:
            first = makeNeutralCoefficients(sampleRate);
            second = makeNeutralCoefficients(sampleRate);
            break;
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        filterA[(size_t) channel].coefficients = first;
        filterB[(size_t) channel].coefficients = second;
    }
}

float SynthVoice::processCombSample(float input, std::vector<float>& delayBuffer, int& writePosition, float delaySamples, float feedback, bool invert)
{
    const auto bufferSize = (int) delayBuffer.size();
    if (bufferSize < 4)
        return input;

    const float wrappedDelay = juce::jlimit(1.0f, (float) (bufferSize - 2), delaySamples);
    const int delayInt = (int) wrappedDelay;
    const float fraction = wrappedDelay - (float) delayInt;

    int readIndex1 = writePosition - delayInt;
    while (readIndex1 < 0)
        readIndex1 += bufferSize;

    int readIndex2 = readIndex1 - 1;
    while (readIndex2 < 0)
        readIndex2 += bufferSize;

    const float delayed = juce::jmap(fraction, delayBuffer[(size_t) readIndex1], delayBuffer[(size_t) readIndex2]);
    const float polarity = invert ? -1.0f : 1.0f;
    const float output = input + delayed * polarity * 0.85f;
    delayBuffer[(size_t) writePosition] = juce::jlimit(-8.0f, 8.0f, input + delayed * feedback * polarity);

    ++writePosition;
    if (writePosition >= bufferSize)
        writePosition = 0;

    return juce::jlimit(-8.0f, 8.0f, output);
}

float SynthVoice::processFilterSample(int channel, float input, FilterMode mode, float cutoffHz, float resonance)
{
    const float resonanceNorm = resonanceToNormalised(resonance);
    const float cutoffNorm = (float) juce::mapFromLog10(juce::jlimit(20.0f, 20000.0f, cutoffHz), 20.0f, 20000.0f);

    if (mode == FilterMode::combPlus || mode == FilterMode::combMinus)
    {
        const float delaySamples = juce::jlimit(18.0f, 3000.0f, (float) sampleRate / juce::jlimit(40.0f, 12000.0f, cutoffHz));
        const float feedback = juce::jlimit(0.16f, 0.97f, 0.20f + resonanceNorm * 0.72f);
        auto& combBuffer = channel == 0 ? combBufferL : combBufferR;
        int& writePosition = channel == 0 ? combWritePositionL : combWritePositionR;
        return processCombSample(input,
                                 combBuffer,
                                 writePosition,
                                 delaySamples,
                                 feedback,
                                 mode == FilterMode::combMinus);
    }

    if (mode == FilterMode::flangePlus || mode == FilterMode::flangeMinus)
    {
        const float delaySamples = juce::jmap(cutoffNorm, 180.0f, 6.0f);
        const float feedback = juce::jlimit(0.12f, 0.88f, 0.18f + resonanceNorm * 0.55f);
        auto& combBuffer = channel == 0 ? combBufferL : combBufferR;
        int& writePosition = channel == 0 ? combWritePositionL : combWritePositionR;
        return processCombSample(input,
                                 combBuffer,
                                 writePosition,
                                 delaySamples,
                                 feedback,
                                 mode == FilterMode::flangeMinus);
    }

    if (mode == FilterMode::metalComb)
    {
        const float delaySamples = juce::jmap(cutoffNorm, 180.0f, 4.0f);
        const float feedback = juce::jlimit(0.24f, 0.96f, 0.28f + resonanceNorm * 0.62f);
        auto& combBuffer = channel == 0 ? combBufferL : combBufferR;
        int& writePosition = channel == 0 ? combWritePositionL : combWritePositionR;
        return saturate(processCombSample(input, combBuffer, writePosition, delaySamples, feedback, false), 1.4f + resonanceNorm * 2.5f);
    }

    if (mode == FilterMode::formant)
    {
        auto& formant1 = filterA[(size_t) channel];
        auto& formant2 = filterB[(size_t) channel];
        const float a = formant1.processSample(input);
        const float b = formant2.processSample(input);
        return input * 0.18f + a * 0.56f + b * 0.44f;
    }

    if (mode == FilterMode::vowelA || mode == FilterMode::vowelE || mode == FilterMode::vowelI
        || mode == FilterMode::vowelO || mode == FilterMode::vowelU)
    {
        auto& vowel1 = filterA[(size_t) channel];
        auto& vowel2 = filterB[(size_t) channel];
        const float a = vowel1.processSample(input);
        const float b = vowel2.processSample(input);
        return input * 0.10f + a * 0.62f + b * 0.52f;
    }

    if (mode == FilterMode::resonator)
    {
        auto& resonA = filterA[(size_t) channel];
        auto& resonB = filterB[(size_t) channel];
        const float a = resonA.processSample(input);
        const float b = resonB.processSample(a);
        return input * 0.16f + a * 0.60f + b * 0.46f;
    }

    if (mode == FilterMode::ladderDrive)
    {
        auto& stageA = filterA[(size_t) channel];
        auto& stageB = filterB[(size_t) channel];
        const float driven = saturate(input, 1.6f + resonanceNorm * 3.2f);
        const float out = stageB.processSample(stageA.processSample(driven));
        return saturate(out, 1.1f + resonanceNorm * 1.2f);
    }

    if (mode == FilterMode::phaseWarp)
    {
        auto& stageA = filterA[(size_t) channel];
        auto& stageB = filterB[(size_t) channel];
        const float phased = stageB.processSample(stageA.processSample(input));
        return input * 0.52f + phased * 0.72f;
    }

    auto& firstStage = filterA[(size_t) channel];
    auto& secondStage = filterB[(size_t) channel];
    const float firstPass = firstStage.processSample(input);

    if (mode == FilterMode::low24 || mode == FilterMode::high24 || mode == FilterMode::band24)
        return secondStage.processSample(firstPass);

    return firstPass;
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    const bool filterEnabled = filterOn->load() >= 0.5f;
    const auto currentMode = static_cast<FilterMode>(juce::jlimit(0, (int) FilterMode::phaseWarp, juce::roundToInt(filterMode->load())));
    const int currentModeIndex = filterEnabled ? (int) currentMode : -1;
    const float cutoffBase = filterCutoff->load();
    const bool warpSaturationAfterFilter = warpSaturatorPostFilter->load() >= 0.5f;
    const int noiseTypeIndex = juce::roundToInt(noiseType->load());
    const bool noiseRoutedToFilter = filterEnabled && noiseToFilter->load() >= 0.5f;
    const bool noiseUsesAmpEnv = noiseToAmpEnv->load() >= 0.5f;
    const float pitchBendSemitones = pitchWheelNormalised >= 0.0f
        ? pitchWheelNormalised * pitchBendUp->load()
        : pitchWheelNormalised * pitchBendDown->load();
    const float pitchBendRatio = std::pow(2.0f, pitchBendSemitones / 12.0f);
    const float modWheelCutoffOctaves = modWheelValue * modWheelCutoff->load() * 3.0f;
    const std::array<PluginProcessor::OscillatorSampleData::Ptr, 3> oscSampleData{{
        processor.getOscillatorSampleData(1),
        processor.getOscillatorSampleData(2),
        processor.getOscillatorSampleData(3)
    }};

    const auto modulationSnapshot = processor.getModulationSnapshot();
    if (lfoPhases.size() != modulationSnapshot.lfos.size())
    {
        lfoPhases.assign(modulationSnapshot.lfos.size(), 0.0f);
        lfoOneShotFinished.assign(modulationSnapshot.lfos.size(), false);
    }

    std::array<int, reactormod::matrixSlotCount> matrixDestinations{};
    std::array<float, reactormod::matrixSlotCount> matrixAmounts{};
    for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
    {
        matrixDestinations[(size_t) slot] = modMatrixDestinations[(size_t) slot] != nullptr
            ? juce::roundToInt(modMatrixDestinations[(size_t) slot]->load())
            : 0;
        matrixAmounts[(size_t) slot] = modMatrixAmounts[(size_t) slot] != nullptr
            ? modMatrixAmounts[(size_t) slot]->load()
            : 0.0f;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        std::vector<float> lfoValues(modulationSnapshot.lfos.size(), 0.0f);
        for (int lfo = 0; lfo < (int) modulationSnapshot.lfos.size(); ++lfo)
        {
            const auto& lfoData = modulationSnapshot.lfos[(size_t) lfo];
            const bool oneShotFinished = juce::isPositiveAndBelow((size_t) lfo, lfoOneShotFinished.size())
                ? lfoOneShotFinished[(size_t) lfo]
                : false;

            if (lfoData.triggerMode == reactormod::TriggerMode::oneShot && oneShotFinished)
            {
                lfoValues[(size_t) lfo] = reactormod::evaluateLfoValue(lfoData, 0.9999f, lfo + 1);
                continue;
            }

            lfoValues[(size_t) lfo] = reactormod::evaluateLfoValue(lfoData, lfoPhases[(size_t) lfo], lfo + 1);
            const float rateHz = reactormod::computeLfoRateHz(lfoData, processor.getCurrentHostBpm());
            lfoPhases[(size_t) lfo] += rateHz / (float) sampleRate;

            if (lfoData.triggerMode == reactormod::TriggerMode::oneShot)
            {
                if (lfoPhases[(size_t) lfo] >= 1.0f)
                {
                    lfoPhases[(size_t) lfo] = 1.0f;
                    lfoOneShotFinished[(size_t) lfo] = true;
                }
            }
            else if (lfoPhases[(size_t) lfo] >= 1.0f)
            {
                lfoPhases[(size_t) lfo] -= std::floor(lfoPhases[(size_t) lfo]);
            }
        }

        ModulationState modulation;
        for (int slot = 0; slot < reactormod::matrixSlotCount; ++slot)
        {
            if (! modulationSnapshot.matrixEnabled[(size_t) slot])
                continue;

            const int sourceIndex = juce::jmax(0, modulationSnapshot.matrixSources[(size_t) slot]);
            if (sourceIndex <= 0)
                continue;

            const auto destination = reactormod::destinationFromChoiceIndex(matrixDestinations[(size_t) slot]);
            if (destination == reactormod::Destination::none)
                continue;

            const float amount = matrixAmounts[(size_t) slot];
            if (std::abs(amount) <= 0.0001f)
                continue;

            if (! juce::isPositiveAndBelow(sourceIndex - 1, (int) lfoValues.size()))
                continue;

            applyDestination(modulation, destination, lfoValues[(size_t) (sourceIndex - 1)] * amount);
        }

        adsrParameters.attack = juce::jlimit(0.001f, 5.0f, ampAttack->load() + modulation.ampAttack);
        adsrParameters.decay = juce::jlimit(0.001f, 5.0f, ampDecay->load() + modulation.ampDecay);
        adsrParameters.sustain = juce::jlimit(0.0f, 1.0f, ampSustain->load() + modulation.ampSustain);
        adsrParameters.release = juce::jlimit(0.001f, 5.0f, ampRelease->load() + modulation.ampRelease);
        ampEnvelope.setParameters(adsrParameters);

        filterAdsrParameters.attack = juce::jlimit(0.001f, 5.0f, filterAttack->load() + modulation.envAttack);
        filterAdsrParameters.decay = juce::jlimit(0.001f, 5.0f, filterDecay->load() + modulation.envDecay);
        filterAdsrParameters.sustain = juce::jlimit(0.0f, 1.0f, filterSustain->load() + modulation.envSustain);
        filterAdsrParameters.release = juce::jlimit(0.001f, 5.0f, filterRelease->load() + modulation.envRelease);
        filterEnvelope.setParameters(filterAdsrParameters);

        const float ampEnv = ampEnvelope.getNextSample();
        const float filterEnvValue = filterEnvelope.getNextSample();
        if (!ampEnvelope.isActive() && ampEnv <= 0.0f)
        {
            clearCurrentNote();
            break;
        }

        const float lfo = ! lfoValues.empty() ? lfoValues[0] : 0.0f;
        const float resonance = juce::jlimit(0.2f, 18.0f, filterResonance->load() + modulation.filterResonance);
        const float filterEnvDepth = juce::jlimit(0.0f, 1.0f, filterEnvAmount->load() + modulation.filterEnvAmount);
        const float filterDriveAmount = juce::jlimit(0.0f, 1.0f, filterDrive->load() + modulation.filterDrive);
        const float drive = 1.0f + filterDriveAmount * 9.0f;
        const float warpSaturationAmount = juce::jlimit(0.0f, 1.0f, warpSaturator->load() + modulation.warpSaturator);
        const float warpDrive = 1.0f + warpSaturationAmount * 14.0f;
        const float mutateAmount = juce::jlimit(0.0f, 1.0f, warpMutate->load() + modulation.warpMutate);
        const float noiseLevel = juce::jlimit(0.0f, 0.4f, noiseAmount->load() + modulation.noiseAmount);
        const float subOscLevel = juce::jlimit(0.0f, 1.0f, (subLevel != nullptr ? subLevel->load() : 0.0f) + modulation.subLevel);
        const float width = juce::jlimit(0.0f, 1.0f, masterSpread->load() + modulation.masterSpread);
        const float gain = 0.1f + juce::jlimit(0.0f, 1.0f, masterGain->load() + modulation.masterGain) * 0.42f;
        const float glideTime = juce::jlimit(0.0f, 2.0f, voiceGlide->load() + modulation.glide);
        const bool glideEnabled = voicePortamento->load() >= 0.5f && glideTime > 0.0001f;
        const float glideCoeff = glideEnabled ? (1.0f - std::exp(-1.0f / ((float) sampleRate * juce::jmax(0.0005f, glideTime)))) : 1.0f;
        const float pitchVibratoSemitones = lfo * modWheelValue * modWheelVibrato->load() * 1.5f;
        const float pitchModRatio = std::pow(2.0f, pitchVibratoSemitones / 12.0f);

        currentNoteFrequency += (targetNoteFrequency - currentNoteFrequency) * glideCoeff;
        const float currentNoteNumber = 69.0f + 12.0f * std::log2(juce::jmax(1.0f, currentNoteFrequency) / 440.0f);
        const float cutoffModOctaves = filterEnvDepth * filterEnvValue * 5.0f
                                     + modulation.filterCutoffOctaves
                                     + modWheelCutoffOctaves;
        const float targetCutoff = juce::jlimit(20.0f, 20000.0f, cutoffBase * std::pow(2.0f, cutoffModOctaves));
        smoothedCutoff += (targetCutoff - smoothedCutoff) * 0.18f;

        if (currentModeIndex != lastFilterModeIndex
            || filterUpdateCountdown <= 0
            || std::abs(smoothedCutoff - lastFilterCutoff) > 10.0f
            || std::abs(resonance - lastFilterResonance) > 0.03f)
        {
            if (filterEnabled)
                updateFilterState(currentMode, smoothedCutoff, resonance);

            lastFilterModeIndex = currentModeIndex;
            lastFilterCutoff = smoothedCutoff;
            lastFilterResonance = resonance;
            filterUpdateCountdown = 4;
        }
        else
        {
            --filterUpdateCountdown;
        }

        float filteredLeft = 0.0f;
        float filteredRight = 0.0f;
        float bypassLeft = 0.0f;
        float bypassRight = 0.0f;
        const float baseNoiseSample = renderNoiseSample(noiseTypeIndex);
        const std::array<float, 3> wavetablePositions{{
            juce::jlimit(0.0f, 1.0f, oscWTPosition[0] != nullptr ? oscWTPosition[0]->load() + modulation.oscWTPos[0] : modulation.oscWTPos[0]),
            juce::jlimit(0.0f, 1.0f, oscWTPosition[1] != nullptr ? oscWTPosition[1]->load() + modulation.oscWTPos[1] : modulation.oscWTPos[1]),
            juce::jlimit(0.0f, 1.0f, oscWTPosition[2] != nullptr ? oscWTPosition[2]->load() + modulation.oscWTPos[2] : modulation.oscWTPos[2])
        }};

        for (int osc = 0; osc < 3; ++osc)
        {
            const int oscTypeIndex = juce::roundToInt(oscType[(size_t) osc]->load());
            const float level = juce::jlimit(0.0f, 1.0f, oscLevel[(size_t) osc]->load() + modulation.oscLevel[(size_t) osc]);
            const int octaveIndex = juce::jlimit(0, 4, juce::roundToInt(oscOctave[(size_t) osc]->load()));
            const float octaveShift = octaveOffsets[(size_t) octaveIndex];
            const float detuneCents = oscDetune[(size_t) osc]->load() + modulation.oscDetune[(size_t) osc];
            const float oscBasePan = juce::jlimit(-1.0f, 1.0f,
                                                  (oscPan[(size_t) osc] != nullptr ? oscPan[(size_t) osc]->load() : 0.0f)
                                                  + modulation.oscPan[(size_t) osc]);
            const float oscWidthAmount = juce::jlimit(0.0f, 1.0f,
                                                      oscWidth[(size_t) osc] != nullptr ? oscWidth[(size_t) osc]->load() : 0.5f);
            const float sampleRootNote = oscSampleRoot[(size_t) osc]->load();
            const bool routeToFilter = filterEnabled && oscToFilter[(size_t) osc]->load() >= 0.5f;
            const bool unisonEnabled = oscUnisonOn[(size_t) osc]->load() >= 0.5f;
            const int selectedVoices = juce::jlimit(2, maxUnisonVoices, juce::roundToInt(oscUnisonVoices[(size_t) osc]->load()));
            const float unisonAmount = juce::jlimit(0.0f, 1.0f, oscUnison[(size_t) osc]->load() + modulation.oscUnison[(size_t) osc]);
            const std::array<float, 2> warpAmounts{{
                juce::jlimit(0.0f, 1.0f, oscWarpAmount[(size_t) osc][0]->load() + modulation.oscWarp1[(size_t) osc]),
                juce::jlimit(0.0f, 1.0f, oscWarpAmount[(size_t) osc][1]->load() + modulation.oscWarp2[(size_t) osc])
            }};
            const int voiceCount = unisonEnabled ? selectedVoices : 1;
            const float baseFrequency = currentNoteFrequency * pitchBendRatio * pitchModRatio * std::pow(2.0f, octaveShift) * std::pow(2.0f, detuneCents / 1200.0f);
            const float maxSpreadCents = 4.0f + unisonAmount * 34.0f;
            const float voiceGain = level / std::sqrt((float) voiceCount);

            for (int voice = 0; voice < voiceCount; ++voice)
            {
                const float voicePosition = voiceCount == 1 ? 0.0f : juce::jmap((float) voice, 0.0f, (float) (voiceCount - 1), -1.0f, 1.0f);
                const float unisonDetuneCents = voicePosition * maxSpreadCents;
                float oscSample = 0.0f;

                if (oscTypeIndex == 1)
                {
                    if (const auto& sampleData = oscSampleData[(size_t) osc];
                        sampleData != nullptr && sampleData->buffer.getNumSamples() > 0)
                    {
                        const float playbackNote = currentNoteNumber
                                                 + octaveShift * 12.0f
                                                 + (detuneCents + unisonDetuneCents) / 100.0f
                                                 + pitchBendSemitones
                                                 + pitchVibratoSemitones;
                        const double increment = (sampleData->sourceSampleRate / sampleRate)
                                               * std::pow(2.0, (playbackNote - sampleRootNote) / 12.0f);
                        const double sampleLength = (double) sampleData->buffer.getNumSamples();
                        auto& samplePosition = oscSamplePositions[(size_t) osc][(size_t) voice];
                        samplePosition += increment;

                        while (samplePosition >= sampleLength)
                            samplePosition -= sampleLength;
                        while (samplePosition < 0.0)
                            samplePosition += sampleLength;

                        float warpedPhase = (float) (samplePosition / sampleLength);
                        for (int slot = 0; slot < 2; ++slot)
                        {
                            const int modeIndex = juce::roundToInt(oscWarpMode[(size_t) osc][(size_t) slot]->load());
                            warpedPhase = applyWarpMode(osc, voice, warpedPhase, modeIndex, warpAmounts[(size_t) slot],
                                                        baseNoiseSample, lfo, mutateAmount, wavetablePositions);
                        }

                        const double warpedPosition = (double) wrapPhase(warpedPhase) * sampleLength;
                        oscSample = readInterpolatedSample(sampleData->buffer, warpedPosition);

                        if (mutateAmount > 0.0001f)
                        {
                            const float harmonicPhase = wrapPhase(warpedPhase + 0.11f * mutateAmount
                                * std::sin(twoPi * warpedPhase * (2.0f + (float) osc)));
                            const float harmonic = readInterpolatedSample(sampleData->buffer, (double) harmonicPhase * sampleLength);
                            const float folded = std::sin((oscSample + harmonic * 0.45f) * (1.0f + mutateAmount * 4.5f));
                            oscSample = juce::jlimit(-1.0f, 1.0f, juce::jmap(mutateAmount, oscSample, folded));
                        }

                        oscSample *= voiceGain;
                    }
                }
                else
                {
                    const float freq = baseFrequency * std::pow(2.0f, unisonDetuneCents / 1200.0f);
                    auto& phase = oscPhases[(size_t) osc][(size_t) voice];
                    const float phaseIncrement = freq / (float) sampleRate;

                    phase = wrapPhase(phase + phaseIncrement);

                    float warpedPhase = phase;
                    for (int slot = 0; slot < 2; ++slot)
                    {
                        const int modeIndex = juce::roundToInt(oscWarpMode[(size_t) osc][(size_t) slot]->load());
                        warpedPhase = applyWarpMode(osc, voice, warpedPhase, modeIndex, warpAmounts[(size_t) slot],
                                                    baseNoiseSample, lfo, mutateAmount, wavetablePositions);
                    }

                    oscSample = renderOscillatorSample(osc, warpedPhase, wavetablePositions);
                    oscSample = applyMutate(osc, warpedPhase, oscSample, mutateAmount, wavetablePositions) * voiceGain;
                }

                const float stereoSpread = 0.6f + oscWidthAmount * 0.8f;
                const float voicePan = juce::jlimit(-1.0f, 1.0f, oscBasePan + voicePosition * unisonAmount * 0.42f * stereoSpread);
                float oscLeft = oscSample * panLeft(voicePan);
                float oscRight = oscSample * panRight(voicePan);

                if (oscWidthAmount > 0.52f)
                {
                    float sideSample = 0.0f;
                    if (oscTypeIndex == 1)
                    {
                        if (const auto& sampleData = oscSampleData[(size_t) osc];
                            sampleData != nullptr && sampleData->buffer.getNumSamples() > 0)
                        {
                            const double sampleLength = (double) sampleData->buffer.getNumSamples();
                            const double offset = sampleLength * (0.0008 + (double) (oscWidthAmount - 0.5f) * 0.0026);
                            sideSample = readInterpolatedSample(sampleData->buffer, oscSamplePositions[(size_t) osc][(size_t) voice] + offset) * voiceGain;
                        }
                    }
                    else
                    {
                        const float sidePhase = wrapPhase(oscPhases[(size_t) osc][(size_t) voice] + 0.04f + (oscWidthAmount - 0.5f) * 0.10f);
                        sideSample = renderOscillatorSample(osc, sidePhase, wavetablePositions) * voiceGain;
                    }

                    const float sideAmount = (oscWidthAmount - 0.5f) * 0.34f;
                    oscLeft -= sideSample * sideAmount;
                    oscRight += sideSample * sideAmount;
                }

                if (routeToFilter)
                {
                    filteredLeft += oscLeft;
                    filteredRight += oscRight;
                }
                else
                {
                    bypassLeft += oscLeft;
                    bypassRight += oscRight;
                }
            }
        }

        if (subOscLevel > 0.0001f)
        {
            const int subWaveIndex = subWave != nullptr ? juce::jlimit(0, 3, juce::roundToInt(subWave->load())) : 0;
            const int subOctaveIndex = subOctave != nullptr ? juce::jlimit(0, 4, juce::roundToInt(subOctave->load())) : 1;
            const float subFrequency = currentNoteFrequency * pitchBendRatio * pitchModRatio
                                     * std::pow(2.0f, octaveOffsets[(size_t) subOctaveIndex]);
            subOscPhase = wrapPhase(subOscPhase + subFrequency / (float) sampleRate);

            float subSample = 0.0f;
            switch (subWaveIndex)
            {
                case 0: subSample = std::sin(twoPi * subOscPhase); break;
                case 1: subSample = 2.0f * subOscPhase - 1.0f; break;
                case 2: subSample = subOscPhase < 0.5f ? 1.0f : -1.0f; break;
                case 3: subSample = 1.0f - 4.0f * std::abs(subOscPhase - 0.5f); break;
                default: break;
            }

            subSample *= subOscLevel * 0.55f;

            if (filterEnabled && (subDirectOut == nullptr || subDirectOut->load() < 0.5f))
            {
                filteredLeft += subSample;
                filteredRight += subSample;
            }
            else
            {
                bypassLeft += subSample;
                bypassRight += subSample;
            }
        }

        if (!warpSaturationAfterFilter && warpSaturationAmount > 0.0001f)
        {
            filteredLeft = saturate(filteredLeft, warpDrive);
            filteredRight = saturate(filteredRight, warpDrive);
            bypassLeft = saturate(bypassLeft, warpDrive);
            bypassRight = saturate(bypassRight, warpDrive);
        }

        const float noiseSample = baseNoiseSample * noiseLevel;
        if (noiseUsesAmpEnv && noiseRoutedToFilter)
        {
            filteredLeft += noiseSample;
            filteredRight += noiseSample;
        }
        else if (noiseUsesAmpEnv)
        {
            bypassLeft += noiseSample;
            bypassRight += noiseSample;
        }

        if (filterEnabled)
        {
            filteredLeft = saturate(filteredLeft, drive);
            filteredRight = saturate(filteredRight, drive);
            filteredLeft = processFilterSample(0, filteredLeft, currentMode, smoothedCutoff, resonance);
            filteredRight = processFilterSample(1, filteredRight, currentMode, smoothedCutoff, resonance);
        }

        const float envGain = ampEnv * noteVelocity * gain;
        const float noiseEnvGain = noiseUsesAmpEnv ? envGain : noteVelocity * gain;
        const float oscOutLeft = (filteredLeft + bypassLeft) * envGain;
        const float oscOutRight = (filteredRight + bypassRight) * envGain;

        float extraNoiseLeft = 0.0f;
        float extraNoiseRight = 0.0f;
        if (!noiseUsesAmpEnv && noiseLevel > 0.0f)
        {
            const float dryNoise = renderNoiseSample(noiseTypeIndex) * noiseLevel;
            if (noiseRoutedToFilter && filterEnabled)
            {
                extraNoiseLeft = processFilterSample(0, saturate(dryNoise, drive), currentMode, smoothedCutoff, resonance) * noiseEnvGain;
                extraNoiseRight = processFilterSample(1, saturate(dryNoise, drive), currentMode, smoothedCutoff, resonance) * noiseEnvGain;
            }
            else
            {
                extraNoiseLeft = dryNoise * noiseEnvGain;
                extraNoiseRight = dryNoise * noiseEnvGain;
            }
        }

        float left = oscOutLeft + extraNoiseLeft;
        float right = oscOutRight + extraNoiseRight;
        if (warpSaturationAfterFilter && warpSaturationAmount > 0.0001f)
        {
            left = saturate(left, warpDrive);
            right = saturate(right, warpDrive);
        }

        const float mid = 0.5f * (left + right);
        const float side = 0.5f * (left - right) * width;
        left = mid + side;
        right = mid - side;

        outputBuffer.addSample(0, startSample + sample, left);

        if (outputChannels > 1 && outputBuffer.getNumChannels() > 1)
            outputBuffer.addSample(1, startSample + sample, right);
    }
}
