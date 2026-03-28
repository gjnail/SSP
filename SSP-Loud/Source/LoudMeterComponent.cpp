#include "LoudMeterComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3a4b57); }
juce::Colour subtleText() { return juce::Colour(0xffa9bbc8); }
juce::Colour ink() { return juce::Colour(0xff0b1016); }
juce::Colour cyan() { return juce::Colour(0xff67d5ff); }
juce::Colour mint() { return juce::Colour(0xff73f2c9); }
juce::Colour amber() { return juce::Colour(0xffffc771); }
juce::Colour coral() { return juce::Colour(0xffff8d6f); }

juce::String formatLoudness(float value)
{
    if (value <= -99.0f)
        return "--.-";

    return juce::String(value, 1);
}

juce::String formatPeak(float value)
{
    if (value <= -59.9f)
        return "--.-";

    return juce::String(value, 1);
}

void drawMetricCard(juce::Graphics& g,
                    juce::Rectangle<float> area,
                    const juce::String& title,
                    const juce::String& value,
                    const juce::String& units,
                    juce::Colour accent,
                    bool hero)
{
    juce::ColourGradient fill(juce::Colour(0xff121922), area.getTopLeft(),
                              juce::Colour(0xff0c1118), area.getBottomRight(),
                              false);
    fill.addColour(0.45, juce::Colour(0xff101720));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(area, 22.0f);

    auto accentBar = area.reduced(18.0f, 14.0f).removeFromTop(5.0f);
    juce::ColourGradient accentFill(accent.withAlpha(0.70f), accentBar.getX(), accentBar.getCentreY(),
                                    juce::Colours::white.withAlpha(0.22f), accentBar.getRight(), accentBar.getCentreY(), false);
    g.setGradientFill(accentFill);
    g.fillRoundedRectangle(accentBar.withTrimmedLeft(accentBar.getWidth() * 0.28f), 3.0f);

    g.setColour(subtleText());
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(title.toUpperCase(), area.reduced(18).removeFromTop(22).toNearestInt(), juce::Justification::topLeft, 1);

    auto valueArea = area.reduced(18).withTrimmedTop(hero ? 50.0f : 46.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(hero ? 50.0f : 38.0f, juce::Font::bold));
    g.drawFittedText(value, valueArea.removeFromTop(hero ? 58.0f : 44.0f).toNearestInt(), juce::Justification::centredLeft, 1);

    g.setColour(accent);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawFittedText(units, area.reduced(18).removeFromBottom(24).toNearestInt(), juce::Justification::bottomLeft, 1);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(area.reduced(0.5f), 22.0f, 1.0f);
}

float loudnessToNormalisedY(float lufs)
{
    return juce::jmap(juce::jlimit(-54.0f, 0.0f, lufs), -54.0f, 0.0f, 1.0f, 0.0f);
}

void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label, float level, juce::Colour accent)
{
    g.setColour(juce::Colour(0xff10151d));
    g.fillRoundedRectangle(area, 14.0f);

    auto fill = area.withTrimmedLeft(area.getWidth() * (1.0f - juce::jlimit(0.0f, 1.0f, level)));
    juce::ColourGradient gradient(accent, fill.getX(), fill.getCentreY(),
                                  amber(), fill.getRight(), fill.getCentreY(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(fill, 14.0f);

    g.setColour(subtleText());
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawFittedText(label, area.toNearestInt().reduced(12, 0), juce::Justification::centredLeft, 1);

    const float db = juce::Decibels::gainToDecibels(juce::jmax(level, 0.00001f), -60.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(juce::String(db, 1) + " dB", area.toNearestInt().reduced(12, 0), juce::Justification::centredRight, 1);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(area.reduced(0.5f), 14.0f, 1.0f);
}
} // namespace

LoudMeterComponent::LoudMeterComponent(PluginProcessor& p)
    : processor(p)
{
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff17202a));
    resetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff203244));
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    resetButton.onClick = [this] { processor.requestReset(); };
    addAndMakeVisible(resetButton);

    refreshFromProcessor();
    startTimerHz(20);
}

LoudMeterComponent::~LoudMeterComponent()
{
    stopTimer();
}

void LoudMeterComponent::timerCallback()
{
    refreshFromProcessor();
}

void LoudMeterComponent::refreshFromProcessor()
{
    momentaryLufs = processor.getMomentaryLufs();
    shortTermLufs = processor.getShortTermLufs();
    integratedLufs = processor.getIntegratedLufs();
    peakDb = processor.getPeakDb();
    leftLevel = processor.getLeftMeterLevel();
    rightLevel = processor.getRightMeterLevel();
    processor.copyHistory(historyValues);
    repaint();
}

void LoudMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff101720), bounds.getTopLeft(),
                               juce::Colour(0xff091017), bounds.getBottomRight(),
                               false);
    shell.addColour(0.48, juce::Colour(0xff0d141c));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto upperGlow = bounds.reduced(26.0f, 18.0f).removeFromTop(120.0f);
    juce::ColourGradient glow(cyan().withAlpha(0.14f), upperGlow.getX(), upperGlow.getY(),
                              mint().withAlpha(0.08f), upperGlow.getRight(), upperGlow.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(upperGlow, 20.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);

    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(44);

    g.setColour(subtleText());
    g.setFont(juce::Font(13.0f));
    g.drawFittedText("Momentary, short-term, integrated, and peak loudness with a rolling trend view.", header.removeFromLeft(640), juce::Justification::centredLeft, 1);

    area.removeFromTop(8);

    auto topRow = area.removeFromTop(180);
    auto integratedArea = topRow.removeFromLeft((int) std::round(topRow.getWidth() * 0.34f)).toFloat();
    topRow.removeFromLeft(12);
    const int smallCardWidth = (topRow.getWidth() - 24) / 3;
    auto momentaryArea = topRow.removeFromLeft(smallCardWidth).toFloat();
    topRow.removeFromLeft(12);
    auto shortTermArea = topRow.removeFromLeft(smallCardWidth).toFloat();
    topRow.removeFromLeft(12);
    auto peakArea = topRow.toFloat();

    drawMetricCard(g, integratedArea, "Integrated", formatLoudness(integratedLufs), "LUFS", amber(), true);
    drawMetricCard(g, momentaryArea, "Momentary", formatLoudness(momentaryLufs), "LUFS-M", cyan(), false);
    drawMetricCard(g, shortTermArea, "Short-Term", formatLoudness(shortTermLufs), "LUFS-S", mint(), false);
    drawMetricCard(g, peakArea, "Peak", formatPeak(peakDb), "dBFS", coral(), false);

    area.removeFromTop(14);

    auto lowerRow = area;
    auto sidePanel = lowerRow.removeFromRight(220).toFloat();
    lowerRow.removeFromRight(14);
    auto graphArea = lowerRow.toFloat();

    juce::ColourGradient graphFill(juce::Colour(0xff121923), graphArea.getTopLeft(),
                                   juce::Colour(0xff0c1118), graphArea.getBottomRight(),
                                   false);
    g.setGradientFill(graphFill);
    g.fillRoundedRectangle(graphArea, 24.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(graphArea.reduced(0.5f), 24.0f, 1.0f);

    auto graphInner = graphArea.reduced(20.0f, 18.0f);
    auto graphHeader = graphInner.removeFromTop(22.0f);
    g.setColour(subtleText());
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("SHORT-TERM TREND", graphHeader.toNearestInt(), juce::Justification::topLeft, 1);

    auto plotArea = graphInner.reduced(0.0f, 6.0f);
    const std::array<float, 4> guideValues{{-36.0f, -23.0f, -14.0f, -6.0f}};
    for (float guide : guideValues)
    {
        const float y = plotArea.getY() + plotArea.getHeight() * loudnessToNormalisedY(guide);
        g.setColour(juce::Colour(0x22394a57));
        g.drawHorizontalLine((int) std::round(y), plotArea.getX(), plotArea.getRight());
        g.setColour(subtleText().withAlpha(0.7f));
        g.setFont(juce::Font(10.5f));
        g.drawText(juce::String(guide, 0), juce::Rectangle<int>((int) plotArea.getX(), (int) y - 8, 40, 16), juce::Justification::left);
    }

    if (historyValues.size() > 1)
    {
        juce::Path path;
        for (size_t i = 0; i < historyValues.size(); ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (historyValues.size() - 1), plotArea.getX() + 4.0f, plotArea.getRight() - 4.0f);
            const float y = plotArea.getY() + plotArea.getHeight() * loudnessToNormalisedY(historyValues[i]);

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        juce::Path fillPath(path);
        fillPath.lineTo(plotArea.getRight() - 4.0f, plotArea.getBottom() - 2.0f);
        fillPath.lineTo(plotArea.getX() + 4.0f, plotArea.getBottom() - 2.0f);
        fillPath.closeSubPath();

        juce::ColourGradient pathFill(cyan().withAlpha(0.22f), plotArea.getCentreX(), plotArea.getY(),
                                      juce::Colour(0x0044d2ff), plotArea.getCentreX(), plotArea.getBottom(), false);
        g.setGradientFill(pathFill);
        g.fillPath(fillPath);

        g.setColour(cyan());
        g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::ColourGradient sideFill(juce::Colour(0xff121923), sidePanel.getTopLeft(),
                                  juce::Colour(0xff0c1118), sidePanel.getBottomRight(),
                                  false);
    g.setGradientFill(sideFill);
    g.fillRoundedRectangle(sidePanel, 24.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(sidePanel.reduced(0.5f), 24.0f, 1.0f);

    auto sideInner = sidePanel.reduced(18.0f, 18.0f);
    g.setColour(subtleText());
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("INPUT LEVEL", sideInner.removeFromTop(20.0f).toNearestInt(), juce::Justification::topLeft, 1);
    sideInner.removeFromTop(8.0f);

    auto leftBar = sideInner.removeFromTop(48.0f);
    auto rightBar = sideInner.removeFromTop(48.0f);
    drawMeterBar(g, leftBar, "Left", leftLevel, mint());
    drawMeterBar(g, rightBar, "Right", rightLevel, cyan());

    sideInner.removeFromTop(14.0f);
    g.setColour(subtleText());
    g.setFont(juce::Font(11.5f));
    g.drawFittedText("Target streaming loudness often lands around -14 LUFS, while broadcast is commonly closer to -23 LUFS.",
                     sideInner.toNearestInt(),
                     juce::Justification::topLeft,
                     4);
}

void LoudMeterComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(44);
    resetButton.setBounds(header.removeFromRight(150).withSizeKeepingCentre(138, 28));
}
