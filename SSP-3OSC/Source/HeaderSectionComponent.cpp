#include "HeaderSectionComponent.h"
#include "ReactorUI.h"

namespace
{
juce::Path createRadiationIcon(juce::Rectangle<float> area)
{
    juce::Path icon;
    const auto centre = area.getCentre();
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.48f;
    const float hubRadius = radius * 0.18f;

    for (int i = 0; i < 3; ++i)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) i / 3.0f - juce::MathConstants<float>::halfPi;
        juce::Path lobe;
        lobe.addPieSegment(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre),
                           angle - 0.52f, angle + 0.52f, 0.44f);
        icon.addPath(lobe);
    }

    icon.addEllipse(juce::Rectangle<float>(hubRadius * 2.0f, hubRadius * 2.0f).withCentre(centre));
    return icon;
}
}

HeaderSectionComponent::HeaderSectionComponent()
{
    addAndMakeVisible(titleLabel);

    titleLabel.setText("SSP REACTOR", juce::dontSendNotification);
    titleLabel.setFont(reactorui::headerFont(38.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reactorui::textStrong());

    for (size_t i = 0; i < tubePhases.size(); ++i)
    {
        tubePhases[i] = random.nextFloat() * juce::MathConstants<float>::twoPi;
        tubeGlow[i] = 0.55f + random.nextFloat() * 0.25f;
    }

    startTimerHz(26);
}

void HeaderSectionComponent::paint(juce::Graphics& g)
{
    titleLabel.setColour(juce::Label::textColourId, reactorui::textStrong());

    auto bounds = getLocalBounds().toFloat();
    reactorui::drawPanelBackground(g, bounds, juce::Colour(0xff8dfd63), 18.0f);

    auto area = bounds.reduced(12.0f, 10.0f);
    auto badgeArea = area.removeFromLeft(82.0f);
    auto tubeRackArea = area.removeFromRight(430.0f);
    area.removeFromLeft(10.0f);

    auto badge = badgeArea.reduced(6.0f);
    juce::ColourGradient badgeFill(reactorui::isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff2d343a), badge.getTopLeft(),
                                   reactorui::isLightTheme() ? juce::Colour(0xffece2d3) : juce::Colour(0xff101418), badge.getBottomLeft(), false);
    g.setGradientFill(badgeFill);
    g.fillRoundedRectangle(badge, 5.0f);
    g.setColour(reactorui::isLightTheme() ? juce::Colour(0x55101216) : juce::Colour(0xff050607));
    g.drawRoundedRectangle(badge, 5.0f, 1.6f);
    g.setColour(juce::Colour(0xff8eff63));
    g.fillPath(createRadiationIcon(badge.reduced(14.0f)));

    auto textPlate = area.reduced(2.0f, 3.0f);
    juce::ColourGradient textFill(reactorui::isLightTheme() ? juce::Colour(0xccfffdf8) : juce::Colour(0xdd0c1015), textPlate.getTopLeft(),
                                  reactorui::isLightTheme() ? juce::Colour(0x99efe6d8) : juce::Colour(0xaa090c0f), textPlate.getBottomLeft(), false);
    g.setGradientFill(textFill);
    g.fillRoundedRectangle(textPlate, 5.0f);
    g.setColour(reactorui::isLightTheme() ? juce::Colour(0x55ffffff) : juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(textPlate, 5.0f, 1.0f);

    auto tubeRack = tubeRackArea.reduced(4.0f, 3.0f);
    juce::ColourGradient rackFill(reactorui::isLightTheme() ? juce::Colour(0xfffffdf8) : juce::Colour(0xff222528), tubeRack.getTopLeft(),
                                  reactorui::isLightTheme() ? juce::Colour(0xffece3d6) : juce::Colour(0xff0d1013), tubeRack.getBottomLeft(), false);
    g.setGradientFill(rackFill);
    g.fillRoundedRectangle(tubeRack, 5.0f);
    g.setColour(reactorui::isLightTheme() ? juce::Colour(0x55101216) : juce::Colour(0xff06080a));
    g.drawRoundedRectangle(tubeRack, 5.0f, 1.5f);
    g.setColour(reactorui::isLightTheme() ? juce::Colour(0x70ffffff) : juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(tubeRack.reduced(1.5f), 4.0f, 1.0f);

    auto tubeTitle = tubeRack.reduced(14.0f, 10.0f).removeFromTop(14.0f);
    g.setColour(reactorui::textMuted().withAlpha(0.82f));
    g.setFont(reactorui::bodyFont(11.5f));
    g.drawFittedText("THERMIONIC CORE", tubeTitle.getSmallestIntegerContainer(),
                     juce::Justification::centredRight, 1);

    auto lamp = tubeTitle.removeFromLeft(12.0f).reduced(1.0f);
    juce::ColourGradient lampFill(juce::Colour(0xffff8678), lamp.getTopLeft(),
                                  juce::Colour(0xff9b1208), lamp.getBottomLeft(), false);
    g.setGradientFill(lampFill);
    g.fillEllipse(lamp);
    g.setColour(juce::Colour(0xffffc5bd).withAlpha(0.55f));
    g.fillEllipse(lamp.reduced(2.4f));

    auto tubeArea = tubeRack.reduced(18.0f, 16.0f);
    tubeArea.removeFromTop(18.0f);
    const float gap = 14.0f;
    const float tubeWidth = (tubeArea.getWidth() - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto tubeBounds = tubeArea.removeFromLeft(tubeWidth);
        drawTube(g, tubeBounds, tubeGlow[(size_t) i], i);
        if (i < 3)
            tubeArea.removeFromLeft(gap);
    }
}

void HeaderSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    area.removeFromLeft(92);
    area.removeFromRight(438);
    auto textPlate = area.reduced(2, 3);
    titleLabel.setBounds(textPlate.removeFromTop(42));
}

void HeaderSectionComponent::timerCallback()
{
    for (size_t i = 0; i < tubePhases.size(); ++i)
    {
        tubePhases[i] += 0.12f + 0.035f * (float) i;
        const float flicker = 0.06f * (random.nextFloat() * 2.0f - 1.0f);
        tubeGlow[i] = juce::jlimit(0.32f, 1.0f, 0.58f + 0.18f * std::sin(tubePhases[i]) + flicker);
    }

    repaint();
}

void HeaderSectionComponent::drawTube(juce::Graphics& g, juce::Rectangle<float> area, float glow, int tubeIndex)
{
    auto base = area.removeFromBottom(14.0f);
    auto tube = area.reduced(2.0f, 2.0f);

    juce::ColourGradient baseFill(juce::Colour(0xff3a3d41), base.getTopLeft(),
                                  juce::Colour(0xff14181b), base.getBottomLeft(), false);
    g.setGradientFill(baseFill);
    g.fillRoundedRectangle(base, 3.0f);
    g.setColour(juce::Colour(0xff060809));
    g.drawRoundedRectangle(base, 3.0f, 1.2f);

    auto shadow = tube.translated(0.0f, 5.0f).reduced(10.0f, 20.0f);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillEllipse(shadow);

    juce::Colour filamentCore = juce::Colour(0xffffd277).interpolatedWith(juce::Colour(0xffff6932), 0.32f + 0.12f * (float) tubeIndex);
    juce::Colour glassGlow = filamentCore.withAlpha(0.14f + glow * 0.16f);
    g.setColour(glassGlow);
    g.fillRoundedRectangle(tube.reduced(8.0f, 10.0f), 18.0f);

    auto glass = tube.reduced(10.0f, 6.0f);
    juce::ColourGradient glassFill(juce::Colour(0x18ffffff), glass.getTopLeft(),
                                   juce::Colour(0x06ffffff), glass.getBottomLeft(), false);
    glassFill.addColour(0.55, juce::Colour(0x102f3946));
    glassFill.addColour(1.0, juce::Colour(0x0e050607));
    g.setGradientFill(glassFill);
    g.fillRoundedRectangle(glass, 18.0f);
    g.setColour(juce::Colour(0x22ffffff));
    g.drawRoundedRectangle(glass, 18.0f, 1.0f);

    auto plate = glass.reduced(glass.getWidth() * 0.24f, glass.getHeight() * 0.17f);
    auto leftPlate = plate.withWidth(plate.getWidth() * 0.34f);
    auto rightPlate = plate.withTrimmedLeft(plate.getWidth() * 0.66f);
    juce::ColourGradient plateFill(juce::Colour(0xff44484c), leftPlate.getTopLeft(),
                                   juce::Colour(0xff111315), leftPlate.getBottomLeft(), false);
    g.setGradientFill(plateFill);
    g.fillRoundedRectangle(leftPlate, 3.0f);
    g.fillRoundedRectangle(rightPlate, 3.0f);

    auto filament = juce::Rectangle<float>(glass.getCentreX() - glass.getWidth() * 0.16f,
                                           glass.getBottom() - glass.getHeight() * 0.28f,
                                           glass.getWidth() * 0.32f,
                                           6.0f);
    g.setColour(filamentCore.withAlpha(0.35f + glow * 0.50f));
    g.fillRoundedRectangle(filament.expanded(2.0f, 2.0f), 4.0f);
    g.setColour(filamentCore.withAlpha(0.95f));
    g.fillRoundedRectangle(filament, 3.0f);

    juce::Path rods;
    rods.startNewSubPath(filament.getX() + 2.0f, filament.getBottom());
    rods.lineTo(filament.getX() + 2.0f, glass.getBottom() - 4.0f);
    rods.startNewSubPath(filament.getRight() - 2.0f, filament.getBottom());
    rods.lineTo(filament.getRight() - 2.0f, glass.getBottom() - 4.0f);
    g.setColour(juce::Colour(0xff767b83));
    g.strokePath(rods, juce::PathStrokeType(1.2f));

    auto reflection = juce::Rectangle<float>(glass.getX() + glass.getWidth() * 0.16f,
                                             glass.getY() + glass.getHeight() * 0.12f,
                                             glass.getWidth() * 0.18f,
                                             glass.getHeight() * 0.60f);
    juce::ColourGradient reflect(juce::Colours::white.withAlpha(0.20f), reflection.getTopLeft(),
                                 juce::Colours::transparentWhite, reflection.getBottomLeft(), false);
    g.setGradientFill(reflect);
    g.fillRoundedRectangle(reflection, 6.0f);
}
