#pragma once

#include <JuceHeader.h>
#include "CurveInterp.h"

class PluginProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numSCBands = 1;

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    void setBandCurve(int band, std::vector<CurvePoint> points);
    void getBandCurve(int band, std::vector<CurvePoint>& out) const;
    float getBandSidechainLevel(int band) const noexcept;
    float getBandGainReduction(int band) const noexcept;
    float getTriggerActivity() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void initDefaultCurve();
    void syncCurveToValueTree();
    void loadCurveFromValueTree();

    double currentSampleRate = 44100.0;

    std::atomic<float> sidechainMeter{0.0f};
    std::atomic<float> gainReductionMeter{0.0f};
    std::atomic<float> triggerActivity{0.0f};
    float smoothedVolumeGain = 1.0f;

    double fallbackSyncPhase = 0.0;
    double midiTriggerPhase = 1.0;
    double audioTriggerPhase = 1.0;
    bool midiTriggerActive = false;
    bool audioTriggerActive = false;
    bool audioTriggerArmed = true;
    float audioTriggerDetector = 0.0f;

    std::vector<CurvePoint> bandCurve;
    mutable juce::CriticalSection curveLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
