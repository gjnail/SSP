#include "ReverbVectorUI.h"

namespace
{
reverbui::VectorLookAndFeel vectorLookAndFeel;

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
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* editorTextureDrawable()
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
juce::Colour gridColour() { return juce::Colour(0x2ef4efe4); }
juce::Colour graphFillColour() { return juce::Colour(0x10192330); }

juce::Font makeFont(float size, bool bold)
{
    return juce::Font(size, bold ? juce::Font::bold : juce::Font::plain);
}

class KnobValueEntry final : public juce::Component
{
public:
    explicit KnobValueEntry(reverbui::SSPKnob& knobToControl)
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

        auto safeEditor = juce::Component::SafePointer<juce::TextEditor>(&valueEditor);
        juce::MessageManager::callAsync([safeEditor]
        {
            if (safeEditor != nullptr)
            {
                safeEditor->grabKeyboardFocus();
                safeEditor->selectAll();
            }
        });
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

    juce::Component::SafePointer<reverbui::SSPKnob> knob;
    juce::Label nameLabel;
    juce::TextEditor valueEditor;
};
} // namespace

namespace reverbui
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
juce::Colour graphGrid() { return gridColour(); }
juce::Colour graphFill() { return graphFillColour(); }

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

        juce::ColourGradient tint(panelTopColour().withAlpha(0.34f), bounds.getTopLeft(),
                                  juce::Colour(0xff0c1219).withAlpha(0.10f), bounds.getBottomLeft(), false);
        g.setGradientFill(tint);
        g.fillRoundedRectangle(bounds, radius);
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
    g.setColour((on ? juce::Colour(0xfffff4d1) : outlineColour()).withAlpha(0.92f));
    g.drawRoundedRectangle(lens, 4.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.26f : 0.10f));
    g.fillRoundedRectangle(lens.removeFromTop(lens.getHeight() * 0.40f), 4.0f);
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

    auto arrowArea = bounds.removeFromRight(28.0f);
    juce::Path arrow;
    arrow.startNewSubPath(arrowArea.getCentreX() - 5.0f, arrowArea.getCentreY() - 2.0f);
    arrow.lineTo(arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    arrow.lineTo(arrowArea.getCentreX() + 5.0f, arrowArea.getCentreY() - 2.0f);
    g.setColour(strongTextColour());
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

void VectorLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    auto switchArea = bounds.removeFromRight(52.0f).reduced(2.0f, 4.0f);
    const bool on = button.getToggleState();
    auto tint = on ? cyanColour() : juce::Colour(0xff324556);

    if (shouldDrawButtonAsHighlighted)
        tint = tint.brighter(0.08f);

    juce::ColourGradient body(on ? tint.withAlpha(0.92f) : juce::Colour(0xff15202b), switchArea.getTopLeft(),
                              on ? tint.darker(0.30f) : juce::Colour(0xff0e151d), switchArea.getBottomLeft(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);

    g.setColour(on ? tint.brighter(0.5f) : outlineSoftColour());
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

    juce::ColourGradient fill(juce::Colour(0xff2b2b2b), 0.0f, 0.0f,
                              juce::Colour(0xff202020), 0.0f, (float) box.getHeight(), false);
    g.setGradientFill(fill);
    g.fillPath(path);

    g.setColour(juce::Colour(0xff484848));
    g.strokePath(path, juce::PathStrokeType(1.2f));
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
    callout.setDismissalMouseClicksAreAlwaysConsumed(true);
}

SSPButton::SSPButton(const juce::String& text)
    : juce::TextButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}

SSPDropdown::SSPDropdown()
{
    setLookAndFeel(&getVectorLookAndFeel());
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111822));
    setColour(juce::ComboBox::outlineColourId, outlineColour());
    setColour(juce::ComboBox::textColourId, strongTextColour());
    setColour(juce::ComboBox::arrowColourId, strongTextColour());
}

SSPToggle::SSPToggle(const juce::String& text)
    : juce::ToggleButton(text)
{
    setLookAndFeel(&getVectorLookAndFeel());
}
} // namespace reverbui
