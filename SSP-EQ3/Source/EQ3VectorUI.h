#pragma once

#include <JuceHeader.h>

namespace eq3ui
{
juce::Colour background();
juce::Colour backgroundSoft();
juce::Colour panelTop();
juce::Colour panelBottom();
juce::Colour outline();
juce::Colour outlineSoft();
juce::Colour textStrong();
juce::Colour textMuted();
juce::Colour brandTeal();
juce::Colour brandAmber();
juce::Colour brandIce();

juce::Font titleFont(float size);
juce::Font bodyFont(float size);
juce::Font smallCapsFont(float size);

void drawEditorBackdrop(juce::Graphics&, juce::Rectangle<float> bounds);
void drawPanelBackground(juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour accent, float radius = 14.0f);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

LookAndFeel& getVectorLookAndFeel();

class SSPKnob final : public juce::Slider
{
public:
    SSPKnob();
    void paint(juce::Graphics&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
};

class SSPToggle final : public juce::ToggleButton
{
public:
    explicit SSPToggle(const juce::String& text = {});
};
}
