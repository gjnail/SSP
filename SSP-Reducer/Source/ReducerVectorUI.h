#pragma once

#include <JuceHeader.h>

namespace reducerui
{
juce::Colour background();
juce::Colour backgroundSoft();
juce::Colour panelTop();
juce::Colour panelBottom();
juce::Colour outline();
juce::Colour outlineSoft();
juce::Colour textStrong();
juce::Colour textMuted();
juce::Colour accent();
juce::Colour accentBright();

juce::Font titleFont(float size);
juce::Font bodyFont(float size);
juce::Font smallCapsFont(float size);

void drawEditorBackdrop(juce::Graphics&, juce::Rectangle<float> bounds);
void drawPanelBackground(juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour accentColour, float radius = 14.0f);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

LookAndFeel& getVectorLookAndFeel();

class SSPKnob final : public juce::Slider
{
public:
    SSPKnob();
    ~SSPKnob() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void showValueEntry();
};

class SSPToggle final : public juce::ToggleButton
{
public:
    explicit SSPToggle(const juce::String& text = {});
};
}
