#pragma once

#include <JuceHeader.h>

namespace mixerui
{
class VectorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};

VectorLookAndFeel& getVectorLookAndFeel();

juce::Colour background();
juce::Colour panelTop();
juce::Colour panelBottom();
juce::Colour outline();
juce::Colour outlineSoft();
juce::Colour textStrong();
juce::Colour textMuted();
juce::Colour brandGold();
juce::Colour brandCyan();
juce::Colour brandMint();
juce::Colour graphGrid();

juce::Font titleFont(float size);
juce::Font bodyFont(float size);
juce::Font smallCapsFont(float size);

void drawEditorBackdrop(juce::Graphics&, juce::Rectangle<float> bounds);
void drawPanelBackground(juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour accent, float radius = 14.0f);

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

class SSPDropdown final : public juce::ComboBox
{
public:
    SSPDropdown();
};

class SSPToggle final : public juce::ToggleButton
{
public:
    explicit SSPToggle(const juce::String& text = {});
};
} // namespace mixerui
