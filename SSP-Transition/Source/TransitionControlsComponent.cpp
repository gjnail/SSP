#include "TransitionControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff313740); }
juce::Colour subtleText() { return juce::Colour(0xff95a3b0); }
juce::Colour warmYellow() { return juce::Colour(0xffffcf58); }
juce::Colour warmOrange() { return juce::Colour(0xffff9340); }
juce::Colour deepPanel() { return juce::Colour(0xff131820); }
} // namespace

class TransitionKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit TransitionKnobLookAndFeel(bool heroStyle)
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
        const float stroke = hero ? juce::jmax(14.0f, radius * 0.12f) : juce::jmax(9.0f, radius * 0.11f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.28f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 10.0f : 7.0f));

        juce::ColourGradient halo(warmYellow().withAlpha(hero ? 0.32f : 0.22f), centre.x, bounds.getY(),
                                  warmOrange().withAlpha(hero ? 0.18f : 0.10f), centre.x, bounds.getBottom(), true);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        g.setColour(juce::Colour(0xff0f1318));
        g.fillEllipse(bounds.expanded(1.6f));

        juce::ColourGradient shell(juce::Colour(0xff3b3b40), centre.x, bounds.getY(),
                                   juce::Colour(0xff17191d), centre.x, bounds.getBottom(), false);
        shell.addColour(0.3, juce::Colour(0xff2d2e33));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        auto rim = bounds.reduced(radius * 0.05f);

        juce::Path baseArc;
        baseArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff11161d));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient accent(warmOrange(), rim.getBottomLeft(), warmYellow(), rim.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.28f : 0.30f));
        juce::ColourGradient capFill(juce::Colour(0xff11141a), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff1f252d), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.54f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 6.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(warmYellow());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff171c22));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.26f, radius * 0.26f).withCentre(centre));
    }

private:
    bool hero = false;
};

class TransitionControlsComponent::TransitionKnob final : public juce::Component
{
public:
    TransitionKnob(juce::AudioProcessorValueTreeState& state,
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
        titleLabel.setFont(juce::Font(hero ? 30.0f : 20.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 24.0f : 16.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, warmYellow());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.5f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~TransitionKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff171b22) : juce::Colour(0xff161a22),
                                  area.getTopLeft(),
                                  juce::Colour(0xff101319),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.42, juce::Colour(0xff171c24));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 26.0f : 22.0f);

        auto accent = area.reduced(hero ? 26.0f : 20.0f, hero ? 18.0f : 16.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(warmOrange().withAlpha(0.45f), accent.getX(), accent.getCentreY(),
                                        warmYellow().withAlpha(0.90f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.22f).withTrimmedRight(accent.getWidth() * 0.22f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 26.0f : 22.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 22 : 18, hero ? 18 : 16);
        titleLabel.setBounds(area.removeFromTop(hero ? 36 : 26));
        valueLabel.setBounds(area.removeFromTop(hero ? 30 : 22));
        captionLabel.setBounds(area.removeFromBottom(22));
        area.removeFromBottom(hero ? 8 : 6);

        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(juce::String(juce::roundToInt((float) slider.getValue() * 100.0f)) + "%", juce::dontSendNotification);
    }

    TransitionKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool hero = false;
};

TransitionControlsComponent::TransitionControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    addAndMakeVisible(presetLabel);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(descriptionLabel);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, subtleText());

    presetBox.addItemList(PluginProcessor::getPresetNames(), 1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, deepPanel());
    presetBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::arrowColourId, warmYellow());
    presetBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff1c212a));
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "preset", presetBox);

    descriptionLabel.setFont(juce::Font(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, subtleText());
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);

    amountKnob = std::make_unique<TransitionKnob>(state, "amount", "Transition", "One move for fade, filter, width, and wash.", true);
    mixKnob = std::make_unique<TransitionKnob>(state, "mix", "Mix", "Blend the transition against the dry signal.", false);
    addAndMakeVisible(*amountKnob);
    addAndMakeVisible(*mixKnob);

    refreshDescription();
    startTimerHz(10);
}

TransitionControlsComponent::~TransitionControlsComponent()
{
    stopTimer();
}

void TransitionControlsComponent::timerCallback()
{
    refreshDescription();
}

void TransitionControlsComponent::refreshDescription()
{
    descriptionLabel.setText(processor.getCurrentPresetDescription(), juce::dontSendNotification);
}

void TransitionControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff161b23), bounds.getTopLeft(), juce::Colour(0xff101318), bounds.getBottomRight(), false);
    shell.addColour(0.45, juce::Colour(0xff171c24));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(28.0f, 18.0f).removeFromTop(90.0f);
    juce::ColourGradient glow(juce::Colour(0x00ffb347), topGlow.getX(), topGlow.getCentreY(),
                              juce::Colour(0x00ffb347), topGlow.getRight(), topGlow.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x28ffb347));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 24.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void TransitionControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(78);
    presetLabel.setBounds(topRow.removeFromTop(18));
    topRow.removeFromTop(8);
    auto presetBoxArea = topRow.removeFromLeft(230);
    presetBox.setBounds(presetBoxArea.removeFromTop(38));
    topRow.removeFromLeft(16);
    descriptionLabel.setBounds(topRow.removeFromTop(40));

    area.removeFromTop(10);
    auto heroArea = area.removeFromLeft((int) std::round(area.getWidth() * 0.64f));
    area.removeFromLeft(16);

    amountKnob->setBounds(heroArea);
    mixKnob->setBounds(area.withTrimmedTop(34).withTrimmedBottom(34));
}
