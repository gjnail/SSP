#include "TransientControlsComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff4a403c); }
juce::Colour subtleText() { return juce::Colour(0xffd5cfc4); }
juce::Colour shellText() { return juce::Colour(0xfff5eee8); }
juce::Colour ember() { return juce::Colour(0xffff8a5b); }
juce::Colour gold() { return juce::Colour(0xffffcf70); }
juce::Colour rose() { return juce::Colour(0xffff6f61); }

juce::String formatSignedPercent(double value)
{
    const auto rounded = juce::roundToInt((float) value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatDecibels(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}
} // namespace

class TransientControlsComponent::EnvelopeDisplay final : public juce::Component
{
public:
    explicit EnvelopeDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient fill(juce::Colour(0xff15100d), bounds.getTopLeft(),
                                  juce::Colour(0xff0f0b09), bounds.getBottomRight(),
                                  false);
        fill.addColour(0.52, juce::Colour(0xff1d1612));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 22.0f);

        auto plot = bounds.reduced(18.0f, 16.0f);
        const auto titleArea = plot.removeFromTop(18.0f);
        const float baselineY = plot.getBottom() - 8.0f;

        g.setColour(juce::Colour(0x22ffffff));
        for (int i = 0; i < 4; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, 3.0f, plot.getY(), baselineY);
            g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        }

        g.setColour(juce::Colour(0x20ffffff));
        for (int i = 0; i < 6; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, 5.0f, plot.getX(), plot.getRight());
            g.drawVerticalLine((int) std::round(x), plot.getY(), baselineY);
        }

        auto makeEnvelopePath = [&](float attackAmount, float sustainAmount, float clipAmount, float wetMix)
        {
            juce::Path path;

            auto valueForX = [&](float normX)
            {
                const float clampedX = juce::jlimit(0.0f, 1.0f, normX);

                const float baseAttack = std::exp(-std::pow((clampedX - 0.1f) / 0.06f, 2.0f));
                const float baseBody = 0.52f * std::exp(-clampedX * 4.4f);
                float shaped = baseAttack + baseBody;

                const float attackBoost = attackAmount * 0.75f * std::exp(-std::pow((clampedX - 0.085f) / 0.05f, 2.0f));
                const float sustainBoost = sustainAmount * 0.58f * std::exp(-juce::jmax(0.0f, clampedX - 0.18f) * 2.6f);
                shaped += attackBoost + sustainBoost;
                shaped = juce::jmax(0.0f, shaped);

                if (clipAmount > 0.001f)
                {
                    const float drive = 1.0f + clipAmount * 3.2f;
                    shaped = std::tanh(shaped * drive) / std::tanh(drive);
                }

                const float dryShape = juce::jlimit(0.0f, 1.4f, baseAttack + baseBody);
                return juce::jlimit(0.0f, 1.25f, juce::jmap(wetMix, dryShape, shaped));
            };

            for (int i = 0; i <= 128; ++i)
            {
                const float normX = (float) i / 128.0f;
                const float x = plot.getX() + normX * plot.getWidth();
                const float value = valueForX(normX);
                const float y = juce::jmap(value, 0.0f, 1.25f, baselineY, plot.getY() + 4.0f);

                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            return path;
        };

        const float attackAmount = juce::jlimit(-1.0f, 1.0f, apvts.getRawParameterValue("attack")->load() / 100.0f);
        const float sustainAmount = juce::jlimit(-1.0f, 1.0f, apvts.getRawParameterValue("sustain")->load() / 100.0f);
        const float clipAmount = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("clip")->load() / 100.0f);
        const float wetMix = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("mix")->load() / 100.0f);

        const auto dryPath = makeEnvelopePath(0.0f, 0.0f, 0.0f, 0.0f);
        const auto shapedPath = makeEnvelopePath(attackAmount, sustainAmount, clipAmount, wetMix);

        juce::Path filledShape(shapedPath);
        filledShape.lineTo(plot.getRight(), baselineY);
        filledShape.lineTo(plot.getX(), baselineY);
        filledShape.closeSubPath();

        juce::ColourGradient shapeFill(ember().withAlpha(0.26f), plot.getX(), plot.getY(),
                                       rose().withAlpha(0.12f), plot.getRight(), baselineY, false);
        g.setGradientFill(shapeFill);
        g.fillPath(filledShape);

        g.setColour(juce::Colour(0x55f5eee8));
        g.strokePath(dryPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(gold());
        g.strokePath(shapedPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("ENVELOPE PREVIEW", titleArea.toNearestInt(), juce::Justification::topLeft, false);

        auto legendArea = juce::Rectangle<float>(bounds.getRight() - 180.0f, bounds.getY() + 12.0f, 158.0f, 24.0f);
        g.setColour(juce::Colour(0x55f5eee8));
        g.drawLine(legendArea.getX(), legendArea.getCentreY(), legendArea.getX() + 24.0f, legendArea.getCentreY(), 2.0f);
        g.setColour(subtleText());
        g.drawText("Dry", juce::Rectangle<int>((int) legendArea.getX() + 30, (int) legendArea.getY(), 40, (int) legendArea.getHeight()),
                   juce::Justification::centredLeft, false);

        g.setColour(gold());
        g.drawLine(legendArea.getX() + 70.0f, legendArea.getCentreY(), legendArea.getX() + 94.0f, legendArea.getCentreY(), 3.0f);
        g.setColour(subtleText());
        g.drawText("Shaped",
                   juce::Rectangle<int>((int) legendArea.getX() + 100, (int) legendArea.getY(), 56, (int) legendArea.getHeight()),
                   juce::Justification::centredLeft,
                   false);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 22.0f, 1.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class TransientKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit TransientKnobLookAndFeel(bool heroStyle)
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

        juce::ColourGradient halo(ember().withAlpha(hero ? 0.27f : 0.18f), centre.x, bounds.getBottom(),
                                  rose().withAlpha(hero ? 0.16f : 0.11f), centre.x, bounds.getY(), false);
        g.setGradientFill(halo);
        g.fillEllipse(bounds.expanded(hero ? 10.0f : 6.0f));

        juce::ColourGradient shell(juce::Colour(0xff2a221e), centre.x, bounds.getY(),
                                   juce::Colour(0xff14110f), centre.x, bounds.getBottom(), false);
        shell.addColour(0.35, juce::Colour(0xff1d1714));
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
        g.setColour(juce::Colour(0xff0f0b09));
        g.strokePath(baseArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y,
                               arcBounds.getWidth() * 0.5f,
                               arcBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(gold(), arcBounds.getBottomLeft(), ember(), arcBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * (hero ? 0.30f : 0.33f));
        juce::ColourGradient capFill(juce::Colour(0xff1b1613), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff2a211d), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const float pointerLength = radius * (hero ? 0.55f : 0.48f);
        const float pointerThickness = hero ? 8.0f : 5.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.5f);
        g.setColour(shellText());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff13100e));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class TransientControlsComponent::TransientKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    TransientKnob(juce::AudioProcessorValueTreeState& state,
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
        titleLabel.setFont(juce::Font(hero ? 28.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 22.0f : 15.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, gold());

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

    ~TransientKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff201915) : juce::Colour(0xff1a1512),
                                  area.getTopLeft(),
                                  juce::Colour(0xff120f0d),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff261e19));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 28.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(gold().withAlpha(0.62f), accent.getX(), accent.getCentreY(),
                                        ember().withAlpha(0.78f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.18f).withTrimmedRight(accent.getWidth() * 0.18f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 28.0f : 20.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 20 : 14, hero ? 16 : 12);

        auto header = area.removeFromTop(hero ? 54 : 40);
        titleLabel.setBounds(header.removeFromTop(hero ? 28 : 20));
        valueLabel.setBounds(header);

        const int captionHeight = hero ? 26 : 22;
        captionLabel.setBounds(area.removeFromBottom(captionHeight));
        area.removeFromBottom(hero ? 10 : 8);

        area = area.reduced(hero ? 6 : 4, 0);
        const auto knobSize = juce::jmin(area.getWidth(), area.getHeight());
        slider.setBounds(area.withSizeKeepingCentre(knobSize, knobSize));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter != nullptr ? formatter(slider.getValue()) : juce::String(slider.getValue(), 2),
                           juce::dontSendNotification);
    }

    TransientKnobLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

TransientControlsComponent::TransientControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    summaryLabel.setText("Shape attack and tail energy independently, with a live envelope preview so the knob moves make visual sense.",
                         juce::dontSendNotification);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    badgeLabel.setText("TRANSIENT SHAPER", juce::dontSendNotification);
    badgeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    badgeLabel.setColour(juce::Label::backgroundColourId, gold());
    badgeLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff655135));
    addAndMakeVisible(badgeLabel);

    attackActivityLabel.setJustificationType(juce::Justification::centredLeft);
    attackActivityLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    attackActivityLabel.setColour(juce::Label::textColourId, shellText());
    addAndMakeVisible(attackActivityLabel);

    sustainActivityLabel.setJustificationType(juce::Justification::centredLeft);
    sustainActivityLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    sustainActivityLabel.setColour(juce::Label::textColourId, shellText());
    addAndMakeVisible(sustainActivityLabel);

    envelopeDisplay = std::make_unique<EnvelopeDisplay>(state);
    attackKnob = std::make_unique<TransientKnob>(state,
                                                 "attack",
                                                 "Attack",
                                                 "Boost or soften the front edge of hits.",
                                                 formatSignedPercent,
                                                 true);
    sustainKnob = std::make_unique<TransientKnob>(state,
                                                  "sustain",
                                                  "Sustain",
                                                  "Boost or tighten the body and tail.",
                                                  formatSignedPercent,
                                                  true);
    clipKnob = std::make_unique<TransientKnob>(state,
                                               "clip",
                                               "Clip",
                                               "Blend in soft clipping after shaping.",
                                               formatPercent,
                                               false);
    outputKnob = std::make_unique<TransientKnob>(state,
                                                 "output",
                                                 "Output",
                                                 "Trim the final level after processing.",
                                                 formatDecibels,
                                                 false);
    mixKnob = std::make_unique<TransientKnob>(state,
                                              "mix",
                                              "Mix",
                                              "Blend the shaped signal with the dry input.",
                                              formatPercent,
                                              false);

    addAndMakeVisible(*envelopeDisplay);
    addAndMakeVisible(*attackKnob);
    addAndMakeVisible(*sustainKnob);
    addAndMakeVisible(*clipKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(*mixKnob);

    refreshFromProcessor();
    startTimerHz(24);
}

TransientControlsComponent::~TransientControlsComponent()
{
    stopTimer();
}

void TransientControlsComponent::timerCallback()
{
    refreshFromProcessor();
    if (envelopeDisplay != nullptr)
        envelopeDisplay->repaint();
}

void TransientControlsComponent::refreshFromProcessor()
{
    attackActivityLabel.setText("Attack Detector: " + juce::String(juce::roundToInt(processor.getCurrentAttackActivity() * 100.0f)) + "%",
                                juce::dontSendNotification);
    sustainActivityLabel.setText("Sustain Detector: " + juce::String(juce::roundToInt(processor.getCurrentSustainActivity() * 100.0f)) + "%",
                                 juce::dontSendNotification);
}

void TransientControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff17120f), bounds.getTopLeft(),
                               juce::Colour(0xff0d0a09), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff211914));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 30.0f);

    auto topGlow = bounds.reduced(24.0f, 16.0f).removeFromTop(118.0f);
    juce::ColourGradient glow(ember().withAlpha(0.18f), topGlow.getX(), topGlow.getY(),
                              rose().withAlpha(0.10f), topGlow.getRight(), topGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(topGlow, 22.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 30.0f, 1.0f);
}

void TransientControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);

    auto topRow = area.removeFromTop(78);
    summaryLabel.setBounds(topRow.removeFromTop(34));

    auto lowerTop = topRow;
    badgeLabel.setBounds(lowerTop.removeFromLeft(170).reduced(0, 2));
    lowerTop.removeFromLeft(16);
    attackActivityLabel.setBounds(lowerTop.removeFromLeft(240));
    sustainActivityLabel.setBounds(lowerTop.removeFromLeft(250));

    area.removeFromTop(14);

    auto previewArea = area.removeFromTop(150);
    envelopeDisplay->setBounds(previewArea);

    area.removeFromTop(18);

    auto topControls = area.removeFromTop((int) std::round(area.getHeight() * 0.58f));
    auto leftHero = topControls.removeFromLeft((topControls.getWidth() - 18) / 2);
    attackKnob->setBounds(leftHero);
    topControls.removeFromLeft(18);
    sustainKnob->setBounds(topControls);

    area.removeFromTop(18);

    auto clipArea = area.removeFromLeft((area.getWidth() - 36) / 3);
    clipKnob->setBounds(clipArea);
    area.removeFromLeft(18);
    auto outputArea = area.removeFromLeft((area.getWidth() - 18) / 2);
    outputKnob->setBounds(outputArea);
    area.removeFromLeft(18);
    mixKnob->setBounds(area);
}
