#pragma once

#include <array>
#include <memory>
#include <vector>
#include <JuceHeader.h>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    enum DisplayMode
    {
        stereo = 0,
        leftRight,
        midSide,
        stereoSidechain
    };

    enum FftSizeMode
    {
        fft2048 = 0,
        fft4096,
        fft8192,
        fft16384
    };

    enum WindowMode
    {
        hann = 0,
        blackmanHarris,
        flatTop
    };

    static constexpr int maxFftOrder = 14;
    static constexpr int maxFftSize = 1 << maxFftOrder;
    static constexpr int waveformHistorySize = 1 << 17;
    static constexpr int displayPointCount = 640;
    static constexpr int waveformPointCount = 420;
    static constexpr int vectorscopePointCount = 192;
    static constexpr float silenceDb = -120.0f;

    struct ScopePoint
    {
        float mid = 0.0f;
        float side = 0.0f;
    };

    struct FactoryPreset
    {
        juce::String name;
        juce::String description;
        int displayMode = stereo;
        int fftSize = fft8192;
        int overlap = 2;
        float average = 55.0f;
        float smooth = 28.0f;
        float slope = 4.5f;
        float range = 84.0f;
        float hold = 2.5f;
        int window = hann;
        int theme = 0;
        bool freeze = false;
        bool waterfall = true;
    };

    struct AnalyzerSnapshot
    {
        std::array<float, displayPointCount> primary{};
        std::array<float, displayPointCount> secondary{};
        std::array<float, displayPointCount> sidechain{};
        std::array<float, displayPointCount> peakHold{};
        std::array<float, displayPointCount> maxHold{};
        std::array<float, waveformPointCount> waveformMin{};
        std::array<float, waveformPointCount> waveformMax{};
        std::array<ScopePoint, vectorscopePointCount> vectorscope{};
        bool secondaryVisible = false;
        bool sidechainVisible = false;
        bool sidechainConnected = false;
        bool clipping = false;
        bool freezeActive = false;
        bool waterfallEnabled = true;
        int displayMode = stereo;
        int themeIndex = 0;
        float rangeDb = 84.0f;
        float peakLeftDb = silenceDb;
        float peakRightDb = silenceDb;
        float rmsLeftDb = silenceDb;
        float rmsRightDb = silenceDb;
        float sidechainPeakDb = silenceDb;
        float correlation = 1.0f;
        float shortLoudnessDb = silenceDb;
        float crestDb = 0.0f;
        float dominantFrequencyHz = 0.0f;
        float dominantDb = silenceDb;
        double sampleRate = 44100.0;
        float waveformSpanSeconds = 0.0f;
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

    AnalyzerSnapshot getAnalyzerSnapshotCopy() const;
    void clearAnalyzerPeaks();
    void clearClipFlag();

    int getThemeIndex() const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    int getMatchingFactoryPresetIndex() const;
    juce::String getCurrentPresetName() const;
    juce::String getCurrentPresetDescription() const;
    bool loadFactoryPreset(int index);
    bool stepFactoryPreset(int delta);

    static const juce::StringArray& getDisplayModeNames();
    static const juce::StringArray& getFftSizeNames();
    static const juce::StringArray& getOverlapNames();
    static const juce::StringArray& getWindowNames();
    static const juce::StringArray& getThemeNames();
    static const juce::Array<FactoryPreset>& getFactoryPresets();

private:
    struct TraceBuffers
    {
        std::array<float, displayPointCount> averageMag{};
        std::array<float, displayPointCount> displayDb{};
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float getRawValue(const juce::String& parameterId) const;
    int getChoiceValue(const juce::String& parameterId) const;

    void cacheParameterPointers();
    void setChoiceParameter(const juce::String& parameterId, int value);
    void setFloatParameter(const juce::String& parameterId, float value);
    void setBoolParameter(const juce::String& parameterId, bool value);
    bool currentSettingsMatchPreset(const FactoryPreset&) const;
    void pushInputFrame(float left, float right, float sidechain) noexcept;
    void updateMeters(const juce::AudioBuffer<float>& mainBuffer, const juce::AudioBuffer<float>& sidechainBuffer);
    void renderAnalyzerFrame();
    void updateWindowTable(int fftChoice, int windowChoice);
    void fillRecentSamples(std::array<float, maxFftSize>& destination, const std::array<float, maxFftSize>& source, int count) const;
    void buildWaveformSummary(std::array<float, waveformPointCount>& minValues,
                              std::array<float, waveformPointCount>& maxValues,
                              float& spanSeconds) const;
    void buildVectorscope(std::array<ScopePoint, vectorscopePointCount>& points) const;
    void performSpectrum(std::array<float, displayPointCount>& destinationDb,
                         TraceBuffers& trace,
                         const std::array<float, maxFftSize>& timeDomain,
                         int fftChoice,
                         float slopeDbPerOct,
                         float smoothingAmount,
                         float averagingAmount);
    static void smoothAcrossBins(std::array<float, displayPointCount>& values, float amount);
    static float frequencyAtPosition(float normalised, double sampleRate);
    static float magnitudeToDb(float magnitude);
    static float noteNumberForFrequency(float frequency);

    double currentSampleRate = 44100.0;
    int writePosition = 0;
    int waveformWritePosition = 0;
    int samplesSinceLastFrame = 0;

    std::array<float, maxFftSize> historyLeft{};
    std::array<float, maxFftSize> historyRight{};
    std::array<float, maxFftSize> historySidechain{};
    std::array<float, waveformHistorySize> waveformLeftHistory{};
    std::array<float, waveformHistorySize> waveformRightHistory{};
    std::array<float, maxFftSize> tempPrimaryInput{};
    std::array<float, maxFftSize> tempSecondaryInput{};
    std::array<float, maxFftSize> tempSidechainInput{};
    std::array<float, maxFftSize * 2> fftWorkA{};
    std::array<float, maxFftSize * 2> fftWorkB{};

    std::array<std::unique_ptr<juce::dsp::FFT>, 4> fftObjects;
    std::vector<float> windowTable;
    int cachedWindowFftChoice = -1;
    int cachedWindowChoice = -1;

    TraceBuffers primaryTrace;
    TraceBuffers secondaryTrace;
    TraceBuffers sidechainTrace;
    std::array<float, displayPointCount> peakHoldTrace{};
    std::array<float, displayPointCount> peakHoldTimers{};
    std::array<float, displayPointCount> maxHoldTrace{};

    AnalyzerSnapshot analyzerSnapshot;
    mutable juce::SpinLock analyzerLock;

    std::atomic<float>* displayModeParam = nullptr;
    std::atomic<float>* fftSizeParam = nullptr;
    std::atomic<float>* overlapParam = nullptr;
    std::atomic<float>* averageParam = nullptr;
    std::atomic<float>* smoothParam = nullptr;
    std::atomic<float>* slopeParam = nullptr;
    std::atomic<float>* rangeParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* windowParam = nullptr;
    std::atomic<float>* themeParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* waterfallParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
