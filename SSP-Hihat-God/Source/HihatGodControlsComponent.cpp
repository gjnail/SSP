#include "HihatGodControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3e3f54); }
juce::Colour subtleText() { return juce::Colour(0xffc4c7d8); }
juce::Colour silver() { return juce::Colour(0xffdce5f3); }
juce::Colour cyan() { return juce::Colour(0xff7ad2ff); }
juce::Colour aqua() { return juce::Colour(0xff67f0d5); }
juce::Colour amber() { return juce::Colour(0xffffc97a); }

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatDecibels(double value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatRate(double value)
{
    static const juce::StringArray labels {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 Bar", "2 Bar", "4 Bar"};
    const int index = juce::jlimit(0, labels.size() - 1, juce::roundToInt((float) value));
    return labels[index];
}
} // namespace

class GodKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit GodKnobLookAndFeel(bool heroStyle)
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
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(hero ? 8.0f : 10.0f);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float stroke = hero ? juce::jmax(13.0f, radius * 0.13f) : juce::jmax(8.0f, radius * 0.11f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.26f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 9.0f : 6.0f));

        juce::ColourGradient halo(cyan().withAlpha(hero ? 0.26f : 0.18f), centre.x, bounds.getBottom(),
                                  aqua().withAlpha(hero ? 0.16f : 0.11f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff252a35), centre.x, bounds.getY(),
                                   juce::Colour(0xff12141b), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1b1f28));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        juce::Path baseArc;
        const auto arcBounds = bounds.reduced(radius * 0.08f);
        baseArc.addCentredArc(centre.x, centre.y,
                              arcBounds.getWidth() * 0.5f,
                              arcBounds.getHeight() * 0.5f,
                              0.0f,
                              rotaryStartAngle,
                              rotaryEndAngle,
                              true);
        g.setColour(juce::Colour(0xff0d1016));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(aqua(), arcBounds.getBottomLeft(), cyan(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff181b24), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff222735), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(silver());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff12151c));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class HihatGodControlsComponent::GodKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    GodKnob(juce::AudioProcessorValueTreeState& state,
            const juce::String& paramId,
            const juce::String& heading,
            const juce::String& caption,
            Formatter formatterToUse,
            bool heroStyle)
        : lookAndFeel(heroStyle),
          attachment(state, paramId, slider),
          formatter(std::move(formatterToUse)),
          hero(heroStyle)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(hero ? 30.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 23.0f : 15.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, cyan());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.3f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~GodKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff181c25) : juce::Colour(0xff171922),
                                  area.getTopLeft(),
                                  juce::Colour(0xff11131a),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff1d2230));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 28.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(aqua().withAlpha(0.60f), accent.getX(), accent.getCentreY(),
                                        cyan().withAlpha(0.76f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.18f).withTrimmedRight(accent.getWidth() * 0.18f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 28.0f : 20.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 22 : 16, hero ? 18 : 14);
        titleLabel.setBounds(area.removeFromTop(hero ? 38 : 24));
        valueLabel.setBounds(area.removeFromTop(hero ? 30 : 22));
        captionLabel.setBounds(area.removeFromBottom(24));
        area.removeFromBottom(hero ? 8 : 6);

        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    GodKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

HihatGodControlsComponent::HihatGodControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Host-synced sine-LFO gain and pan motion for hi-hats so repeated patterns feel less robotic and more played.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("HAT MOTION", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, amber());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff584c31));
    addAndMakeVisible(badgeLabel);

    currentGainLabel.setJustificationType(juce::Justification::centredLeft);
    currentGainLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    currentGainLabel.setColour(juce::Label::textColourId, silver());
    addAndMakeVisible(currentGainLabel);

    currentPanLabel.setJustificationType(juce::Justification::centredLeft);
    currentPanLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    currentPanLabel.setColour(juce::Label::textColourId, silver());
    addAndMakeVisible(currentPanLabel);

    variationKnob = std::make_unique<GodKnob>(state,
                                              "variation",
                                              "Variation",
                                              "Scales both the gain sweep and stereo sweep.",
                                              formatPercent,
                                              true);
    rateKnob = std::make_unique<GodKnob>(state,
                                         "rateIndex",
                                         "Rate",
                                         "Cycles through host-synced modulation rates.",
                                         formatRate,
                                         false);
    volumeRangeKnob = std::make_unique<GodKnob>(state,
                                                "volumeRangeDb",
                                                "Volume Range",
                                                "Maximum gain swing across the sine cycle.",
                                                formatDecibels,
                                                false);
    panRangeKnob = std::make_unique<GodKnob>(state,
                                             "panRange",
                                             "Pan Range",
                                             "Maximum left-right move across the sine cycle.",
                                             formatPercent,
                                             false);

    addAndMakeVisible(*variationKnob);
    addAndMakeVisible(*rateKnob);
    addAndMakeVisible(*volumeRangeKnob);
    addAndMakeVisible(*panRangeKnob);

    refreshFromProcessor();
    startTimerHz(24);
}

HihatGodControlsComponent::~HihatGodControlsComponent()
{
    stopTimer();
}

void HihatGodControlsComponent::timerCallback()
{
    refreshFromProcessor();
}

void HihatGodControlsComponent::refreshFromProcessor()
{
    const float currentGain = processor.getCurrentGainOffsetDb();
    const float currentPan = processor.getCurrentPanOffset();

    currentGainLabel.setText("Current Gain Sweep: " + juce::String(currentGain, 1) + " dB", juce::dontSendNotification);
    currentPanLabel.setText("Current Pan Sweep: " + juce::String(currentPan * 100.0f, 0) + "%", juce::dontSendNotification);
}

void HihatGodControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff13151d), bounds.getTopLeft(),
                               juce::Colour(0xff0d1016), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff171b24));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 30.0f);

    auto topGlow = bounds.reduced(24.0f, 16.0f).removeFromTop(110.0f);
    juce::ColourGradient glow(cyan().withAlpha(0.16f), topGlow.getX(), topGlow.getY(),
                              aqua().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 22.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 30.0f, 1.0f);
}

void HihatGodControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(78);
    summaryLabel.setBounds(topRow.removeFromTop(34));

    auto lowerTop = topRow;
    badgeLabel.setBounds(lowerTop.removeFromLeft(150).reduced(0, 2));
    lowerTop.removeFromLeft(16);
    currentGainLabel.setBounds(lowerTop.removeFromLeft(250));
    currentPanLabel.setBounds(lowerTop.removeFromLeft(220));

    area.removeFromTop(14);

    auto heroArea = area.removeFromLeft((int) std::round(area.getWidth() * 0.40f));
    area.removeFromLeft(18);
    auto rightArea = area;

    variationKnob->setBounds(heroArea);

    auto topRight = rightArea.removeFromTop((int) std::round(rightArea.getHeight() * 0.48f));
    rateKnob->setBounds(topRight);
    rightArea.removeFromTop(18);

    auto bottomLeft = rightArea.removeFromLeft((rightArea.getWidth() - 18) / 2);
    volumeRangeKnob->setBounds(bottomLeft);
    rightArea.removeFromLeft(18);
    panRangeKnob->setBounds(rightArea);
}
