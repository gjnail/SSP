#include "ReducerVectorUI.h"

namespace
{
reducerui::LookAndFeel lookAndFeel;

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

void drawCornerScrews(juce::Graphics& g, juce::Rectangle<float> bounds, float inset = 14.0f, float size = 15.0f)
{
    if (bounds.getWidth() < size * 3.0f || bounds.getHeight() < size * 3.0f)
        return;

    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({ bounds.getX() + inset, bounds.getY() + inset }), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({ bounds.getRight() - inset, bounds.getY() + inset }), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({ bounds.getX() + inset, bounds.getBottom() - inset }), 0.85f);
    drawCachedDrawable(g, screwDrawable(), juce::Rectangle<float>(size, size).withCentre({ bounds.getRight() - inset, bounds.getBottom() - inset }), 0.85f);
}

float hardRadius(float requested = 8.0f)
{
    return juce::jlimit(4.0f, 8.0f, requested * 0.35f);
}
}

namespace reducerui
{
juce::Colour background() { return juce::Colour(0xff0a1016); }
juce::Colour backgroundSoft() { return juce::Colour(0xff121a23); }
juce::Colour panelTop() { return juce::Colour(0xff1b2631); }
juce::Colour panelBottom() { return juce::Colour(0xff141c24); }
juce::Colour outline() { return juce::Colour(0xff8f99a8); }
juce::Colour outlineSoft() { return juce::Colour(0xff2f4251); }
juce::Colour textStrong() { return juce::Colour(0xffede5d8); }
juce::Colour textMuted() { return juce::Colour(0xff93a0af); }
juce::Colour accent() { return juce::Colour(0xff11d4ff); }
juce::Colour accentBright() { return juce::Colour(0xff9cecff); }

juce::Font titleFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }
juce::Font bodyFont(float size) { return juce::Font(size * 1.34f, juce::Font::plain); }
juce::Font smallCapsFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 10.0f;
    juce::ColourGradient bg(juce::Colour(0xff0d151d), bounds.getTopLeft(),
                            juce::Colour(0xff111923), bounds.getBottomLeft(), false);
    bg.addColour(0.2, juce::Colour(0xff121c27));
    bg.addColour(0.78, juce::Colour(0xff0a1017));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);

    auto glow = bounds.reduced(18.0f, 50.0f);
    juce::ColourGradient accentGlow(juce::Colours::transparentBlack, glow.getX(), glow.getCentreY(),
                                    juce::Colours::transparentBlack, glow.getRight(), glow.getCentreY(), false);
    accentGlow.addColour(0.5, accent().withAlpha(0.16f));
    g.setGradientFill(accentGlow);
    g.fillEllipse(glow.expanded(24.0f, 30.0f));

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), radius - 1.0f, 1.0f);
    drawCornerScrews(g, bounds, 18.0f, 16.0f);
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accentColour, float radius)
{
    const float hard = hardRadius(radius);
    juce::DropShadow(juce::Colours::black.withAlpha(0.3f), 18, { 0, 8 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(), panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.18, panelTop().brighter(0.03f));
    fill.addColour(0.86, panelBottom().darker(0.03f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, hard);

    auto inner = bounds.reduced(1.0f);
    auto headerBand = inner.removeFromTop(34.0f);
    juce::ColourGradient headerFill(juce::Colour(0x20121b25), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accentColour.withAlpha(0.12f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, hard);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accentColour.withAlpha(0.96f), accentLine.getTopLeft(),
                              accentColour.withAlpha(0.12f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), hard, 2.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds.reduced(1.8f), hard - 0.5f, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, hard - 1.0f), 1.0f);
    drawCornerScrews(g, bounds, 13.0f, 13.0f);
}

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(bodyFont(11.5f));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, backgroundSoft());
    label->setColour(juce::Label::outlineColourId, outline());
    label->setColour(juce::Label::textColourId, textStrong());
    return label;
}

juce::Slider::SliderLayout LookAndFeel::getSliderLayout(juce::Slider& slider)
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

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                   juce::Slider& slider)
{
    auto available = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(1.0f);
    const auto ringColour = slider.findColour(juce::Slider::rotarySliderFillColourId, true);
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

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(ringColour.withAlpha(0.95f));
    g.strokePath(active, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobBounds = bounds.reduced(bounds.getWidth() * 0.02f);
    drawCachedDrawable(g, knobFaceDrawable(), knobBounds, 1.0f);

    const float pointerLength = knobBounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff6ecd2));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                               int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);

    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    auto tint = accent();

    juce::ColourGradient fill(backgroundSoft(), bounds.getTopLeft(), juce::Colour(0xff0d141b), bounds.getBottomLeft(), false);
    if (isButtonDown)
        fill.addColour(0.2, tint.withAlpha(0.10f));

    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(outlineSoft().withAlpha(0.9f));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - 14.0f;
    const auto cy = bounds.getCentreY() + 1.0f;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.5f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(accentBright().withAlpha(box.isEnabled() ? 0.95f : 0.45f));
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return bodyFont(11.0f);
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    auto switchArea = bounds.removeFromRight(52.0f).reduced(2.0f, 4.0f);
    const auto on = button.getToggleState();
    auto tint = on ? accent() : juce::Colour(0xff324556);

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

LookAndFeel& getVectorLookAndFeel()
{
    return lookAndFeel;
}

SSPKnob::SSPKnob()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setTextBoxIsEditable(false);
    setMouseDragSensitivity(220);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setPopupMenuEnabled(false);
}

SSPKnob::~SSPKnob()
{
    setLookAndFeel(nullptr);
}

void SSPKnob::paint(juce::Graphics& g)
{
    auto available = getLocalBounds().toFloat().reduced(1.0f);
    const auto ringColour = findColour(juce::Slider::rotarySliderFillColourId, true);
    const auto rotary = getRotaryParameters();
    const float sliderPos = (float) valueToProportionOfLength(getValue());
    const float angle = rotary.startAngleRadians + sliderPos * (rotary.endAngleRadians - rotary.startAngleRadians);

    const float squareSize = juce::jmin(available.getWidth(), available.getHeight()) * 0.98f;
    auto bounds = juce::Rectangle<float>(squareSize, squareSize).withCentre(available.getCentre()).reduced(squareSize * 0.04f);
    const float lineW = juce::jmin(7.0f, bounds.getWidth() * 0.08f);
    const float ringRadius = bounds.getWidth() * 0.49f - lineW * 0.5f;
    const auto centre = bounds.getCentre();

    for (int i = 0; i <= 18; ++i)
    {
        const float prop = (float) i / 18.0f;
        const float tickAngle = rotary.startAngleRadians + prop * (rotary.endAngleRadians - rotary.startAngleRadians);
        juce::Point<float> a(centre.x + std::cos(tickAngle) * (ringRadius + 4.0f),
                             centre.y + std::sin(tickAngle) * (ringRadius + 4.0f));
        juce::Point<float> b(centre.x + std::cos(tickAngle) * (ringRadius + 10.0f),
                             centre.y + std::sin(tickAngle) * (ringRadius + 10.0f));
        g.setColour(juce::Colour(0xffd8cfbf).withAlpha(i % 3 == 0 ? 0.18f : 0.10f));
        g.drawLine({ a, b }, i % 3 == 0 ? 1.4f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                        rotary.startAngleRadians, rotary.endAngleRadians, true);
    g.setColour(juce::Colour(0xff121922));
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                         rotary.startAngleRadians, angle, true);
    g.setColour(ringColour.withAlpha(0.95f));
    g.strokePath(active, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobBounds = bounds.reduced(bounds.getWidth() * 0.02f);
    drawCachedDrawable(g, knobFaceDrawable(), knobBounds, 1.0f);

    const float pointerLength = knobBounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff6ecd2));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void SSPKnob::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        showValueEntry();
        return;
    }

    juce::Slider::mouseDown(event);
}

void SSPKnob::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::Slider::mouseDoubleClick(event);
}

void SSPKnob::mouseUp(const juce::MouseEvent& event)
{
    juce::Slider::mouseUp(event);
}

void SSPKnob::showValueEntry()
{
    auto* editor = new juce::AlertWindow("Set Value", {}, juce::MessageBoxIconType::NoIcon, this);
    editor->setLookAndFeel(&getVectorLookAndFeel());
    editor->addTextEditor("value", getTextFromValue(getValue()), "Value");
    editor->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    editor->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* textEditor = editor->getTextEditor("value"))
    {
        textEditor->setSelectAllWhenFocused(true);
        textEditor->selectAll();
    }

    auto safeThis = juce::Component::SafePointer<SSPKnob>(this);
    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(editor);

    editor->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, safeWindow] (int result)
    {
        if (result == 1 && safeThis != nullptr && safeWindow != nullptr)
        {
            const auto parsedValue = juce::jlimit(safeThis->getMinimum(), safeThis->getMaximum(),
                                                  safeThis->getValueFromText(safeWindow->getTextEditorContents("value")));
            safeThis->setValue(parsedValue, juce::sendNotificationAsync);
        }
    }), true);
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}
}
