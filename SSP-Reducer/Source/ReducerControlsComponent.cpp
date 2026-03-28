#include "ReducerControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff4a3128); }
juce::Colour subtleText() { return juce::Colour(0xffc6a89c); }
juce::Colour accentOrange() { return juce::Colour(0xffff7a4d); }
juce::Colour accentPeach() { return juce::Colour(0xffffbe9a); }
juce::Colour deepPanel() { return juce::Colour(0xff16100f); }
} // namespace

class ReducerKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit ReducerKnobLookAndFeel(bool heroStyle)
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

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.30f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 10.0f : 7.0f));

        juce::ColourGradient halo(accentOrange().withAlpha(hero ? 0.30f : 0.22f), centre.x, bounds.getY(),
                                  accentPeach().withAlpha(hero ? 0.18f : 0.10f), centre.x, bounds.getBottom(), true);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        g.setColour(juce::Colour(0xff130f0f));
        g.fillEllipse(bounds.expanded(1.6f));

        juce::ColourGradient shell(juce::Colour(0xff3a2c28), centre.x, bounds.getY(),
                                   juce::Colour(0xff171111), centre.x, bounds.getBottom(), false);
        shell.addColour(0.3, juce::Colour(0xff2a1f1d));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        auto rim = bounds.reduced(radius * 0.05f);
        juce::Path baseArc;
        baseArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff140f0e));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f, 0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient accent(accentOrange(), rim.getBottomLeft(), accentPeach(), rim.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.28f : 0.31f));
        juce::ColourGradient capFill(juce::Colour(0xff16100f), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff251919), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.54f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 6.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(accentPeach());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff1f1615));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.24f, radius * 0.24f).withCentre(centre));
    }

private:
    bool hero = false;
};

class ReducerControlsComponent::ReducerKnob final : public juce::Component
{
public:
    ReducerKnob(juce::AudioProcessorValueTreeState& state,
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
        valueLabel.setColour(juce::Label::textColourId, accentPeach());

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

    ~ReducerKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff201615) : juce::Colour(0xff1b1312),
                                  area.getTopLeft(),
                                  juce::Colour(0xff120d0c),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.42, juce::Colour(0xff241716));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 26.0f : 22.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 18.0f : 16.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(accentOrange().withAlpha(0.48f), accent.getX(), accent.getCentreY(),
                                        accentPeach().withAlpha(0.92f), accent.getRight(), accent.getCentreY(), false);
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

    ReducerKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool hero = false;
};

ReducerControlsComponent::ReducerControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
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
    modeBox.setColour(juce::ComboBox::arrowColourId, accentPeach());
    modeBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff231716));
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);

    descriptionLabel.setFont(juce::Font(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, subtleText());
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);

    mixKnob = std::make_unique<ReducerKnob>(state, "mix", "Mix", "Blend the crushed signal back against the original.", true);
    bitsKnob = std::make_unique<ReducerKnob>(state, "bits", "Bits", "Lower resolution for grit, stair-steps, and fuzz.", false);
    rateKnob = std::make_unique<ReducerKnob>(state, "rate", "Rate", "Reduce sample refresh for hold-style aliasing.", false);
    toneKnob = std::make_unique<ReducerKnob>(state, "tone", "Tone", "Shape the top end after the crush.", false);
    jitterKnob = std::make_unique<ReducerKnob>(state, "jitter", "Jitter", "Add instability so the reduction feels less static.", false);

    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*bitsKnob);
    addAndMakeVisible(*rateKnob);
    addAndMakeVisible(*toneKnob);
    addAndMakeVisible(*jitterKnob);

    refreshDescription();
    startTimerHz(10);
}

ReducerControlsComponent::~ReducerControlsComponent()
{
    stopTimer();
}

void ReducerControlsComponent::timerCallback()
{
    refreshDescription();
}

void ReducerControlsComponent::refreshDescription()
{
    descriptionLabel.setText(processor.getCurrentModeDescription(), juce::dontSendNotification);
}

void ReducerControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff1b1413), bounds.getTopLeft(), juce::Colour(0xff120d0c), bounds.getBottomRight(), false);
    shell.addColour(0.44, juce::Colour(0xff241716));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto topGlow = bounds.reduced(28.0f, 18.0f).removeFromTop(88.0f);
    juce::ColourGradient glow(juce::Colour(0x00ff7a4d), topGlow.getX(), topGlow.getCentreY(),
                              juce::Colour(0x00ff7a4d), topGlow.getRight(), topGlow.getCentreY(), false);
    glow.addColour(0.5, juce::Colour(0x24ff7a4d));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 24.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void ReducerControlsComponent::resized()
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
    bitsKnob->setBounds(topKnobs.removeFromLeft(topWidth));
    topKnobs.removeFromLeft(gap);
    rateKnob->setBounds(topKnobs);

    area.removeFromTop(14);
    auto bottomWidth = (area.getWidth() - gap) / 2;
    toneKnob->setBounds(area.removeFromLeft(bottomWidth));
    area.removeFromLeft(gap);
    jitterKnob->setBounds(area);
}
