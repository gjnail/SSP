#include "MinimizeResponseComponent.h"
#include "MinimizeUI.h"

namespace
{
juce::Colour textMuted() { return minimizeui::textMuted(); }
juce::Colour accentMint() { return minimizeui::brandCyan(); }
juce::Colour accentGold() { return minimizeui::brandGold(); }
juce::Colour accentEmber() { return minimizeui::brandEmber(); }
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
    const auto differenceSolo = processor.isSoloDifferenceEnabled();

    g.setColour(textMuted());
    g.setFont(minimizeui::bodyFont(12.0f));
    g.drawText("Resonance map", topLine.removeFromLeft(180.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText(differenceSolo ? "Difference trace soloed" : "Adaptive suppression trace", topLine.toNearestInt(),
               juce::Justification::centredRight, false);

    auto graphArea = content.reduced(0.0f, 6.0f);
    minimizeui::drawDisplayPanel(g, graphArea, minimizeui::brandCyan());

    auto graph = graphArea.reduced(18.0f, 18.0f);
    auto footer = graph.removeFromBottom(24.0f);

    g.setColour(juce::Colours::white.withAlpha(0.04f));
    for (int row = 0; row < 8; ++row)
    {
        const auto y = juce::jmap((float) row / 7.0f, graph.getY(), graph.getBottom());
        g.drawHorizontalLine((int) y, graph.getX(), graph.getRight());
    }

    g.setColour(accentGold().withAlpha(0.10f));
    for (int col = 0; col < 12; ++col)
    {
        const auto x = juce::jmap((float) col / 11.0f, graph.getX(), graph.getRight());
        g.drawVerticalLine((int) x, graph.getY(), graph.getBottom());
    }

    auto visuals = processor.getBandVisuals();
    juce::Path attenuationCurve;
    juce::Path attenuationFill;
    juce::Path differenceCurve;
    juce::Path differenceFill;
    bool started = false;

    for (const auto& band : visuals)
    {
        const auto normHz = juce::jlimit(0.0f, 1.0f, (std::log(band.centreHz) - std::log(90.0f)) / (std::log(16000.0f) - std::log(90.0f)));
        const auto x = juce::jmap(normHz, 0.0f, 1.0f, graph.getX(), graph.getRight());
        const auto attenuationY = juce::jmap(band.attenuation, 0.0f, 0.9f, graph.getBottom() - 8.0f, graph.getY() + 18.0f);
        const auto differenceY = juce::jmap(band.differenceLevel, 0.0f, 1.0f, graph.getBottom() - 8.0f, graph.getY() + 18.0f);

        if (!started)
        {
            attenuationCurve.startNewSubPath(x, attenuationY);
            attenuationFill.startNewSubPath(x, graph.getBottom() - 2.0f);
            attenuationFill.lineTo(x, attenuationY);
            differenceCurve.startNewSubPath(x, differenceY);
            differenceFill.startNewSubPath(x, graph.getBottom() - 2.0f);
            differenceFill.lineTo(x, differenceY);
            started = true;
        }
        else
        {
            attenuationCurve.lineTo(x, attenuationY);
            attenuationFill.lineTo(x, attenuationY);
            differenceCurve.lineTo(x, differenceY);
            differenceFill.lineTo(x, differenceY);
        }
    }

    if (started)
    {
        attenuationFill.lineTo(graph.getRight(), graph.getBottom() - 2.0f);
        attenuationFill.closeSubPath();
        differenceFill.lineTo(graph.getRight(), graph.getBottom() - 2.0f);
        differenceFill.closeSubPath();

        juce::ColourGradient attenuationGradient(accentMint().withAlpha(differenceSolo ? 0.14f : 0.24f), graph.getCentreX(), graph.getY(),
                                                 accentMint().withAlpha(0.02f), graph.getCentreX(), graph.getBottom(), false);
        g.setGradientFill(attenuationGradient);
        g.fillPath(attenuationFill);

        juce::ColourGradient differenceGradient(accentEmber().withAlpha(differenceSolo ? 0.28f : 0.18f), graph.getCentreX(), graph.getY(),
                                                accentEmber().withAlpha(0.02f), graph.getCentreX(), graph.getBottom(), false);
        g.setGradientFill(differenceGradient);
        g.fillPath(differenceFill);

        g.setColour(accentMint().withAlpha(differenceSolo ? 0.55f : 1.0f));
        g.strokePath(attenuationCurve, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(accentEmber().withAlpha(differenceSolo ? 1.0f : 0.82f));
        g.strokePath(differenceCurve, juce::PathStrokeType(differenceSolo ? 3.2f : 2.4f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
    }

    for (const auto& band : visuals)
    {
        const auto normHz = juce::jlimit(0.0f, 1.0f, (std::log(band.centreHz) - std::log(90.0f)) / (std::log(16000.0f) - std::log(90.0f)));
        const auto x = juce::jmap(normHz, 0.0f, 1.0f, graph.getX(), graph.getRight());
        const auto attenuationY = juce::jmap(band.attenuation, 0.0f, 0.9f, graph.getBottom() - 8.0f, graph.getY() + 18.0f);
        const auto differenceY = juce::jmap(band.differenceLevel, 0.0f, 1.0f, graph.getBottom() - 8.0f, graph.getY() + 18.0f);

        g.setColour(accentGold());
        g.fillEllipse(x - 3.5f, attenuationY - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colour(0xfffaf4e8));
        g.drawEllipse(x - 3.5f, attenuationY - 3.5f, 7.0f, 7.0f, 1.0f);

        g.setColour(accentEmber().withAlpha(differenceSolo ? 1.0f : 0.9f));
        g.fillEllipse(x - 2.5f, differenceY - 2.5f, 5.0f, 5.0f);
    }

    g.setColour(textMuted());
    g.setFont(minimizeui::bodyFont(11.0f));
    g.drawText("90 Hz", footer.removeFromLeft(80.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("16 kHz", footer.removeFromRight(80.0f).toNearestInt(), juce::Justification::centredRight, false);
    auto legend = footer.reduced(40.0f, 0.0f);
    g.setColour(accentMint());
    g.fillRoundedRectangle(legend.removeFromLeft(14.0f).withHeight(4.0f).withCentre({ legend.getX() + 7.0f, legend.getCentreY() }), 2.0f);
    legend.removeFromLeft(8.0f);
    g.setColour(textMuted());
    g.drawText("Suppression", legend.removeFromLeft(100.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.setColour(accentEmber());
    g.fillRoundedRectangle(legend.removeFromLeft(14.0f).withHeight(4.0f).withCentre({ legend.getX() + 7.0f, legend.getCentreY() }), 2.0f);
    legend.removeFromLeft(8.0f);
    g.setColour(textMuted());
    g.drawText("Difference", legend.toNearestInt(), juce::Justification::centredLeft, false);
}
