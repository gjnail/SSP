#pragma once

#include <functional>
#include <JuceHeader.h>

namespace spectrumui
{
struct Palette
{
    juce::Colour background;
    juce::Colour backgroundSoft;
    juce::Colour panelTop;
    juce::Colour panelBottom;
    juce::Colour outline;
    juce::Colour outlineSoft;
    juce::Colour textStrong;
    juce::Colour textMuted;
    juce::Colour accentPrimary;
    juce::Colour accentSecondary;
    juce::Colour accentWarm;
    juce::Colour positive;
    juce::Colour warning;
    juce::Colour danger;
};

const Palette& getPalette(int themeIndex);
juce::StringArray getThemeNames();

juce::Font titleFont(float size);
juce::Font bodyFont(float size);
juce::Font smallCapsFont(float size);

void drawEditorBackdrop(juce::Graphics&, juce::Rectangle<float>, const Palette&);
void drawPanelBackground(juce::Graphics&, juce::Rectangle<float>, const Palette&, juce::Colour accent, float radius = 14.0f);
void drawBadge(juce::Graphics&, juce::Rectangle<float>, const Palette&, juce::String text, juce::Colour fill, juce::Colour textColour);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    using ThemeGetter = std::function<int()>;

    explicit LookAndFeel(ThemeGetter themeGetter = {});

    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool isMouseOverButton,
                              bool isButtonDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool isMouseOverButton, bool isButtonDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

private:
    const Palette& palette() const;

    ThemeGetter getThemeIndex;
};

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
} // namespace spectrumui
