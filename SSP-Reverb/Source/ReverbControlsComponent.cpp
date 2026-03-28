#include "ReverbControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff2f3946); }
juce::Colour subtleText() { return juce::Colour(0xff93a2b2); }
juce::Colour brightBlue() { return juce::Colour(0xff52d7ff); }
juce::Colour paleBlue() { return juce::Colour(0xffb4f1ff); }
juce::Colour deepPanel() { return juce::Colour(0xff10161d); }
} // namespace

class ReverbKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit ReverbKnobLookAndFeel(bool heroStyle)
        : hero(heroStyle)
    {
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(hero ? 10.0f : 12.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float stroke = hero ? juce::jmax(14.0f, radius * 0.12f) : juce::jmax(9.0f, radius * 0.10f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.28f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 10.0f : 7.0f));

        juce::ColourGradient halo(brightBlue().withAlpha(hero ? 0.28f : 0.20f), centre.x, bounds.getY(),
                                  paleBlue().withAlpha(hero ? 0.18f : 0.10f), centre.x, bounds.getBottom(), true);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        g.setColour(juce::Colour(0xff10151b));
        g.fillEllipse(bounds.expanded(1.6f));

        juce::ColourGradient shell(juce::Colour(0xff303844), centre.x, bounds.getY(),
                                   juce::Colour(0xff161b22), centre.x, bounds.getBottom(), false);
        shell.addColour(0.3, juce::Colour(0xff262d38));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        auto rim = bounds.reduced(radius * 0.05f);
        juce::Path baseArc;
        baseArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff11161d));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient accent(brightBlue(), rim.getBottomLeft(), paleBlue(), rim.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.28f : 0.31f));
        juce::ColourGradient capFill(juce::Colour(0xff121821), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff1d2732), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.54f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 6.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(paleBlue());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff16202a));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.24f, radius * 0.24f).withCentre(centre));
    }

private:
    bool hero = false;
};

class ReverbControlsComponent::ReverbKnob final : public juce::Component
{
public:
    ReverbKnob(juce::AudioProcessorValueTreeState& state,
               const juce::String& paramId,
               const juce::String& heading,
               const juce::String& caption,
               bool heroStyle)
        : lookAndFeel(heroStyle),
          attachment(state, paramId, slider),
          hero(heroStyle)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(hero ? 28.0f : 19.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 23.0f : 16.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, paleBlue());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.0f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~ReverbKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff151c25) : juce::Colour(0xff141a22),
                                  area.getTopLeft(),
                                  juce::Colour(0xff0e1218),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.42, juce::Colour(0xff151e28));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 26.0f : 22.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 18.0f : 16.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(brightBlue().withAlpha(0.42f), accent.getX(), accent.getCentreY(),
                                        paleBlue().withAlpha(0.92f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.20f).withTrimmedRight(accent.getWidth() * 0.20f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 26.0f : 22.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 22 : 16, hero ? 18 : 14);
        titleLabel.setBounds(area.removeFromTop(hero ? 34 : 24));
        valueLabel.setBounds(area.removeFromTop(hero ? 28 : 20));
        captionLabel.setBounds(area.removeFromBottom(20));
        area.removeFromBottom(hero ? 10 : 6);
        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(juce::String(juce::roundToInt((float) slider.getValue() * 100.0f)) + "%", juce::dontSendNotification);
    }

    ReverbKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool hero = false;
};

ReverbControlsComponent::ReverbControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeBox);
    addAndMakeVisible(descriptionLabel);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    modeLabel.setColour(juce::Label::textColourId, subtleText());

    modeBox.addItemList(PluginProcessor::getModeNames(), 1);
    modeBox.setColour(juce::ComboBox::backgroundColourId, deepPanel());
    modeBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    modeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    modeBox.setColour(juce::ComboBox::arrowColourId, paleBlue());
    modeBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1a212b));
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);

    descriptionLabel.setFont(juce::Font(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, subtleText());
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);

    mixKnob = std::make_unique<ReverbKnob>(state, "mix", "Mix", "Set the wet blend without diving into menus.", true);
    decayKnob = std::make_unique<ReverbKnob>(state, "decay", "Decay", "Shape the tail from tight to huge.", false);
    toneKnob = std::make_unique<ReverbKnob>(state, "tone", "Tone", "Push dark and warm or bright and airy.", false);
    widthKnob = std::make_unique<ReverbKnob>(state, "width", "Width", "Spread the stereo field of the reverb tail.", false);

    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*decayKnob);
    addAndMakeVisible(*toneKnob);
    addAndMakeVisible(*widthKnob);

    refreshDescription();
    startTimerHz(10);
}

ReverbControlsComponent::~ReverbControlsComponent()
{
    stopTimer();
}

void ReverbControlsComponent::timerCallback()
{
    refreshDescription();
}

void ReverbControlsComponent::refreshDescription()
{
    descriptionLabel.setText(processor.getCurrentModeDescription(), juce::dontSendNotification);
}

void ReverbControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff151c24), bounds.getTopLeft(), juce::Colour(0xff10141a), bounds.getBottomRight(), false);
    shell.addColour(0.44, juce::Colour(0xff15202a));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(28.0f, 18.0f).removeFromTop(88.0f);
    juce::ColourGradient glow(juce::Colour(0x0028c8ff), topGlow.getX(), topGlow.getCentreY(),
                              juce::Colour(0x0028c8ff), topGlow.getRight(), topGlow.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x2428c8ff));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 24.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void ReverbControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(82);
    modeLabel.setBounds(topRow.removeFromTop(18));
    topRow.removeFromTop(8);
    auto comboArea = topRow.removeFromLeft(220);
    modeBox.setBounds(comboArea.removeFromTop(38));
    topRow.removeFromLeft(18);
    descriptionLabel.setBounds(topRow.removeFromTop(42));

    area.removeFromTop(12);
    auto heroArea = area.removeFromLeft(300);
    area.removeFromLeft(16);

    mixKnob->setBounds(heroArea);

    auto topKnobs = area.removeFromTop(area.getHeight() / 2);
    const int gap = 14;
    const int topWidth = (topKnobs.getWidth() - gap) / 2;
    decayKnob->setBounds(topKnobs.removeFromLeft(topWidth));
    topKnobs.removeFromLeft(gap);
    toneKnob->setBounds(topKnobs);

    area.removeFromTop(14);
    widthKnob->setBounds(area);
}
