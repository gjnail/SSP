#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ssp::ui
{
juce::Colour getStereoModeColour(int stereoMode);
juce::String getStereoModeShortLabel(int stereoMode);

class VectorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};

VectorLookAndFeel& getVectorLookAndFeel();

class SSPKnob final : public juce::Slider
{
public:
    SSPKnob();
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
};

class SSPButton final : public juce::TextButton
{
public:
    explicit SSPButton(const juce::String& text = {});
};

class SSPToggle final : public juce::ToggleButton
{
public:
    explicit SSPToggle(const juce::String& text = {});
};

class SSPDropdown final : public juce::ComboBox
{
public:
    SSPDropdown();
};

class SSPSlider final : public juce::Slider
{
public:
    SSPSlider();
};
} // namespace ssp::ui
