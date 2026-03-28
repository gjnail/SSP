#include "EQ3VectorUI.h"

namespace
{
eq3ui::LookAndFeel lookAndFeel;

std::unique_ptr<juce::Drawable> createDrawableFromSvg(const char* svgText)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(svgText));
    return xml != nullptr ? juce::Drawable::createFromSVG(*xml) : nullptr;
}

juce::Drawable* knobFaceDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240">
  <defs>
    <radialGradient id="bezel" cx="40%" cy="28%" r="78%">
      <stop offset="0%" stop-color="#70757c"/>
      <stop offset="24%" stop-color="#3b4047"/>
      <stop offset="72%" stop-color="#13171c"/>
      <stop offset="100%" stop-color="#050607"/>
    </radialGradient>
    <radialGradient id="skirt" cx="36%" cy="24%" r="82%">
      <stop offset="0%" stop-color="#2a2f35"/>
      <stop offset="58%" stop-color="#11151a"/>
      <stop offset="100%" stop-color="#050709"/>
    </radialGradient>
    <radialGradient id="cap" cx="34%" cy="24%" r="86%">
      <stop offset="0%" stop-color="#ffffff"/>
      <stop offset="16%" stop-color="#e9edf1"/>
      <stop offset="44%" stop-color="#aeb6c0"/>
      <stop offset="72%" stop-color="#7b848f"/>
      <stop offset="100%" stop-color="#525962"/>
    </radialGradient>
    <linearGradient id="capSheen" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#ffffff" stop-opacity="0.82"/>
      <stop offset="58%" stop-color="#ffffff" stop-opacity="0.0"/>
    </linearGradient>
  </defs>
  <ellipse cx="120" cy="132" rx="102" ry="102" fill="#000000" opacity="0.34"/>
  <circle cx="120" cy="120" r="102" fill="#050607"/>
  <circle cx="120" cy="120" r="98" fill="url(#bezel)"/>
  <circle cx="120" cy="120" r="82" fill="url(#skirt)"/>
  <circle cx="120" cy="120" r="56" fill="url(#cap)"/>
  <ellipse cx="104" cy="82" rx="44" ry="18" fill="url(#capSheen)"/>
  <g stroke="#0b0e12" stroke-width="1.9" opacity="0.18">
    <line x1="120" y1="120" x2="120" y2="56"/>
    <line x1="120" y1="120" x2="162" y2="72"/>
    <line x1="120" y1="120" x2="182" y2="102"/>
    <line x1="120" y1="120" x2="174" y2="152"/>
    <line x1="120" y1="120" x2="138" y2="182"/>
    <line x1="120" y1="120" x2="102" y2="182"/>
    <line x1="120" y1="120" x2="66" y2="152"/>
    <line x1="120" y1="120" x2="58" y2="102"/>
    <line x1="120" y1="120" x2="78" y2="72"/>
  </g>
  <circle cx="120" cy="120" r="6.5" fill="#11151a"/>
</svg>
)svg");
    return drawable.get();
}

void drawCachedDrawable(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> area, float opacity = 1.0f)
{
    if (drawable == nullptr)
        return;

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(opacity);
    drawable->drawWithin(g, area, juce::RectanglePlacement::stretchToFit, 1.0f);
}
}

namespace eq3ui
{
juce::Colour background() { return juce::Colour(0xff091019); }
juce::Colour backgroundSoft() { return juce::Colour(0xff0d1722); }
juce::Colour panelTop() { return juce::Colour(0xff162230); }
juce::Colour panelBottom() { return juce::Colour(0xff0f1824); }
juce::Colour outline() { return juce::Colour(0xff8a99ad); }
juce::Colour outlineSoft() { return juce::Colour(0xff324558); }
juce::Colour textStrong() { return juce::Colour(0xffece6d9); }
juce::Colour textMuted() { return juce::Colour(0xff92a2b7); }
juce::Colour brandTeal() { return juce::Colour(0xff49d8d7); }
juce::Colour brandAmber() { return juce::Colour(0xffffbb58); }
juce::Colour brandIce() { return juce::Colour(0xffa9d9ff); }

juce::Font titleFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }
juce::Font bodyFont(float size) { return juce::Font(size * 1.18f, juce::Font::plain); }
juce::Font smallCapsFont(float size) { return juce::Font(size * 1.14f, juce::Font::bold); }

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient fill(backgroundSoft(), bounds.getTopLeft(), background(), bounds.getBottomLeft(), false);
    fill.addColour(0.2, juce::Colour(0xff12202d));
    fill.addColour(0.72, juce::Colour(0xff081018));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 18.0f);

    g.setColour(brandTeal().withAlpha(0.07f));
    g.fillEllipse(bounds.getX() - bounds.getWidth() * 0.18f,
                  bounds.getY() - bounds.getWidth() * 0.12f,
                  bounds.getWidth() * 0.45f,
                  bounds.getWidth() * 0.45f);

    g.setColour(brandIce().withAlpha(0.05f));
    g.fillEllipse(bounds.getRight() - bounds.getWidth() * 0.28f,
                  bounds.getBottom() - bounds.getWidth() * 0.26f,
                  bounds.getWidth() * 0.36f,
                  bounds.getWidth() * 0.36f);

    g.setColour(juce::Colour(0xff04080d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 18.0f, 1.8f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), 16.0f, 1.0f);
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float radius)
{
    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(), panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.18, panelTop().brighter(0.03f));
    fill.addColour(0.82, panelBottom().darker(0.02f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    auto band = bounds.reduced(16.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient accentLine(accent.withAlpha(0.95f), band.getTopLeft(),
                                    accent.withAlpha(0.06f), { band.getRight(), band.getY() }, false);
    g.setGradientFill(accentLine);
    g.fillRect(band);

    g.setColour(juce::Colour(0xff04080d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.7f);
    g.setColour(outlineSoft());
    g.drawRoundedRectangle(bounds.reduced(1.6f), radius - 0.5f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, radius - 1.5f), 0.9f);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                   juce::Slider& slider)
{
    auto available = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(1.0f);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const float squareSize = juce::jmin(available.getWidth(), available.getHeight()) * 0.98f;
    auto bounds = juce::Rectangle<float>(squareSize, squareSize).withCentre(available.getCentre()).reduced(squareSize * 0.04f);
    const float lineW = juce::jmin(7.0f, bounds.getWidth() * 0.08f);
    const float ringRadius = bounds.getWidth() * 0.49f - lineW * 0.5f;
    const auto centre = bounds.getCentre();

    for (int i = 0; i <= 18; ++i)
    {
        const float prop = (float) i / 18.0f;
        const float tickAngle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);
        juce::Point<float> a(centre.x + std::cos(tickAngle) * (ringRadius + 4.0f),
                             centre.y + std::sin(tickAngle) * (ringRadius + 4.0f));
        juce::Point<float> b(centre.x + std::cos(tickAngle) * (ringRadius + 10.0f),
                             centre.y + std::sin(tickAngle) * (ringRadius + 10.0f));
        g.setColour(juce::Colour(0xffd8cfbf).withAlpha(i % 3 == 0 ? 0.18f : 0.10f));
        g.drawLine({ a, b }, i % 3 == 0 ? 1.4f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff121922));
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.95f));
    g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobBounds = bounds.reduced(bounds.getWidth() * 0.02f);
    drawCachedDrawable(g, knobFaceDrawable(), knobBounds, 1.0f);

    const float pointerLength = knobBounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff6ecd2));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    auto switchArea = bounds.removeFromRight(52.0f).reduced(2.0f, 4.0f);
    const auto on = button.getToggleState();
    auto tint = on ? brandTeal() : juce::Colour(0xff324556);

    if (shouldDrawButtonAsHighlighted)
        tint = tint.brighter(0.08f);

    juce::ColourGradient body(on ? tint.withAlpha(0.92f) : juce::Colour(0xff15202b), switchArea.getTopLeft(),
                              on ? tint.darker(0.30f) : juce::Colour(0xff0e151d), switchArea.getBottomLeft(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);

    g.setColour(on ? tint.brighter(0.5f) : outlineSoft());
    g.drawRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f, 1.0f);

    auto thumb = juce::Rectangle<float>(switchArea.getHeight() - 6.0f, switchArea.getHeight() - 6.0f)
                     .withCentre({ on ? switchArea.getRight() - switchArea.getHeight() * 0.5f
                                      : switchArea.getX() + switchArea.getHeight() * 0.5f,
                                   switchArea.getCentreY() });
    g.setColour(juce::Colour(0xfff7f2e6));
    g.fillEllipse(thumb);
    g.setColour(juce::Colour(0xff0e141b));
    g.drawEllipse(thumb, 1.0f);

    juce::Path powerGlyph;
    auto icon = thumb.reduced(thumb.getWidth() * 0.28f);
    powerGlyph.addCentredArc(icon.getCentreX(), icon.getCentreY(), icon.getWidth() * 0.46f, icon.getHeight() * 0.46f,
                             0.0f, juce::MathConstants<float>::pi * 1.1f, juce::MathConstants<float>::pi * 1.9f, true);
    g.setColour(on ? tint.darker(0.65f) : juce::Colour(0xff5d6a79));
    g.strokePath(powerGlyph, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine(icon.getCentreX(), icon.getY() - 1.0f, icon.getCentreX(), icon.getCentreY() - 1.5f, 1.3f);

    g.setColour(textStrong());
    g.setFont(bodyFont(11.5f));
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredRight, 1);
}

LookAndFeel& getLookAndFeel()
{
    return lookAndFeel;
}

SSPKnob::SSPKnob()
{
    setLookAndFeel(&getLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity(200);
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
    setLookAndFeel(&getLookAndFeel());
}
}
