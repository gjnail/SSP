#include "BandCrossoverEQComponent.h"
#include <cmath>

namespace
{
constexpr float displayMinHz = 20.0f;
constexpr float displayMaxHz = 20000.0f;

constexpr float lowMinHz = 50.0f;
constexpr float lowMaxHz = 2000.0f;
constexpr float highMinHz = 500.0f;
constexpr float highMaxHz = 12000.0f;

constexpr int hitPixels = 10;

void setParamHz(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float hz)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
        p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(hz));
}

float smoothStep(float edge0, float edge1, float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / juce::jmax(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

BandCrossoverEQComponent::BandCrossoverEQComponent(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    startTimerHz(30);
}

BandCrossoverEQComponent::~BandCrossoverEQComponent()
{
    stopTimer();
}

void BandCrossoverEQComponent::timerCallback()
{
    const float l = apvts.getRawParameterValue(lowParamId)->load();
    const float h = apvts.getRawParameterValue(highParamId)->load();
    if (std::abs(l - lastLowHz) > 0.05f || std::abs(h - lastHighHz) > 0.05f)
        repaint();
}

juce::Rectangle<float> BandCrossoverEQComponent::getPlotBounds() const
{
    auto r = getLocalBounds().toFloat().reduced(10.0f, 8.0f);
    r.removeFromTop(28.0f);
    r.removeFromBottom(24.0f);
    return r;
}

float BandCrossoverEQComponent::frequencyToX(float hz) const
{
    const auto plot = getPlotBounds();
    const float t = (std::log(juce::jmax(displayMinHz, hz)) - std::log(displayMinHz))
                    / (std::log(displayMaxHz) - std::log(displayMinHz));
    return plot.getX() + t * plot.getWidth();
}

float BandCrossoverEQComponent::xToFrequency(float x) const
{
    const auto plot = getPlotBounds();
    const float t = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
    return std::exp(std::log(displayMinHz) + t * (std::log(displayMaxHz) - std::log(displayMinHz)));
}

int BandCrossoverEQComponent::hitTestCrossover(int x) const
{
    const float lowHz = apvts.getRawParameterValue(lowParamId)->load();
    const float highHz = apvts.getRawParameterValue(highParamId)->load();
    const float x1 = frequencyToX(lowHz);
    const float x2 = frequencyToX(highHz);

    if (std::abs((float)x - x1) <= (float)hitPixels)
        return 1;
    if (std::abs((float)x - x2) <= (float)hitPixels)
        return 2;
    return 0;
}

void BandCrossoverEQComponent::applyCrossoverFromX(int x, int which)
{
    float hz = xToFrequency((float)x);

    if (which == 1)
    {
        hz = juce::jlimit(lowMinHz, lowMaxHz, hz);
        float curHigh = apvts.getRawParameterValue(highParamId)->load();
        constexpr float minGap = 1.12f;
        if (hz * minGap >= curHigh)
        {
            float nh = juce::jmin(highMaxHz, hz * minGap + 1.0f);
            nh = juce::jmax(nh, highMinHz);
            setParamHz(apvts, highParamId, nh);
        }
        setParamHz(apvts, lowParamId, hz);
    }
    else if (which == 2)
    {
        hz = juce::jlimit(highMinHz, highMaxHz, hz);
        float curLow = apvts.getRawParameterValue(lowParamId)->load();
        constexpr float minGap = 1.12f;
        if (hz <= curLow * minGap)
        {
            float nl = juce::jmax(lowMinHz, hz / minGap - 1.0f);
            nl = juce::jmin(nl, lowMaxHz);
            setParamHz(apvts, lowParamId, nl);
        }
        setParamHz(apvts, highParamId, hz);
    }
}

void BandCrossoverEQComponent::mouseDown(const juce::MouseEvent& e)
{
    dragWhich = hitTestCrossover(e.x);
    if (dragWhich == 0)
        dragWhich = (e.x < getWidth() / 2) ? 1 : 2;

    lastMouse = e.getPosition();
    applyCrossoverFromX(e.x, dragWhich);
    repaint();
}

void BandCrossoverEQComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragWhich != 0)
        applyCrossoverFromX(e.x, dragWhich);
    lastMouse = e.getPosition();
}

void BandCrossoverEQComponent::mouseUp(const juce::MouseEvent&)
{
    dragWhich = 0;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void BandCrossoverEQComponent::mouseMove(const juce::MouseEvent& e)
{
    const int h = hitTestCrossover(e.x);
    if (h != 0)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void BandCrossoverEQComponent::paint(juce::Graphics& g)
{
    const float lowHz = apvts.getRawParameterValue(lowParamId)->load();
    const float highHz = apvts.getRawParameterValue(highParamId)->load();
    lastLowHz = lowHz;
    lastHighHz = highHz;

    g.fillAll(juce::Colour(0xff12151a));
    g.setColour(juce::Colour(0xff2a3038));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

    const auto plot = getPlotBounds();
    const float xLow = frequencyToX(lowHz);
    const float xHigh = frequencyToX(highHz);
    const float lowNorm = (xLow - plot.getX()) / juce::jmax(1.0f, plot.getWidth());
    const float highNorm = (xHigh - plot.getX()) / juce::jmax(1.0f, plot.getWidth());

    juce::ColourGradient bg(juce::Colour(0xff171d25), plot.getTopLeft(),
                            juce::Colour(0xff0f1318), plot.getBottomRight(), false);
    bg.addColour(0.55, juce::Colour(0xff111923));
    g.setGradientFill(bg);
    g.fillRoundedRectangle(plot, 6.0f);

    g.setColour(juce::Colour(0xff202833));
    for (float f : {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f})
    {
        const float lx = frequencyToX(f);
        g.drawVerticalLine(juce::roundToInt(lx), plot.getY(), plot.getBottom());
    }

    for (int i = 1; i < 4; ++i)
    {
        const float y = plot.getY() + plot.getHeight() * (float)i / 4.0f;
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }

    juce::Path lowFill;
    juce::Path midFill;
    juce::Path highFill;
    juce::Path lowStroke;
    juce::Path midStroke;
    juce::Path highStroke;

    auto buildBandY = [&](float t, int band)
    {
        const float lowBody = 1.0f - smoothStep(lowNorm - 0.10f, lowNorm + 0.08f, t);
        const float highBody = smoothStep(highNorm - 0.08f, highNorm + 0.10f, t);
        const float midRise = smoothStep(lowNorm - 0.08f, lowNorm + 0.08f, t);
        const float midFall = 1.0f - smoothStep(highNorm - 0.08f, highNorm + 0.08f, t);
        const float midBody = juce::jlimit(0.0f, 1.0f, midRise * midFall);

        const float lowHump = 0.20f * std::exp(-std::pow((t - lowNorm) / 0.055f, 2.0f));
        const float highHump = 0.20f * std::exp(-std::pow((t - highNorm) / 0.055f, 2.0f));

        float magnitude = 0.0f;
        if (band == 0)
            magnitude = juce::jlimit(0.0f, 1.0f, 0.10f + 0.72f * lowBody + lowHump);
        else if (band == 1)
            magnitude = juce::jlimit(0.0f, 1.0f, 0.08f + 0.64f * midBody + lowHump * 0.45f + highHump * 0.45f);
        else
            magnitude = juce::jlimit(0.0f, 1.0f, 0.10f + 0.72f * highBody + highHump);

        return plot.getBottom() - magnitude * plot.getHeight() * 0.86f;
    };

    auto appendBandShape = [&](juce::Path& fillPath, juce::Path& strokePath, int band)
    {
        constexpr int steps = 180;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float)i / (float)steps;
            const float x = plot.getX() + plot.getWidth() * t;
            const float y = buildBandY(t, band);

            if (i == 0)
            {
                fillPath.startNewSubPath(x, plot.getBottom());
                fillPath.lineTo(x, y);
                strokePath.startNewSubPath(x, y);
            }
            else
            {
                fillPath.lineTo(x, y);
                strokePath.lineTo(x, y);
            }
        }

        fillPath.lineTo(plot.getRight(), plot.getBottom());
        fillPath.closeSubPath();
    };

    appendBandShape(lowFill, lowStroke, 0);
    appendBandShape(midFill, midStroke, 1);
    appendBandShape(highFill, highStroke, 2);

    g.setColour(juce::Colour(0x402b74b9));
    g.fillPath(lowFill);
    g.setColour(juce::Colour(0x4033a07a));
    g.fillPath(midFill);
    g.setColour(juce::Colour(0x40d6853a));
    g.fillPath(highFill);

    g.setColour(juce::Colour(0xff69bbff));
    g.strokePath(lowStroke, juce::PathStrokeType(2.0f));
    g.setColour(juce::Colour(0xff6ce0b0));
    g.strokePath(midStroke, juce::PathStrokeType(2.0f));
    g.setColour(juce::Colour(0xffffb35a));
    g.strokePath(highStroke, juce::PathStrokeType(2.0f));

    auto drawBandTitle = [&](float cx1, float cx2, const juce::String& txt)
    {
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(txt, juce::Rectangle<float>(cx1, plot.getY() + 8.0f, cx2 - cx1, 20.0f), juce::Justification::centred);
    };

    drawBandTitle(plot.getX(), xLow, "LOW");
    drawBandTitle(xLow, xHigh, "MID");
    drawBandTitle(xHigh, plot.getRight(), "HIGH");

    auto drawCrossoverLine = [&](float x, juce::Colour lineCol, const juce::String& tag, const juce::String& hzText)
    {
        const int xi = juce::roundToInt(x);
        g.setColour(lineCol.withAlpha(0.95f));
        g.drawVerticalLine(xi, plot.getY(), plot.getBottom());
        g.setColour(lineCol.withAlpha(0.18f));
        g.fillRect(x - 9.0f, plot.getY(), 18.0f, plot.getHeight());
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.setFont(11.0f);
        g.drawText(tag + "  " + hzText, juce::Rectangle<float>(x - 60.0f, plot.getY() + 4.0f, 120.0f, 16.0f), juce::Justification::centred);
    };

    drawCrossoverLine(xLow, juce::Colour(0xff44aaff), "Low", juce::String(juce::roundToInt(lowHz)) + " Hz");
    drawCrossoverLine(xHigh, juce::Colour(0xffffaa44), "High", juce::String(juce::roundToInt(highHz)) + " Hz");

    g.setColour(juce::Colour(0xffaab4c0));
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("Band split response", 10, 6, 220, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff7b8793));
    g.setFont(11.0f);
    g.drawText("Drag the crossover lines to move the filter handoff between bands.", 220, 7, getWidth() - 230, 18,
               juce::Justification::centredRight);

    g.setColour(juce::Colour(0xff8899aa));
    g.setFont(10.0f);
    const juce::StringArray ticks = {"20 Hz", "100 Hz", "1 kHz", "10 kHz", "20 kHz"};
    const float freqs[] = {20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f};
    for (int i = 0; i < ticks.size(); ++i)
    {
        const float fx = frequencyToX(freqs[i]);
        g.drawText(ticks[i], juce::Rectangle<float>(fx - 28.0f, (float)getHeight() - 22.0f, 56.0f, 18.0f), juce::Justification::centred);
        g.drawVerticalLine(juce::roundToInt(fx), plot.getBottom(), plot.getBottom() + 4.0f);
    }
}

void BandCrossoverEQComponent::resized() {}
