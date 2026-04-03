#include "EchoVectorUI.h"

namespace
{
ssp::ui::VectorLookAndFeel vectorLookAndFeel;

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

juce::Drawable* screwDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <radialGradient id="head" cx="35%" cy="28%" r="76%">
      <stop offset="0%" stop-color="#e2e6ea"/>
      <stop offset="25%" stop-color="#9ca3ab"/>
      <stop offset="70%" stop-color="#4b5056"/>
      <stop offset="100%" stop-color="#1f2226"/>
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

void drawCornerScrew(juce::Graphics& g, juce::Point<float> centre, float size, float opacity = 1.0f)
{
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre(centre), opacity);
}

void drawCornerScrews(juce::Graphics& g, juce::Rectangle<float> bounds, float inset = 14.0f, float size = 15.0f)
{
    if (bounds.getWidth() < size * 3.0f || bounds.getHeight() < size * 3.0f)
        return;

    drawCornerScrew(g, { bounds.getX() + inset, bounds.getY() + inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getRight() - inset, bounds.getY() + inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getX() + inset, bounds.getBottom() - inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getRight() - inset, bounds.getBottom() - inset }, size, 0.85f);
}

juce::Colour background() { return juce::Colour(0xff0b1118); }
juce::Colour backgroundSoft() { return juce::Colour(0xff111922); }
juce::Colour panelTop() { return juce::Colour(0xff1c2530); }
juce::Colour panelBottom() { return juce::Colour(0xff151d26); }
juce::Colour outline() { return juce::Colour(0xff8b95a6); }
juce::Colour outlineSoft() { return juce::Colour(0xff31404f); }
juce::Colour textStrong() { return juce::Colour(0xffede5d8); }
juce::Colour textMuted() { return juce::Colour(0xff98a2b3); }
juce::Colour brandGold() { return juce::Colour(0xffffcf4d); }
juce::Colour brandCyan() { return juce::Colour(0xff3ecfff); }

juce::Font sectionFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }
juce::Font bodyFont(float size) { return juce::Font(size * 1.34f, juce::Font::plain); }

float hardRadius(float requested = 8.0f)
{
    return juce::jlimit(4.0f, 8.0f, requested * 0.35f);
}
} // namespace

namespace ssp::ui
{
void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 10.0f;
    juce::ColourGradient bg(juce::Colour(0xff0e151d), bounds.getTopLeft(),
                            juce::Colour(0xff111922), bounds.getBottomLeft(), false);
    bg.addColour(0.18, juce::Colour(0xff111922));
    bg.addColour(0.72, juce::Colour(0xff0a1017));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(brandGold().withAlpha(0.09f));
    g.fillEllipse(bounds.getX() - bounds.getWidth() * 0.16f,
                  bounds.getY() - bounds.getWidth() * 0.16f,
                  bounds.getWidth() * 0.42f,
                  bounds.getWidth() * 0.42f);

    g.setColour(brandCyan().withAlpha(0.07f));
    g.fillEllipse(bounds.getRight() - bounds.getWidth() * 0.20f,
                  bounds.getBottom() - bounds.getWidth() * 0.20f,
                  bounds.getWidth() * 0.34f,
                  bounds.getWidth() * 0.34f);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), radius - 1.0f, 1.0f);
    drawCornerScrews(g, bounds, 18.0f, 16.0f);
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float requestedRadius)
{
    const float radius = hardRadius(requestedRadius);
    juce::DropShadow(juce::Colours::black.withAlpha(0.3f), 18, { 0, 8 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(), panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.16, panelTop().brighter(0.03f));
    fill.addColour(0.84, panelBottom().darker(0.03f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    auto inner = bounds.reduced(1.0f);
    auto headerBand = inner.removeFromTop(34.0f);
    juce::ColourGradient headerFill(juce::Colour(0x20121922), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accent.withAlpha(0.12f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, radius);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.96f), accentLine.getTopLeft(),
                              accent.withAlpha(0.12f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds.reduced(1.8f), radius - 0.5f, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, radius - 1.0f), 1.0f);

    drawCornerScrews(g, bounds, 13.0f, 13.0f);
}

juce::Label* VectorLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(bodyFont(11.5f));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, juce::Colour(0xff111922));
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
        g.setColour(juce::Colour(0xffd8cfbf).withAlpha(i % 3 == 0 ? 0.18f : 0.10f));
        g.drawLine({ a, b }, i % 3 == 0 ? 1.4f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff121922));
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(active, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobBounds = bounds.reduced(bounds.getWidth() * 0.02f);
    drawCachedDrawable(g, knobFaceDrawable(), knobBounds, 1.0f);

    const float pointerLength = knobBounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff6ecd2));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void VectorLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    auto lensTop = on ? brandGold() : juce::Colour(0xff1b2430);
    auto lensBottom = on ? juce::Colour(0xffc69511) : juce::Colour(0xff111922);

    if (over)
    {
        lensTop = lensTop.brighter(0.08f);
        lensBottom = lensBottom.brighter(0.04f);
    }

    if (down)
    {
        lensTop = lensTop.darker(0.07f);
        lensBottom = lensBottom.darker(0.09f);
    }

    g.setColour(juce::Colour(0xff0a0e13));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds, 5.0f, 1.4f);

    auto lens = bounds.reduced(4.0f, 3.0f);
    juce::ColourGradient fill(lensTop, lens.getTopLeft(), lensBottom, lens.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(lens, 4.0f);
    g.setColour((on ? juce::Colour(0xfffff4d1) : outline()).withAlpha(0.92f));
    g.drawRoundedRectangle(lens, 4.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.26f : 0.10f));
    g.fillRoundedRectangle(lens.removeFromTop(lens.getHeight() * 0.40f), 4.0f);
}

void VectorLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(sectionFont(12.0f));
    g.setColour(button.getToggleState() ? juce::Colour(0xff0b0d10) : textStrong());
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

void VectorLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    auto bounds = button.getLocalBounds();
    const float tickSize = 22.0f;
    auto box = juce::Rectangle<float>(0.0f, (float) ((bounds.getHeight() - (int) tickSize) / 2), tickSize, tickSize).reduced(1.0f);
    const bool ticked = button.getToggleState();

    g.setColour(juce::Colour(0xff0c1117));
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(box, 4.0f, 1.1f);

    auto lens = box.reduced(3.0f);
    auto top = ticked ? brandGold() : juce::Colour(0xff18212b);
    auto bottom = ticked ? juce::Colour(0xffc69511) : juce::Colour(0xff111922);
    if (over || down)
    {
        top = top.brighter(0.07f);
        bottom = bottom.brighter(0.03f);
    }

    juce::ColourGradient fill(top, lens.getTopLeft(), bottom, lens.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(lens, 3.0f);
    g.setColour((ticked ? juce::Colour(0xfffff3cc) : outline()).withAlpha(button.isEnabled() ? 0.95f : 0.4f));
    g.drawRoundedRectangle(lens, 3.0f, 1.0f);

    if (ticked)
    {
        juce::Path tick;
        tick.startNewSubPath(lens.getX() + lens.getWidth() * 0.22f, lens.getCentreY());
        tick.lineTo(lens.getX() + lens.getWidth() * 0.43f, lens.getBottom() - lens.getHeight() * 0.24f);
        tick.lineTo(lens.getRight() - lens.getWidth() * 0.18f, lens.getY() + lens.getHeight() * 0.22f);
        g.setColour(juce::Colour(0xff0b0d10));
        g.strokePath(tick, juce::PathStrokeType(1.65f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(button.isEnabled() ? textStrong() : textMuted());
    g.setFont(bodyFont(13.0f));
    g.drawFittedText(button.getButtonText(), bounds.withTrimmedLeft(30), juce::Justification::centredLeft, 1);
}

void VectorLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    juce::ColourGradient bg(juce::Colour(0xff18212b), bounds.getTopLeft(),
                            juce::Colour(0xff111922), bounds.getBottomLeft(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(juce::Colour(0xff0a0e13));
    g.drawRoundedRectangle(bounds, 5.0f, 1.6f);
    g.setColour(box.hasKeyboardFocus(true) ? brandGold() : outline());
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

VectorLookAndFeel& getVectorLookAndFeel()
{
    return vectorLookAndFeel;
}

SSPKnob::SSPKnob()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    setTextBoxIsEditable(true);
    setMouseDragSensitivity(220);
}

void SSPKnob::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::Slider::mouseDoubleClick(event);
}

void SSPKnob::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && isDoubleClickReturnEnabled())
        setValue(getDoubleClickReturnValue(), juce::sendNotificationSync);

    juce::Slider::mouseUp(event);
}

SSPButton::SSPButton(const juce::String& text)
    : juce::TextButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
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
    setColour(juce::ComboBox::outlineColourId, outline());
    setColour(juce::ComboBox::textColourId, textStrong());
    setColour(juce::ComboBox::arrowColourId, textStrong());
}
} // namespace ssp::ui
