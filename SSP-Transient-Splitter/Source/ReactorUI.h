#pragma once

#include <JuceHeader.h>
#include "ModulationConfig.h"

namespace reactorui
{
constexpr float headerFontScale = 1.04f;
constexpr float sectionFontScale = 1.08f;
constexpr float bodyFontScale = 1.34f;

enum class ThemeMode
{
    dark = 0,
    light
};

inline ThemeMode currentThemeMode = ThemeMode::dark;

inline void setThemeMode(ThemeMode mode) noexcept { currentThemeMode = mode; }
inline ThemeMode getThemeMode() noexcept { return currentThemeMode; }
inline bool isLightTheme() noexcept { return getThemeMode() == ThemeMode::light; }

inline juce::Colour background()      { return isLightTheme() ? juce::Colour(0xffede7db) : juce::Colour(0xff0b1118); }
inline juce::Colour backgroundSoft()  { return isLightTheme() ? juce::Colour(0xffefe8dc) : juce::Colour(0xff111922); }
inline juce::Colour panelTop()        { return isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff1c2530); }
inline juce::Colour panelBottom()     { return isLightTheme() ? juce::Colour(0xfffaf6ef) : juce::Colour(0xff151d26); }
inline juce::Colour outline()         { return isLightTheme() ? juce::Colour(0xff101216) : juce::Colour(0xff8b95a6); }
inline juce::Colour outlineSoft()     { return isLightTheme() ? juce::Colour(0xffc9beae) : juce::Colour(0xff31404f); }
inline juce::Colour textStrong()      { return isLightTheme() ? juce::Colour(0xff101216) : juce::Colour(0xffede5d8); }
inline juce::Colour textMuted()       { return isLightTheme() ? juce::Colour(0xff5a5d66) : juce::Colour(0xff98a2b3); }
inline juce::Colour displayBg()       { return juce::Colour(0xff0a1016); }
inline juce::Colour displayLine()     { return juce::Colour(0xff314657); }
inline juce::Colour brandGold()       { return isLightTheme() ? juce::Colour(0xffffd84d) : juce::Colour(0xffffcf4d); }
inline juce::Colour brandCyan()       { return isLightTheme() ? juce::Colour(0xff22c7ff) : juce::Colour(0xff3ecfff); }
inline juce::Colour brandEmber()      { return isLightTheme() ? juce::Colour(0xffff7448) : juce::Colour(0xffff8b61); }
inline juce::Colour controlSurface()  { return isLightTheme() ? juce::Colour(0xfff3ecdf) : juce::Colour(0xff111922); }
inline juce::Colour controlInset()    { return isLightTheme() ? juce::Colour(0xffece3d4) : juce::Colour(0xff0c1117); }

inline juce::Colour modulationSourceColour(int sourceIndex)
{
    static const std::array<juce::Colour, 8> lfoColours{{
        brandCyan(),
        brandGold(),
        juce::Colour(0xff8bff9c),
        brandEmber(),
        juce::Colour(0xffb7a2ff),
        juce::Colour(0xffffb66f),
        juce::Colour(0xff7ef7df),
        juce::Colour(0xff9cc4ff)
    }};

    static const std::array<juce::Colour, 6> macroColours{{
        juce::Colour(0xffff8b3d),
        juce::Colour(0xffffa347),
        juce::Colour(0xffff7a32),
        juce::Colour(0xffffb15b),
        juce::Colour(0xffff9151),
        juce::Colour(0xffffa86a)
    }};

    if (sourceIndex <= 0)
        return brandCyan();

    if (reactormod::isMacroSourceIndex(sourceIndex))
        return macroColours[(size_t) ((reactormod::macroNumberForSourceIndex(sourceIndex) - 1) % (int) macroColours.size())];

    return lfoColours[(size_t) ((reactormod::lfoNumberForSourceIndex(sourceIndex) - 1) % (int) lfoColours.size())];
}

inline juce::Font headerFont(float size)  { return juce::Font(size * headerFontScale, juce::Font::bold); }
inline juce::Font sectionFont(float size) { return juce::Font(size * sectionFontScale, juce::Font::bold); }
inline juce::Font bodyFont(float size)    { return juce::Font(size * bodyFontScale, juce::Font::plain); }

inline std::unique_ptr<juce::Drawable> createDrawableFromSvg(const char* svgText)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(svgText));
    return xml != nullptr ? juce::Drawable::createFromSVG(*xml) : nullptr;
}

inline float hardRadius(float requested = 8.0f)
{
    return juce::jlimit(4.0f, 8.0f, requested * 0.35f);
}

inline juce::Drawable* knobFaceDrawable()
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

inline juce::Drawable* screwDrawable()
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

inline juce::Drawable* panelTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 320">
  <defs>
    <linearGradient id="panel" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#202b36"/>
      <stop offset="52%" stop-color="#161e28"/>
      <stop offset="100%" stop-color="#101720"/>
    </linearGradient>
    <radialGradient id="glow" cx="0%" cy="0%" r="55%">
      <stop offset="0%" stop-color="#ffcf4d" stop-opacity="0.14"/>
      <stop offset="100%" stop-color="#ffcf4d" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <rect width="800" height="320" fill="url(#panel)"/>
  <rect width="800" height="320" fill="url(#glow)"/>
  <g opacity="0.09" stroke="#f4efe4" stroke-width="1">
    <line x1="0" y1="0" x2="800" y2="0"/>
    <line x1="0" y1="24" x2="800" y2="24"/>
    <line x1="0" y1="48" x2="800" y2="48"/>
    <line x1="0" y1="72" x2="800" y2="72"/>
    <line x1="0" y1="96" x2="800" y2="96"/>
    <line x1="0" y1="120" x2="800" y2="120"/>
    <line x1="0" y1="144" x2="800" y2="144"/>
    <line x1="0" y1="168" x2="800" y2="168"/>
    <line x1="0" y1="192" x2="800" y2="192"/>
    <line x1="0" y1="216" x2="800" y2="216"/>
    <line x1="0" y1="240" x2="800" y2="240"/>
    <line x1="0" y1="264" x2="800" y2="264"/>
    <line x1="0" y1="288" x2="800" y2="288"/>
    <line x1="0" y1="312" x2="800" y2="312"/>
  </g>
  <g opacity="0.08" stroke="#f4efe4" stroke-width="1">
    <line x1="0" y1="0" x2="0" y2="320"/>
    <line x1="24" y1="0" x2="24" y2="320"/>
    <line x1="48" y1="0" x2="48" y2="320"/>
    <line x1="72" y1="0" x2="72" y2="320"/>
    <line x1="96" y1="0" x2="96" y2="320"/>
    <line x1="120" y1="0" x2="120" y2="320"/>
    <line x1="144" y1="0" x2="144" y2="320"/>
    <line x1="168" y1="0" x2="168" y2="320"/>
    <line x1="192" y1="0" x2="192" y2="320"/>
    <line x1="216" y1="0" x2="216" y2="320"/>
    <line x1="240" y1="0" x2="240" y2="320"/>
    <line x1="264" y1="0" x2="264" y2="320"/>
    <line x1="288" y1="0" x2="288" y2="320"/>
    <line x1="312" y1="0" x2="312" y2="320"/>
    <line x1="336" y1="0" x2="336" y2="320"/>
    <line x1="360" y1="0" x2="360" y2="320"/>
    <line x1="384" y1="0" x2="384" y2="320"/>
    <line x1="408" y1="0" x2="408" y2="320"/>
    <line x1="432" y1="0" x2="432" y2="320"/>
    <line x1="456" y1="0" x2="456" y2="320"/>
    <line x1="480" y1="0" x2="480" y2="320"/>
    <line x1="504" y1="0" x2="504" y2="320"/>
    <line x1="528" y1="0" x2="528" y2="320"/>
    <line x1="552" y1="0" x2="552" y2="320"/>
    <line x1="576" y1="0" x2="576" y2="320"/>
    <line x1="600" y1="0" x2="600" y2="320"/>
    <line x1="624" y1="0" x2="624" y2="320"/>
    <line x1="648" y1="0" x2="648" y2="320"/>
    <line x1="672" y1="0" x2="672" y2="320"/>
    <line x1="696" y1="0" x2="696" y2="320"/>
    <line x1="720" y1="0" x2="720" y2="320"/>
    <line x1="744" y1="0" x2="744" y2="320"/>
    <line x1="768" y1="0" x2="768" y2="320"/>
    <line x1="792" y1="0" x2="792" y2="320"/>
  </g>
  <g opacity="0.10">
    <rect x="0" y="0" width="800" height="54" fill="#ffffff" fill-opacity="0.03"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

inline juce::Drawable* editorTextureDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1200">
  <defs>
    <radialGradient id="goldGlow" cx="0%" cy="0%" r="48%">
      <stop offset="0%" stop-color="#ffcf4d" stop-opacity="0.18"/>
      <stop offset="100%" stop-color="#ffcf4d" stop-opacity="0"/>
    </radialGradient>
    <radialGradient id="cyanGlow" cx="100%" cy="100%" r="52%">
      <stop offset="0%" stop-color="#3ecfff" stop-opacity="0.10"/>
      <stop offset="100%" stop-color="#3ecfff" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <rect width="1600" height="1200" fill="#0b1118"/>
  <rect width="1600" height="1200" fill="url(#goldGlow)"/>
  <rect width="1600" height="1200" fill="url(#cyanGlow)"/>
  <g opacity="0.08" stroke="#f4efe4" stroke-width="1.2">
    <line x1="0" y1="0" x2="1600" y2="0"/>
    <line x1="0" y1="24" x2="1600" y2="24"/>
    <line x1="0" y1="48" x2="1600" y2="48"/>
    <line x1="0" y1="72" x2="1600" y2="72"/>
    <line x1="0" y1="96" x2="1600" y2="96"/>
    <line x1="0" y1="120" x2="1600" y2="120"/>
    <line x1="0" y1="144" x2="1600" y2="144"/>
    <line x1="0" y1="168" x2="1600" y2="168"/>
    <line x1="0" y1="192" x2="1600" y2="192"/>
    <line x1="0" y1="216" x2="1600" y2="216"/>
    <line x1="0" y1="240" x2="1600" y2="240"/>
    <line x1="0" y1="264" x2="1600" y2="264"/>
    <line x1="0" y1="288" x2="1600" y2="288"/>
    <line x1="0" y1="312" x2="1600" y2="312"/>
    <line x1="0" y1="336" x2="1600" y2="336"/>
    <line x1="0" y1="360" x2="1600" y2="360"/>
    <line x1="0" y1="384" x2="1600" y2="384"/>
    <line x1="0" y1="408" x2="1600" y2="408"/>
    <line x1="0" y1="432" x2="1600" y2="432"/>
    <line x1="0" y1="456" x2="1600" y2="456"/>
    <line x1="0" y1="480" x2="1600" y2="480"/>
    <line x1="0" y1="504" x2="1600" y2="504"/>
    <line x1="0" y1="528" x2="1600" y2="528"/>
    <line x1="0" y1="552" x2="1600" y2="552"/>
    <line x1="0" y1="576" x2="1600" y2="576"/>
    <line x1="0" y1="600" x2="1600" y2="600"/>
    <line x1="0" y1="624" x2="1600" y2="624"/>
    <line x1="0" y1="648" x2="1600" y2="648"/>
    <line x1="0" y1="672" x2="1600" y2="672"/>
    <line x1="0" y1="696" x2="1600" y2="696"/>
    <line x1="0" y1="720" x2="1600" y2="720"/>
    <line x1="0" y1="744" x2="1600" y2="744"/>
    <line x1="0" y1="768" x2="1600" y2="768"/>
    <line x1="0" y1="792" x2="1600" y2="792"/>
    <line x1="0" y1="816" x2="1600" y2="816"/>
    <line x1="0" y1="840" x2="1600" y2="840"/>
    <line x1="0" y1="864" x2="1600" y2="864"/>
    <line x1="0" y1="888" x2="1600" y2="888"/>
    <line x1="0" y1="912" x2="1600" y2="912"/>
    <line x1="0" y1="936" x2="1600" y2="936"/>
    <line x1="0" y1="960" x2="1600" y2="960"/>
    <line x1="0" y1="984" x2="1600" y2="984"/>
    <line x1="0" y1="1008" x2="1600" y2="1008"/>
    <line x1="0" y1="1032" x2="1600" y2="1032"/>
    <line x1="0" y1="1056" x2="1600" y2="1056"/>
    <line x1="0" y1="1080" x2="1600" y2="1080"/>
    <line x1="0" y1="1104" x2="1600" y2="1104"/>
    <line x1="0" y1="1128" x2="1600" y2="1128"/>
    <line x1="0" y1="1152" x2="1600" y2="1152"/>
    <line x1="0" y1="1176" x2="1600" y2="1176"/>
    <line x1="0" y1="1200" x2="1600" y2="1200"/>
  </g>
  <g opacity="0.08" stroke="#f4efe4" stroke-width="1.2">
    <line x1="0" y1="0" x2="0" y2="1200"/>
    <line x1="24" y1="0" x2="24" y2="1200"/>
    <line x1="48" y1="0" x2="48" y2="1200"/>
    <line x1="72" y1="0" x2="72" y2="1200"/>
    <line x1="96" y1="0" x2="96" y2="1200"/>
    <line x1="120" y1="0" x2="120" y2="1200"/>
    <line x1="144" y1="0" x2="144" y2="1200"/>
    <line x1="168" y1="0" x2="168" y2="1200"/>
    <line x1="192" y1="0" x2="192" y2="1200"/>
    <line x1="216" y1="0" x2="216" y2="1200"/>
    <line x1="240" y1="0" x2="240" y2="1200"/>
    <line x1="264" y1="0" x2="264" y2="1200"/>
    <line x1="288" y1="0" x2="288" y2="1200"/>
    <line x1="312" y1="0" x2="312" y2="1200"/>
    <line x1="336" y1="0" x2="336" y2="1200"/>
    <line x1="360" y1="0" x2="360" y2="1200"/>
    <line x1="384" y1="0" x2="384" y2="1200"/>
    <line x1="408" y1="0" x2="408" y2="1200"/>
    <line x1="432" y1="0" x2="432" y2="1200"/>
    <line x1="456" y1="0" x2="456" y2="1200"/>
    <line x1="480" y1="0" x2="480" y2="1200"/>
    <line x1="504" y1="0" x2="504" y2="1200"/>
    <line x1="528" y1="0" x2="528" y2="1200"/>
    <line x1="552" y1="0" x2="552" y2="1200"/>
    <line x1="576" y1="0" x2="576" y2="1200"/>
    <line x1="600" y1="0" x2="600" y2="1200"/>
    <line x1="624" y1="0" x2="624" y2="1200"/>
    <line x1="648" y1="0" x2="648" y2="1200"/>
    <line x1="672" y1="0" x2="672" y2="1200"/>
    <line x1="696" y1="0" x2="696" y2="1200"/>
    <line x1="720" y1="0" x2="720" y2="1200"/>
    <line x1="744" y1="0" x2="744" y2="1200"/>
    <line x1="768" y1="0" x2="768" y2="1200"/>
    <line x1="792" y1="0" x2="792" y2="1200"/>
    <line x1="816" y1="0" x2="816" y2="1200"/>
    <line x1="840" y1="0" x2="840" y2="1200"/>
    <line x1="864" y1="0" x2="864" y2="1200"/>
    <line x1="888" y1="0" x2="888" y2="1200"/>
    <line x1="912" y1="0" x2="912" y2="1200"/>
    <line x1="936" y1="0" x2="936" y2="1200"/>
    <line x1="960" y1="0" x2="960" y2="1200"/>
    <line x1="984" y1="0" x2="984" y2="1200"/>
    <line x1="1008" y1="0" x2="1008" y2="1200"/>
    <line x1="1032" y1="0" x2="1032" y2="1200"/>
    <line x1="1056" y1="0" x2="1056" y2="1200"/>
    <line x1="1080" y1="0" x2="1080" y2="1200"/>
    <line x1="1104" y1="0" x2="1104" y2="1200"/>
    <line x1="1128" y1="0" x2="1128" y2="1200"/>
    <line x1="1152" y1="0" x2="1152" y2="1200"/>
    <line x1="1176" y1="0" x2="1176" y2="1200"/>
    <line x1="1200" y1="0" x2="1200" y2="1200"/>
    <line x1="1224" y1="0" x2="1224" y2="1200"/>
    <line x1="1248" y1="0" x2="1248" y2="1200"/>
    <line x1="1272" y1="0" x2="1272" y2="1200"/>
    <line x1="1296" y1="0" x2="1296" y2="1200"/>
    <line x1="1320" y1="0" x2="1320" y2="1200"/>
    <line x1="1344" y1="0" x2="1344" y2="1200"/>
    <line x1="1368" y1="0" x2="1368" y2="1200"/>
    <line x1="1392" y1="0" x2="1392" y2="1200"/>
    <line x1="1416" y1="0" x2="1416" y2="1200"/>
    <line x1="1440" y1="0" x2="1440" y2="1200"/>
    <line x1="1464" y1="0" x2="1464" y2="1200"/>
    <line x1="1488" y1="0" x2="1488" y2="1200"/>
    <line x1="1512" y1="0" x2="1512" y2="1200"/>
    <line x1="1536" y1="0" x2="1536" y2="1200"/>
    <line x1="1560" y1="0" x2="1560" y2="1200"/>
    <line x1="1584" y1="0" x2="1584" y2="1200"/>
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

inline void drawCornerScrew(juce::Graphics& g, juce::Point<float> centre, float size, float opacity = 1.0f)
{
    auto area = juce::Rectangle<float>(size, size).withCentre(centre);
    drawCachedDrawable(g, screwDrawable(), area, opacity);
}

inline void drawCornerScrews(juce::Graphics& g, juce::Rectangle<float> bounds, float inset = 14.0f, float size = 15.0f)
{
    if (bounds.getWidth() < size * 3.0f || bounds.getHeight() < size * 3.0f)
        return;

    drawCornerScrew(g, { bounds.getX() + inset, bounds.getY() + inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getRight() - inset, bounds.getY() + inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getX() + inset, bounds.getBottom() - inset }, size, 0.85f);
    drawCornerScrew(g, { bounds.getRight() - inset, bounds.getBottom() - inset }, size, 0.85f);
}

inline void styleTitle(juce::Label& label, float size = 15.0f)
{
    label.setFont(sectionFont(size));
    label.setColour(juce::Label::textColourId, textStrong());
    label.setJustificationType(juce::Justification::centredLeft);
}

inline void styleMeta(juce::Label& label, float size = 11.5f, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(bodyFont(size));
    label.setColour(juce::Label::textColourId, textMuted());
    label.setJustificationType(just);
}

inline void styleKnobSlider(juce::Slider& slider, juce::Colour accent, int textBoxWidth = 68, int textBoxHeight = 22)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.35f));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, outline());
    slider.setColour(juce::Slider::textBoxTextColourId, textStrong());
    slider.setColour(juce::Slider::textBoxBackgroundColourId, controlSurface());
    slider.setColour(juce::Slider::textBoxOutlineColourId, outline());
}

inline void drawSubtleGrid(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, int cols, int rows)
{
    auto gridColour = colour;
    if (isLightTheme())
        gridColour = gridColour.withAlpha(juce::jlimit(0.12f, 0.26f, colour.getFloatAlpha() * 1.85f));

    g.setColour(gridColour);
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

inline void drawThemeGrid(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float cellSize, float thickness = 1.0f)
{
    g.setColour(colour);

    for (float x = area.getX(); x <= area.getRight(); x += cellSize)
        g.fillRect(juce::Rectangle<float>(x, area.getY(), thickness, area.getHeight()));

    for (float y = area.getY(); y <= area.getBottom(); y += cellSize)
        g.fillRect(juce::Rectangle<float>(area.getX(), y, area.getWidth(), thickness));
}

inline void drawEditorBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float radius = 10.0f;
    juce::ColourGradient bg(isLightTheme() ? juce::Colour(0xfff0eadf) : juce::Colour(0xff0e151d), bounds.getTopLeft(),
                            isLightTheme() ? juce::Colour(0xffefe8dc) : juce::Colour(0xff111922), bounds.getBottomLeft(), false);
    bg.addColour(0.18, isLightTheme() ? juce::Colour(0xffe8e0d2) : juce::Colour(0xff111922));
    bg.addColour(0.72, isLightTheme() ? juce::Colour(0xffefe8dc) : juce::Colour(0xff0a1017));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(brandGold().withAlpha(0.09f));
    g.fillEllipse(bounds.getX() - bounds.getWidth() * 0.16f,
                  bounds.getY() - bounds.getWidth() * 0.16f,
                  bounds.getWidth() * 0.42f,
                  bounds.getWidth() * 0.42f);

    g.setColour(brandCyan().withAlpha(isLightTheme() ? 0.05f : 0.07f));
    g.fillEllipse(bounds.getRight() - bounds.getWidth() * 0.20f,
                  bounds.getBottom() - bounds.getWidth() * 0.20f,
                  bounds.getWidth() * 0.34f,
                  bounds.getWidth() * 0.34f);

    g.setColour(isLightTheme() ? juce::Colour(0x55101216) : juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(isLightTheme() ? juce::Colours::white.withAlpha(0.50f) : juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), radius - 1.0f, 1.0f);
    drawCornerScrews(g, bounds, 18.0f, 16.0f);
}

inline void drawDisplayPanel(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    const float radius = 6.0f;
    juce::DropShadow((isLightTheme() ? juce::Colour(0xff6b6257) : juce::Colours::black).withAlpha(isLightTheme() ? 0.16f : 0.36f),
                     14, { 0, 6 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient frame(isLightTheme() ? juce::Colour(0xfff2ebdd) : juce::Colour(0xff1f2933), bounds.getTopLeft(),
                               isLightTheme() ? juce::Colour(0xffded4c4) : juce::Colour(0xff10161d), bounds.getBottomLeft(), false);
    g.setGradientFill(frame);
    g.fillRoundedRectangle(bounds, radius);
    auto inner = bounds.reduced(10.0f, 8.0f);
    juce::ColourGradient screen(juce::Colour(0xff13202a), inner.getTopLeft(),
                                juce::Colour(0xff091017), inner.getBottomLeft(), false);
    g.setGradientFill(screen);
    g.fillRoundedRectangle(inner, 5.0f);
    g.setColour(accent.withAlpha(0.08f));
    g.fillRoundedRectangle(inner.removeFromTop(inner.getHeight() * 0.36f), 5.0f);

    auto lineInner = bounds.reduced(11.0f, 9.0f);
    g.setColour(displayLine().withAlpha(0.28f));
    g.drawRoundedRectangle(lineInner, 5.0f, 1.0f);

    g.setColour(outlineSoft());
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    g.setColour(isLightTheme() ? juce::Colours::white.withAlpha(0.44f) : juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), radius - 1.0f, 1.0f);
}

inline void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float requestedRadius = 18.0f)
{
    const float radius = hardRadius(requestedRadius);
    juce::DropShadow((isLightTheme() ? juce::Colour(0xff6b6257) : juce::Colours::black).withAlpha(isLightTheme() ? 0.14f : 0.3f),
                     18, { 0, 8 }).drawForRectangle(g, bounds.getSmallestIntegerContainer());

    juce::ColourGradient fill(panelTop(), bounds.getTopLeft(),
                              panelBottom(), bounds.getBottomLeft(), false);
    fill.addColour(0.16, panelTop().brighter(0.03f));
    fill.addColour(0.84, panelBottom().darker(0.03f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, radius);

    auto inner = bounds.reduced(1.0f);

    auto headerBand = inner.removeFromTop(34.0f);
    juce::ColourGradient headerFill((isLightTheme() ? juce::Colour(0x12ffffff) : juce::Colour(0x20121922)), headerBand.getTopLeft(),
                                    juce::Colours::transparentBlack, headerBand.getBottomLeft(), false);
    headerFill.addColour(0.0, accent.withAlpha(isLightTheme() ? 0.09f : 0.12f));
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(headerBand, radius);

    auto accentLine = bounds.reduced(18.0f, 10.0f).removeFromTop(3.0f);
    juce::ColourGradient line(accent.withAlpha(0.96f), accentLine.getTopLeft(),
                              accent.withAlpha(0.12f), { accentLine.getRight(), accentLine.getY() }, false);
    g.setGradientFill(line);
    g.fillRect(accentLine);

    g.setColour(isLightTheme() ? juce::Colour(0x55101216) : juce::Colour(0xff06090d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 2.0f);
    g.setColour(outline());
    g.drawRoundedRectangle(bounds.reduced(1.8f), radius - 0.5f, 1.2f);
    g.setColour(isLightTheme() ? juce::Colours::white.withAlpha(0.46f) : juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), juce::jmax(1.0f, radius - 1.0f), 1.0f);

    drawCornerScrews(g, bounds, 13.0f, 13.0f);
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        refreshPalette();
    }

    void refreshPalette()
    {
        setColour(juce::PopupMenu::backgroundColourId, controlSurface());
        setColour(juce::PopupMenu::highlightedBackgroundColourId, isLightTheme() ? juce::Colour(0xffeadfcf) : juce::Colour(0xff22303d));
        setColour(juce::PopupMenu::highlightedTextColourId, textStrong());
        setColour(juce::PopupMenu::textColourId, textStrong());
        setColour(juce::ComboBox::backgroundColourId, controlSurface());
        setColour(juce::ComboBox::textColourId, textStrong());
        setColour(juce::ComboBox::outlineColourId, outline());
        setColour(juce::ComboBox::arrowColourId, textStrong());
        setColour(juce::Label::textColourId, textStrong());
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return bodyFont(13.0f);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(bodyFont(11.5f));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::backgroundColourId, controlSurface());
        label->setColour(juce::Label::outlineColourId, outline());
        label->setColour(juce::Label::textColourId, textStrong());
        return label;
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout(slider);

        if (slider.getSliderStyle() != juce::Slider::RotaryHorizontalVerticalDrag
            && slider.getSliderStyle() != juce::Slider::RotaryVerticalDrag
            && slider.getSliderStyle() != juce::Slider::Rotary)
            return layout;

        auto bounds = slider.getLocalBounds().reduced(2);
        const int textBoxHeight = juce::jmax(0, slider.getTextBoxHeight());
        const int gap = 0;

        layout.textBoxBounds = bounds.removeFromBottom(textBoxHeight);
        bounds.removeFromBottom(gap);
        layout.sliderBounds = bounds;
        return layout;
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
        juce::ColourGradient bg(isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff18212b), bounds.getTopLeft(),
                                isLightTheme() ? juce::Colour(0xffeee4d6) : juce::Colour(0xff111922), bounds.getBottomLeft(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(isLightTheme() ? juce::Colour(0x55101216) : juce::Colour(0xff0a0e13));
        g.drawRoundedRectangle(bounds, 5.0f, 1.6f);
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

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool on = button.getToggleState();
        auto bezel = outline();
        auto lensTop = on ? brandGold() : (isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff1b2430));
        auto lensBottom = on ? (isLightTheme() ? juce::Colour(0xffc9a92b) : juce::Colour(0xffc69511))
                             : (isLightTheme() ? juce::Colour(0xffeee3d3) : juce::Colour(0xff111922));

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

        g.setColour(isLightTheme() ? juce::Colour(0x40101216) : juce::Colour(0xff0a0e13));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(bezel);
        g.drawRoundedRectangle(bounds, 5.0f, 1.4f);

        auto lens = bounds.reduced(4.0f, 3.0f);
        juce::ColourGradient fill(lensTop, lens.getTopLeft(), lensBottom, lens.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(lens, 4.0f);
        g.setColour((on ? (isLightTheme() ? juce::Colour(0xfffff8e2) : juce::Colour(0xfffff4d1)) : outline()).withAlpha(0.92f));
        g.drawRoundedRectangle(lens, 4.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(on ? (isLightTheme() ? 0.42f : 0.26f) : (isLightTheme() ? 0.28f : 0.10f)));
        g.fillRoundedRectangle(lens.removeFromTop(lens.getHeight() * 0.40f), 4.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setFont(sectionFont(12.0f));
        g.setColour(button.getToggleState() ? juce::Colour(0xff0b0d10) : textStrong());
        g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }

    void drawTickBox(juce::Graphics& g, juce::Component&, float x, float y, float w, float h,
                     bool ticked, bool, bool enabled, bool over) override
    {
        auto box = juce::Rectangle<float>(x, y, w, h).reduced(1.0f);
        const float radius = 4.0f;

        g.setColour(controlInset());
        g.fillRoundedRectangle(box, radius);
        g.setColour(outline());
        g.drawRoundedRectangle(box, radius, 1.1f);

        auto lens = box.reduced(3.0f);
        auto top = ticked ? brandGold() : (isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff18212b));
        auto bottom = ticked ? (isLightTheme() ? juce::Colour(0xffc9a92b) : juce::Colour(0xffc69511))
                             : (isLightTheme() ? juce::Colour(0xffeee3d3) : juce::Colour(0xff111922));
        if (over)
        {
            top = top.brighter(0.07f);
            bottom = bottom.brighter(0.03f);
        }

        juce::ColourGradient fill(top, lens.getTopLeft(), bottom, lens.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(lens, 3.0f);
        g.setColour((ticked ? juce::Colour(0xfffff3cc) : outline()).withAlpha(enabled ? 0.95f : 0.4f));
        g.drawRoundedRectangle(lens, 3.0f, 1.0f);

        if (ticked)
        {
            juce::Path tick;
            tick.startNewSubPath(lens.getX() + lens.getWidth() * 0.22f, lens.getCentreY());
            tick.lineTo(lens.getX() + lens.getWidth() * 0.43f, lens.getBottom() - lens.getHeight() * 0.24f);
            tick.lineTo(lens.getRight() - lens.getWidth() * 0.18f, lens.getY() + lens.getHeight() * 0.22f);
            g.setColour(enabled ? juce::Colour(0xff0b0d10) : textMuted());
            g.strokePath(tick, juce::PathStrokeType(1.65f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down) override
    {
        auto bounds = button.getLocalBounds();
        const float tickSize = 22.0f;
        drawTickBox(g, button, 0.0f, (float) ((bounds.getHeight() - (int) tickSize) / 2), tickSize, tickSize,
                    button.getToggleState(), false, button.isEnabled(), over || down);
        g.setColour(button.isEnabled() ? textStrong() : textMuted());
        g.setFont(bodyFont(13.0f));
        g.drawFittedText(button.getButtonText(), bounds.withTrimmedLeft(30), juce::Justification::centredLeft, 1);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
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
        g.setColour(isLightTheme() ? juce::Colour(0xffd7cdbd) : juce::Colour(0xff121922));
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
};
}
