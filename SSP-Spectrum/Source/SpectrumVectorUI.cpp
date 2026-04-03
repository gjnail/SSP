#include "SpectrumVectorUI.h"

namespace
{
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

float hardRadius(float requested = 8.0f)
{
    return juce::jlimit(4.0f, 10.0f, requested * 0.42f);
}
} // namespace

namespace spectrumui
{
const Palette& getPalette(int themeIndex)
{
    static const std::array<Palette, 5> palettes{{
        {
            juce::Colour(0xff0a1118), juce::Colour(0xff101a23), juce::Colour(0xff1a2731), juce::Colour(0xff111820),
            juce::Colour(0xff8592a2), juce::Colour(0xff283846), juce::Colour(0xfff2ede3), juce::Colour(0xff9aabbd),
            juce::Colour(0xff5fd8ff), juce::Colour(0xffffcf52), juce::Colour(0xffff9960), juce::Colour(0xff75e2a8),
            juce::Colour(0xffffc978), juce::Colour(0xffff7272)
        },
        {
            juce::Colour(0xff110d17), juce::Colour(0xff191224), juce::Colour(0xff281a37), juce::Colour(0xff17111f),
            juce::Colour(0xff9486ab), juce::Colour(0xff35284a), juce::Colour(0xfff4edf8), juce::Colour(0xffb2a3c5),
            juce::Colour(0xffb08cff), juce::Colour(0xff61dcff), juce::Colour(0xffff8f70), juce::Colour(0xff7ce8c4),
            juce::Colour(0xffffd075), juce::Colour(0xffff6d83)
        },
        {
            juce::Colour(0xff091510), juce::Colour(0xff102019), juce::Colour(0xff173227), juce::Colour(0xff0d1814),
            juce::Colour(0xff85a28f), juce::Colour(0xff213d31), juce::Colour(0xffedf5ef), juce::Colour(0xff9ab5a4),
            juce::Colour(0xff76f0c0), juce::Colour(0xff6eb6ff), juce::Colour(0xffffcf74), juce::Colour(0xff7af2b1),
            juce::Colour(0xffffd075), juce::Colour(0xffff7f77)
        },
        {
            juce::Colour(0xff170b0b), juce::Colour(0xff251111), juce::Colour(0xff351717), juce::Colour(0xff1b0d0d),
            juce::Colour(0xffa38a8a), juce::Colour(0xff452323), juce::Colour(0xfff7ece7), juce::Colour(0xffc0a39f),
            juce::Colour(0xffff6e5e), juce::Colour(0xffffc05e), juce::Colour(0xff69d8ff), juce::Colour(0xff8ee29c),
            juce::Colour(0xffffcf75), juce::Colour(0xffff6a6a)
        },
        {
            juce::Colour(0xff0c1015), juce::Colour(0xff141922), juce::Colour(0xff1d2430), juce::Colour(0xff11161e),
            juce::Colour(0xff8b96a8), juce::Colour(0xff2c3543), juce::Colour(0xffedf0f4), juce::Colour(0xffa1aaba),
            juce::Colour(0xff85c8ff), juce::Colour(0xfff0b86b), juce::Colour(0xff8ce3f2), juce::Colour(0xff79d7a0),
            juce::Colour(0xffffd37d), juce::Colour(0xffff7f7f)
        }
    }};

    return palettes[(size_t) juce::jlimit(0, (int) palettes.size() - 1, themeIndex)];
}

juce::StringArray getThemeNames()
{
    return { "Ocean", "Nova", "Mint", "Heat", "Steel" };
}

juce::Font titleFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }
juce::Font bodyFont(float size) { return juce::Font(size * 1.32f, juce::Font::plain); }
juce::Font smallCapsFont(float size) { return juce::Font(size * 1.08f, juce::Font::bold); }

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette)
{
    const float radius = 11.0f;
    juce::ColourGradient bg(palette.background.brighter(0.08f), bounds.getTopLeft(),
                            palette.backgroundSoft.darker(0.10f), bounds.getBottomLeft(), false);
    bg.addColour(0.24, palette.backgroundSoft);
    bg.addColour(0.78, palette.background.darker(0.12f));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(palette.accentPrimary.withAlpha(0.08f));
    g.fillEllipse(bounds.getX() - bounds.getWidth() * 0.18f,
                  bounds.getY() - bounds.getWidth() * 0.18f,
                  bounds.getWidth() * 0.46f,
                  bounds.getWidth() * 0.46f);

    g.setColour(palette.accentSecondary.withAlpha(0.07f));
    g.fillEllipse(bounds.getRight() - bounds.getWidth() * 0.24f,
                  bounds.getBottom() - bounds.getWidth() * 0.24f,
                  bounds.getWidth() * 0.40f,
                  bounds.getWidth() * 0.40f);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), radius - 1.0f, 1.0f);
    drawCornerScrews(g, bounds, 18.0f, 16.0f);
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette, juce::Colour accent, float radius)
{
    const float hard = hardRadius(radius);
    juce::DropShadow(juce::Colours::black.withAlpha(0.28f), 18, { 0, 8 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(palette.panelTop, bounds.getTopLeft(), palette.panelBottom, bounds.getBottomLeft(), false);
    fill.addColour(0.14, palette.panelTop.brighter(0.03f));
    fill.addColour(0.82, palette.panelBottom.darker(0.05f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, hard);

    auto inner = bounds.reduced(1.0f);
    auto headerBand = inner.removeFromTop(34.0f);
    juce::ColourGradient headerFill(juce::Colour(0x20121922), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accent.withAlpha(0.14f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, hard);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.96f), accentLine.getTopLeft(),
                              accent.withAlpha(0.10f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), hard, 2.0f);
    g.setColour(palette.outline);
    g.drawRoundedRectangle(bounds.reduced(1.8f), hard - 0.5f, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, hard - 1.0f), 1.0f);
    drawCornerScrews(g, bounds, 13.0f, 13.0f);
}

void drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette, juce::String text,
               juce::Colour fill, juce::Colour textColour)
{
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.36f);
    g.setColour(palette.outline.withAlpha(0.65f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.36f, 1.0f);
    g.setColour(textColour);
    g.setFont(smallCapsFont(bounds.getHeight() * 0.38f));
    g.drawFittedText(text, bounds.toNearestInt(), juce::Justification::centred, 1);
}

LookAndFeel::LookAndFeel(ThemeGetter themeGetter)
    : getThemeIndex(std::move(themeGetter))
{
}

const Palette& LookAndFeel::palette() const
{
    return getPalette(getThemeIndex != nullptr ? getThemeIndex() : 0);
}

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    const auto& c = palette();
    label->setFont(bodyFont(11.5f));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, c.backgroundSoft);
    label->setColour(juce::Label::outlineColourId, c.outline);
    label->setColour(juce::Label::textColourId, c.textStrong);
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
    const auto& c = palette();
    auto available = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(1.0f);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId).isTransparent()
                            ? c.accentPrimary
                            : slider.findColour(juce::Slider::rotarySliderFillColourId);
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
        g.setColour(c.textStrong.withAlpha(i % 3 == 0 ? 0.18f : 0.10f));
        g.drawLine({ a, b }, i % 3 == 0 ? 1.4f : 1.0f);
    }

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(c.backgroundSoft.darker(0.22f));
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(active, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    drawCachedDrawable(g, knobFaceDrawable(), bounds.reduced(bounds.getWidth() * 0.02f), 1.0f);

    const float pointerLength = bounds.getHeight() * 0.34f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.5f, -pointerLength, 5.0f, pointerLength, 2.5f);
    g.setColour(juce::Colour(0xfff6ecd2));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    const auto& c = palette();
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    auto lensTop = on ? c.accentSecondary : c.panelTop.brighter(0.02f);
    auto lensBottom = on ? c.accentPrimary : c.backgroundSoft;

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
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(c.outline);
    g.drawRoundedRectangle(bounds, 6.0f, 1.3f);

    auto lens = bounds.reduced(4.0f, 3.0f);
    juce::ColourGradient fill(lensTop, lens.getTopLeft(), lensBottom, lens.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(lens, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(lens.reduced(0.5f), 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto& c = palette();
    g.setColour(button.getToggleState() ? c.background : c.textStrong);
    g.setFont(smallCapsFont((float) button.getHeight() * 0.36f));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(8, 1), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    const auto& c = palette();
    auto bounds = button.getLocalBounds().toFloat();
    auto body = bounds.reduced(0.5f);
    auto switchArea = body.removeFromLeft(58.0f).reduced(2.0f, 8.0f);

    g.setColour(c.textStrong.withAlpha(button.isEnabled() ? 1.0f : 0.45f));
    g.setFont(bodyFont(12.0f));
    g.drawFittedText(button.getButtonText(), body.toNearestInt(), juce::Justification::centredLeft, 1);

    auto track = switchArea.withHeight(20.0f).withCentre(switchArea.getCentre());
    g.setColour(juce::Colour(0xff0b1015));
    g.fillRoundedRectangle(track, 10.0f);
    g.setColour(c.outline);
    g.drawRoundedRectangle(track, 10.0f, 1.1f);

    auto fill = track.reduced(3.0f);
    fill.setWidth(fill.getWidth() * (button.getToggleState() ? 1.0f : 0.56f));
    juce::ColourGradient toggleFill(button.getToggleState() ? c.accentSecondary : c.panelTop,
                                    fill.getTopLeft(),
                                    button.getToggleState() ? c.accentPrimary : c.backgroundSoft,
                                    fill.getBottomLeft(),
                                    false);
    g.setGradientFill(toggleFill);
    g.fillRoundedRectangle(fill, 7.0f);

    auto knob = juce::Rectangle<float>(18.0f, 18.0f).withCentre({
        button.getToggleState() ? track.getRight() - 12.0f : track.getX() + 12.0f,
        track.getCentreY()
    });

    if (over)
        knob = knob.expanded(0.5f);
    if (down)
        knob = knob.translated(0.0f, 0.5f);

    g.setColour(juce::Colour(0xfff3efe5));
    g.fillEllipse(knob);
    g.setColour(c.outlineSoft.withAlpha(0.85f));
    g.drawEllipse(knob, 1.0f);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                               int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&)
{
    const auto& c = palette();
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    g.setColour(juce::Colour(0xff0a0e13));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(c.outline);
    g.drawRoundedRectangle(bounds, 6.0f, 1.3f);

    auto fill = bounds.reduced(4.0f, 3.0f);
    juce::ColourGradient gradient(c.panelTop, fill.getTopLeft(), c.backgroundSoft, fill.getBottomLeft(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(fill, 4.0f);

    juce::Path arrow;
    const float cx = (float) buttonX + (float) buttonW * 0.5f;
    const float cy = (float) buttonY + (float) buttonH * 0.5f;
    arrow.startNewSubPath(cx - 5.0f, cy - 2.5f);
    arrow.lineTo(cx, cy + 3.0f);
    arrow.lineTo(cx + 5.0f, cy - 2.5f);

    g.setColour(c.textStrong.withAlpha(0.9f));
    g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return bodyFont(11.5f);
}

juce::Font LookAndFeel::getLabelFont(juce::Label&)
{
    return bodyFont(11.5f);
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(box.getLocalBounds().reduced(10, 0).withTrimmedRight(26));
    label.setFont(getComboBoxFont(box));
}

SSPKnob::SSPKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 22);
    setTextBoxIsEditable(true);
    setRotaryParameters(juce::degreesToRadians(215.0f),
                        juce::degreesToRadians(505.0f),
                        true);
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
    setTriggeredOnMouseDown(false);
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
}

SSPDropdown::SSPDropdown()
{
    setJustificationType(juce::Justification::centredLeft);
}
} // namespace spectrumui
