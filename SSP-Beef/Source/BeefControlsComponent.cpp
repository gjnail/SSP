#include "BeefControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff4a3128); }
juce::Colour subtleText() { return juce::Colour(0xffc8a48c); }
juce::Colour warmYellow() { return juce::Colour(0xffffd05f); }
juce::Colour warmOrange() { return juce::Colour(0xffff8c3f); }
juce::Colour emberRed() { return juce::Colour(0xfffc5a32); }
}

class BeefKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
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
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(12.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float stroke = juce::jmax(16.0f, radius * 0.12f);

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillEllipse(bounds.translated(0.0f, 11.0f));

        juce::ColourGradient halo(warmOrange().withAlpha(0.34f), centre.x, bounds.getY(),
                                  emberRed().withAlpha(0.12f), centre.x, bounds.getBottom(), true);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(14.0f));

        g.setColour(juce::Colour(0xff100c0e));
        g.fillEllipse(bounds.expanded(1.8f));

        juce::ColourGradient shell(juce::Colour(0xff3a3232), centre.x, bounds.getY(),
                                   juce::Colour(0xff151214), centre.x, bounds.getBottom(), false);
        shell.addColour(0.25, juce::Colour(0xff2a2526));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        auto rim = bounds.reduced(radius * 0.06f);
        juce::Path baseArc;
        baseArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f,
                              rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff120f11));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f,
                               rotaryStartAngle, angle, true);
        juce::ColourGradient accent(emberRed(), rim.getBottomLeft(), warmYellow(), rim.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * 0.30f);
        juce::ColourGradient capFill(juce::Colour(0xff11141a), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff20252d), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        auto highlight = cap.removeFromTop(cap.getHeight() * 0.30f).reduced(cap.getWidth() * 0.16f, 0.0f);
        g.setColour(juce::Colour(0x32ffffff));
        g.fillEllipse(highlight);

        juce::Path pointer;
        const float pointerLength = radius * 0.58f;
        const float pointerThickness = 9.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(warmYellow());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff171c22));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.25f, radius * 0.25f).withCentre(centre));
    }
};

class BeefControlsComponent::BeefKnob final : public juce::Component
{
public:
    BeefKnob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
        : attachment(state, paramId, slider)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText("Beef", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(34.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(25.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, warmYellow());

        captionLabel.setText("One move for squash, weight, and harmonic body.", juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(12.0f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~BeefKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff191317), area.getTopLeft(),
                                  juce::Colour(0xff100d10), area.getBottomRight(), false);
        fill.addColour(0.45, juce::Colour(0xff1e1619));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, 28.0f);

        auto accent = area.reduced(34.0f, 18.0f).removeFromTop(6.0f);
        juce::ColourGradient accentFill(emberRed().withAlpha(0.42f), accent.getX(), accent.getCentreY(),
                                        warmYellow().withAlpha(0.94f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.24f).withTrimmedRight(accent.getWidth() * 0.24f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), 28.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(28, 18);
        titleLabel.setBounds(area.removeFromTop(38));
        valueLabel.setBounds(area.removeFromTop(30));
        captionLabel.setBounds(area.removeFromBottom(24));
        area.removeFromBottom(10);

        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(juce::String(juce::roundToInt((float) slider.getValue() * 100.0f)) + "%", juce::dontSendNotification);
    }

    BeefKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

BeefControlsComponent::BeefControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    addAndMakeVisible(descriptionLabel);

    descriptionLabel.setFont(juce::Font(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, subtleText());
    descriptionLabel.setJustificationType(juce::Justification::centred);

    amountKnob = std::make_unique<BeefKnob>(state, "amount");
    addAndMakeVisible(*amountKnob);

    refreshDescription();
    startTimerHz(10);
}

BeefControlsComponent::~BeefControlsComponent()
{
    stopTimer();
}

void BeefControlsComponent::timerCallback()
{
    refreshDescription();
}

void BeefControlsComponent::refreshDescription()
{
    descriptionLabel.setText(processor.getDescription(), juce::dontSendNotification);
}

void BeefControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff171116), bounds.getTopLeft(),
                               juce::Colour(0xff100c0f), bounds.getBottomRight(), false);
    shell.addColour(0.52, juce::Colour(0xff1d1418));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(26.0f, 18.0f).removeFromTop(120.0f);
    juce::ColourGradient glow(juce::Colour(0x00ff9242), topGlow.getX(), topGlow.getCentreY(),
                              juce::Colour(0x00ff9242), topGlow.getRight(), topGlow.getCentreY(), false);
    glow.addColour(0.46, juce::Colour(0x34ff9242));
    glow.addColour(0.72, juce::Colour(0x20ffcf58));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 24.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void BeefControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    descriptionLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    amountKnob->setBounds(area);
}
