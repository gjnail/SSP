#pragma once

#include <JuceHeader.h>

/** Log-frequency band display with draggable crossover lines (low / mid / high). */
class BandCrossoverEQComponent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit BandCrossoverEQComponent(juce::AudioProcessorValueTreeState&);
    ~BandCrossoverEQComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    void applyCrossoverFromX(int x, int which);
    [[nodiscard]] int hitTestCrossover(int x) const;

    juce::AudioProcessorValueTreeState& apvts;

    float lastLowHz = -1.0f;
    float lastHighHz = -1.0f;

    int dragWhich = 0; // 0 = none, 1 = low crossover, 2 = high crossover
    juce::Point<int> lastMouse;

    static constexpr const char* lowParamId = "lowCrossHz";
    static constexpr const char* highParamId = "highCrossHz";

    [[nodiscard]] juce::Rectangle<float> getPlotBounds() const;
    [[nodiscard]] float xToFrequency(float x) const;
    [[nodiscard]] float frequencyToX(float hz) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandCrossoverEQComponent)
};
