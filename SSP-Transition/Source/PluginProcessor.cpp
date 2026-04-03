#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float macroControlMaximum = 1.5f;

struct TransitionPreset
{
    const char* name;
    const char* badge;
    const char* description;
    float highPassMaxHz;
    float lowPassMinHz;
    float delayMs;
    float delayFeedback;
    float delayWet;
    float reverbWet;
    float roomSize;
    float damping;
    float widthBoost;
    float noiseAmount;
    float riserAmount;
    float duckDb;
    float outputFadeDb;
    float responseCurve;
    float colourDrive;
    float buildStart;
    float wetBoostDb;
    float phaserRateHz;
    float phaserDepth;
    float phaserMix;
    float delayDriftAmount;
    float riserBaseHz;
    float riserOctaves;
    float riserMotionScale;
    float noiseTone;
    bool fullKillAtMax;
};

const std::array<TransitionPreset, 8>& getTransitionPresets()
{
    static const std::array<TransitionPreset, 8> presets{{
        { "Easy Wash", "WASH OUT", "A soft endless-style exit: tiny high-pass movement, wide reverb bloom, and almost no aggression.",
          220.0f, 19000.0f, 0.0f, 0.0f, 0.0f, 0.92f, 0.98f, 0.36f, 0.60f, 0.00f, 0.00f, 10.0f, 4.0f, 0.62f, 0.12f, 0.88f, 2.8f, 0.10f, 0.22f, 0.10f,
          0.012f, 180.0f, 2.0f, 0.65f, 0.18f, false },
        { "Gloss Fade", "SMOOTH EXIT", "Still a wash-out first, but with a little echo and motion so the tail feels finished.",
          480.0f, 15000.0f, 260.0f, 0.24f, 0.12f, 0.78f, 0.94f, 0.32f, 0.54f, 0.00f, 0.04f, 12.0f, 6.0f, 0.66f, 0.16f, 0.78f, 2.4f, 0.16f, 0.28f, 0.14f,
          0.018f, 220.0f, 2.1f, 0.70f, 0.30f, false },
        { "Helium Lift", "AIR BUILD", "Airy and bright, with almost no noise. This should feel like a clean upward pull, not a harsh riser.",
          1500.0f, 20000.0f, 260.0f, 0.30f, 0.12f, 0.62f, 0.80f, 0.18f, 0.52f, 0.00f, 0.08f, 11.0f, 9.0f, 0.72f, 0.16f, 0.54f, 1.2f, 0.28f, 0.18f, 0.06f,
          0.010f, 260.0f, 1.5f, 0.55f, 0.10f, false },
        { "Crowd Lift", "CLASSIC BUILD", "The obvious EDM build: high-pass, ping-pong space, a bit of grit, but still mostly musical and open.",
          2600.0f, 18500.0f, 380.0f, 0.36f, 0.20f, 0.70f, 0.90f, 0.22f, 0.70f, 0.06f, 0.18f, 13.0f, 10.0f, 0.86f, 0.24f, 0.38f, 1.8f, 0.18f, 0.22f, 0.10f,
          0.022f, 210.0f, 2.3f, 0.95f, 0.38f, false },
        { "Breakdown Melt", "SMEAR BUILD", "A darker, wetter bloom that melts into the next section rather than shouting at the drop.",
          1800.0f, 9500.0f, 620.0f, 0.52f, 0.36f, 1.00f, 1.00f, 0.18f, 0.60f, 0.00f, 0.02f, 14.0f, 11.0f, 0.76f, 0.18f, 0.62f, 3.6f, 0.10f, 0.54f, 0.32f,
          0.060f, 150.0f, 1.2f, 0.42f, 0.08f, false },
        { "Neon Push", "SHARP RISER", "A thin, bright, anxious riser. Less bloom, more bite, and the most obvious pre-drop panic.",
          6200.0f, 17500.0f, 300.0f, 0.32f, 0.18f, 0.58f, 0.74f, 0.18f, 0.52f, 0.34f, 0.48f, 18.0f, 18.0f, 1.02f, 0.30f, 0.34f, 0.8f, 0.30f, 0.12f, 0.06f,
          0.010f, 380.0f, 3.6f, 1.60f, 1.00f, false },
        { "Festival Hands", "WALL OF ENERGY", "A huge wet stadium lift. Slower, wider, darker, and more washed than sharp.",
          3000.0f, 11000.0f, 820.0f, 0.66f, 0.54f, 1.00f, 1.00f, 0.20f, 1.00f, 0.10f, 0.22f, 8.0f, 7.0f, 1.06f, 0.74f, 0.12f, 4.2f, 0.12f, 0.48f, 0.34f,
          0.085f, 130.0f, 1.4f, 0.68f, 0.14f, false },
        { "Vacuum Out", "KILL END", "The only true end-stop preset: narrow band, heavy duck, and a disappear-on-release ending.",
          6200.0f, 7200.0f, 220.0f, 0.18f, 0.12f, 0.32f, 0.74f, 0.42f, 0.12f, 0.00f, 0.00f, 20.0f, 24.0f, 1.08f, 0.12f, 0.94f, 1.2f, 0.10f, 0.12f, 0.08f,
          0.012f, 180.0f, 1.8f, 0.58f, 0.12f, true },
    }};

    return presets;
}

float normalise(float value, float minimum, float maximum)
{
    return juce::jlimit(0.0f, 1.0f, (value - minimum) / (maximum - minimum));
}

float shapeAmount(float amount, float curve)
{
    return std::pow(juce::jlimit(0.0f, 1.0f, amount), juce::jmax(0.2f, curve));
}

float buildMacro(float amount, float start)
{
    if (start >= 0.999f)
        return 0.0f;

    const float remapped = juce::jlimit(0.0f, 1.0f, (amount - start) / (1.0f - start));
    return remapped * remapped * (3.0f - 2.0f * remapped);
}

float clampMacroControl(float value)
{
    return juce::jlimit(0.0f, macroControlMaximum, value);
}

const std::array<const char*, 5>& getResettableParameterIds()
{
    static const std::array<const char*, 5> ids{{
        "mix",
        "filter",
        "space",
        "width",
        "rise"
    }};

    return ids;
}

int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        return param->getIndex();

    return 0;
}

const TransitionPreset& getPresetForIndex(int index)
{
    const auto& presets = getTransitionPresets();
    return presets[(size_t) juce::jlimit(0, (int) presets.size() - 1, index)];
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount",
        "Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix",
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filter",
        "Filter",
        juce::NormalisableRange<float>(0.0f, macroControlMaximum, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "space",
        "Space",
        juce::NormalisableRange<float>(0.0f, macroControlMaximum, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "width",
        "Width",
        juce::NormalisableRange<float>(0.0f, macroControlMaximum, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rise",
        "Rise",
        juce::NormalisableRange<float>(0.0f, macroControlMaximum, 0.001f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "preset",
        "Preset",
        getPresetNames(),
        0));

    return {params.begin(), params.end()};
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    apvts.addParameterListener("preset", this);
}

PluginProcessor::~PluginProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener("preset", this);
}

const juce::StringArray& PluginProcessor::getPresetNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& preset : getTransitionPresets())
            result.add(preset.name);
        return result;
    }();

    return names;
}

juce::String PluginProcessor::getCurrentPresetDescription() const
{
    return getPresetForIndex(getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "preset")).description;
}

PluginProcessor::TransitionVisualState PluginProcessor::getVisualState() const
{
    const auto& preset = getPresetForIndex(getChoiceIndex(const_cast<juce::AudioProcessorValueTreeState&>(apvts), "preset"));
    const float amount = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("amount")->load());
    const float mix = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    const float filterControl = clampMacroControl(apvts.getRawParameterValue("filter")->load());
    const float spaceControl = clampMacroControl(apvts.getRawParameterValue("space")->load());
    const float widthControl = clampMacroControl(apvts.getRawParameterValue("width")->load());
    const float riseControl = clampMacroControl(apvts.getRawParameterValue("rise")->load());
    const float shapedAmount = shapeAmount(amount, preset.responseCurve);
    const float buildAmountShaped = buildMacro(amount, preset.buildStart);
    const float filterBlend = juce::jlimit(0.0f, 1.0f, (shapedAmount * 0.45f + buildAmountShaped * 0.75f) * filterControl);
    const float visibleAmount = shapedAmount * (0.25f + 0.75f * mix);

    TransitionVisualState state;
    state.amount = amount;
    state.mix = mix;
    state.shapedAmount = shapedAmount;
    state.filterAmount = juce::jlimit(0.0f, 1.0f, visibleAmount * filterBlend * normalise(preset.highPassMaxHz, 180.0f, 6200.0f));
    state.spaceAmount = juce::jlimit(0.0f, 1.0f, visibleAmount * (0.25f + preset.reverbWet * 0.55f + preset.delayWet * 0.45f) * spaceControl);
    state.widthAmount = juce::jlimit(0.0f, 1.0f, visibleAmount * juce::jlimit(0.0f, 1.0f, preset.widthBoost / 0.9f) * widthControl);
    state.riseAmount = juce::jlimit(0.0f, 1.0f, visibleAmount * buildAmountShaped * juce::jlimit(0.0f, 1.0f, preset.riserAmount + preset.noiseAmount * 0.55f) * riseControl);
    state.duckAmount = visibleAmount * juce::jlimit(0.0f, 1.0f, (preset.duckDb + preset.outputFadeDb * 0.5f) / 24.0f);
    state.presetName = preset.name;
    state.presetBadge = preset.badge;
    state.presetDescription = preset.description;
    return state;
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    spec.numChannels = 1;

    auto stereoSpec = spec;
    stereoSpec.numChannels = 2;

    for (auto& filter : highPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency(20.0f);
    }

    for (auto& filter : lowPassFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(20000.0f);
    }

    for (auto& delayLine : delayLines)
    {
        delayLine.reset();
        delayLine.prepare(spec);
        delayLine.setDelay(1.0f);
    }

    phaser.reset();
    phaser.prepare(stereoSpec);
    phaser.setRate(0.15f);
    phaser.setDepth(0.2f);
    phaser.setFeedback(0.0f);
    phaser.setMix(0.0f);
    phaser.setCentreFrequency(900.0f);

    reverb.reset();
    amountSmoothed.reset(sampleRate, 0.08);
    mixSmoothed.reset(sampleRate, 0.06);
    filterSmoothed.reset(sampleRate, 0.08);
    spaceSmoothed.reset(sampleRate, 0.08);
    widthSmoothed.reset(sampleRate, 0.08);
    riseSmoothed.reset(sampleRate, 0.08);
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    filterSmoothed.setCurrentAndTargetValue(clampMacroControl(apvts.getRawParameterValue("filter")->load()));
    spaceSmoothed.setCurrentAndTargetValue(clampMacroControl(apvts.getRawParameterValue("space")->load()));
    widthSmoothed.setCurrentAndTargetValue(clampMacroControl(apvts.getRawParameterValue("width")->load()));
    riseSmoothed.setCurrentAndTargetValue(clampMacroControl(apvts.getRawParameterValue("rise")->load()));
    riserPhases.fill(0.0);
    riserMotionPhase = 0.0;
    previousNoiseInput.fill(0.0f);
    highPassedNoiseState.fill(0.0f);
    lowPassedNoiseState.fill(0.0f);
    excitationEnvelope = 0.0f;
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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numChannels <= 0)
        return;

    const float amountTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("amount")->load());
    const float mixTarget = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load());
    const float filterTarget = clampMacroControl(apvts.getRawParameterValue("filter")->load());
    const float spaceTarget = clampMacroControl(apvts.getRawParameterValue("space")->load());
    const float widthTarget = clampMacroControl(apvts.getRawParameterValue("width")->load());
    const float riseTarget = clampMacroControl(apvts.getRawParameterValue("rise")->load());
    amountSmoothed.setTargetValue(amountTarget);
    mixSmoothed.setTargetValue(mixTarget);
    filterSmoothed.setTargetValue(filterTarget);
    spaceSmoothed.setTargetValue(spaceTarget);
    widthSmoothed.setTargetValue(widthTarget);
    riseSmoothed.setTargetValue(riseTarget);

    if (amountTarget <= 0.0001f || mixTarget <= 0.0001f)
    {
        amountSmoothed.setCurrentAndTargetValue(amountTarget);
        mixSmoothed.setCurrentAndTargetValue(mixTarget);
        filterSmoothed.setCurrentAndTargetValue(filterTarget);
        spaceSmoothed.setCurrentAndTargetValue(spaceTarget);
        widthSmoothed.setCurrentAndTargetValue(widthTarget);
        riseSmoothed.setCurrentAndTargetValue(riseTarget);
        phaser.reset();
        reverb.reset();
        for (auto& delayLine : delayLines)
            delayLine.reset();
        excitationEnvelope = 0.0f;
        return;
    }

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);
    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf(buffer, true);

    const auto& preset = getPresetForIndex(getChoiceIndex(apvts, "preset"));

    const float amount = juce::jlimit(0.0f, 1.0f, amountSmoothed.skip(numSamples));
    const float outputMix = juce::jlimit(0.0f, 1.0f, mixSmoothed.skip(numSamples));
    const float filterControl = clampMacroControl(filterSmoothed.skip(numSamples));
    const float spaceControl = clampMacroControl(spaceSmoothed.skip(numSamples));
    const float widthControl = clampMacroControl(widthSmoothed.skip(numSamples));
    const float riseControl = clampMacroControl(riseSmoothed.skip(numSamples));
    const float washAmount = shapeAmount(amount, preset.responseCurve);
    const float riseStage = buildMacro(amount, preset.buildStart);
    const float filterBlend = juce::jlimit(0.0f, 1.0f, (washAmount * 0.45f + riseStage * 0.75f) * filterControl);
    const float lowPassBlend = juce::jlimit(0.0f, 1.0f, (washAmount * 0.22f + riseStage * 0.72f) * filterControl);
    const float highPassHz = juce::jlimit(20.0f, 18000.0f, 20.0f + filterBlend * (preset.highPassMaxHz - 20.0f));
    const float lowPassHz = juce::jlimit(3200.0f, 20000.0f, 20000.0f - lowPassBlend * (20000.0f - preset.lowPassMinHz));
    const float delaySamplesLeft = preset.delayMs > 0.0f ? juce::jlimit(1.0f, 220500.0f, (float) currentSampleRate * preset.delayMs * 0.001f) : 1.0f;
    const float delaySamplesRight = preset.delayMs > 0.0f ? juce::jlimit(1.0f, 220500.0f, (float) currentSampleRate * preset.delayMs * 0.001f * 1.18f) : 1.0f;
    const float spaceWeight = spaceControl;
    const float widthWeight = widthControl;
    const float riseWeight = riseControl;
    const float delayFeedback = juce::jlimit(0.0f, 0.84f, preset.delayFeedback * (0.45f + washAmount * 0.55f) * spaceWeight);
    const float delayWet = juce::jlimit(0.0f, 1.0f, preset.delayWet * (0.30f + washAmount * 0.70f) * spaceWeight);
    const float dryFadeShape = std::pow(juce::jlimit(0.0f, 1.0f, 1.0f - washAmount), 1.6f);
    const float presetDryDuck = juce::Decibels::decibelsToGain(-(preset.duckDb * washAmount + preset.outputFadeDb * riseStage));
    const float dryGain = presetDryDuck * dryFadeShape;
    const float wetGain = juce::Decibels::decibelsToGain(preset.wetBoostDb * washAmount) * spaceWeight;
    const float risePresence = std::pow(riseStage, 1.65f);
    const float noiseLevel = 0.20f * preset.noiseAmount * risePresence * (0.20f + 0.80f * washAmount) * riseWeight;
    const float riserLevel = 0.26f * preset.riserAmount * risePresence * (0.25f + 0.75f * washAmount) * riseWeight;
    const float drive = 1.0f + preset.colourDrive * (0.35f + washAmount * 0.65f + riseStage * 0.45f);
    const float phaserMix = juce::jlimit(0.0f, 1.0f, preset.phaserMix * (0.15f + 0.85f * washAmount) * spaceWeight);
    const float phaserDepth = juce::jlimit(0.0f, 1.0f, preset.phaserDepth * (0.20f + 0.80f * washAmount));
    const float phaserRate = preset.phaserRateHz * (0.80f + 0.60f * riseStage) * (0.70f + 0.30f * riseWeight);
    const float delayDrift = preset.delayDriftAmount * (0.15f + 0.85f * juce::jmax(washAmount, riseStage)) * (0.75f + 0.25f * spaceWeight);
    const float sourceCarry = juce::jlimit(0.015f, 0.48f, 0.48f - washAmount * 0.34f - riseStage * 0.16f);
    const float noiseTone = juce::jlimit(0.0f, 1.0f, preset.noiseTone);
    const double motionAdvance = preset.riserMotionScale * (0.08 + 0.28 * washAmount + 0.64 * riseStage) * (0.65 + 0.35 * riseWeight) / currentSampleRate;
    const double baseRiseHz = preset.riserBaseHz + 140.0 * washAmount + preset.highPassMaxHz * 0.018 * (0.65 + 0.35 * filterControl);
    const double twoPi = juce::MathConstants<double>::twoPi;
    const float excitationAttack = 0.22f;
    const float excitationReleaseCoeff = std::exp(-1.0f / ((float) currentSampleRate * 0.60f));
    const float feedbackScaleFloor = 0.62f;

    for (auto& filter : highPassFilters)
        filter.setCutoffFrequency(highPassHz);

    for (auto& filter : lowPassFilters)
        filter.setCutoffFrequency(lowPassHz);

    phaser.setRate(phaserRate);
    phaser.setDepth(phaserDepth);
    phaser.setFeedback(juce::jlimit(-0.95f, 0.95f, 0.06f + 0.20f * washAmount));
    phaser.setMix(phaserMix);
    phaser.setCentreFrequency(juce::jlimit(180.0f, 1800.0f, 420.0f + highPassHz * 0.26f));

    auto* wetLeft = wetBuffer.getWritePointer(0);
    auto* wetRight = numChannels > 1 ? wetBuffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        riserMotionPhase += motionAdvance;
        riserMotionPhase -= std::floor(riserMotionPhase);

        const float delayModLeft = 1.0f + delayDrift * std::sin((float) (twoPi * riserMotionPhase));
        const float delayModRight = 1.0f + delayDrift * std::sin((float) (twoPi * (riserMotionPhase + 0.19)));
        delayLines[0].setDelay(delaySamplesLeft * delayModLeft);
        delayLines[1].setDelay(delaySamplesRight * delayModRight);

        const float inputLeft = wetLeft[sample];
        const float inputRight = wetRight != nullptr ? wetRight[sample] : inputLeft;
        const float inputDrive = juce::jlimit(0.0f, 1.0f, 2.8f * 0.5f * (std::abs(inputLeft) + std::abs(inputRight)));

        if (inputDrive > excitationEnvelope)
            excitationEnvelope += (inputDrive - excitationEnvelope) * excitationAttack;
        else
            excitationEnvelope = inputDrive + excitationReleaseCoeff * (excitationEnvelope - inputDrive);

        const float syntheticExcitation = juce::jlimit(0.0f, 1.0f, excitationEnvelope);
        const float dynamicFeedback = delayFeedback * (feedbackScaleFloor + (1.0f - feedbackScaleFloor) * syntheticExcitation);

        float filteredLeft = highPassFilters[0].processSample(0, inputLeft);
        filteredLeft = lowPassFilters[0].processSample(0, filteredLeft);

        float filteredRight = filteredLeft;
        if (wetRight != nullptr)
        {
            filteredRight = highPassFilters[1].processSample(0, inputRight);
            filteredRight = lowPassFilters[1].processSample(0, filteredRight);
        }

        const float delayedLeft = delayLines[0].popSample(0);
        const float delayedRight = wetRight != nullptr ? delayLines[1].popSample(0) : delayedLeft;

        delayLines[0].pushSample(0, filteredLeft + delayedRight * dynamicFeedback);
        if (wetRight != nullptr)
            delayLines[1].pushSample(0, filteredRight + delayedLeft * dynamicFeedback);

        float barberTone = 0.0f;
        for (size_t voice = 0; voice < riserPhases.size(); ++voice)
        {
            double voicePosition = riserMotionPhase + (double) voice / (double) riserPhases.size();
            voicePosition -= std::floor(voicePosition);

            float envelope = std::sin((float) voicePosition * juce::MathConstants<float>::pi);
            envelope *= envelope;

            const double voiceFrequency = baseRiseHz * std::pow(2.0, voicePosition * preset.riserOctaves);
            riserPhases[voice] += twoPi * voiceFrequency / currentSampleRate;
            if (riserPhases[voice] >= twoPi)
                riserPhases[voice] -= twoPi * std::floor(riserPhases[voice] / twoPi);

            barberTone += std::sin((float) riserPhases[voice]) * envelope;
        }
        barberTone *= 0.24f;

        const float whiteLeft = random.nextFloat() * 2.0f - 1.0f;
        const float whiteRight = random.nextFloat() * 2.0f - 1.0f;
        highPassedNoiseState[0] = 0.985f * (highPassedNoiseState[0] + whiteLeft - previousNoiseInput[0]);
        highPassedNoiseState[1] = 0.985f * (highPassedNoiseState[1] + whiteRight - previousNoiseInput[1]);
        const float lowpassCoeff = 0.010f + 0.080f * (1.0f - noiseTone);
        lowPassedNoiseState[0] += lowpassCoeff * (whiteLeft - lowPassedNoiseState[0]);
        lowPassedNoiseState[1] += lowpassCoeff * (whiteRight - lowPassedNoiseState[1]);
        previousNoiseInput[0] = whiteLeft;
        previousNoiseInput[1] = whiteRight;

        const float stereoPush = std::sin((float) (twoPi * riserMotionPhase)) * 0.18f;
        const float colouredNoiseLeft = lowPassedNoiseState[0] * (1.0f - noiseTone) + highPassedNoiseState[0] * noiseTone;
        const float colouredNoiseRight = lowPassedNoiseState[1] * (1.0f - noiseTone) + highPassedNoiseState[1] * noiseTone;
        const float riseLeft = syntheticExcitation * (barberTone * riserLevel * (1.0f - stereoPush) + colouredNoiseLeft * noiseLevel);
        const float riseRight = syntheticExcitation * (barberTone * riserLevel * (1.0f + stereoPush) + colouredNoiseRight * noiseLevel);

        wetLeft[sample] = std::tanh((filteredLeft * sourceCarry + delayedLeft * delayWet + riseLeft) * drive);

        if (wetRight != nullptr)
            wetRight[sample] = std::tanh((filteredRight * sourceCarry + delayedRight * delayWet + riseRight) * drive);
    }

    juce::dsp::AudioBlock<float> wetBlock(wetBuffer);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);
    phaser.process(wetContext);

    juce::Reverb::Parameters reverbParams;
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, (0.46f + washAmount * preset.roomSize * 0.52f) * (0.40f + 0.60f * spaceWeight));
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, 0.14f + preset.damping * 0.86f);
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, (0.18f + washAmount * preset.reverbWet * 0.78f) * spaceWeight);
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, 0.74f - washAmount * 0.44f);
    reverbParams.width = juce::jlimit(0.0f, 1.0f, (0.32f + washAmount * 0.68f) * (0.35f + 0.65f * widthWeight));
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    if (numChannels >= 2)
        reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1), numSamples);
    else if (numChannels == 1)
        reverb.processMono(wetBuffer.getWritePointer(0), numSamples);

    if (numChannels >= 2)
    {
        const float widthScale = 1.0f + preset.widthBoost * widthWeight * (0.25f + 0.75f * washAmount);
        auto* left = wetBuffer.getWritePointer(0);
        auto* right = wetBuffer.getWritePointer(1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float mid = 0.5f * (left[sample] + right[sample]);
            const float side = 0.5f * (left[sample] - right[sample]) * widthScale;
            left[sample] = mid + side;
            right[sample] = mid - side;
        }
    }

    wetBuffer.applyGain(wetGain);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* out = buffer.getWritePointer(channel);
        const auto* dry = dryBuffer.getReadPointer(channel);
        const auto* wet = wetBuffer.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float processed = dry[sample] * dryGain + wet[sample];

            if (preset.fullKillAtMax && amount >= 0.999f)
                processed = 0.0f;

            processed = std::tanh(processed * (1.0f + riseStage * 0.18f));
            out[sample] = dry[sample] * (1.0f - outputMix) + processed * outputMix;
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

void PluginProcessor::parameterChanged(const juce::String& parameterId, float)
{
    if (parameterId != "preset")
        return;

    presetResetPending.store(true);
    triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    if (! presetResetPending.exchange(false))
        return;

    resetMacroParametersToDefaults();
}

void PluginProcessor::resetMacroParametersToDefaults()
{
    for (const auto* paramId : getResettableParameterIds())
    {
        if (auto* parameter = apvts.getParameter(paramId))
        {
            const float defaultValue = parameter->getDefaultValue();

            if (std::abs(parameter->getValue() - defaultValue) < 0.0001f)
                continue;

            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(defaultValue);
            parameter->endChangeGesture();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
