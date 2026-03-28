#include "VintageKnobComponent.h"

namespace
{
juce::StringArray getScaleLabels(const juce::String& label, bool isLargeKnob)
{
    if (label.equalsIgnoreCase("Input"))
        return {juce::String(CharPointer_UTF8("\xE2\x88\x9E")), "48", "36", "30", "24", "18", "12", "6", "0"};

    if (label.equalsIgnoreCase("Output"))
        return {juce::String(CharPointer_UTF8("\xE2\x88\x9E")), "48", "36", "30", "24", "18", "12", "6", "0"};

    if (label.equalsIgnoreCase("Attack") || label.equalsIgnoreCase("Release"))
        return {"1", "3", "5", "7"};

    if (label.equalsIgnoreCase("Mix"))
        return {"DRY", "", "WET"};

    return isLargeKnob ? juce::StringArray{"0", "5", "10"} : juce::StringArray{"0", "5", "10"};
}
}

VintageKnobComponent::VintageKnobComponent(juce::String label, juce::Colour accent, bool largeKnob)
    : labelText(std::move(label)),
      accentColour(accent),
      isLargeKnob(largeKnob),
      lookAndFeel(accent)
{
    slider.setLookAndFeel(&lookAndFeel);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.18f,
                               juce::MathConstants<float>::pi * 2.82f,
                               true);
    addAndMakeVisible(slider);

    valueFormatter = [] (double value) { return juce::String(value, 1); };
}

VintageKnobComponent::~VintageKnobComponent()
{
    slider.setLookAndFeel(nullptr);
}

void VintageKnobComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto bottomBox = bounds.removeFromBottom(isLargeKnob ? 34 : 30);
    auto labelBand = juce::Rectangle<int>(bounds.getX(), bottomBox.getY() - 18, bounds.getWidth(), 16);
    const auto dialBounds = slider.getBounds().toFloat();
    const auto centre = dialBounds.getCentre();
    const float radius = juce::jmin(dialBounds.getWidth(), dialBounds.getHeight()) * 0.5f;
    const float startAngle = juce::MathConstants<float>::pi * 1.18f;
    const float endAngle = juce::MathConstants<float>::pi * 2.82f;

    const auto scaleLabels = getScaleLabels(labelText, isLargeKnob);
    const bool denseScale = isLargeKnob && scaleLabels.size() > 7;
    const int tickCount = juce::jmax(10, scaleLabels.size() * (isLargeKnob ? 2 : 3));
    for (int i = 0; i <= tickCount; ++i)
    {
        const float angle = juce::jmap((float) i, 0.0f, (float) tickCount, startAngle, endAngle);
        const bool major = (i % (isLargeKnob ? 2 : 3) == 0);
        const auto dotPoint = centre.getPointOnCircumference(radius + (isLargeKnob ? 7.0f : 5.0f), angle);
        g.setColour(major ? juce::Colour(0xffe4e7eb) : juce::Colour(0xff8993a0));
        g.fillEllipse(dotPoint.x - (major ? 2.0f : 1.2f), dotPoint.y - (major ? 2.0f : 1.2f), major ? 4.0f : 2.4f, major ? 4.0f : 2.4f);
    }

    for (int i = 0; i < scaleLabels.size(); ++i)
    {
        const float angle = scaleLabels.size() == 1
            ? startAngle
            : juce::jmap((float) i, 0.0f, (float) juce::jmax(1, scaleLabels.size() - 1), startAngle, endAngle);
        const auto labelPoint = centre.getPointOnCircumference(radius + (denseScale ? 34.0f : isLargeKnob ? 30.0f : 22.0f), angle);
        g.setColour(juce::Colour(0xffeff2f5).withAlpha(isLargeKnob ? 0.92f : 0.85f));
        g.setFont(juce::Font("Arial", denseScale ? 12.5f : isLargeKnob ? 16.0f : 12.0f, juce::Font::plain));
        g.drawFittedText(scaleLabels[i],
                         juce::Rectangle<int>((int) labelPoint.x - (denseScale ? 20 : 18),
                                              (int) labelPoint.y - 10,
                                              denseScale ? 40 : 36,
                                              20),
                         juce::Justification::centred, 1);
    }

    g.setColour(juce::Colour(0xffe3ebf4).withAlpha(0.95f));
    g.setFont(juce::Font("Arial", isLargeKnob ? 15.0f : 13.0f, juce::Font::bold));
    g.drawText(labelText.toUpperCase(), labelBand, juce::Justification::centred, false);

    auto valueOuter = bottomBox.reduced(isLargeKnob ? 28 : 20, 4);
    g.setColour(juce::Colour(0xaa050608));
    g.fillRoundedRectangle(valueOuter.toFloat(), 7.0f);
    g.setColour(juce::Colour(0xff8f9baa).withAlpha(0.7f));
    g.drawRoundedRectangle(valueOuter.toFloat().reduced(0.5f), 7.0f, 1.0f);

    g.setColour(juce::Colour(0xfff7efca));
    g.setFont(juce::Font("Consolas", isLargeKnob ? 15.0f : 13.0f, juce::Font::bold));
    g.drawText(valueFormatter(slider.getValue()), valueOuter, juce::Justification::centred, false);
}

void VintageKnobComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(isLargeKnob ? 10 : 8);
    area.removeFromBottom(isLargeKnob ? 56 : 48);
    slider.setBounds(area.reduced(isLargeKnob ? 14 : 12));
}

void VintageKnobComponent::setValueFormatter(std::function<juce::String(double)> formatter)
{
    valueFormatter = std::move(formatter);
    repaint();
}

VintageKnobComponent::KnobLookAndFeel::KnobLookAndFeel(juce::Colour accentIn)
    : accent(accentIn)
{
}

void VintageKnobComponent::KnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                             float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                                             juce::Slider& slider)
{
    auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(8.0f);
    const auto radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();

    juce::ColourGradient outer(juce::Colour(0xff4f5257), centre.x, area.getY(),
                               juce::Colour(0xff090b0e), centre.x, area.getBottom(), false);
    g.setGradientFill(outer);
    g.fillEllipse(area);
    g.setColour(juce::Colour(0xff0d0f12));
    g.drawEllipse(area, 1.2f);

    const float angleStep = juce::MathConstants<float>::twoPi / 46.0f;
    for (int i = 0; i < 46; ++i)
    {
        const float angle = angleStep * (float) i;
        const auto p1 = centre.getPointOnCircumference(radius - 3.0f, angle);
        const auto p2 = centre.getPointOnCircumference(radius - 10.0f, angle);
        g.setColour(i % 2 == 0 ? juce::Colour(0xff1e2329) : juce::Colour(0xff050607));
        g.drawLine({p1, p2}, slider.getWidth() > 180 ? 1.7f : 1.2f);
    }

    auto ring = area.reduced(radius * 0.1f);
    g.setColour(juce::Colour(0xff050607));
    g.fillEllipse(ring);

    const float arcRadius = radius - (slider.getWidth() > 180 ? 14.0f : 12.0f);
    const float angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
    juce::ignoreUnused(arcRadius, accent);

    auto cap = ring.reduced(slider.getWidth() > 180 ? 22.0f : 17.0f);
    juce::ColourGradient capGradient(juce::Colour(0xfff3f5f8), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff848a92), cap.getCentreX(), cap.getBottom(), false);
    capGradient.addColour(0.3, juce::Colour(0xffd9dde2));
    capGradient.addColour(0.6, juce::Colour(0xff565b63));
    g.setGradientFill(capGradient);
    g.fillEllipse(cap);
    g.setColour(juce::Colour(0xff0f1012).withAlpha(0.45f));
    g.drawEllipse(cap, 1.0f);

    for (int i = 0; i < 14; ++i)
    {
        const float spokeAngle = juce::MathConstants<float>::twoPi * (float) i / 14.0f;
        const auto start = cap.getCentre().getPointOnCircumference(cap.getWidth() * 0.08f, spokeAngle);
        const auto end = cap.getCentre().getPointOnCircumference(cap.getWidth() * 0.48f, spokeAngle);
        g.setColour(juce::Colour(0x18ffffff));
        g.drawLine({start, end}, 0.8f);
    }

    auto highlight = cap.removeFromTop(cap.getHeight() * 0.28f).reduced(cap.getWidth() * 0.14f, 0.0f);
    g.setColour(juce::Colour(0x30ffffff));
    g.fillEllipse(highlight);

    const auto pointerBase = centre.getPointOnCircumference(radius * 0.1f, angle);
    const auto pointerTip = centre.getPointOnCircumference(radius * 0.64f, angle);
    g.setColour(juce::Colour(0xfffaf2d5));
    g.drawLine({pointerBase, pointerTip}, slider.getWidth() > 180 ? 3.8f : 2.5f);
    g.setColour(juce::Colour(0xff141619));
    g.fillEllipse(centre.x - 4.5f, centre.y - 4.5f, 9.0f, 9.0f);
}
