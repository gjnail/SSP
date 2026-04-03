#include "MixerControlsComponent.h"

namespace
{
std::unique_ptr<juce::Drawable> createDrawableFromSvg(const char* svgText)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(svgText));
    return xml != nullptr ? juce::Drawable::createFromSVG(*xml) : nullptr;
}

void drawDrawable(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> bounds, float opacity = 1.0f)
{
    if (drawable == nullptr)
        return;

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(opacity);
    drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
}

juce::Drawable* swapIconDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <g fill="none" stroke="#ecf2fb" stroke-linecap="round" stroke-linejoin="round">
    <path d="M16 30h42" stroke-width="7"/>
    <path d="m48 18 18 12-18 12" stroke-width="7"/>
    <path d="M80 66H38" stroke-width="7"/>
    <path d="m48 54-18 12 18 12" stroke-width="7"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* monoIconDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <g fill="none" stroke="#ecf2fb" stroke-linecap="round" stroke-linejoin="round" stroke-width="7">
    <path d="M22 22v50"/>
    <path d="M74 22v50"/>
    <path d="M22 47h52"/>
    <path d="m40 65 8 9 8-9"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* invertLeftIconDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <g fill="none" stroke="#ecf2fb" stroke-linecap="round" stroke-linejoin="round" stroke-width="7">
    <circle cx="34" cy="48" r="18"/>
    <path d="M20 48h28"/>
    <path d="M62 32v32"/>
    <path d="M54 40h16"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* invertRightIconDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <g fill="none" stroke="#ecf2fb" stroke-linecap="round" stroke-linejoin="round" stroke-width="7">
    <circle cx="62" cy="48" r="18"/>
    <path d="M48 48h28"/>
    <path d="M34 32v32"/>
    <path d="M26 56h16"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

juce::Drawable* autoCompIconDrawable()
{
    static auto drawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <g fill="none" stroke="#ecf2fb" stroke-linecap="round" stroke-linejoin="round" stroke-width="7">
    <path d="M24 62V32"/>
    <path d="M48 72V24"/>
    <path d="M72 54V40"/>
    <path d="M18 62h12"/>
    <path d="M42 72h12"/>
    <path d="M66 54h12"/>
  </g>
</svg>
)svg");
    return drawable.get();
}

juce::String formatPercent01(double value)
{
    return juce::String(juce::roundToInt((float) value * 100.0f)) + "%";
}

juce::String formatSignedPercent(double value)
{
    const int rounded = juce::roundToInt((float) value * 100.0f);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatDb(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}
} // namespace

class MixerControlsComponent::ChannelKnob final : public juce::Component
{
public:
    enum class Mode
    {
        direct,
        cross,
        decibels
    };

    ChannelKnob(juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                const juce::String& labelText,
                Mode knobMode)
        : attachment(state, parameterID, slider),
          label(labelText),
          mode(knobMode)
    {
        addAndMakeVisible(slider);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff6da8ff));
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        if (auto* parameter = state.getParameter(parameterID))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(mode == Mode::cross ? 36 : 32);
        auto valueArea = footer.removeFromTop(16);
        auto labelArea = footer.removeFromBottom(16);

        g.setColour(mode == Mode::cross ? juce::Colour(0xff9fd7ff) : mixerui::textStrong());
        g.setFont(mixerui::smallCapsFont(11.0f));
        g.drawText(getReadout(), valueArea, juce::Justification::centred, false);

        g.setColour(mixerui::textMuted());
        g.setFont(mixerui::smallCapsFont(10.5f));
        g.drawText(label, labelArea, juce::Justification::centred, false);

        if (mode == Mode::cross)
        {
            auto ticks = footer.removeFromBottom(12);
            g.setFont(mixerui::bodyFont(7.8f));
            g.drawText("-100", ticks.removeFromLeft(32), juce::Justification::left, false);
            g.drawText("0", ticks.removeFromLeft(juce::jmax(0, ticks.getWidth() - 32)).withX(ticks.getX()), juce::Justification::centred, false);
            g.drawText("+100", ticks, juce::Justification::right, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromBottom(mode == Mode::cross ? 36 : 32);
        slider.setBounds(area.reduced(6, 0));
    }

private:
    juce::String getReadout() const
    {
        switch (mode)
        {
            case Mode::direct: return formatPercent01(slider.getValue());
            case Mode::cross: return formatSignedPercent(slider.getValue());
            case Mode::decibels: return formatDb(slider.getValue());
        }

        return {};
    }

    mixerui::SSPKnob slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::String label;
    Mode mode;
};

class MixerControlsComponent::SvgToggleButton final : public juce::Button
{
public:
    SvgToggleButton(const juce::String& text, juce::Drawable* iconDrawable, juce::Colour accentColour)
        : juce::Button(text), icon(iconDrawable), accent(accentColour)
    {
        setName(text);
        setClickingTogglesState(true);
        setTriggeredOnMouseDown(false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const bool on = getToggleState();
        auto fill = on ? accent.withAlpha(0.22f) : juce::Colour(0xff151d26);
        auto border = on ? accent.withAlpha(0.92f) : mixerui::outlineSoft();

        if (isMouseOverButton)
            fill = fill.brighter(0.08f);
        if (isButtonDown)
            fill = fill.darker(0.08f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(border);
        g.drawRoundedRectangle(bounds, 10.0f, on ? 1.5f : 1.0f);

        auto iconArea = bounds.removeFromLeft(34.0f).reduced(6.0f, 6.0f);
        drawDrawable(g, icon, iconArea, on ? 1.0f : 0.78f);

        g.setColour(on ? mixerui::textStrong() : mixerui::textMuted());
        g.setFont(mixerui::smallCapsFont(10.5f));
        g.drawText(getName(), bounds.toNearestInt(), juce::Justification::centred, false);
    }

private:
    juce::Drawable* icon = nullptr;
    juce::Colour accent;
};

class MixerControlsComponent::MiniActionButton final : public juce::TextButton
{
public:
    explicit MiniActionButton(const juce::String& text)
        : juce::TextButton(text)
    {
        setLookAndFeel(&mixerui::getVectorLookAndFeel());
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        auto fill = juce::Colour(0xff18212b);
        auto border = mixerui::outlineSoft();

        if (isMouseOverButton)
            fill = fill.brighter(0.08f);
        if (isButtonDown)
            fill = fill.darker(0.08f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 9.0f);
        g.setColour(border);
        g.drawRoundedRectangle(bounds, 9.0f, 1.0f);
        g.setColour(mixerui::textMuted());
        g.setFont(mixerui::smallCapsFont(10.0f));
        g.drawText(getButtonText(), bounds.toNearestInt(), juce::Justification::centred, false);
    }
};

class MixerControlsComponent::RouterDisplay final : public juce::Component
{
public:
    explicit RouterDisplay(juce::AudioProcessorValueTreeState& state,
                           std::array<float, 4>& displayedRouteValues,
                           float& phaseReference)
        : apvts(state),
          displayedRoutes(displayedRouteValues),
          animationPhase(phaseReference)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(20.0f, 18.0f);
        auto plot = area.reduced(10.0f, 6.0f);

        g.setColour(juce::Colour(0xff0c1219));
        g.fillRoundedRectangle(plot, 14.0f);
        g.setColour(mixerui::outlineSoft().withAlpha(0.9f));
        g.drawRoundedRectangle(plot.reduced(0.5f), 14.0f, 1.0f);

        const auto leftIn = juce::Point<float>(plot.getX() + 56.0f, plot.getY() + plot.getHeight() * 0.30f);
        const auto rightIn = juce::Point<float>(plot.getX() + 56.0f, plot.getY() + plot.getHeight() * 0.70f);
        const auto leftOut = juce::Point<float>(plot.getRight() - 56.0f, plot.getY() + plot.getHeight() * 0.30f);
        const auto rightOut = juce::Point<float>(plot.getRight() - 56.0f, plot.getY() + plot.getHeight() * 0.70f);

        g.setColour(mixerui::textMuted());
        g.setFont(mixerui::smallCapsFont(11.0f));
        g.drawText("Input L", juce::Rectangle<float>(leftIn.x - 24.0f, leftIn.y - 34.0f, 64.0f, 16.0f).toNearestInt(), juce::Justification::centred, false);
        g.drawText("Input R", juce::Rectangle<float>(rightIn.x - 24.0f, rightIn.y + 18.0f, 64.0f, 16.0f).toNearestInt(), juce::Justification::centred, false);
        g.drawText("Out L", juce::Rectangle<float>(leftOut.x - 30.0f, leftOut.y - 34.0f, 60.0f, 16.0f).toNearestInt(), juce::Justification::centred, false);
        g.drawText("Out R", juce::Rectangle<float>(rightOut.x - 30.0f, rightOut.y + 18.0f, 60.0f, 16.0f).toNearestInt(), juce::Justification::centred, false);

        drawRoute(g, leftIn, leftOut, displayedRoutes[0], mixerui::brandCyan(), false);
        drawRoute(g, rightIn, leftOut, displayedRoutes[1], displayedRoutes[1] >= 0.0f ? mixerui::brandGold() : juce::Colour(0xfff07f7f), true);
        drawRoute(g, leftIn, rightOut, displayedRoutes[2], displayedRoutes[2] >= 0.0f ? mixerui::brandMint() : juce::Colour(0xfff07f7f), true);
        drawRoute(g, rightIn, rightOut, displayedRoutes[3], mixerui::brandCyan().brighter(0.12f), false);

        drawNode(g, leftIn, juce::Colour(0xff6da8ff));
        drawNode(g, rightIn, juce::Colour(0xff6da8ff));
        drawNode(g, leftOut, juce::Colour(0xff6da8ff));
        drawNode(g, rightOut, juce::Colour(0xff6da8ff));

        auto infoRow = juce::Rectangle<float>(plot.getX() + 12.0f, plot.getBottom() - 32.0f, plot.getWidth() - 24.0f, 18.0f);
        g.setColour(mixerui::textMuted());
        g.setFont(mixerui::smallCapsFont(10.0f));
        g.drawText("Cross routes start at 0 in the center. Negative values flip the contribution.", infoRow.toNearestInt(), juce::Justification::centred, false);

        juce::ignoreUnused(apvts);
    }

private:
    void drawRoute(juce::Graphics& g,
                   juce::Point<float> start,
                   juce::Point<float> end,
                   float amount,
                   juce::Colour colour,
                   bool crossRoute) const
    {
        juce::Path route;
        const float dx = (end.x - start.x) * 0.42f;
        route.startNewSubPath(start);
        route.cubicTo(start.x + dx, start.y, end.x - dx, end.y, end.x, end.y);

        const float magnitude = std::abs(amount);
        g.setColour(colour.withAlpha(0.12f + magnitude * 0.52f));
        g.strokePath(route, juce::PathStrokeType(1.5f + magnitude * 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (magnitude > 0.01f)
        {
            for (int pulse = 0; pulse < 2; ++pulse)
            {
                float progress = std::fmod(animationPhase + pulse * 0.5f + (crossRoute ? 0.17f : 0.0f), 1.0f);
                if (amount < 0.0f)
                    progress = 1.0f - progress;

                const auto point = route.getPointAlongPath(progress * route.getLength());
                g.setColour(colour.withAlpha(0.42f + magnitude * 0.50f));
                g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(point));
            }
        }
    }

    static void drawNode(juce::Graphics& g, juce::Point<float> centre, juce::Colour colour)
    {
        g.setColour(juce::Colour(0xff111922));
        g.fillEllipse(juce::Rectangle<float>(16.0f, 16.0f).withCentre(centre));
        g.setColour(colour);
        g.drawEllipse(juce::Rectangle<float>(16.0f, 16.0f).withCentre(centre), 1.4f);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::array<float, 4>& displayedRoutes;
    float& animationPhase;
};

MixerControlsComponent::MixerControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    leftFromLeftKnob = std::make_unique<ChannelKnob>(state, "leftFromLeft", "L <- L", ChannelKnob::Mode::direct);
    leftFromRightKnob = std::make_unique<ChannelKnob>(state, "leftFromRight", "L <- R", ChannelKnob::Mode::cross);
    rightFromLeftKnob = std::make_unique<ChannelKnob>(state, "rightFromLeft", "R <- L", ChannelKnob::Mode::cross);
    rightFromRightKnob = std::make_unique<ChannelKnob>(state, "rightFromRight", "R <- R", ChannelKnob::Mode::direct);
    outputTrimKnob = std::make_unique<ChannelKnob>(state, "outputTrimDb", "Output Trim", ChannelKnob::Mode::decibels);
    routerDisplay = std::make_unique<RouterDisplay>(state, displayedRoutes, animationPhase);

    swapToggle = std::make_unique<SvgToggleButton>("Swap", swapIconDrawable(), mixerui::brandGold());
    monoToggle = std::make_unique<SvgToggleButton>("Mono", monoIconDrawable(), mixerui::brandMint());
    invertLeftToggle = std::make_unique<SvgToggleButton>("Invert L", invertLeftIconDrawable(), mixerui::brandCyan());
    invertRightToggle = std::make_unique<SvgToggleButton>("Invert R", invertRightIconDrawable(), mixerui::brandCyan());
    autoCompToggle = std::make_unique<SvgToggleButton>("Auto Comp", autoCompIconDrawable(), mixerui::brandMint());

    resetButton = std::make_unique<MiniActionButton>("Reset");
    resetButton->onClick = [this] { processor.resetToDefault(); };

    for (auto* component : {
             static_cast<juce::Component*>(leftFromLeftKnob.get()),
             static_cast<juce::Component*>(leftFromRightKnob.get()),
             static_cast<juce::Component*>(rightFromLeftKnob.get()),
             static_cast<juce::Component*>(rightFromRightKnob.get()),
             static_cast<juce::Component*>(outputTrimKnob.get()),
             static_cast<juce::Component*>(routerDisplay.get()),
             static_cast<juce::Component*>(swapToggle.get()),
             static_cast<juce::Component*>(monoToggle.get()),
             static_cast<juce::Component*>(invertLeftToggle.get()),
             static_cast<juce::Component*>(invertRightToggle.get()),
             static_cast<juce::Component*>(autoCompToggle.get()),
             static_cast<juce::Component*>(resetButton.get()) })
        addAndMakeVisible(component);

    swapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "swap", *swapToggle);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "mono", *monoToggle);
    invertLeftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "invertLeft", *invertLeftToggle);
    invertRightAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "invertRight", *invertRightToggle);
    autoCompAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "autoCompensate", *autoCompToggle);

    startTimerHz(30);
}

MixerControlsComponent::~MixerControlsComponent()
{
    stopTimer();
}

void MixerControlsComponent::timerCallback()
{
    const float targets[4] = {
        apvts.getRawParameterValue("leftFromLeft")->load(),
        apvts.getRawParameterValue("leftFromRight")->load(),
        apvts.getRawParameterValue("rightFromLeft")->load(),
        apvts.getRawParameterValue("rightFromRight")->load()
    };

    for (int i = 0; i < 4; ++i)
        displayedRoutes[(size_t) i] += (targets[i] - displayedRoutes[(size_t) i]) * 0.22f;

    animationPhase = std::fmod(animationPhase + 0.028f, 1.0f);
    repaint();
}

void MixerControlsComponent::paint(juce::Graphics& g)
{
    mixerui::drawPanelBackground(g, knobsPanelBounds.toFloat(), mixerui::brandCyan(), 18.0f);
    mixerui::drawPanelBackground(g, visualizerPanelBounds.toFloat(), mixerui::brandGold(), 18.0f);

    auto drawSection = [&](juce::Rectangle<int> bounds, const juce::String& title, const juce::String& subtitle, juce::Colour accent)
    {
        auto titleArea = bounds.reduced(18, 10).removeFromTop(24);
        g.setColour(mixerui::textStrong());
        g.setFont(mixerui::titleFont(15.0f));
        g.drawText(title, titleArea, juce::Justification::centredLeft, false);
        g.setColour(mixerui::textMuted());
        g.setFont(mixerui::bodyFont(8.8f));
        g.drawText(subtitle, titleArea.translated(0, 18).withHeight(16), juce::Justification::centredLeft, false);
        g.setColour(accent);
        g.fillEllipse((float) titleArea.getX() - 8.0f, (float) titleArea.getCentreY() - 2.5f, 5.0f, 5.0f);
    };

    drawSection(knobsPanelBounds, "Channel Mixer", "Cross routes are centered. Double-click any knob to reset it.", mixerui::brandCyan());
    drawSection(visualizerPanelBounds, "Visualizer", "Utility toggles, output trim, auto compensation, and reset.", mixerui::brandGold());
}

void MixerControlsComponent::resized()
{
    auto area = getLocalBounds();
    knobsPanelBounds = area.removeFromTop(290);
    area.removeFromTop(14);
    visualizerPanelBounds = area;

    auto knobsArea = knobsPanelBounds.reduced(18, 48);
    auto trimArea = knobsArea.removeFromRight(120);
    knobsArea.removeFromRight(10);
    const int knobWidth = (knobsArea.getWidth() - 30) / 4;
    const int knobHeight = 178;

    leftFromLeftKnob->setBounds(knobsArea.removeFromLeft(knobWidth).withHeight(knobHeight));
    knobsArea.removeFromLeft(10);
    leftFromRightKnob->setBounds(knobsArea.removeFromLeft(knobWidth).withHeight(knobHeight));
    knobsArea.removeFromLeft(10);
    rightFromLeftKnob->setBounds(knobsArea.removeFromLeft(knobWidth).withHeight(knobHeight));
    knobsArea.removeFromLeft(10);
    rightFromRightKnob->setBounds(knobsArea.removeFromLeft(knobWidth).withHeight(knobHeight));
    outputTrimKnob->setBounds(trimArea.withHeight(knobHeight));

    auto visualArea = visualizerPanelBounds.reduced(18, 44);
    auto utilityRow = visualArea.removeFromBottom(40);
    visualArea.removeFromBottom(8);
    routerDisplay->setBounds(visualArea);

    const int toggleGap = 8;
    const int toggleWidth = (utilityRow.getWidth() - toggleGap * 5) / 6;
    swapToggle->setBounds(utilityRow.removeFromLeft(toggleWidth));
    utilityRow.removeFromLeft(toggleGap);
    monoToggle->setBounds(utilityRow.removeFromLeft(toggleWidth));
    utilityRow.removeFromLeft(toggleGap);
    invertLeftToggle->setBounds(utilityRow.removeFromLeft(toggleWidth));
    utilityRow.removeFromLeft(toggleGap);
    invertRightToggle->setBounds(utilityRow.removeFromLeft(toggleWidth));
    utilityRow.removeFromLeft(toggleGap);
    autoCompToggle->setBounds(utilityRow.removeFromLeft(toggleWidth));
    utilityRow.removeFromLeft(toggleGap);
    resetButton->setBounds(utilityRow.removeFromLeft(toggleWidth));
}
