#include "MinimizeControlsComponent.h"
#include "MinimizeUI.h"

namespace
{
juce::Colour subtleText() { return minimizeui::textMuted(); }
juce::Colour strongText() { return minimizeui::textStrong(); }
juce::Colour accentMint() { return minimizeui::brandCyan(); }
juce::Colour accentGold() { return minimizeui::brandGold(); }
juce::Colour accentEmber() { return minimizeui::brandEmber(); }
}

class MinimizeKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit MinimizeKnobLookAndFeel(bool emphasizedStyle)
        : emphasized(emphasizedStyle)
    {
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(emphasized ? 6.0f : 10.0f);
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto ringBounds = bounds.reduced(radius * (emphasized ? 0.10f : 0.14f));
        auto stroke = emphasized ? 14.0f : 10.0f;

        g.setColour(juce::Colours::black.withAlpha(emphasized ? 0.18f : 0.11f));
        g.fillEllipse(bounds.translated(0.0f, emphasized ? 8.0f : 6.0f));

        juce::ColourGradient halo((emphasized ? accentGold() : accentMint()).withAlpha(emphasized ? 0.28f : 0.18f),
                                  centre.x, bounds.getY(),
                                  accentEmber().withAlpha(emphasized ? 0.14f : 0.08f),
                                  centre.x, bounds.getBottom(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(emphasized ? 4.0f : 2.0f));

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, ringBounds.getWidth() * 0.5f, ringBounds.getHeight() * 0.5f,
                                    0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff161b22));
        g.strokePath(backgroundArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, ringBounds.getWidth() * 0.5f, ringBounds.getHeight() * 0.5f,
                               0.0f, rotaryStartAngle, angle, true);
        juce::ColourGradient accent(emphasized ? accentGold() : accentMint(),
                                    ringBounds.getBottomLeft(),
                                    emphasized ? accentEmber() : accentGold(),
                                    ringBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto faceBounds = emphasized ? bounds.reduced(radius * 0.16f) : bounds.reduced(radius * 0.18f);
        minimizeui::drawCachedDrawable(g,
                                       emphasized ? minimizeui::heroKnobFaceDrawable() : minimizeui::smallKnobFaceDrawable(),
                                       faceBounds,
                                       1.0f);

        juce::Path pointer;
        const auto pointerLength = faceBounds.getHeight() * (emphasized ? 0.34f : 0.30f);
        const auto pointerThickness = emphasized ? 8.0f : 6.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);

        g.setColour(juce::Colour(0xff15181c));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xfffaf4e7));
        g.fillEllipse(centre.x - (emphasized ? 7.0f : 5.0f), centre.y - (emphasized ? 7.0f : 5.0f),
                      emphasized ? 14.0f : 10.0f, emphasized ? 14.0f : 10.0f);
        g.setColour(juce::Colour(0xff171a1f));
        g.drawEllipse(centre.x - (emphasized ? 7.0f : 5.0f), centre.y - (emphasized ? 7.0f : 5.0f),
                      emphasized ? 14.0f : 10.0f, emphasized ? 14.0f : 10.0f, 1.0f);
    }

private:
    bool emphasized = false;
};

class MinimizeControlsComponent::MinimizeKnob final : public juce::Component
{
public:
    MinimizeKnob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title, const juce::String& captionText,
                 bool emphasizedStyle, bool showCaptionText)
        : lookAndFeel(emphasizedStyle),
          attachment(state, paramId, slider),
          emphasized(emphasizedStyle),
          showCaption(showCaptionText)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(slider);

        if (showCaption)
            addAndMakeVisible(captionLabel);

        titleLabel.setText(title, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(minimizeui::sectionFont(emphasized ? 30.0f : 17.0f));
        titleLabel.setColour(juce::Label::textColourId, strongText());

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(minimizeui::sectionFont(emphasized ? 23.0f : 15.0f));
        valueLabel.setColour(juce::Label::textColourId, emphasized ? accentGold() : strongText());

        captionLabel.setText(captionText, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(minimizeui::bodyFont(12.0f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValue(); };

        refreshValue();
    }

    ~MinimizeKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        if (emphasized)
        {
            juce::ColourGradient fill(juce::Colour(0xfffffbf4), area.getTopLeft(),
                                      juce::Colour(0xffecdfca), area.getBottomLeft(), false);
            fill.addColour(0.35, juce::Colour(0xfff8eedf));
            g.setGradientFill(fill);
            g.fillRoundedRectangle(area, 30.0f);

            g.setColour(juce::Colours::black.withAlpha(0.06f));
            g.drawRoundedRectangle(area.reduced(5.0f), 25.0f, 1.0f);
            g.setColour(accentGold().withAlpha(0.85f));
            g.fillRoundedRectangle(area.reduced(24.0f, 18.0f).removeFromTop(3.0f), 2.0f);
        }
        else
        {
            auto capArea = area.removeFromTop(24.0f);
            g.setColour(juce::Colours::black.withAlpha(0.05f));
            g.drawLine(capArea.getX() + 8.0f, capArea.getBottom(), capArea.getRight() - 8.0f, capArea.getBottom(), 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (emphasized)
        {
            area.reduce(18, 16);
            titleLabel.setBounds(area.removeFromTop(36));
            valueLabel.setBounds(area.removeFromTop(28));
            area.removeFromTop(6);
            auto captionArea = area.removeFromBottom(24);
            captionLabel.setBounds(captionArea);
            area.removeFromBottom(4);
            auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
            slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
            return;
        }

        titleLabel.setBounds(area.removeFromTop(24));
        auto valueArea = area.removeFromBottom(22);
        valueLabel.setBounds(valueArea);
        area.reduce(4, 0);
        auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValue()
    {
        valueLabel.setText(juce::String(juce::roundToInt((float) slider.getValue() * 100.0f)) + "%", juce::dontSendNotification);
    }

    MinimizeKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool emphasized = false;
    bool showCaption = false;
};

MinimizeControlsComponent::MinimizeControlsComponent(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    depthKnob = std::make_unique<MinimizeKnob>(apvts, "depth", "Depth", "Drive the amount of resonance reduction.", true, true);
    sensitivityKnob = std::make_unique<MinimizeKnob>(apvts, "sensitivity", "Sensitivity", juce::String(), false, false);
    sharpnessKnob = std::make_unique<MinimizeKnob>(apvts, "sharpness", "Sharpness", juce::String(), false, false);
    focusKnob = std::make_unique<MinimizeKnob>(apvts, "focus", "Focus", juce::String(), false, false);
    attackKnob = std::make_unique<MinimizeKnob>(apvts, "attack", "Attack", juce::String(), false, false);
    releaseKnob = std::make_unique<MinimizeKnob>(apvts, "release", "Release", juce::String(), false, false);
    mixKnob = std::make_unique<MinimizeKnob>(apvts, "mix", "Mix", juce::String(), false, false);

    addAndMakeVisible(*depthKnob);
    addAndMakeVisible(*sensitivityKnob);
    addAndMakeVisible(*sharpnessKnob);
    addAndMakeVisible(*focusKnob);
    addAndMakeVisible(*attackKnob);
    addAndMakeVisible(*releaseKnob);
    addAndMakeVisible(*mixKnob);
}

MinimizeControlsComponent::~MinimizeControlsComponent() = default;

void MinimizeControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    minimizeui::drawPanelBackground(g, bounds, minimizeui::brandCyan(), 24.0f);

    auto inner = bounds.reduced(24.0f, 18.0f);
    auto eyebrow = inner.removeFromTop(18.0f);
    g.setColour(subtleText());
    g.setFont(minimizeui::bodyFont(12.0f));
    g.drawText("Control dock", eyebrow.toNearestInt(), juce::Justification::centredLeft, false);

    auto dividerX = inner.getX() + 228.0f;
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.drawLine(dividerX, inner.getY() + 8.0f, dividerX, inner.getBottom() - 8.0f, 1.0f);

    auto horizontal = inner.withTrimmedLeft(246.0f);
    auto lineY = horizontal.getCentreY();
    g.setColour(minimizeui::brandCyan().withAlpha(0.16f));
    g.drawLine(horizontal.getX() + 10.0f, lineY, horizontal.getRight() - 10.0f, lineY, 1.0f);
}

void MinimizeControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(24, 18);
    area.removeFromTop(24);

    auto depthArea = area.removeFromLeft(220);
    depthKnob->setBounds(depthArea.reduced(0, 4));

    area.removeFromLeft(28);

    auto topRow = area.removeFromTop(area.getHeight() / 2);
    const int gap = 12;
    const int topWidth = (topRow.getWidth() - gap * 2) / 3;
    sensitivityKnob->setBounds(topRow.removeFromLeft(topWidth).reduced(6, 4));
    topRow.removeFromLeft(gap);
    sharpnessKnob->setBounds(topRow.removeFromLeft(topWidth).reduced(6, 4));
    topRow.removeFromLeft(gap);
    focusKnob->setBounds(topRow.reduced(6, 4));

    const int bottomWidth = (area.getWidth() - gap * 2) / 3;
    attackKnob->setBounds(area.removeFromLeft(bottomWidth).reduced(6, 4));
    area.removeFromLeft(gap);
    releaseKnob->setBounds(area.removeFromLeft(bottomWidth).reduced(6, 4));
    area.removeFromLeft(gap);
    mixKnob->setBounds(area.reduced(6, 4));
}
