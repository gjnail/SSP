#pragma once

#include <JuceHeader.h>

namespace clipperui
{
juce::Colour textStrong();
juce::Colour textMuted();
juce::Colour accentGold();
juce::Colour accentCyan();
juce::Colour accentCoral();
juce::Colour accentMint();

juce::Font titleFont(float size);
juce::Font sectionFont(float size);
juce::Font bodyFont(float size);

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds);
void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float radius = 18.0f);

class VectorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override;
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
};

VectorLookAndFeel& getLookAndFeel();

class Knob final : public juce::Slider
{
public:
    Knob();
};

class Button final : public juce::TextButton
{
public:
    explicit Button(const juce::String& text);
};

class Dropdown final : public juce::ComboBox
{
public:
    Dropdown();
};
} // namespace clipperui
