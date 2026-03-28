#pragma once

#include <JuceHeader.h>

namespace minimizeui
{
inline juce::Colour background()      { return juce::Colour(0xffece4d4); }
inline juce::Colour backgroundSoft()  { return juce::Colour(0xfff7f0e3); }
inline juce::Colour panelTop()        { return juce::Colour(0xfffffaf1); }
inline juce::Colour panelBottom()     { return juce::Colour(0xffeee4d1); }
inline juce::Colour outline()         { return juce::Colour(0xff121419); }
inline juce::Colour outlineSoft()     { return juce::Colour(0xff4b505a); }
inline juce::Colour textStrong()      { return juce::Colour(0xff11141a); }
inline juce::Colour textMuted()       { return juce::Colour(0xff646977); }
inline juce::Colour displayBg()       { return juce::Colour(0xff0e1620); }
inline juce::Colour brandGold()       { return juce::Colour(0xffbf8613); }
inline juce::Colour brandCyan()       { return juce::Colour(0xff42cff7); }
inline juce::Colour brandEmber()      { return juce::Colour(0xfff26a43); }
inline juce::Colour brandCream()      { return juce::Colour(0xfff8f1e5); }

inline juce::Font headerFont(float size)  { return juce::Font(size, juce::Font::bold); }
inline juce::Font sectionFont(float size) { return juce::Font(size, juce::Font::bold); }
inline juce::Font bodyFont(float size)    { return juce::Font(size, juce::Font::plain); }

inline std::unique_ptr<juce::Drawable> createDrawableFromSvg(const char* svgText)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(svgText));
    return xml != nullptr ? juce::Drawable::createFromSVG(*xml) : nullptr;
}

inline juce::Drawable* heroKnobFaceDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240">
  <defs>
    <linearGradient id="shell" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#303740"/>
      <stop offset="48%" stop-color="#161b22"/>
      <stop offset="100%" stop-color="#090d12"/>
    </radialGradient>
    <linearGradient id="inner" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#fffdf7"/>
      <stop offset="55%" stop-color="#f3e8d4"/>
      <stop offset="100%" stop-color="#dcc9ab"/>
    </linearGradient>
    <radialGradient id="core" cx="36%" cy="30%" r="74%">
      <stop offset="0%" stop-color="#fffef9"/>
      <stop offset="60%" stop-color="#f7ecd9"/>
      <stop offset="100%" stop-color="#ead6b6"/>
    </radialGradient>
  </defs>
  <circle cx="120" cy="128" r="96" fill="#000000" opacity="0.16"/>
  <circle cx="120" cy="120" r="96" fill="url(#shell)"/>
  <circle cx="120" cy="120" r="72" fill="url(#inner)"/>
  <circle cx="120" cy="120" r="52" fill="url(#core)"/>
  <circle cx="120" cy="120" r="90" fill="none" stroke="#4bd3ff" stroke-opacity="0.08" stroke-width="4"/>
  <path d="M120 44 A76 76 0 0 1 188 84" fill="none" stroke="#f0b449" stroke-width="10" stroke-linecap="round"/>
  <circle cx="96" cy="82" r="14" fill="#ffffff" opacity="0.34"/>
</svg>
)svg");
    return drawable.get();
}

inline juce::Drawable* smallKnobFaceDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240">
  <defs>
    <linearGradient id="shell" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#2c333c"/>
      <stop offset="100%" stop-color="#0c1116"/>
    </linearGradient>
    <linearGradient id="face" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#fffdf8"/>
      <stop offset="60%" stop-color="#f2e7d3"/>
      <stop offset="100%" stop-color="#ddccb0"/>
    </linearGradient>
  </defs>
  <circle cx="120" cy="126" r="88" fill="#000000" opacity="0.14"/>
  <circle cx="120" cy="120" r="88" fill="url(#shell)"/>
  <circle cx="120" cy="120" r="62" fill="url(#face)"/>
  <circle cx="120" cy="120" r="83" fill="none" stroke="#4bd3ff" stroke-opacity="0.11" stroke-width="4"/>
  <path d="M58 162 A78 78 0 0 1 92 54" fill="none" stroke="#4bd3ff" stroke-width="11" stroke-linecap="round"/>
  <path d="M149 52 A78 78 0 0 1 184 88" fill="none" stroke="#bf8613" stroke-width="11" stroke-linecap="round"/>
  <circle cx="96" cy="82" r="12" fill="#ffffff" opacity="0.3"/>
</svg>
)svg");
    return drawable.get();
}

inline juce::Drawable* screwDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <radialGradient id="head" cx="35%" cy="28%" r="76%">
      <stop offset="0%" stop-color="#ece4d6"/>
      <stop offset="25%" stop-color="#c7c0b4"/>
      <stop offset="70%" stop-color="#676b73"/>
      <stop offset="100%" stop-color="#1d2126"/>
    </radialGradient>
  </defs>
  <circle cx="32" cy="32" r="28" fill="#090b0d"/>
  <circle cx="32" cy="32" r="24" fill="url(#head)"/>
  <path d="M17 31h30" stroke="#2d3136" stroke-width="5" stroke-linecap="round"/>
  <path d="M18 29h28" stroke="#e6ddcf" stroke-width="1.4" stroke-linecap="round" opacity="0.6"/>
</svg>
)svg");
    return drawable.get();
}

inline juce::Drawable* panelTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 320">
  <defs>
    <linearGradient id="paper" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#fffaf0"/>
      <stop offset="18%" stop-color="#f7efdf"/>
      <stop offset="58%" stop-color="#efe6d6"/>
      <stop offset="100%" stop-color="#ebe1cf"/>
    </linearGradient>
  </defs>
  <rect width="800" height="320" fill="url(#paper)"/>
  <g opacity="0.07" stroke="#191d22" stroke-width="1">
    <line x1="0" y1="10" x2="800" y2="10"/>
    <line x1="0" y1="18" x2="800" y2="18"/>
    <line x1="0" y1="26" x2="800" y2="26"/>
    <line x1="0" y1="34" x2="800" y2="34"/>
    <line x1="0" y1="42" x2="800" y2="42"/>
    <line x1="0" y1="50" x2="800" y2="50"/>
    <line x1="0" y1="58" x2="800" y2="58"/>
    <line x1="0" y1="66" x2="800" y2="66"/>
    <line x1="0" y1="74" x2="800" y2="74"/>
    <line x1="0" y1="82" x2="800" y2="82"/>
    <line x1="0" y1="90" x2="800" y2="90"/>
    <line x1="0" y1="98" x2="800" y2="98"/>
    <line x1="0" y1="106" x2="800" y2="106"/>
    <line x1="0" y1="114" x2="800" y2="114"/>
    <line x1="0" y1="122" x2="800" y2="122"/>
    <line x1="0" y1="130" x2="800" y2="130"/>
    <line x1="0" y1="138" x2="800" y2="138"/>
    <line x1="0" y1="146" x2="800" y2="146"/>
    <line x1="0" y1="154" x2="800" y2="154"/>
    <line x1="0" y1="162" x2="800" y2="162"/>
    <line x1="0" y1="170" x2="800" y2="170"/>
    <line x1="0" y1="178" x2="800" y2="178"/>
    <line x1="0" y1="186" x2="800" y2="186"/>
    <line x1="0" y1="194" x2="800" y2="194"/>
    <line x1="0" y1="202" x2="800" y2="202"/>
    <line x1="0" y1="210" x2="800" y2="210"/>
    <line x1="0" y1="218" x2="800" y2="218"/>
    <line x1="0" y1="226" x2="800" y2="226"/>
    <line x1="0" y1="234" x2="800" y2="234"/>
    <line x1="0" y1="242" x2="800" y2="242"/>
    <line x1="0" y1="250" x2="800" y2="250"/>
    <line x1="0" y1="258" x2="800" y2="258"/>
    <line x1="0" y1="266" x2="800" y2="266"/>
    <line x1="0" y1="274" x2="800" y2="274"/>
    <line x1="0" y1="282" x2="800" y2="282"/>
    <line x1="0" y1="290" x2="800" y2="290"/>
    <line x1="0" y1="298" x2="800" y2="298"/>
  </g>
  <g opacity="0.06" stroke="#101216" stroke-width="1.2">
    <line x1="44" y1="0" x2="0" y2="58"/>
    <line x1="224" y1="0" x2="24" y2="250"/>
    <line x1="420" y1="0" x2="138" y2="320"/>
    <line x1="610" y1="0" x2="332" y2="320"/>
    <line x1="792" y1="0" x2="520" y2="320"/>
  </g>
  <g opacity="0.08" stroke="#ffd24d" stroke-width="0.9">
    <line x1="84" y1="84" x2="170" y2="50"/>
    <line x1="492" y1="116" x2="640" y2="88"/>
    <line x1="594" y1="266" x2="742" y2="206"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

inline juce::Drawable* editorTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1200">
  <rect width="1600" height="1200" fill="#efe7d9"/>
  <g opacity="0.05" stroke="#1d2126" stroke-width="2">
    <line x1="0" y1="150" x2="1600" y2="150"/>
    <line x1="0" y1="430" x2="1600" y2="430"/>
    <line x1="0" y1="720" x2="1600" y2="720"/>
    <line x1="0" y1="1010" x2="1600" y2="1010"/>
  </g>
  <g opacity="0.05" stroke="#b9b0a1" stroke-width="1.2">
    <path d="M0 240 C230 170 390 150 602 198 S960 286 1188 214 S1440 154 1600 182"/>
    <path d="M0 612 C260 552 436 534 640 592 S982 686 1210 632 S1458 592 1600 632"/>
    <path d="M0 962 C218 914 426 870 660 926 S986 1002 1224 966 S1458 928 1600 956"/>
  </g>
  <g opacity="0.08" stroke="#ffd24d" stroke-width="1.1">
    <line x1="126" y1="0" x2="0" y2="202"/>
    <line x1="420" y1="0" x2="110" y2="420"/>
    <line x1="1040" y1="0" x2="662" y2="470"/>
    <line x1="1600" y1="184" x2="1208" y2="618"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

inline void drawCachedDrawable(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> area, float opacity = 1.0f)
{
    if (drawable == nullptr)
        return;

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(opacity);
    drawable->drawWithin(g, area, juce::RectanglePlacement::stretchToFit, 1.0f);
}

inline void drawCornerScrews(juce::Graphics& g, juce::Rectangle<float> bounds, float inset = 14.0f, float size = 15.0f)
{
    if (bounds.getWidth() < size * 3.0f || bounds.getHeight() < size * 3.0f)
        return;

    auto drawOne = [&](juce::Point<float> centre)
    {
        auto area = juce::Rectangle<float>(size, size).withCentre(centre);
        drawCachedDrawable(g, screwDrawable(), area, 0.88f);
    };

    drawOne({ bounds.getX() + inset, bounds.getY() + inset });
    drawOne({ bounds.getRight() - inset, bounds.getY() + inset });
    drawOne({ bounds.getX() + inset, bounds.getBottom() - inset });
    drawOne({ bounds.getRight() - inset, bounds.getBottom() - inset });
}

inline void drawSubtleGrid(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, int cols, int rows)
{
    g.setColour(colour);
    for (int i = 1; i < cols; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) cols;
        g.drawVerticalLine((int) x, area.getY(), area.getBottom());
    }

    for (int i = 1; i < rows; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / (float) rows;
        g.drawHorizontalLine((int) y, area.getX(), area.getRight());
    }
}

inline void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 18.0f;
    juce::ColourGradient bg(juce::Colour(0xfff2ebdf), bounds.getTopLeft(),
                            juce::Colour(0xffe6dcc9), bounds.getBottomLeft(), false);
    bg.addColour(0.30, juce::Colour(0xfffaf3e8));
    bg.addColour(0.78, juce::Colour(0xffe0d4bf));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);
    drawCachedDrawable(g, editorTextureDrawable(), bounds.reduced(1.0f), 0.48f);

    g.setColour(juce::Colour(0xff13171d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds.reduced(6.0f), radius - 5.0f, 1.0f);
}

inline void drawDisplayPanel(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    const float radius = 22.0f;
    juce::DropShadow(juce::Colours::black.withAlpha(0.18f), 22, { 0, 10 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient frame(juce::Colour(0xff1d2730), bounds.getTopLeft(),
                               juce::Colour(0xff0a1118), bounds.getBottomLeft(), false);
    g.setGradientFill(frame);
    g.fillRoundedRectangle(bounds, radius);

    auto inner = bounds.reduced(14.0f);
    juce::ColourGradient screen(displayBg().brighter(0.08f), inner.getTopLeft(),
                                displayBg().darker(0.10f), inner.getBottomLeft(), false);
    g.setGradientFill(screen);
    g.fillRoundedRectangle(inner, 16.0f);

    auto wash = inner;
    wash.removeFromBottom(wash.getHeight() * 0.42f);
    g.setColour(accent.withAlpha(0.08f));
    g.fillRoundedRectangle(wash, 16.0f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(inner.reduced(0.5f), 16.0f, 1.0f);
    g.setColour(outlineSoft().withAlpha(0.7f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

inline void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float radius = 8.0f)
{
    juce::DropShadow(juce::Colours::black.withAlpha(0.12f), 18, { 0, 8 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(),
                              panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.22, juce::Colour(0xfffffbf3));
    fill.addColour(0.84, juce::Colour(0xffece0cb));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    drawCachedDrawable(g, panelTextureDrawable(), bounds.reduced(1.0f), 0.18f);

    auto accentLine = bounds.reduced(24.0f, 14.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.92f), accentLine.getTopLeft(),
                              accent.withAlpha(0.06f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(outline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.06f));
    g.drawRoundedRectangle(bounds.reduced(4.0f), juce::jmax(1.0f, radius - 3.0f), 1.0f);
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff11161c));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, brandGold());
        setColour(juce::PopupMenu::highlightedTextColourId, textStrong());
        setColour(juce::PopupMenu::textColourId, textStrong());
        setColour(juce::ComboBox::backgroundColourId, backgroundSoft());
        setColour(juce::ComboBox::textColourId, textStrong());
        setColour(juce::ComboBox::outlineColourId, outline());
        setColour(juce::Label::textColourId, textStrong());
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return bodyFont(13.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
        juce::ColourGradient bg(juce::Colour(0xfffffaf0), bounds.getTopLeft(),
                                juce::Colour(0xffefe5d5), bounds.getBottomLeft(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(outline());
        g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
        g.setColour(box.hasKeyboardFocus(true) ? brandGold() : outline());
        g.drawRoundedRectangle(bounds.reduced(1.6f), 4.0f, 1.0f);

        auto arrowArea = bounds.removeFromRight(28.0f);
        juce::Path arrow;
        arrow.startNewSubPath(arrowArea.getCentreX() - 5.0f, arrowArea.getCentreY() - 2.0f);
        arrow.lineTo(arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
        arrow.lineTo(arrowArea.getCentreX() + 5.0f, arrowArea.getCentreY() - 2.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(10, 1, box.getWidth() - 38, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }
};
}
