#include "SubValidatorComponent.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff3e3842); }
juce::Colour subtleText() { return juce::Colour(0xffc8becb); }
juce::Colour cyan() { return juce::Colour(0xff74d7ff); }
juce::Colour mint() { return juce::Colour(0xff79f0b7); }
juce::Colour amber() { return juce::Colour(0xffffc978); }
juce::Colour red() { return juce::Colour(0xffff6f7a); }
juce::Colour darkPanel() { return juce::Colour(0xff111019); }

juce::String formatDb(float value)
{
    if (value <= -80.0f)
        return "--.- dB";

    return juce::String(value, 1) + " dB";
}

float mapDbToY(float db)
{
    return juce::jmap(juce::jlimit(-10.0f, 0.5f, db), 0.5f, -10.0f, 0.0f, 1.0f);
}
} // namespace

SubValidatorComponent::SubValidatorComponent(PluginProcessor& p)
    : processor(p)
{
    freezeButton.setClickingTogglesState(true);
    freezeButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(freezeButton);

    refreshFromProcessor();
    startTimerHz(24);
}

SubValidatorComponent::~SubValidatorComponent()
{
    stopTimer();
}

void SubValidatorComponent::timerCallback()
{
    refreshFromProcessor();
}

void SubValidatorComponent::refreshFromProcessor()
{
    if (!freezeButton.getToggleState())
    {
        displayedSubPeakDb = processor.getSubPeakDb();
        displayedSubInRange = processor.isSubInRange();
        processor.copySpectrum(spectrumValues);
    }

    repaint();
}

juce::String SubValidatorComponent::getStatusText() const
{
    if (displayedSubPeakDb <= -80.0f)
        return "WAITING FOR LOW END";

    return displayedSubInRange ? "SUB IN RANGE" : "SUB OUT OF RANGE";
}

void SubValidatorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff14111a), bounds.getTopLeft(),
                               juce::Colour(0xff0c0c13), bounds.getBottomRight(),
                               false);
    shell.addColour(0.5, juce::Colour(0xff17131b));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 28.0f);

    auto glow = bounds.reduced(30.0f, 18.0f).removeFromTop(120.0f);
    juce::ColourGradient headerGlow(cyan().withAlpha(0.15f), glow.getX(), glow.getY(),
                                    mint().withAlpha(0.08f), glow.getRight(), glow.getBottom(), false);
    g.setGradientFill(headerGlow);
    g.fillRoundedRectangle(glow, 24.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);

    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(44);

    g.setColour(subtleText());
    g.setFont(juce::Font(13.0f));
    g.drawFittedText("Focused 0-200 Hz spectrum with a simple pass/fail sub check against the target -3 dB to 0 dB window.",
                     header.removeFromLeft(720),
                     juce::Justification::centredLeft,
                     2);

    area.removeFromTop(8);

    auto statusArea = area.removeFromTop(160).toFloat();
    juce::Colour statusColour = displayedSubInRange ? mint() : red();

    juce::ColourGradient statusFill(statusColour.withAlpha(0.28f), statusArea.getTopLeft(),
                                    darkPanel(), statusArea.getBottomRight(),
                                    false);
    g.setGradientFill(statusFill);
    g.fillRoundedRectangle(statusArea, 26.0f);
    g.setColour(statusColour.withAlpha(0.7f));
    g.drawRoundedRectangle(statusArea.reduced(0.5f), 26.0f, 1.3f);

    auto statusInner = statusArea.reduced(22.0f, 18.0f);
    g.setColour(subtleText());
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("SUB VALIDATOR", statusInner.removeFromTop(22.0f).toNearestInt(), juce::Justification::topLeft, 1);

    auto statusRow = statusInner.removeFromTop(74.0f);
    g.setColour(statusColour);
    g.setFont(juce::Font(42.0f, juce::Font::bold));
    g.drawFittedText(getStatusText(), statusRow.toNearestInt(), juce::Justification::centredLeft, 1);

    auto lowerStatus = statusInner;
    auto valueArea = lowerStatus.removeFromLeft(230);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(25.0f, juce::Font::bold));
    g.drawFittedText(formatDb(displayedSubPeakDb), valueArea.removeFromTop(34.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setColour(subtleText());
    g.setFont(juce::Font(12.5f));
    g.drawFittedText("Peak level found between 20 Hz and 200 Hz", valueArea.toNearestInt(), juce::Justification::centredLeft, 2);

    g.setColour(subtleText());
    g.setFont(juce::Font(12.5f));
    g.drawFittedText("Green means the detected sub peak is inside the target zone. Red means the low end is either below target or pushing past 0 dB.",
                     lowerStatus.toNearestInt(),
                     juce::Justification::centredLeft,
                     3);

    area.removeFromTop(14);

    auto analyzerArea = area.toFloat();
    juce::ColourGradient analyzerFill(juce::Colour(0xff12101a), analyzerArea.getTopLeft(),
                                      juce::Colour(0xff0b0b11), analyzerArea.getBottomRight(),
                                      false);
    g.setGradientFill(analyzerFill);
    g.fillRoundedRectangle(analyzerArea, 26.0f);
    g.setColour(panelOutline());
    g.drawRoundedRectangle(analyzerArea.reduced(0.5f), 26.0f, 1.0f);

    auto inner = analyzerArea.reduced(20.0f, 18.0f);
    auto top = inner.removeFromTop(24.0f);
    g.setColour(subtleText());
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("SUB SPECTRUM 0-200 HZ", top.toNearestInt(), juce::Justification::topLeft, 1);

    if (freezeButton.getToggleState())
    {
        auto badge = top.removeFromRight(88.0f).reduced(4.0f, 0.0f);
        g.setColour(juce::Colour(0xff2d2b3a));
        g.fillRoundedRectangle(badge, 10.0f);
        g.setColour(amber());
        g.drawRoundedRectangle(badge.reduced(0.5f), 10.0f, 1.0f);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawFittedText("FROZEN", badge.toNearestInt(), juce::Justification::centred, 1);
    }

    auto plot = inner.reduced(0.0f, 8.0f);

    const std::array<float, 8> dbGuides{{0.0f, -1.0f, -2.0f, -3.0f, -4.0f, -5.0f, -6.0f, -10.0f}};
    for (float guide : dbGuides)
    {
        const float y = plot.getY() + plot.getHeight() * mapDbToY(guide);
        g.setColour(guide >= -6.0f ? juce::Colour(guide >= -3.0f ? 0x2a39b972 : 0x183b8ed6)
                                   : juce::Colour(0x163b3f52));
        g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        g.setColour(subtleText().withAlpha(0.8f));
        g.setFont(juce::Font(10.5f));
        g.drawText(juce::String(guide, 0), juce::Rectangle<int>((int) plot.getX(), (int) y - 8, 36, 16), juce::Justification::left);
    }

    auto targetBand = plot.withTop(plot.getY() + plot.getHeight() * mapDbToY(0.0f))
                         .withBottom(plot.getY() + plot.getHeight() * mapDbToY(-3.0f));
    g.setColour(juce::Colour(0x1637c86f));
    g.fillRoundedRectangle(targetBand.reduced(0.0f, 1.0f), 10.0f);

    const std::array<int, 8> freqGuides{{20, 30, 40, 50, 80, 100, 150, 200}};
    for (int freq : freqGuides)
    {
        const float x = juce::jmap(std::log10((float) freq),
                                   std::log10(20.0f),
                                   std::log10(200.0f),
                                   plot.getX(),
                                   plot.getRight());
        g.setColour(juce::Colour(0x223b3f52));
        g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
        g.setColour(subtleText().withAlpha(0.8f));
        g.setFont(juce::Font(10.5f));
        g.drawText(juce::String(freq), juce::Rectangle<int>((int) x - 16, (int) plot.getBottom() - 18, 32, 16), juce::Justification::centred);
    }

    if (spectrumValues.size() > 1)
    {
        juce::Path spectrumPath;
        std::vector<juce::Point<float>> points;
        points.reserve(spectrumValues.size());

        for (size_t i = 0; i < spectrumValues.size(); ++i)
        {
            const float frequency = std::pow(10.0f,
                                             juce::jmap((float) i / (float) (spectrumValues.size() - 1),
                                                        0.0f,
                                                        1.0f,
                                                        std::log10(20.0f),
                                                        std::log10(200.0f)));
            const float x = juce::jmap(std::log10(frequency),
                                       std::log10(20.0f),
                                       std::log10(200.0f),
                                       plot.getX(),
                                       plot.getRight());
            const float y = plot.getY() + plot.getHeight() * mapDbToY(spectrumValues[i]);
            points.push_back({x, y});
        }

        if (!points.empty())
        {
            spectrumPath.startNewSubPath(points.front());

            if (points.size() == 2)
            {
                spectrumPath.lineTo(points.back());
            }
            else
            {
                for (size_t i = 0; i + 1 < points.size(); ++i)
                {
                    const auto p0 = i == 0 ? points[i] : points[i - 1];
                    const auto p1 = points[i];
                    const auto p2 = points[i + 1];
                    const auto p3 = (i + 2 < points.size()) ? points[i + 2] : points[i + 1];

                    const juce::Point<float> c1{
                        p1.x + (p2.x - p0.x) / 6.0f,
                        p1.y + (p2.y - p0.y) / 6.0f
                    };

                    const juce::Point<float> c2{
                        p2.x - (p3.x - p1.x) / 6.0f,
                        p2.y - (p3.y - p1.y) / 6.0f
                    };

                    spectrumPath.cubicTo(c1, c2, p2);
                }
            }
        }

        juce::Path fillPath(spectrumPath);
        fillPath.lineTo(plot.getRight(), plot.getBottom());
        fillPath.lineTo(plot.getX(), plot.getBottom());
        fillPath.closeSubPath();

        juce::ColourGradient spectrumFill(statusColour.withAlpha(0.20f), plot.getCentreX(), plot.getY(),
                                          juce::Colour(0x00000000), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill(spectrumFill);
        g.fillPath(fillPath);

        g.setColour(statusColour);
        g.strokePath(spectrumPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void SubValidatorComponent::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(44);
    freezeButton.setBounds(header.removeFromRight(120).withSizeKeepingCentre(104, 28));
}
