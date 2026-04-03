#include "ClipperVectorUI.h"

namespace
{
clipperui::VectorLookAndFeel vectorLookAndFeel;

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
      <stop offset="0%" stop-color="#6d7884"/>
      <stop offset="26%" stop-color="#303741"/>
      <stop offset="72%" stop-color="#12161b"/>
      <stop offset="100%" stop-color="#050607"/>
    </radialGradient>
    <radialGradient id="cap" cx="34%" cy="24%" r="86%">
      <stop offset="0%" stop-color="#ffffff"/>
      <stop offset="18%" stop-color="#edf1f5"/>
      <stop offset="52%" stop-color="#aeb8c3"/>
      <stop offset="100%" stop-color="#545f69"/>
    </radialGradient>
  </defs>
  <ellipse cx="120" cy="132" rx="102" ry="102" fill="#000000" opacity="0.34"/>
  <circle cx="120" cy="120" r="102" fill="#040607"/>
  <circle cx="120" cy="120" r="98" fill="url(#bezel)"/>
  <circle cx="120" cy="120" r="82" fill="#0c1116"/>
  <circle cx="120" cy="120" r="58" fill="url(#cap)"/>
  <ellipse cx="104" cy="84" rx="46" ry="18" fill="#ffffff" opacity="0.55"/>
  <circle cx="120" cy="120" r="6.5" fill="#0c1217"/>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* screwDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <radialGradient id="head" cx="35%" cy="28%" r="76%">
      <stop offset="0%" stop-color="#dfe5eb"/>
      <stop offset="25%" stop-color="#98a2ad"/>
      <stop offset="70%" stop-color="#464c53"/>
      <stop offset="100%" stop-color="#1d2024"/>
    </radialGradient>
  </defs>
  <circle cx="32" cy="32" r="28" fill="#090b0d"/>
  <circle cx="32" cy="32" r="24" fill="url(#head)"/>
  <path d="M17 31h30" stroke="#2d3136" stroke-width="5" stroke-linecap="round"/>
  <path d="M18 29h28" stroke="#d1d6db" stroke-width="1.4" stroke-linecap="round" opacity="0.6"/>
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

void drawCornerScrews(juce::Graphics& g, juce::Rectangle<float> bounds, float inset = 14.0f, float size = 14.0f)
{
    if (bounds.getWidth() < size * 3.0f || bounds.getHeight() < size * 3.0f)
        return;

    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({bounds.getX() + inset, bounds.getY() + inset}), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({bounds.getRight() - inset, bounds.getY() + inset}), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({bounds.getX() + inset, bounds.getBottom() - inset}), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({bounds.getRight() - inset, bounds.getBottom() - inset}), 0.85f);
}

juce::Colour background() { return juce::Colour(0xff0b1116); }
juce::Colour backgroundSoft() { return juce::Colour(0xff101821); }
juce::Colour panelTop() { return juce::Colour(0xff19232d); }
juce::Colour panelBottom() { return juce::Colour(0xff121920); }
juce::Colour outline() { return juce::Colour(0xff768598); }
juce::Colour panelShadow() { return juce::Colour(0x70000000); }
} // namespace

namespace clipperui
{
juce::Colour textStrong() { return juce::Colour(0xfff0eadc); }
juce::Colour textMuted() { return juce::Colour(0xff93a2b4); }
juce::Colour accentGold() { return juce::Colour(0xffffc95f); }
juce::Colour accentCyan() { return juce::Colour(0xff5fd6ff); }
juce::Colour accentCoral() { return juce::Colour(0xffff8b69); }
juce::Colour accentMint() { return juce::Colour(0xff8bf4cb); }

juce::Font titleFont(float size) { return juce::Font(size, juce::Font::bold); }
juce::Font sectionFont(float size) { return juce::Font(size, juce::Font::bold); }
juce::Font bodyFont(float size) { return juce::Font(size, juce::Font::plain); }

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 11.0f;
    juce::ColourGradient bg(juce::Colour(0xff0d141a), bounds.getTopLeft(),
                            juce::Colour(0xff101821), bounds.getBottomLeft(), false);
    bg.addColour(0.24, juce::Colour(0xff121b24));
    bg.addColour(0.78, juce::Colour(0xff091016));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(accentGold().withAlpha(0.08f));
    g.fillEllipse(bounds.getX() - bounds.getWidth() * 0.18f,
                  bounds.getY() - bounds.getWidth() * 0.12f,
                  bounds.getWidth() * 0.44f,
                  bounds.getWidth() * 0.44f);

    g.setColour(accentCyan().withAlpha(0.06f));
    g.fillEllipse(bounds.getRight() - bounds.getWidth() * 0.24f,
                  bounds.getBottom() - bounds.getWidth() * 0.20f,
                  bounds.getWidth() * 0.36f,
                  bounds.getWidth() * 0.36f);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), radius - 1.0f, 1.0f);
    drawCornerScrews(g, bounds, 18.0f, 15.0f);
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float radius)
{
    juce::DropShadow(panelShadow(), 18, {0, 8}).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(), panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.18, panelTop().brighter(0.04f));
    fill.addColour(0.86, panelBottom().darker(0.05f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    auto headerBand = bounds.reduced(1.0f).removeFromTop(34.0f);
    juce::ColourGradient headerFill(juce::Colour(0x180f171f), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accent.withAlpha(0.14f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, radius);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.96f), accentLine.getTopLeft(),
                              accent.withAlpha(0.14f), {accentLine.getRight(), accentLine.getY()}, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds.reduced(1.7f), radius - 0.5f, 1.15f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, radius - 1.0f), 1.0f);
    drawCornerScrews(g, bounds, 13.0f, 12.0f);
}

juce::Label* VectorLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(bodyFont(11.5f));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, backgroundSoft());
    label->setColour(juce::Label::outlineColourId, outline());
    label->setColour(juce::Label::textColourId, textStrong());
    return label;
}

juce::Slider::SliderLayout VectorLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto layout = LookAndFeel_V4::getSliderLayout(slider);

    if (slider.getSliderStyle() != juce::Slider::RotaryHorizontalVerticalDrag
        && slider.getSliderStyle() != juce::Slider::RotaryVerticalDrag
        && slider.getSliderStyle() != juce::Slider::Rotary)
        return layout;

    auto bounds = slider.getLocalBounds().reduced(2);
    const int textBoxHeight = juce::jmax(0, slider.getTextBoxHeight());
    layout.textBoxBounds = bounds.removeFromBottom(textBoxHeight);
    layout.sliderBounds = bounds;
    return layout;
}

void VectorLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto available = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(1.0f);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

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
        g.setColour(textStrong().withAlpha(i % 3 == 0 ? 0.18f : 0.10f));
        g.drawLine({a, b}, i % 3 == 0 ? 1.35f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff111820));
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.94f));
    g.strokePath(active, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    drawCachedDrawable(g, knobFaceDrawable(), bounds, 1.0f);

    const float pointerLength = bounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff7edd6));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void VectorLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    auto lensTop = on ? accentGold() : juce::Colour(0xff1a2430);
    auto lensBottom = on ? juce::Colour(0xffd39f2d) : juce::Colour(0xff101721);

    if (over)
    {
        lensTop = lensTop.brighter(0.08f);
        lensBottom = lensBottom.brighter(0.04f);
    }

    if (down)
    {
        lensTop = lensTop.darker(0.06f);
        lensBottom = lensBottom.darker(0.08f);
    }

    g.setColour(juce::Colour(0xff0a0f14));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds, 5.0f, 1.2f);

    auto lens = bounds.reduced(4.0f, 3.0f);
    juce::ColourGradient fill(lensTop, lens.getTopLeft(), lensBottom, lens.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(lens, 4.0f);
    g.setColour((on ? juce::Colour(0xfffff3d2) : outline()).withAlpha(0.92f));
    g.drawRoundedRectangle(lens, 4.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.24f : 0.08f));
    g.fillRoundedRectangle(lens.removeFromTop(lens.getHeight() * 0.40f), 4.0f);
}

void VectorLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(sectionFont(12.0f));
    g.setColour(button.getToggleState() ? juce::Colour(0xff0b0d10) : textStrong());
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

void VectorLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    juce::ColourGradient bg(juce::Colour(0xff18212b), bounds.getTopLeft(),
                            juce::Colour(0xff111922), bounds.getBottomLeft(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(juce::Colour(0xff0a0f14));
    g.drawRoundedRectangle(bounds, 5.0f, 1.5f);
    g.setColour(box.hasKeyboardFocus(true) ? accentGold() : outline());
    g.drawRoundedRectangle(bounds.reduced(1.6f), 4.0f, 1.0f);

    auto arrowArea = bounds.removeFromRight(28.0f);
    juce::Path arrow;
    arrow.startNewSubPath(arrowArea.getCentreX() - 5.0f, arrowArea.getCentreY() - 2.0f);
    arrow.lineTo(arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    arrow.lineTo(arrowArea.getCentreX() + 5.0f, arrowArea.getCentreY() - 2.0f);
    g.setColour(textStrong());
    g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font VectorLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return bodyFont(13.0f);
}

juce::Font VectorLookAndFeel::getLabelFont(juce::Label&)
{
    return bodyFont(11.5f);
}

void VectorLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 38, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

VectorLookAndFeel& getLookAndFeel()
{
    return vectorLookAndFeel;
}

Knob::Knob()
{
    setLookAndFeel(&getLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    setTextBoxIsEditable(true);
    setMouseDragSensitivity(220);
}

Button::Button(const juce::String& text)
    : juce::TextButton(text)
{
    setLookAndFeel(&getLookAndFeel());
}

Dropdown::Dropdown()
{
    setLookAndFeel(&getLookAndFeel());
    setColour(juce::ComboBox::backgroundColourId, backgroundSoft());
    setColour(juce::ComboBox::outlineColourId, outline());
    setColour(juce::ComboBox::textColourId, textStrong());
    setColour(juce::ComboBox::arrowColourId, textStrong());
}
} // namespace clipperui
