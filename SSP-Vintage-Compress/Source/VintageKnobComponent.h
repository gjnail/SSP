#pragma once

#include <JuceHeader.h>

class VintageKnobComponent final : public juce::Component
{
public:
    VintageKnobComponent(juce::String labelText, juce::Colour accentColour, bool isLargeKnob = false);
    ~VintageKnobComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    juce::Slider& getSlider() noexcept { return slider; }
    void setValueFormatter(std::function<juce::String(double)> formatter);

private:
    class KnobLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit KnobLookAndFeel(juce::Colour accent);

        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider&) override;

    private:
        juce::Colour accent;
    };

    juce::String labelText;
    juce::Colour accentColour;
    bool isLargeKnob = false;
    KnobLookAndFeel lookAndFeel;
    juce::Slider slider;
    std::function<juce::String(double)> valueFormatter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageKnobComponent)
};
