#include "ControlsSectionComponent.h"

namespace
{
constexpr int panelGap = 16;

juce::Colour panelOutline() { return juce::Colour(0xff2b3540); }
juce::Colour textSubtle() { return juce::Colour(0xff91a0ae); }
juce::Colour accentYellow() { return juce::Colour(0xffffd83d); }
juce::Colour accentOrange() { return juce::Colour(0xffffa33d); }
} // namespace

class MixKnobLookAndFeel final : public juce::LookAndFeel_V4
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
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(10.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillEllipse(bounds.translated(0.0f, 8.0f));

        juce::ColourGradient outerGlow(accentYellow().withAlpha(0.30f), centre.x, bounds.getY(),
                                       accentOrange().withAlpha(0.10f), centre.x, bounds.getBottom(), true);
        g.setGradientFill(outerGlow);
        g.fillEllipse(bounds.expanded(8.0f));

        g.setColour(juce::Colour(0xff0f141a));
        g.fillEllipse(bounds.expanded(1.5f));

        g.setColour(juce::Colour(0xff202833));
        g.fillEllipse(bounds);

        juce::ColourGradient face(juce::Colour(0xff424242), centre.x, bounds.getY(),
                                  juce::Colour(0xff1a1a1d), centre.x, bounds.getBottom(), false);
        face.addColour(0.35, juce::Colour(0xff2d2d31));
        g.setGradientFill(face);
        g.fillEllipse(bounds.reduced(radius * 0.12f));

        auto rim = bounds.reduced(radius * 0.04f);
        const float stroke = juce::jmax(10.0f, radius * 0.11f);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f,
                                    0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff11161d));
        g.strokePath(backgroundArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, rim.getWidth() * 0.5f, rim.getHeight() * 0.5f,
                               0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient valueFill(accentOrange(), rim.getBottomLeft(),
                                       accentYellow(), rim.getTopRight(), false);
        g.setGradientFill(valueFill);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto innerFace = bounds.reduced(radius * 0.26f);
        juce::ColourGradient cap(juce::Colour(0xff101318), innerFace.getCentreX(), innerFace.getY(),
                                 juce::Colour(0xff1d232b), innerFace.getCentreX(), innerFace.getBottom(), false);
        g.setGradientFill(cap);
        g.fillEllipse(innerFace);

        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillEllipse(innerFace.removeFromTop(innerFace.getHeight() * 0.34f));

        juce::Path pointer;
        const float pointerLength = radius * 0.50f;
        const float pointerThickness = juce::jmax(6.0f, radius * 0.06f);
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);

        g.setColour(accentYellow().brighter(0.08f));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff141920));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.30f, radius * 0.30f).withCentre(centre));
    }
};

class ControlsSectionComponent::MixKnob final : public juce::Component
{
public:
    explicit MixKnob(juce::AudioProcessorValueTreeState& state)
        : attachment(state, "mix", slider)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(tagLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(helperLabel);
        addAndMakeVisible(slider);

        tagLabel.setText("Output Blend", juce::dontSendNotification);
        tagLabel.setJustificationType(juce::Justification::centred);
        tagLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        tagLabel.setColour(juce::Label::textColourId, textSubtle());

        titleLabel.setText("Mix", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setText("", juce::dontSendNotification);
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(19.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, accentYellow());

        helperLabel.setText("Dry on the left, full shaping on the right.", juce::dontSendNotification);
        helperLabel.setJustificationType(juce::Justification::centred);
        helperLabel.setFont(juce::Font(11.0f));
        helperLabel.setColour(juce::Label::textColourId, textSubtle());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~MixKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff171c23), area.getTopLeft(), juce::Colour(0xff0f1318), area.getBottomRight(), false);
        fill.addColour(0.45, juce::Colour(0xff161d27));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, 22.0f);

        auto accentStrip = area.reduced(18.0f, 16.0f).removeFromTop(5.0f);
        juce::ColourGradient accent(accentOrange().withAlpha(0.45f), accentStrip.getX(), accentStrip.getCentreY(),
                                    accentYellow().withAlpha(0.85f), accentStrip.getRight(), accentStrip.getCentreY(), false);
        g.setGradientFill(accent);
        g.fillRoundedRectangle(accentStrip.withTrimmedLeft(area.getWidth() * 0.18f).withTrimmedRight(area.getWidth() * 0.18f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), 22.0f, 1.0f);

        auto labelArea = area.reduced(24.0f, 20.0f).removeFromBottom(30.0f);
        g.setColour(textSubtle());
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("DRY", labelArea.removeFromLeft(56.0f), juce::Justification::centredLeft);
        g.drawText("WET", labelArea.removeFromRight(56.0f), juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        tagLabel.setBounds(area.removeFromTop(18));
        titleLabel.setBounds(area.removeFromTop(30));
        valueLabel.setBounds(area.removeFromTop(28));
        helperLabel.setBounds(area.removeFromBottom(22));
        area.removeFromBottom(10);

        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        const auto percent = juce::roundToInt((float) slider.getValue() * 100.0f);
        valueLabel.setText(juce::String(percent) + "%", juce::dontSendNotification);
    }

    MixKnobLookAndFeel lookAndFeel;
    juce::Label tagLabel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label helperLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

ControlsSectionComponent::ControlsSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state)
    : triggerControls(processor, state)
{
    addAndMakeVisible(triggerControls);
    mixKnob = std::make_unique<MixKnob>(state);
    addAndMakeVisible(*mixKnob);
}

ControlsSectionComponent::~ControlsSectionComponent() = default;

void ControlsSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff141922), bounds.getTopLeft(), juce::Colour(0xff0d1117), bounds.getBottomRight(), false);
    shell.addColour(0.50, juce::Colour(0xff111823));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 24.0f);

    auto glow = bounds.reduced(20.0f, 12.0f).removeFromBottom(bounds.getHeight() * 0.34f);
    juce::ColourGradient floorGlow(juce::Colour(0x00ffd83d), glow.getX(), glow.getCentreY(),
                                   juce::Colour(0x00ffd83d), glow.getRight(), glow.getCentreY(), false);
    floorGlow.addColour(0.5, juce::Colour(0x25ffd83d));
    g.setGradientFill(floorGlow);
    g.fillRoundedRectangle(glow, 34.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 24.0f, 1.0f);
}

void ControlsSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(16, 16);
    auto leftWidth = juce::jmax(320, (int) std::round(area.getWidth() * 0.54f));
    auto triggerArea = area.removeFromLeft(leftWidth);
    area.removeFromLeft(panelGap);

    triggerControls.setBounds(triggerArea);
    mixKnob->setBounds(area);
}
