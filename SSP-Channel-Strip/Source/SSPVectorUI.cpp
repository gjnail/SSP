#include "SSPVectorUI.h"

namespace
{
ssp::ui::VectorLookAndFeel vectorLookAndFeel;

juce::Path makeArcPath(juce::Rectangle<float> bounds, float startAngle, float endAngle, float thickness)
{
    juce::Path path;
    path.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                       bounds.getWidth() * 0.5f - thickness, bounds.getHeight() * 0.5f - thickness,
                       0.0f, startAngle, endAngle, true);
    return path;
}
} // namespace

namespace ssp::ui
{
juce::Colour getStereoModeColour(int stereoMode)
{
    switch (stereoMode)
    {
        case PluginProcessor::mid: return juce::Colour(0xfff0b661);
        case PluginProcessor::side: return juce::Colour(0xff9d82ff);
        case PluginProcessor::left: return juce::Colour(0xff58c9ff);
        case PluginProcessor::right: return juce::Colour(0xff5ce0a0);
        case PluginProcessor::stereo:
        default: return juce::Colour(0xff63d0ff);
    }
}

juce::String getStereoModeShortLabel(int stereoMode)
{
    switch (stereoMode)
    {
        case PluginProcessor::mid: return "M";
        case PluginProcessor::side: return "S";
        case PluginProcessor::left: return "L";
        case PluginProcessor::right: return "R";
        case PluginProcessor::stereo:
        default: return "St";
    }
}

void VectorLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(7.0f, 5.0f);
    auto knobBounds = bounds.removeFromTop(bounds.getWidth());
    const auto trackColour = juce::Colour(0xff0d1217);
    const auto accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    g.setColour(trackColour);
    g.strokePath(makeArcPath(knobBounds, rotaryStartAngle, rotaryEndAngle, 7.0f), juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(accentColour.withAlpha(0.85f));
    g.strokePath(makeArcPath(knobBounds, rotaryStartAngle, angle, 7.0f), juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ColourGradient knobGradient(juce::Colour(0xff2a3036), knobBounds.getCentreX(), knobBounds.getY(),
                                      juce::Colour(0xff11151a), knobBounds.getCentreX(), knobBounds.getBottom(), false);
    g.setGradientFill(knobGradient);
    auto cap = knobBounds.reduced(10.0f);
    g.fillEllipse(cap);
    g.setColour(juce::Colour(0xff050607));
    g.drawEllipse(cap, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawEllipse(cap.reduced(1.6f), 1.0f);

    for (float tick = 0.0f; tick <= 1.0f; tick += 0.25f)
    {
        const float tickAngle = juce::jmap(tick, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        juce::Point<float> inner(knobBounds.getCentreX() + std::cos(tickAngle) * (knobBounds.getWidth() * 0.34f),
                                 knobBounds.getCentreY() + std::sin(tickAngle) * (knobBounds.getWidth() * 0.34f));
        juce::Point<float> outer(knobBounds.getCentreX() + std::cos(tickAngle) * (knobBounds.getWidth() * 0.42f),
                                 knobBounds.getCentreY() + std::sin(tickAngle) * (knobBounds.getWidth() * 0.42f));
        g.setColour(tick == 0.5f ? juce::Colour(0xffede5d8).withAlpha(0.75f) : juce::Colour(0xff4c5663));
        g.drawLine({inner, outer}, tick == 0.5f ? 1.8f : 1.0f);
    }

    juce::Point<float> indicatorStart(knobBounds.getCentreX(), knobBounds.getCentreY());
    juce::Point<float> indicatorEnd(knobBounds.getCentreX() + std::cos(angle) * (knobBounds.getWidth() * 0.24f),
                                    knobBounds.getCentreY() + std::sin(angle) * (knobBounds.getWidth() * 0.24f));
    g.setColour(juce::Colour(0xffede5d8));
    g.drawLine({indicatorStart, indicatorEnd}, 2.2f);
}

void VectorLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(8.0f, 4.0f);
    const auto accentColour = slider.findColour(juce::Slider::trackColourId).isTransparent()
                                  ? juce::Colour(0xff63d0ff)
                                  : slider.findColour(juce::Slider::trackColourId);

    if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical)
    {
        auto track = bounds.withTrimmedLeft(bounds.getWidth() * 0.35f).withTrimmedRight(bounds.getWidth() * 0.35f);
        juce::ColourGradient back(juce::Colour(0xff101418), track.getCentreX(), track.getY(),
                                  juce::Colour(0xff20262d), track.getCentreX(), track.getBottom(), false);
        g.setGradientFill(back);
        g.fillRoundedRectangle(track, 8.0f);
        g.setColour(juce::Colour(0xff050607));
        g.drawRoundedRectangle(track, 8.0f, 1.1f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(track.reduced(1.2f), 7.0f, 1.0f);

        auto filled = track.withY(sliderPos).withBottom(track.getBottom());
        juce::ColourGradient fill(accentColour.brighter(0.15f), filled.getCentreX(), filled.getY(),
                                  accentColour.darker(0.3f), filled.getCentreX(), filled.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(filled, 8.0f);

        auto thumb = juce::Rectangle<float>(bounds.getX() + 2.0f, sliderPos - 8.0f, bounds.getWidth() - 4.0f, 16.0f);
        juce::ColourGradient thumbFill(juce::Colour(0xfff3f1ea), thumb.getCentreX(), thumb.getY(),
                                       juce::Colour(0xff9fa7b0), thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill(thumbFill);
        g.fillRoundedRectangle(thumb, 5.0f);
        g.setColour(juce::Colour(0xff101216));
        g.drawRoundedRectangle(thumb, 5.0f, 1.0f);
        return;
    }

    juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
}

void VectorLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = backgroundColour.isTransparent() ? juce::Colour(0xff24282d) : backgroundColour;
    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.06f);

    juce::ColourGradient gradient(fill.brighter(0.05f), bounds.getCentreX(), bounds.getY(),
                                  fill.darker(0.08f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff050607));
    g.drawRoundedRectangle(bounds, 6.0f, 1.15f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(1.1f), 5.0f, 1.0f);
}

void VectorLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setColour(juce::Colour(0xffede5d8));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void VectorLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto switchBounds = bounds.removeFromLeft(46.0f).reduced(0.0f, 6.0f);
    auto textBounds = bounds.reduced(8.0f, 0.0f);
    const bool on = button.getToggleState();
    auto fill = on ? juce::Colour(0xff3ecfff) : juce::Colour(0xff1a1f25);

    juce::ColourGradient gradient(fill.brighter(0.08f), switchBounds.getCentreX(), switchBounds.getY(),
                                  fill.darker(on ? 0.2f : 0.05f), switchBounds.getCentreX(), switchBounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(switchBounds, switchBounds.getHeight() * 0.5f);
    g.setColour(on ? fill.brighter(0.18f) : juce::Colour(0xff39444f));
    g.drawRoundedRectangle(switchBounds, switchBounds.getHeight() * 0.5f, 1.0f);

    auto thumb = switchBounds.reduced(4.0f);
    thumb.setWidth(thumb.getHeight());
    if (on)
        thumb.setX(switchBounds.getRight() - thumb.getWidth() - 4.0f);

    juce::ColourGradient thumbGradient(juce::Colour(0xfff3f1ea), thumb.getCentreX(), thumb.getY(),
                                       juce::Colour(0xffa9b0b8), thumb.getCentreX(), thumb.getBottom(), false);
    g.setGradientFill(thumbGradient);
    g.fillEllipse(thumb);

    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xffede5d8) : juce::Colour(0xffc4ccd6));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), textBounds.toNearestInt(), juce::Justification::centredLeft, false);
}

void VectorLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    juce::ColourGradient gradient(juce::Colour(0xff24282d), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour(0xff161a1f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff050607));
    g.drawRoundedRectangle(bounds, 6.0f, 1.1f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(1.1f), 5.0f, 1.0f);

    juce::Path arrow;
    const auto arrowBounds = juce::Rectangle<float>((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH).reduced(10.0f, 11.0f);
    arrow.startNewSubPath(arrowBounds.getX(), arrowBounds.getY());
    arrow.lineTo(arrowBounds.getCentreX(), arrowBounds.getBottom());
    arrow.lineTo(arrowBounds.getRight(), arrowBounds.getY());
    g.setColour(juce::Colour(0xffede5d8));
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font VectorLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(12.0f, juce::Font::bold);
}

juce::Font VectorLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(12.0f, juce::Font::bold);
}

void VectorLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

VectorLookAndFeel& getVectorLookAndFeel()
{
    return vectorLookAndFeel;
}

SSPKnob::SSPKnob()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    setDoubleClickReturnValue(true, getDoubleClickReturnValue());
    setMouseDragSensitivity(220);
}

void SSPKnob::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::Slider::mouseDoubleClick(event);
}

void SSPKnob::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        setValue(getDoubleClickReturnValue(), juce::sendNotificationSync);

    juce::Slider::mouseUp(event);
}

SSPButton::SSPButton(const juce::String& text)
    : juce::TextButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff17222f));
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}

SSPDropdown::SSPDropdown()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111822));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff31475d));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
}

SSPSlider::SSPSlider()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 20);
    setColour(juce::Slider::trackColourId, juce::Colour(0xff63d0ff));
    setMouseDragSensitivity(180);
}
} // namespace ssp::ui
