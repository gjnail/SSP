#include "ShifterVectorUI.h"

namespace
{
shifterui::VectorLookAndFeel vectorLookAndFeel;

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

juce::Drawable* panelTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 320">
  <defs>
    <linearGradient id="panel" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#202b36"/>
      <stop offset="52%" stop-color="#161e28"/>
      <stop offset="100%" stop-color="#101720"/>
    </linearGradient>
    <radialGradient id="glowA" cx="0%" cy="0%" r="55%">
      <stop offset="0%" stop-color="#2fdbff" stop-opacity="0.16"/>
      <stop offset="100%" stop-color="#2fdbff" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="glowB" cx="100%" cy="0%" r="65%">
      <stop offset="0%" stop-color="#ffcf4d" stop-opacity="0.14"/>
      <stop offset="100%" stop-color="#ffcf4d" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <rect width="800" height="320" fill="url(#panel)"/>
  <rect width="800" height="320" fill="url(#glowA)"/>
  <rect width="800" height="320" fill="url(#glowB)"/>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* editorTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1200">
  <defs>
    <radialGradient id="cyanGlow" cx="0%" cy="0%" r="52%">
      <stop offset="0%" stop-color="#2fdbff" stop-opacity="0.16"/>
      <stop offset="100%" stop-color="#2fdbff" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="mintGlow" cx="100%" cy="100%" r="44%">
      <stop offset="0%" stop-color="#79f0cf" stop-opacity="0.10"/>
      <stop offset="100%" stop-color="#79f0cf" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="goldGlow" cx="100%" cy="0%" r="54%">
      <stop offset="0%" stop-color="#ffcf4d" stop-opacity="0.16"/>
      <stop offset="100%" stop-color="#ffcf4d" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <rect width="1600" height="1200" fill="#0b1118"/>
  <rect width="1600" height="1200" fill="url(#cyanGlow)"/>
  <rect width="1600" height="1200" fill="url(#mintGlow)"/>
  <rect width="1600" height="1200" fill="url(#goldGlow)"/>
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
    return juce::jlimit(4.0f, 8.0f, requested * 0.35f);
}

juce::Colour backgroundColour() { return juce::Colour(0xff0b1118); }
juce::Colour panelTopColour() { return juce::Colour(0xff1c2530); }
juce::Colour panelBottomColour() { return juce::Colour(0xff151d26); }
juce::Colour outlineColour() { return juce::Colour(0xff8b95a6); }
juce::Colour outlineSoftColour() { return juce::Colour(0xff31404f); }
juce::Colour strongTextColour() { return juce::Colour(0xffede5d8); }
juce::Colour mutedTextColour() { return juce::Colour(0xff98a2b3); }
juce::Colour goldColour() { return juce::Colour(0xffffcf4d); }
juce::Colour cyanColour() { return juce::Colour(0xff3ecfff); }
juce::Colour mintColour() { return juce::Colour(0xff79f0cf); }

juce::Font makeFont(float size, bool bold)
{
    return juce::Font(size, bold ? juce::Font::bold : juce::Font::plain);
}

class KnobValueEntry final : public juce::Component
{
public:
    explicit KnobValueEntry(shifterui::SSPKnob& knobToControl)
        : knob(&knobToControl)
    {
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(valueEditor);

        nameLabel.setText(knobToControl.getName(), juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centredLeft);
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d0d8));
        nameLabel.setFont(makeFont(18.0f, false));

        valueEditor.setText(knobToControl.getTextFromValue(knobToControl.getValue()), juce::dontSendNotification);
        valueEditor.setJustification(juce::Justification::centred);
        valueEditor.setFont(makeFont(18.0f, false));
        valueEditor.setSelectAllWhenFocused(true);
        valueEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff34414e));
        valueEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff4f5f72));
        valueEditor.setColour(juce::TextEditor::focusedOutlineColourId, cyanColour());
        valueEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffedf6ff));
        valueEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff459cff));
        valueEditor.setColour(juce::CaretComponent::caretColourId, juce::Colour(0xffedf6ff));
        valueEditor.onReturnKey = [this] { commitAndClose(); };
        valueEditor.onEscapeKey = [this] { dismiss(); };
        valueEditor.onFocusLost = [this] { dismiss(); };

        const auto titleWidth = nameLabel.getFont().getStringWidthFloat(knobToControl.getName());
        const auto valueWidth = valueEditor.getFont().getStringWidthFloat(knobToControl.getTextFromValue(knobToControl.getValue()));
        const int width = juce::jlimit(220, 320, (int) std::ceil(juce::jmax(titleWidth + 34.0f, valueWidth + 70.0f)));
        setSize(width, 82);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 10);
        nameLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(10);
        valueEditor.setBounds(area.removeFromTop(34));
    }

private:
    void commitAndClose()
    {
        if (knob != nullptr)
        {
            const auto value = juce::jlimit(knob->getMinimum(),
                                            knob->getMaximum(),
                                            knob->getValueFromText(valueEditor.getText()));
            knob->setValue(value, juce::sendNotificationSync);
        }

        dismiss();
    }

    void dismiss()
    {
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
            callout->dismiss();
    }

    juce::Component::SafePointer<shifterui::SSPKnob> knob;
    juce::Label nameLabel;
    juce::TextEditor valueEditor;
};
} // namespace

namespace shifterui
{
juce::Colour background() { return backgroundColour(); }
juce::Colour panelTop() { return panelTopColour(); }
juce::Colour panelBottom() { return panelBottomColour(); }
juce::Colour outline() { return outlineColour(); }
juce::Colour outlineSoft() { return outlineSoftColour(); }
juce::Colour textStrong() { return strongTextColour(); }
juce::Colour textMuted() { return mutedTextColour(); }
juce::Colour brandGold() { return goldColour(); }
juce::Colour brandCyan() { return cyanColour(); }
juce::Colour brandMint() { return mintColour(); }

juce::Font titleFont(float size) { return makeFont(size * 1.08f, true); }
juce::Font bodyFont(float size) { return makeFont(size * 1.28f, false); }
juce::Font smallCapsFont(float size) { return makeFont(size * 1.02f, true); }

void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 10.0f;
    juce::Path clip;
    clip.addRoundedRectangle(bounds, radius);

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(clip);
        g.setColour(backgroundColour());
        g.fillRoundedRectangle(bounds, radius);
        drawCachedDrawable(g, editorTextureDrawable(), bounds, 1.0f);
    }

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

    juce::Path clip;
    clip.addRoundedRectangle(bounds, radius);

    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(clip);
        g.setColour(panelBottomColour());
        g.fillRoundedRectangle(bounds, radius);
        drawCachedDrawable(g, panelTextureDrawable(), bounds, 0.90f);
    }

    auto headerBand = bounds.reduced(1.0f).removeFromTop(34.0f);
    juce::ColourGradient headerFill(juce::Colour(0x20121922), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accent.withAlpha(0.14f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, radius);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.96f), accentLine.getTopLeft(),
                              accent.withAlpha(0.12f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(outlineColour());
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
    label->setColour(juce::Label::outlineColourId, outlineColour());
    label->setColour(juce::Label::textColourId, strongTextColour());
    return label;
}

juce::Slider::SliderLayout VectorLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto layout = LookAndFeel_V4::getSliderLayout(slider);
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

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff121922));
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

void VectorLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    auto lensTop = on ? goldColour() : juce::Colour(0xff1b2430);
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
    g.setColour(outlineColour());
    g.drawRoundedRectangle(bounds, 5.0f, 1.4f);

    auto lens = bounds.reduced(4.0f, 3.0f);
    juce::ColourGradient fill(lensTop, lens.getTopLeft(), lensBottom, lens.getBottomLeft(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(lens, 4.0f);
}

void VectorLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(titleFont(11.0f));
    g.setColour(button.getToggleState() ? juce::Colour(0xff0b0d10) : strongTextColour());
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

    g.setColour(juce::Colour(0xff0a0e13));
    g.drawRoundedRectangle(bounds, 5.0f, 1.6f);
    g.setColour(box.hasKeyboardFocus(true) ? goldColour() : outlineColour());
    g.drawRoundedRectangle(bounds.reduced(1.6f), 4.0f, 1.0f);
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

void VectorLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto switchArea = bounds.removeFromRight(52.0f).reduced(2.0f, 4.0f);
    const bool on = button.getToggleState();
    auto tint = on ? cyanColour() : juce::Colour(0xff324556);

    juce::ColourGradient body(on ? tint.withAlpha(0.92f) : juce::Colour(0xff15202b), switchArea.getTopLeft(),
                              on ? tint.darker(0.30f) : juce::Colour(0xff0e151d), switchArea.getBottomLeft(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);

    auto thumb = juce::Rectangle<float>(switchArea.getHeight() - 6.0f, switchArea.getHeight() - 6.0f)
                     .withCentre({ on ? switchArea.getRight() - switchArea.getHeight() * 0.5f
                                      : switchArea.getX() + switchArea.getHeight() * 0.5f,
                                   switchArea.getCentreY() });
    g.setColour(juce::Colour(0xfff7f2e6));
    g.fillEllipse(thumb);
    g.setColour(strongTextColour());
    g.setFont(bodyFont(11.5f));
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft, 1);
}

void VectorLookAndFeel::drawCallOutBoxBackground(juce::CallOutBox& box, juce::Graphics& g,
                                                 const juce::Path& path, juce::Image& cachedImage)
{
    if (cachedImage.isNull())
    {
        cachedImage = { juce::Image::ARGB, box.getWidth(), box.getHeight(), true };
        juce::Graphics shadowGraphics(cachedImage);
        juce::DropShadow(juce::Colours::black.withAlpha(0.55f), 18, { 0, 10 }).drawForPath(shadowGraphics, path);
    }

    g.drawImageAt(cachedImage, 0, 0);
    g.setColour(juce::Colour(0xff202020));
    g.fillPath(path);
}

int VectorLookAndFeel::getCallOutBoxBorderSize(const juce::CallOutBox&)
{
    return 10;
}

float VectorLookAndFeel::getCallOutBoxCornerSize(const juce::CallOutBox&)
{
    return 4.0f;
}

VectorLookAndFeel& getVectorLookAndFeel()
{
    return vectorLookAndFeel;
}

SSPKnob::SSPKnob()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity(220);
    setPopupDisplayEnabled(true, false, nullptr);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void SSPKnob::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::Slider::mouseDoubleClick(event);
}

void SSPKnob::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        showValueEntryDialog();
        return;
    }

    juce::Slider::mouseUp(event);
}

void SSPKnob::showValueEntryDialog()
{
    auto content = std::make_unique<KnobValueEntry>(*this);
    auto& callout = juce::CallOutBox::launchAsynchronously(std::move(content), getScreenBounds(), nullptr);
    callout.setLookAndFeel(&getVectorLookAndFeel());
    callout.setArrowSize(0.0f);
}

SSPButton::SSPButton(const juce::String& text)
    : juce::TextButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}

SSPDropdown::SSPDropdown()
{
    setLookAndFeel(&getVectorLookAndFeel());
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}
} // namespace shifterui
