#include "MinimizeResponseComponent.h"
#include "MinimizeUI.h"

namespace
{
juce::Colour textMuted() { return minimizeui::textMuted(); }
juce::Colour accentMint() { return minimizeui::brandCyan(); }
juce::Colour accentGold() { return minimizeui::brandGold(); }
}

MinimizeResponseComponent::MinimizeResponseComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(24);
}

MinimizeResponseComponent::~MinimizeResponseComponent()
{
    stopTimer();
}

void MinimizeResponseComponent::timerCallback()
{
    repaint();
}

void MinimizeResponseComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    minimizeui::drawPanelBackground(g, bounds, minimizeui::brandGold(), 28.0f);

    auto content = bounds.reduced(24.0f, 20.0f);
    auto topLine = content.removeFromTop(24.0f);

    g.setColour(textMuted());
    g.setFont(minimizeui::bodyFont(12.0f));
    g.drawText("Resonance map", topLine.removeFromLeft(180.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("Adaptive suppression trace", topLine.toNearestInt(), juce::Justification::centredRight, false);

    auto graphArea = content.reduced(0.0f, 6.0f);
    minimizeui::drawDisplayPanel(g, graphArea, minimizeui::brandCyan());

    auto graph = graphArea.reduced(18.0f, 18.0f);
    auto footer = graph.removeFromBottom(24.0f);

    g.setColour(juce::Colours::white.withAlpha(0.04f));
    for (int row = 0; row < 6; ++row)
    {
        const auto y = juce::jmap((float) row / 5.0f, graph.getY(), graph.getBottom());
        g.drawHorizontalLine((int) y, graph.getX(), graph.getRight());
    }

    g.setColour(accentGold().withAlpha(0.10f));
    for (int col = 0; col < 8; ++col)
    {
        const auto x = juce::jmap((float) col / 7.0f, graph.getX(), graph.getRight());
        g.drawVerticalLine((int) x, graph.getY(), graph.getBottom());
    }

    auto visuals = processor.getBandVisuals();
    juce::Path curve;
    juce::Path fill;
    bool started = false;

    for (const auto& band : visuals)
    {
        const auto normHz = juce::jlimit(0.0f, 1.0f, (std::log(band.centreHz) - std::log(90.0f)) / (std::log(16000.0f) - std::log(90.0f)));
        const auto x = juce::jmap(normHz, 0.0f, 1.0f, graph.getX(), graph.getRight());
        const auto y = juce::jmap(band.attenuation, 0.0f, 0.9f, graph.getBottom() - 8.0f, graph.getY() + 12.0f);

        if (!started)
        {
            curve.startNewSubPath(x, y);
            fill.startNewSubPath(x, graph.getBottom() - 2.0f);
            fill.lineTo(x, y);
            started = true;
        }
        else
        {
            curve.lineTo(x, y);
            fill.lineTo(x, y);
        }
    }

    if (started)
    {
        fill.lineTo(graph.getRight(), graph.getBottom() - 2.0f);
        fill.closeSubPath();

        juce::ColourGradient fillGradient(accentMint().withAlpha(0.22f), graph.getCentreX(), graph.getY(),
                                          accentMint().withAlpha(0.02f), graph.getCentreX(), graph.getBottom(), false);
        g.setGradientFill(fillGradient);
        g.fillPath(fill);

        g.setColour(accentMint());
        g.strokePath(curve, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    for (const auto& band : visuals)
    {
        const auto normHz = juce::jlimit(0.0f, 1.0f, (std::log(band.centreHz) - std::log(90.0f)) / (std::log(16000.0f) - std::log(90.0f)));
        const auto x = juce::jmap(normHz, 0.0f, 1.0f, graph.getX(), graph.getRight());
        const auto y = juce::jmap(band.attenuation, 0.0f, 0.9f, graph.getBottom() - 8.0f, graph.getY() + 12.0f);

        g.setColour(accentGold());
        g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(0xfffaf4e8));
        g.drawEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
    }

    g.setColour(textMuted());
    g.setFont(minimizeui::bodyFont(11.0f));
    g.drawText("90 Hz", footer.removeFromLeft(80.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("16 kHz", footer.removeFromRight(80.0f).toNearestInt(), juce::Justification::centredRight, false);
    g.drawText("More suppression appears higher as bands react", footer.toNearestInt(), juce::Justification::centred, false);
}
