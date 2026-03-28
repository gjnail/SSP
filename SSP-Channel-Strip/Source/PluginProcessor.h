#pragma once

#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    enum PointType
    {
        bell = 0,
        lowShelf,
        highShelf,
        lowCut,
        highCut,
        notch,
        bandPass,
        tiltShelf
    };

    enum StereoMode
    {
        stereo = 0,
        left,
        right,
        mid,
        side
    };

    enum AnalyzerMode
    {
        analyzerPre = 0,
        analyzerPost,
        analyzerPrePost,
        analyzerSidechain
    };

    enum ProcessingMode
    {
        zeroLatency = 0,
        naturalPhase,
        linearPhase
    };

    enum ModuleKind
    {
        moduleInput = 0,
        moduleGate,
        moduleEq,
        moduleCompressor,
        moduleSaturation,
        moduleOutput,
        moduleCount
    };

    struct EqPoint
    {
        bool enabled = false;
        float frequency = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        int type = bell;
        int slopeIndex = 1;
        int stereoMode = stereo;
    };

    static constexpr int maxPoints = 24;
    static constexpr int maxStagesPerPoint = 8;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int reorderableModuleCount = 4;

    using PointArray = std::array<EqPoint, maxPoints>;
    using SpectrumArray = std::array<float, fftSize / 2>;
    using ModuleOrder = std::array<int, reorderableModuleCount>;

    struct AnalyzerFrame
    {
        SpectrumArray pre{};
        SpectrumArray post{};
        SpectrumArray sidechain{};
    };

    struct PointFilterSetup
    {
        std::array<juce::IIRCoefficients, maxStagesPerPoint> coeffs{};
        int numStages = 0;
    };

    struct StageMeterSnapshot
    {
        float peakDb = -60.0f;
        float rmsDb = -60.0f;
        float auxDb = -60.0f;
        bool clipped = false;
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& getPointTypeNames();
    static const juce::StringArray& getPointTypeDisplayNames();
    static const juce::StringArray& getSlopeNames();
    static const juce::StringArray& getStereoModeNames();
    static const juce::StringArray& getAnalyzerModeNames();
    static const juce::StringArray& getProcessingModeNames();
    static const juce::StringArray& getAnalyzerResolutionNames();
    static const juce::StringArray& getModuleNames();
    static const juce::StringArray& getGateModeNames();
    static const juce::StringArray& getCompressorCharacterNames();
    static const juce::StringArray& getSaturationCharacterNames();
    static const juce::StringArray& getFactoryPresetNames();

    EqPoint getPoint(int index) const;
    PointArray getPointsSnapshot() const;
    void setPoint(int index, const EqPoint& point);
    void setPointPosition(int index, float frequency, float gainDb);
    int addPoint(float frequency, float gainDb);
    void removePoint(int index);
    float getResponseForFrequency(float frequency) const;
    float getResponseForFrequencyByStereoMode(float frequency, int stereoMode) const;
    float getBandResponseForFrequency(int index, float frequency) const;
    int getEnabledPointCount() const;
    AnalyzerFrame getAnalyzerFrameCopy() const;

    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    float getOutputGainDb() const;
    bool isGlobalBypassed() const;
    int getAnalyzerMode() const;
    int getProcessingMode() const;
    int getAnalyzerResolution() const;
    float getAnalyzerDecay() const;

    StageMeterSnapshot getStageMeterSnapshot(int moduleIndex) const;
    void clearClipLatch(int moduleIndex);
    ModuleOrder getModuleOrder() const;
    void setModuleOrder(const ModuleOrder& newOrder);
    int getSoloModule() const noexcept;
    void setSoloModule(int moduleIndex) noexcept;
    bool isModuleCompareEnabled(int moduleIndex) const noexcept;
    void setModuleCompareEnabled(int moduleIndex, bool enabled) noexcept;
    float getModuleGainDeltaDb(int moduleIndex) const;
    void applyFactoryPreset(const juce::String& presetName);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncPointCacheFromParameters();
    void pushSpectrumSample(float sample) noexcept;
    void pushPreSpectrumSample(float sample) noexcept;
    void pushPostSpectrumSample(float sample) noexcept;
    void pushSidechainSpectrumSample(float sample) noexcept;
    void performSpectrumAnalysis(const std::array<float, fftSize>& source, SpectrumArray& destination, float decayAmount);
    float processPointForDomain(int domainIndex, int pointIndex, float sample);
    void updateFilterStates();
    void setStageMeterSnapshot(int moduleIndex, float peakLinear, double rmsAccum, int sampleCount, float auxDb, bool clipped, float inputRmsAccum = 0.0f, float outputRmsAccum = 0.0f);

    double currentSampleRate = 44100.0;
    std::array<std::array<std::array<juce::IIRFilter, maxStagesPerPoint>, maxPoints>, 4> filters;
    std::array<juce::IIRFilter, 2> inputHighPassFilters;
    std::array<juce::IIRFilter, 2> inputLowPassFilters;
    juce::IIRFilter gateSidechainHighPass;
    juce::IIRFilter gateSidechainLowPass;
    juce::IIRFilter compSidechainHighPass;
    juce::IIRFilter compSidechainLowPass;
    std::array<float, 2> satToneState{{0.0f, 0.0f}};
    std::array<float, 2> satDcInputState{{0.0f, 0.0f}};
    std::array<float, 2> satDcOutputState{{0.0f, 0.0f}};
    float gateEnvelope = 0.0f;
    float gateGainState = 1.0f;
    int gateHoldSamplesRemaining = 0;
    float compEnvelope = 0.0f;
    float compGainReductionDb = 0.0f;

    juce::SmoothedValue<float> inputTrimSmoothed;
    juce::SmoothedValue<float> outputTrimSmoothed;
    juce::SmoothedValue<float> outputWidthSmoothed;
    juce::SmoothedValue<float> outputBalanceSmoothed;
    juce::SmoothedValue<float> gateRangeSmoothed;
    juce::SmoothedValue<float> compMakeupSmoothed;
    juce::SmoothedValue<float> compMixSmoothed;
    juce::SmoothedValue<float> satDriveSmoothed;
    juce::SmoothedValue<float> satMixSmoothed;
    juce::SmoothedValue<float> satOutputSmoothed;

    mutable juce::SpinLock stateLock;
    mutable juce::SpinLock meterLock;
    PointArray pointCache{};
    std::array<PointFilterSetup, maxPoints> coefficientCache{};
    ModuleOrder moduleOrder{{moduleGate, moduleEq, moduleCompressor, moduleSaturation}};
    std::array<float, moduleCount> stagePeakDb{{-60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f}};
    std::array<float, moduleCount> stageRmsDb{{-60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f}};
    std::array<float, moduleCount> stageAuxDb{{-60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f}};
    std::array<bool, moduleCount> stageClipLatched{{false, false, false, false, false, false}};
    std::array<float, moduleCount> moduleGainDeltaDb{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<std::atomic<bool>, moduleCount> compareEnabled{};
    std::atomic<int> soloModule{-1};

    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    std::array<float, fftSize> preSpectrumFifo{};
    std::array<float, fftSize> postSpectrumFifo{};
    std::array<float, fftSize> sidechainSpectrumFifo{};
    std::array<float, fftSize * 2> fftData{};
    AnalyzerFrame analyzerFrame{};
    int preSpectrumFifoIndex = 0;
    int postSpectrumFifoIndex = 0;
    int sidechainSpectrumFifoIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
