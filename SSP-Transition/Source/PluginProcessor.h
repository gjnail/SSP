#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
{
public:
    struct TransitionVisualState
    {
        float amount = 0.0f;
        float mix = 1.0f;
        float shapedAmount = 0.0f;
        float filterAmount = 0.0f;
        float spaceAmount = 0.0f;
        float widthAmount = 0.0f;
        float riseAmount = 0.0f;
        float duckAmount = 0.0f;
        juce::String presetName;
        juce::String presetBadge;
        juce::String presetDescription;
    };

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& getPresetNames();
    juce::String getCurrentPresetDescription() const;
    TransitionVisualState getVisualState() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String&, float) override;
    void handleAsyncUpdate() override;
    void resetMacroParametersToDefaults();

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> highPassFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowPassFilters;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLines{
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>(220500),
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>(220500)
    };
    juce::dsp::Phaser<float> phaser;
    juce::Reverb reverb;
    juce::SmoothedValue<float> amountSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> filterSmoothed;
    juce::SmoothedValue<float> spaceSmoothed;
    juce::SmoothedValue<float> widthSmoothed;
    juce::SmoothedValue<float> riseSmoothed;
    juce::Random random;
    std::array<double, 3> riserPhases{};
    double riserMotionPhase = 0.0;
    std::array<float, 2> previousNoiseInput{};
    std::array<float, 2> highPassedNoiseState{};
    std::array<float, 2> lowPassedNoiseState{};
    float excitationEnvelope = 0.0f;
    double currentSampleRate = 44100.0;
    std::atomic<bool> presetResetPending{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
