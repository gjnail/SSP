#include "VintageMeterComponent.h"

namespace
{
constexpr float grMeterFloorDb = 0.0f;
constexpr float grMeterCeilingDb = 20.0f;
constexpr float vuFloor = -20.0f;
constexpr float vuCeiling = 3.0f;
constexpr float vuStartDegrees = 162.0f;
constexpr float vuEndDegrees = 18.0f;

juce::Point<float> pointOnMeterArc(juce::Point<float> centre, float xRadius, float yRadius, float degrees)
{
    const auto radians = juce::degreesToRadians(degrees);
    return { centre.x + std::cos(radians) * xRadius,
             centre.y - std::sin(radians) * yRadius };
}

float mapValueToDegrees(float value, float minValue, float maxValue, float startDegrees, float endDegrees)
{
    return juce::jmap(juce::jlimit(minValue, maxValue, value),
                      minValue, maxValue,
                      startDegrees, endDegrees);
}

juce::Path makeArcPath(juce::Point<float> centre, float xRadius, float yRadius,
                       float startDegrees, float endDegrees, int segments = 64)
{
    juce::Path path;

    for (int i = 0; i <= segments; ++i)
    {
        const auto degrees = juce::jmap((float) i, 0.0f, (float) segments, startDegrees, endDegrees);
        const auto point = pointOnMeterArc(centre, xRadius, yRadius, degrees);

        if (i == 0)
            path.startNewSubPath(point);
        else
            path.lineTo(point);
    }

    return path;
}

float outputVuReferenceForMode(int mode)
{
    return mode == 1 ? -14.0f : -18.0f;
}
}

VintageMeterComponent::VintageMeterComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
}

void VintageMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const int meterMode = processor.getCurrentMeterModeIndex();
    const bool gainReductionMode = meterMode == 0;
    const bool offMode = meterMode == 3;

    g.setColour(juce::Colour(0xff0a0d11));
    g.fillRoundedRectangle(bounds, 24.0f);
    g.setColour(juce::Colour(0xff738292).withAlpha(0.9f));
    g.drawRoundedRectangle(bounds.reduced(0.8f), 24.0f, 1.6f);

    const auto bezelShadow = bounds.reduced(10.0f, 10.0f);
    g.setColour(juce::Colour(0xaa050608));
    g.fillRoundedRectangle(bezelShadow, 20.0f);

    auto bezel = bounds.reduced(18.0f, 18.0f);
    juce::ColourGradient bezelFill(juce::Colour(0xff1b2532), bezel.getCentreX(), bezel.getY(),
                                   juce::Colour(0xff070a0d), bezel.getCentreX(), bezel.getBottom(), false);
    bezelFill.addColour(0.25, juce::Colour(0xff121923));
    g.setGradientFill(bezelFill);
    g.fillRoundedRectangle(bezel, 18.0f);

    g.setColour(juce::Colour(0xff2a3544).withAlpha(0.9f));
    g.drawRoundedRectangle(bezel.reduced(1.0f), 18.0f, 1.2f);

    const auto paper = bezel.reduced(15.0f, 14.0f);
    juce::ColourGradient paperFill(juce::Colour(0xfff7edc9), paper.getCentreX(), paper.getY(),
                                   juce::Colour(0xffdfcb8d), paper.getCentreX(), paper.getBottom(), false);
    paperFill.addColour(0.35, juce::Colour(0xfff4e4b1));
    paperFill.addColour(0.72, juce::Colour(0xffebd59b));
    g.setGradientFill(paperFill);
    g.fillRoundedRectangle(paper, 12.0f);

    auto glass = paper.reduced(1.0f);
    juce::ColourGradient glassGlare(juce::Colour(0x42ffffff), glass.getCentreX(), glass.getY(),
                                    juce::Colour(0x00ffffff), glass.getCentreX(), glass.getCentreY(), false);
    g.setGradientFill(glassGlare);
    g.fillRoundedRectangle(glass, 12.0f);

    g.setColour(juce::Colour(0xff1b1b1b));
    g.setFont(juce::Font("Arial", 14.0f, juce::Font::bold));
    g.drawText(gainReductionMode ? "GAIN REDUCTION" : (offMode ? "METER" : "OUTPUT"),
               juce::Rectangle<int>((int) paper.getX(), (int) paper.getY() + 8, (int) paper.getWidth(), 18),
               juce::Justification::centred, false);

    g.setFont(juce::Font("Arial", 11.0f, juce::Font::bold));
    g.drawText(gainReductionMode ? "GR" : (meterMode == 1 ? "+8" : meterMode == 2 ? "+4" : "OFF"),
               juce::Rectangle<int>((int) paper.getRight() - 34, (int) paper.getY() + 10, 22, 16),
               juce::Justification::centred, false);
    g.drawText(gainReductionMode ? "dB" : "VU",
               juce::Rectangle<int>((int) paper.getCentreX() - 16, (int) paper.getY() + 30, 32, 14),
               juce::Justification::centred, false);

    const auto sharedPivot = juce::Point<float>(paper.getCentreX(), paper.getBottom() - 10.0f);
    const float sharedXRadius = paper.getWidth() * 0.41f;
    const float sharedYRadius = paper.getHeight() * 0.68f;

    juce::Point<float> pivot = sharedPivot;
    float xRadius = sharedXRadius;
    float yRadius = sharedYRadius;
    float needleDegrees = 0.0f;

    if (gainReductionMode)
    {
        const float zeroDegrees = mapValueToDegrees(0.0f, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees);
        const auto scalePath = makeArcPath(pivot, xRadius, yRadius, vuStartDegrees, vuEndDegrees, 80);
        const auto warmZonePath = makeArcPath(pivot, xRadius, yRadius,
                                              mapValueToDegrees(12.0f, grMeterFloorDb, grMeterCeilingDb, zeroDegrees, vuStartDegrees),
                                              mapValueToDegrees(20.0f, grMeterFloorDb, grMeterCeilingDb, zeroDegrees, vuStartDegrees),
                                              20);

        g.setColour(juce::Colour(0xff35312b).withAlpha(0.92f));
        g.strokePath(scalePath, juce::PathStrokeType(1.2f));

        g.setColour(juce::Colour(0x5ce56b2b));
        g.strokePath(warmZonePath,
                     juce::PathStrokeType(4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (int i = 0; i <= 23; ++i)
        {
            const float value = juce::jmap((float) i, 0.0f, 23.0f, grMeterFloorDb, grMeterCeilingDb);
            const float degrees = mapValueToDegrees(value, grMeterFloorDb, grMeterCeilingDb, vuStartDegrees, vuEndDegrees);
            const bool major = (i % 2 == 0);
            const bool warm = value >= 12;

            const auto outer = pointOnMeterArc(pivot, xRadius, yRadius, degrees);
            const auto inner = pointOnMeterArc(pivot,
                                               xRadius - (major ? 16.0f : 9.0f),
                                               yRadius - (major ? 16.0f : 9.0f),
                                               degrees);

            g.setColour(warm ? juce::Colour(0xffbf5e2a) : juce::Colour(0xff242424));
            g.drawLine({ inner, outer }, major ? 1.7f : 1.0f);
        }

        static constexpr std::array<int, 6> labels{{5, 7, 10, 12, 14, 20}};
        for (auto value : labels)
        {
            const float degrees = mapValueToDegrees((float) value, grMeterFloorDb, grMeterCeilingDb, zeroDegrees, vuStartDegrees);
            const auto labelPoint = pointOnMeterArc(pivot, xRadius - 38.0f, yRadius - 32.0f, degrees);
            const bool warm = value >= 12;

            g.setColour(warm ? juce::Colour(0xffc8672d) : juce::Colour(0xff222222));
            g.setFont(juce::Font("Arial", 10.5f, juce::Font::bold));
            g.drawFittedText(juce::String(value),
                             juce::Rectangle<int>((int) labelPoint.x - 14, (int) labelPoint.y - 8, 28, 16),
                             juce::Justification::centred, 1);
        }

        const auto zeroLabelPoint = pointOnMeterArc(pivot, xRadius - 38.0f, yRadius - 32.0f, zeroDegrees);
        g.setColour(juce::Colour(0xff222222));
        g.setFont(juce::Font("Arial", 10.5f, juce::Font::bold));
        g.drawFittedText("0",
                         juce::Rectangle<int>((int) zeroLabelPoint.x - 12, (int) zeroLabelPoint.y - 8, 24, 16),
                         juce::Justification::centred, 1);

        const float reduction = juce::jlimit(0.0f, 20.0f, processor.getGainReductionMeterDb());
        needleDegrees = mapValueToDegrees(reduction, grMeterFloorDb, grMeterCeilingDb, zeroDegrees, vuStartDegrees);
    }
    else
    {
        const auto scalePath = makeArcPath(pivot, xRadius, yRadius, vuStartDegrees, vuEndDegrees, 80);
        const auto redZonePath = makeArcPath(pivot, xRadius, yRadius,
                                             mapValueToDegrees(0.0f, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees),
                                             mapValueToDegrees(3.0f, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees),
                                             20);

        g.setColour(juce::Colour(0xff2c2924).withAlpha(offMode ? 0.45f : 0.92f));
        g.strokePath(scalePath, juce::PathStrokeType(1.1f));

        g.setColour(juce::Colour(0x88cf5b38).withAlpha(offMode ? 0.25f : 0.75f));
        g.strokePath(redZonePath,
                     juce::PathStrokeType(4.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        static constexpr std::array<float, 11> vuMarks{{-20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f}};
        for (int i = 0; i <= 23; ++i)
        {
            const float value = juce::jmap((float) i, 0.0f, 23.0f, vuFloor, vuCeiling);
            const float degrees = mapValueToDegrees(value, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees);
            const bool major = (i % 2 == 0);
            const bool warm = value >= 0.0f;

            const auto outer = pointOnMeterArc(pivot, xRadius, yRadius, degrees);
            const auto inner = pointOnMeterArc(pivot,
                                               xRadius - (major ? 16.0f : 9.0f),
                                               yRadius - (major ? 16.0f : 9.0f),
                                               degrees);
            g.setColour(warm ? juce::Colour(0xffb54a29).withAlpha(offMode ? 0.35f : 1.0f)
                             : juce::Colour(0xff272727).withAlpha(offMode ? 0.45f : 1.0f));
            g.drawLine({ inner, outer }, major ? 1.5f : 0.9f);
        }

        for (auto value : vuMarks)
        {
            const float degrees = mapValueToDegrees(value, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees);
            const auto labelPoint = pointOnMeterArc(pivot, xRadius - 36.0f, yRadius - 34.0f, degrees);
            const bool warm = value >= 0.0f;
            g.setColour(warm ? juce::Colour(0xffb24c2a).withAlpha(offMode ? 0.4f : 1.0f)
                             : juce::Colour(0xff222222).withAlpha(offMode ? 0.55f : 1.0f));
            g.setFont(juce::Font("Arial", 10.0f, juce::Font::plain));
            const juce::String text = value > 0.0f ? "+" + juce::String((int) value) : juce::String((int) value);
            g.drawFittedText(text,
                             juce::Rectangle<int>((int) labelPoint.x - 18, (int) labelPoint.y - 8, 36, 16),
                             juce::Justification::centred, 1);
        }

        const float referenceDb = outputVuReferenceForMode(meterMode);
        const float vuValue = offMode ? 0.0f : juce::jlimit(vuFloor, vuCeiling, processor.getOutputMeterDb() - referenceDb);
        needleDegrees = mapValueToDegrees(vuValue, vuFloor, vuCeiling, vuStartDegrees, vuEndDegrees);
    }

    const auto needleTip = pointOnMeterArc(pivot, xRadius - 30.0f, yRadius - 24.0f, needleDegrees);

    g.setColour(juce::Colour(0x30000000));
    g.drawLine({ { pivot.x + 2.0f, pivot.y + 2.0f }, { needleTip.x + 2.0f, needleTip.y + 2.0f } }, 3.2f);
    g.setColour(offMode ? juce::Colour(0x66ffc56a) : juce::Colour(0xffffab42));
    g.drawLine({ pivot, needleTip }, 2.4f);

    g.setColour(juce::Colour(0xaa1b1b1b));
    g.fillEllipse(pivot.x - 8.0f, pivot.y - 8.0f, 16.0f, 16.0f);
    g.setColour(juce::Colour(0xff555555));
    g.drawEllipse(pivot.x - 8.0f, pivot.y - 8.0f, 16.0f, 16.0f, 1.0f);
}

void VintageMeterComponent::timerCallback()
{
    repaint();
}
