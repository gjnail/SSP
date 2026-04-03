#include "TransitionVisualizerComponent.h"
#include "TransitionVectorUI.h"

namespace
{
float smoothTowards(float current, float target, float speed)
{
    return current + (target - current) * speed;
}
} // namespace

TransitionVisualizerComponent::TransitionVisualizerComponent(PluginProcessor& p)
    : processor(p)
{
    displayState = processor.getVisualState();
    startTimerHz(30);
}

void TransitionVisualizerComponent::timerCallback()
{
    const auto target = processor.getVisualState();
    displayState.amount = smoothTowards(displayState.amount, target.amount, 0.18f);
    displayState.mix = smoothTowards(displayState.mix, target.mix, 0.18f);
    displayState.shapedAmount = smoothTowards(displayState.shapedAmount, target.shapedAmount, 0.18f);
    displayState.filterAmount = smoothTowards(displayState.filterAmount, target.filterAmount, 0.16f);
    displayState.spaceAmount = smoothTowards(displayState.spaceAmount, target.spaceAmount, 0.16f);
    displayState.widthAmount = smoothTowards(displayState.widthAmount, target.widthAmount, 0.16f);
    displayState.riseAmount = smoothTowards(displayState.riseAmount, target.riseAmount, 0.16f);
    displayState.duckAmount = smoothTowards(displayState.duckAmount, target.duckAmount, 0.16f);
    displayState.presetName = target.presetName;
    displayState.presetBadge = target.presetBadge;
    displayState.presetDescription = target.presetDescription;

    motionPhase += 0.008f + displayState.riseAmount * 0.030f + displayState.spaceAmount * 0.010f;
    if (motionPhase >= 1.0f)
        motionPhase -= std::floor(motionPhase);

    repaint();
}

void TransitionVisualizerComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    transitionui::drawPanelBackground(g, bounds, transitionui::brandCyan(), 20.0f);

    auto content = bounds.reduced(18.0f, 16.0f);
    auto header = content.removeFromTop(26.0f);
    auto meters = content.removeFromBottom(56.0f);
    auto stage = content.reduced(4.0f, 4.0f);

    g.setColour(transitionui::textMuted());
    g.setFont(transitionui::smallCapsFont(11.0f));
    g.drawText("Transition Map", header.removeFromLeft(160.0f).toNearestInt(), juce::Justification::centredLeft, false);

    auto badgeBounds = header.removeFromRight(132.0f).toNearestInt();
    g.setColour(juce::Colour(0xff17333f));
    g.fillRoundedRectangle(badgeBounds.toFloat(), 8.0f);
    g.setColour(transitionui::brandCyan().withAlpha(0.55f));
    g.drawRoundedRectangle(badgeBounds.toFloat().reduced(0.5f), 8.0f, 1.0f);
    g.setColour(transitionui::textStrong());
    g.setFont(transitionui::smallCapsFont(10.0f));
    g.drawText(displayState.presetBadge, badgeBounds, juce::Justification::centred, false);

    const float visibleAmount = displayState.shapedAmount * (0.25f + 0.75f * displayState.mix);

    g.setColour(juce::Colour(0x22152230));
    g.fillRoundedRectangle(stage, 14.0f);

    for (int i = 0; i < 7; ++i)
    {
        const float y = stage.getY() + stage.getHeight() * (float) i / 6.0f;
        g.setColour(juce::Colour(0xff20303d).withAlpha(i == 6 ? 0.18f : 0.12f));
        g.drawHorizontalLine((int) std::round(y), stage.getX(), stage.getRight());
    }

    for (int i = 0; i < 9; ++i)
    {
        const float x = stage.getX() + stage.getWidth() * (float) i / 8.0f;
        g.setColour(juce::Colour(0xff20303d).withAlpha(0.10f));
        g.drawVerticalLine((int) std::round(x), stage.getY(), stage.getBottom());
    }

    auto stereoHalo = juce::Rectangle<float>(stage.getWidth() * (0.18f + displayState.widthAmount * 0.82f),
                                             stage.getHeight() * (0.14f + displayState.spaceAmount * 0.56f))
                          .withCentre(stage.getCentre());
    juce::ColourGradient stereoGlow(transitionui::brandCyan().withAlpha(0.12f + displayState.widthAmount * 0.12f),
                                    stereoHalo.getCentre(),
                                    juce::Colours::transparentBlack,
                                    { stereoHalo.getRight(), stereoHalo.getCentreY() },
                                    true);
    g.setGradientFill(stereoGlow);
    g.fillEllipse(stereoHalo);

    for (int ring = 0; ring < 4; ++ring)
    {
        const float ringProgress = std::fmod(motionPhase + 0.22f * (float) ring, 1.0f);
        const float alpha = (1.0f - ringProgress) * displayState.spaceAmount * 0.26f;
        auto ringBounds = juce::Rectangle<float>(stage.getWidth() * (0.14f + ringProgress * 0.60f),
                                                 stage.getHeight() * (0.10f + ringProgress * 0.42f))
                              .withCentre(stage.getCentre());
        g.setColour(transitionui::brandCyan().withAlpha(alpha));
        g.drawEllipse(ringBounds, 1.35f);
    }

    const int numBars = 18;
    const float spacing = stage.getWidth() / (float) numBars;
    const float barWidth = juce::jmin(18.0f, spacing * 0.58f);

    for (int bar = 0; bar < numBars; ++bar)
    {
        const float xProp = (float) bar / (float) (numBars - 1);
        const float filterBias = 0.16f + (1.0f - displayState.filterAmount * std::pow(1.0f - xProp, 1.8f)) * 0.84f;
        const float pulse = 0.62f + 0.38f * (0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * (motionPhase * 1.4f + xProp * 0.34f)));
        const float barHeight = stage.getHeight() * (0.10f + filterBias * (0.24f + visibleAmount * 0.34f) * pulse);
        auto barBounds = juce::Rectangle<float>(barWidth, barHeight)
                             .withCentre({ stage.getX() + spacing * ((float) bar + 0.5f), stage.getBottom() - barHeight * 0.5f - 8.0f });

        juce::ColourGradient fill(transitionui::brandAmber().withAlpha(0.28f + visibleAmount * 0.24f),
                                  barBounds.getBottomLeft(),
                                  transitionui::brandCyan().withAlpha(0.50f + displayState.spaceAmount * 0.22f),
                                  barBounds.getTopRight(),
                                  false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(barBounds, 5.0f);

        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(barBounds.removeFromTop(juce::jmax(4.0f, barHeight * 0.16f)), 4.0f);
    }

    for (int ribbon = 0; ribbon < 3; ++ribbon)
    {
        const float phase = std::fmod(motionPhase + 0.24f * (float) ribbon, 1.0f);
        const float startX = stage.getX() + stage.getWidth() * (0.08f + phase * 0.66f);
        const float startY = stage.getBottom() - 12.0f;
        juce::Path rise;
        rise.startNewSubPath(startX, startY);
        rise.cubicTo(startX + 18.0f, startY - stage.getHeight() * 0.22f,
                     startX - 36.0f, stage.getY() + stage.getHeight() * 0.44f,
                     startX + stage.getWidth() * 0.18f, stage.getY() + stage.getHeight() * 0.10f);
        g.setColour(transitionui::brandAmber().withAlpha(displayState.riseAmount * (0.08f + 0.12f * (1.0f - phase))));
        g.strokePath(rise, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    struct MeterInfo
    {
        const char* label;
        float value;
        juce::Colour colour;
    };

    const std::array<MeterInfo, 4> meterInfo{{
        { "Filter", displayState.filterAmount, transitionui::brandAmber() },
        { "Space", displayState.spaceAmount, transitionui::brandCyan() },
        { "Width", displayState.widthAmount, juce::Colour(0xff7bd9ff) },
        { "Rise", displayState.riseAmount, juce::Colour(0xffff9c53) }
    }};

    const float meterWidth = (meters.getWidth() - 18.0f) / 4.0f;
    for (size_t i = 0; i < meterInfo.size(); ++i)
    {
        auto meterBounds = juce::Rectangle<float>(meterWidth, meters.getHeight())
                               .withPosition(meters.getX() + (meterWidth + 6.0f) * (float) i, meters.getY());
        auto card = meterBounds.reduced(2.0f, 4.0f);
        g.setColour(juce::Colour(0xff12202c));
        g.fillRoundedRectangle(card, 9.0f);
        g.setColour(transitionui::outlineSoft());
        g.drawRoundedRectangle(card, 9.0f, 1.0f);

        g.setColour(transitionui::textMuted());
        g.setFont(transitionui::smallCapsFont(9.5f));
        g.drawText(meterInfo[i].label, card.removeFromTop(16.0f).toNearestInt(), juce::Justification::centred, false);

        auto bar = card.reduced(10.0f, 0.0f).removeFromBottom(14.0f);
        g.setColour(juce::Colour(0xff0b131b));
        g.fillRoundedRectangle(bar, 6.0f);

        auto fill = bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, meterInfo[i].value));
        juce::ColourGradient meterFill(meterInfo[i].colour.withAlpha(0.92f), fill.getTopLeft(),
                                       meterInfo[i].colour.darker(0.28f), fill.getBottomLeft(), false);
        g.setGradientFill(meterFill);
        g.fillRoundedRectangle(fill, 6.0f);
    }
}
