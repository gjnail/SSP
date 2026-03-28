#include "EQ3GraphComponent.h"

namespace
{
constexpr double minFrequency = 20.0;
constexpr double maxFrequency = 20000.0;
constexpr float graphMinDb = PluginProcessor::minGainDb;
constexpr float graphMaxDb = PluginProcessor::maxGainDb;
constexpr double zeroTolerance = 1.0e-5;

double xToFrequency(float x, juce::Rectangle<float> plot)
{
    const auto proportion = juce::jlimit(0.0, 1.0, (double) ((x - plot.getX()) / plot.getWidth()));
    return std::exp(std::log(minFrequency) + proportion * (std::log(maxFrequency) - std::log(minFrequency)));
}

void addFilledSegment(juce::Path& destination, juce::Point<float> previous, juce::Point<float> current, float zeroY)
{
    destination.startNewSubPath(previous.x, zeroY);
    destination.lineTo(previous);
    destination.lineTo(current);
    destination.lineTo(current.x, zeroY);
    destination.closeSubPath();
}
}

EQ3GraphComponent::EQ3GraphComponent(PluginProcessor& p)
    : processor(p)
{
    startTimerHz(30);
}

EQ3GraphComponent::~EQ3GraphComponent()
{
    stopTimer();
}

void EQ3GraphComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<float> EQ3GraphComponent::getPlotBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    auto content = bounds.reduced(14.0f, 12.0f);
    content.removeFromTop(6.0f);
    content.removeFromBottom(20.0f);
    content.removeFromLeft(8.0f);
    content.removeFromRight(8.0f);
    return content;
}

float EQ3GraphComponent::frequencyToX(double frequency, juce::Rectangle<float> plot) const
{
    const auto logMin = std::log(minFrequency);
    const auto logMax = std::log(maxFrequency);
    const auto proportion = (std::log(juce::jlimit(minFrequency, maxFrequency, frequency)) - logMin) / (logMax - logMin);
    return plot.getX() + plot.getWidth() * (float) proportion;
}

float EQ3GraphComponent::responseToY(double responseDb, juce::Rectangle<float> plot) const
{
    const auto clamped = juce::jlimit((double) graphMinDb, (double) graphMaxDb, responseDb);
    const auto proportion = (clamped - graphMaxDb) / (graphMinDb - graphMaxDb);
    return plot.getY() + plot.getHeight() * (float) proportion;
}

void EQ3GraphComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    eq3ui::drawPanelBackground(g, bounds, eq3ui::brandTeal(), 14.0f);

    const auto plot = getPlotBounds();
    const auto zeroY = responseToY(0.0, plot);

    const std::array<double, 10> freqLines { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    const std::array<float, 5> gainLines { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };

    g.setColour(juce::Colours::white.withAlpha(0.035f));
    for (auto gain : gainLines)
    {
        const auto y = responseToY(gain, plot);
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }

    g.setColour(eq3ui::outlineSoft().withAlpha(0.55f));
    for (auto frequency : freqLines)
    {
        const auto x = frequencyToX(frequency, plot);
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }

    g.setColour(eq3ui::brandTeal().withAlpha(0.42f));
    g.drawHorizontalLine((int) zeroY, plot.getX(), plot.getRight());

    juce::Path curve;
    juce::Path boostFill;
    juce::Path cutFill;
    juce::Point<float> previousPoint;
    double previousResponse = 0.0;
    bool started = false;

    const int steps = juce::jmax(120, (int) plot.getWidth());

    for (int index = 0; index <= steps; ++index)
    {
        const auto x = juce::jmap((float) index, 0.0f, (float) steps, plot.getX(), plot.getRight());
        const auto frequency = xToFrequency(x, plot);
        const auto responseDb = processor.getResponseForFrequency(frequency);
        const juce::Point<float> currentPoint(x, responseToY(responseDb, plot));

        if (!started)
        {
            curve.startNewSubPath(currentPoint);
            previousPoint = currentPoint;
            previousResponse = responseDb;
            started = true;
            continue;
        }

        curve.lineTo(currentPoint);

        const auto previousSign = std::abs(previousResponse) <= zeroTolerance ? 0 : (previousResponse > 0.0 ? 1 : -1);
        const auto currentSign = std::abs(responseDb) <= zeroTolerance ? 0 : (responseDb > 0.0 ? 1 : -1);

        if (previousSign >= 0 && currentSign >= 0)
        {
            addFilledSegment(boostFill, previousPoint, currentPoint, zeroY);
        }
        else if (previousSign <= 0 && currentSign <= 0)
        {
            addFilledSegment(cutFill, previousPoint, currentPoint, zeroY);
        }
        else
        {
            const auto proportion = (float) (previousResponse / (previousResponse - responseDb));
            const auto crossing = previousPoint + (currentPoint - previousPoint) * proportion;

            if (previousResponse > 0.0)
            {
                addFilledSegment(boostFill, previousPoint, crossing, zeroY);
                addFilledSegment(cutFill, crossing, currentPoint, zeroY);
            }
            else
            {
                addFilledSegment(cutFill, previousPoint, crossing, zeroY);
                addFilledSegment(boostFill, crossing, currentPoint, zeroY);
            }
        }

        previousPoint = currentPoint;
        previousResponse = responseDb;
    }

    juce::ColourGradient boostGradient(eq3ui::brandTeal().withAlpha(0.22f), 0.0f, plot.getY(),
                                       eq3ui::brandTeal().withAlpha(0.03f), 0.0f, zeroY, false);
    g.setGradientFill(boostGradient);
    g.fillPath(boostFill);

    juce::ColourGradient cutGradient(eq3ui::brandTeal().withAlpha(0.15f), 0.0f, zeroY,
                                     eq3ui::brandTeal().withAlpha(0.03f), 0.0f, plot.getBottom(), false);
    g.setGradientFill(cutGradient);
    g.fillPath(cutFill);

    g.setColour(eq3ui::brandTeal());
    g.strokePath(curve, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(eq3ui::textMuted());
    g.setFont(eq3ui::bodyFont(9.5f));

    auto footer = bounds.reduced(16.0f, 0.0f).removeFromBottom(18.0f);
    g.drawText("20 Hz", footer.removeFromLeft(48.0f).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("20 kHz", footer.removeFromRight(56.0f).toNearestInt(), juce::Justification::centredRight, false);
    g.drawText("1 kHz", juce::Rectangle<float>(frequencyToX(1000.0, plot) - 20.0f, footer.getY(), 40.0f, footer.getHeight()).toNearestInt(),
               juce::Justification::centred, false);

    auto rightScale = juce::Rectangle<float>(plot.getRight() - 40.0f, plot.getY() + 4.0f, 40.0f, plot.getHeight() - 8.0f);
    g.drawText("+12", juce::Rectangle<float>(rightScale.getX(), responseToY(12.0, plot) - 8.0f, rightScale.getWidth(), 16.0f).toNearestInt(),
               juce::Justification::centredRight, false);
    g.drawText("0", juce::Rectangle<float>(rightScale.getX(), zeroY - 8.0f, rightScale.getWidth(), 16.0f).toNearestInt(),
               juce::Justification::centredRight, false);
    g.drawText("-12", juce::Rectangle<float>(rightScale.getX(), responseToY(-12.0, plot) - 8.0f, rightScale.getWidth(), 16.0f).toNearestInt(),
               juce::Justification::centredRight, false);
}
