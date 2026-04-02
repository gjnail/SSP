#include "WaveshaperControlsComponent.h"
#include "WaveshaperTables.h"

namespace
{
juce::Colour panelOutline() { return juce::Colour(0xff314655); }
juce::Colour shellText() { return juce::Colour(0xfff5fbff); }
juce::Colour subtleText() { return juce::Colour(0xffb8d0dc); }
juce::Colour cyan() { return juce::Colour(0xff5ae6ff); }
juce::Colour mint() { return juce::Colour(0xff9ffccf); }
juce::Colour coral() { return juce::Colour(0xffff8766); }

juce::String formatDrive(double value)
{
    return "+" + juce::String(value, 1) + " dB";
}

juce::String formatFrame(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatPhase(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 0) + " deg";
}

juce::String formatSignedPercent(double value)
{
    const auto rounded = juce::roundToInt((float) value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatDecibels(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}
} // namespace

class WaveshaperKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit WaveshaperKnobLookAndFeel(bool heroStyle)
        : hero(heroStyle)
    {
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(hero ? 10.0f : 12.0f);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto ringBounds = bounds.reduced(radius * 0.07f);

        g.setColour(juce::Colours::black.withAlpha(hero ? 0.30f : 0.18f));
        g.fillEllipse(bounds.translated(0.0f, hero ? 10.0f : 7.0f));

        juce::ColourGradient shell(juce::Colour(0xff16202a), centre.x, bounds.getY(),
                                   juce::Colour(0xff0b1218), centre.x, bounds.getBottom(), false);
        shell.addColour(0.42, juce::Colour(0xff1b2b35));
        g.setGradientFill(shell);
        g.fillEllipse(bounds);

        juce::Path baseArc;
        baseArc.addCentredArc(centre.x,
                              centre.y,
                              ringBounds.getWidth() * 0.5f,
                              ringBounds.getHeight() * 0.5f,
                              0.0f,
                              rotaryStartAngle,
                              rotaryEndAngle,
                              true);
        g.setColour(juce::Colour(0xff091117));
        g.strokePath(baseArc, juce::PathStrokeType(hero ? 12.0f : 9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x,
                               centre.y,
                               ringBounds.getWidth() * 0.5f,
                               ringBounds.getHeight() * 0.5f,
                               0.0f,
                               rotaryStartAngle,
                               angle,
                               true);
        juce::ColourGradient accent(cyan(), ringBounds.getBottomLeft(), coral(), ringBounds.getTopRight(), false);
        g.setGradientFill(accent);
        g.strokePath(valueArc, juce::PathStrokeType(hero ? 12.0f : 9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = bounds.reduced(radius * 0.31f);
        juce::ColourGradient capFill(juce::Colour(0xff1d2a34), cap.getCentreX(), cap.getY(),
                                     juce::Colour(0xff10181f), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);

        juce::Path pointer;
        const auto pointerLength = radius * (hero ? 0.55f : 0.48f);
        const auto pointerWidth = hero ? 8.0f : 6.0f;
        pointer.addRoundedRectangle(-pointerWidth * 0.5f, -pointerLength, pointerWidth, pointerLength, pointerWidth * 0.5f);
        g.setColour(shellText());
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff091117));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.28f, radius * 0.28f).withCentre(centre));
    }

private:
    bool hero = false;
};

class WaveshaperControlsComponent::WaveshaperKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    WaveshaperKnob(juce::AudioProcessorValueTreeState& state,
                   const juce::String& paramId,
                   const juce::String& heading,
                   const juce::String& caption,
                   Formatter formatterToUse,
                   bool heroStyle)
        : lookAndFeel(heroStyle),
          attachment(state, paramId, slider),
          formatter(std::move(formatterToUse)),
          hero(heroStyle)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(valueLabel);
        addAndMakeVisible(captionLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(heading, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(hero ? 29.0f : 18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, shellText());

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(hero ? 20.0f : 14.5f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, cyan());

        captionLabel.setText(caption, juce::dontSendNotification);
        captionLabel.setJustificationType(juce::Justification::centred);
        captionLabel.setFont(juce::Font(11.0f));
        captionLabel.setColour(juce::Label::textColourId, subtleText());

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lookAndFeel);
        slider.onValueChange = [this] { refreshValueText(); };

        refreshValueText();
    }

    ~WaveshaperKnob() override
    {
        slider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(hero ? juce::Colour(0xff17242d) : juce::Colour(0xff14212a),
                                  area.getTopLeft(),
                                  juce::Colour(0xff0b1218),
                                  area.getBottomRight(),
                                  false);
        fill.addColour(0.46, juce::Colour(0xff223440));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, hero ? 28.0f : 20.0f);

        auto accent = area.reduced(hero ? 24.0f : 18.0f, hero ? 16.0f : 14.0f).removeFromTop(5.0f);
        juce::ColourGradient accentFill(cyan().withAlpha(0.55f), accent.getX(), accent.getCentreY(),
                                        coral().withAlpha(0.70f), accent.getRight(), accent.getCentreY(), false);
        g.setGradientFill(accentFill);
        g.fillRoundedRectangle(accent.withTrimmedLeft(accent.getWidth() * 0.2f).withTrimmedRight(accent.getWidth() * 0.2f), 3.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(area.reduced(0.5f), hero ? 28.0f : 20.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(hero ? 16 : 12);

        titleLabel.setBounds(area.removeFromTop(hero ? 32 : 22));
        valueLabel.setBounds(area.removeFromBottom(hero ? 24 : 20));
        captionLabel.setBounds(area.removeFromBottom(hero ? 24 : 18));
        slider.setBounds(area.reduced(hero ? 0 : 2, hero ? 0 : 2));
    }

private:
    void refreshValueText()
    {
        valueLabel.setText(formatter(slider.getValue()), juce::dontSendNotification);
    }

    WaveshaperKnobLookAndFeel lookAndFeel;
    juce::Slider slider;
    juce::Label titleLabel;
    juce::Label valueLabel;
    juce::Label captionLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter formatter;
    bool hero = false;
};

class WaveshaperControlsComponent::MeterBridge final : public juce::Component
{
public:
    explicit MeterBridge(PluginProcessor& p)
        : processor(p)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff13202a), bounds.getTopLeft(),
                                  juce::Colour(0xff091118), bounds.getBottomRight(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 20.0f);

        const auto meterNames = juce::StringArray{"Input", "Depth", "Output"};
        const std::array<float, 3> values{processor.getInputMeterLevel(),
                                          processor.getDepthMeterLevel(),
                                          processor.getOutputMeterLevel()};
        const std::array<juce::Colour, 3> colours{mint(), coral(), cyan()};

        auto laneArea = bounds.reduced(16.0f, 14.0f);
        const auto laneHeight = laneArea.getHeight() / 3.0f;

        for (int i = 0; i < 3; ++i)
        {
            auto lane = laneArea.removeFromTop(laneHeight);
            auto labelArea = lane.removeFromLeft(56.0f);
            auto barArea = lane.reduced(0.0f, 8.0f);

            g.setColour(subtleText());
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText(meterNames[i], labelArea.toNearestInt(), juce::Justification::centredLeft, false);

            g.setColour(juce::Colour(0xff071017));
            g.fillRoundedRectangle(barArea, 7.0f);

            auto fillWidth = juce::jmax(10.0f, barArea.getWidth() * juce::jlimit(0.0f, 1.0f, values[(size_t) i]));
            auto fillArea = barArea.withWidth(fillWidth);
            juce::ColourGradient barFill(colours[(size_t) i], fillArea.getX(), fillArea.getCentreY(),
                                         juce::Colours::white.withAlpha(0.45f), fillArea.getRight(), fillArea.getCentreY(), false);
            g.setGradientFill(barFill);
            g.fillRoundedRectangle(fillArea, 7.0f);
        }

        g.setColour(panelOutline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 20.0f, 1.0f);
    }

private:
    PluginProcessor& processor;
};

class WaveshaperControlsComponent::TableStackDisplay final : public juce::Component
{
public:
    explicit TableStackDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ssp::waveshaper;

        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff101923), bounds.getTopLeft(),
                                  juce::Colour(0xff0a1118), bounds.getBottomRight(), false);
        fill.addColour(0.52, juce::Colour(0xff172733));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 26.0f);

        auto plot = bounds.reduced(22.0f, 20.0f);
        auto titleArea = plot.removeFromTop(22.0f);
        auto graphArea = plot.reduced(4.0f, 6.0f);

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("3D TABLE", titleArea.toNearestInt(), juce::Justification::centredLeft, false);

        const auto tableIndex = juce::jlimit(0, getTableNames().size() - 1, juce::roundToInt(apvts.getRawParameterValue("table")->load()));
        const auto smooth = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("smooth")->load() / 100.0f);
        const auto activeFrame = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("frame")->load() / 100.0f) * (float) (frameCount - 1);
        const auto overflowMode = static_cast<OverflowMode>(juce::jlimit(0,
                                                                         getOverflowModeNames().size() - 1,
                                                                         juce::roundToInt(apvts.getRawParameterValue("overflow")->load())));

        const auto baseWidth = graphArea.getWidth() * 0.78f;
        const auto baseHeight = graphArea.getHeight() * 0.62f;

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const auto depth = (float) (frameCount - 1 - frame);
            const auto depthT = depth / (float) (frameCount - 1);
            const auto width = juce::jmap(depthT, baseWidth * 0.72f, baseWidth);
            const auto height = juce::jmap(depthT, baseHeight * 0.55f, baseHeight);
            const auto frameArea = juce::Rectangle<float>(graphArea.getX() + depth * 16.0f,
                                                          graphArea.getCentreY() - height * 0.5f - depth * 10.0f,
                                                          width,
                                                          height);

            auto curve = juce::Path();
            for (int point = 0; point <= 160; ++point)
            {
                const auto x = juce::jmap((float) point, 0.0f, 160.0f, -1.0f, 1.0f);
                const auto y = sampleTable(tableIndex,
                                           (float) frame / (float) (frameCount - 1),
                                           x,
                                           smooth,
                                           overflowMode);
                const auto px = juce::jmap(x, -1.0f, 1.0f, frameArea.getX(), frameArea.getRight());
                const auto py = juce::jmap(y, 1.2f, -1.2f, frameArea.getY(), frameArea.getBottom());

                if (point == 0)
                    curve.startNewSubPath(px, py);
                else
                    curve.lineTo(px, py);
            }

            const auto selectedDistance = std::abs(activeFrame - (float) frame);
            const auto selectedAlpha = juce::jlimit(0.18f, 1.0f, 1.0f - selectedDistance * 0.24f);
            const auto colour = cyan().interpolatedWith(coral(), (float) frame / (float) (frameCount - 1)).withAlpha(selectedAlpha);

            g.setColour(colour);
            g.strokePath(curve,
                         juce::PathStrokeType(selectedDistance < 0.6f ? 3.0f : 1.6f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

            g.setColour(subtleText().withAlpha(0.18f));
            g.drawRoundedRectangle(frameArea, 16.0f, 1.0f);
        }

        const auto cursorX = graphArea.getX() + graphArea.getWidth() * juce::jlimit(0.0f, 1.0f, activeFrame / (float) (frameCount - 1));
        g.setColour(mint().withAlpha(0.7f));
        g.drawLine(cursorX, graphArea.getY() + 8.0f, cursorX, graphArea.getBottom() - 8.0f, 2.0f);

        g.setColour(panelOutline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 26.0f, 1.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class WaveshaperControlsComponent::CurveDisplay final : public juce::Component
{
public:
    explicit CurveDisplay(juce::AudioProcessorValueTreeState& state)
        : apvts(state)
    {
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ssp::waveshaper;

        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient fill(juce::Colour(0xff111b24), bounds.getTopLeft(),
                                  juce::Colour(0xff0a1118), bounds.getBottomRight(), false);
        fill.addColour(0.48, juce::Colour(0xff162530));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 26.0f);

        auto plot = bounds.reduced(22.0f, 18.0f);
        auto titleArea = plot.removeFromTop(42.0f);
        auto graphArea = plot.reduced(4.0f, 4.0f);

        const auto tableIndex = juce::jlimit(0, getTableNames().size() - 1, juce::roundToInt(apvts.getRawParameterValue("table")->load()));
        const auto framePosition = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("frame")->load() / 100.0f);
        const auto driveGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("drive")->load());
        const auto bias = apvts.getRawParameterValue("bias")->load() / 100.0f;
        const auto phaseOffset = apvts.getRawParameterValue("phase")->load() / 180.0f;
        const auto smooth = apvts.getRawParameterValue("smooth")->load() / 100.0f;
        const auto overflowMode = static_cast<OverflowMode>(juce::jlimit(0,
                                                                         getOverflowModeNames().size() - 1,
                                                                         juce::roundToInt(apvts.getRawParameterValue("overflow")->load())));
        const auto dcOn = apvts.getRawParameterValue("dcfilter")->load() >= 0.5f;

        g.setColour(subtleText());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("ACTIVE FRAME", titleArea.removeFromTop(18.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setColour(shellText());
        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.drawText(getTableNames()[tableIndex], titleArea.removeFromTop(22.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setColour(subtleText());
        g.setFont(juce::Font(12.0f));
        g.drawText("Frame " + juce::String(juce::roundToInt(framePosition * (float) (frameCount - 1)) + 1)
                       + "  |  " + getOverflowModeNames()[(int) overflowMode]
                       + "  |  " + (dcOn ? "DC Filter On" : "DC Filter Off"),
                   titleArea.toNearestInt(),
                   juce::Justification::centredLeft,
                   false);

        const auto drawX = [&](float x) { return juce::jmap(x, -1.2f, 1.2f, graphArea.getX(), graphArea.getRight()); };
        const auto drawY = [&](float y) { return juce::jmap(y, 1.25f, -1.25f, graphArea.getY(), graphArea.getBottom()); };

        g.setColour(juce::Colour(0x22ffffff));
        for (int i = 0; i < 5; ++i)
        {
            const auto t = juce::jmap((float) i, 0.0f, 4.0f, -1.0f, 1.0f);
            g.drawHorizontalLine((int) std::round(drawY(t)), graphArea.getX(), graphArea.getRight());
            g.drawVerticalLine((int) std::round(drawX(t)), graphArea.getY(), graphArea.getBottom());
        }

        juce::Path dryPath;
        dryPath.startNewSubPath(drawX(-1.0f), drawY(-1.0f));
        dryPath.lineTo(drawX(1.0f), drawY(1.0f));
        g.setColour(subtleText().withAlpha(0.45f));
        g.strokePath(dryPath, juce::PathStrokeType(1.7f));

        juce::Path curve;
        for (int i = 0; i <= 180; ++i)
        {
            const auto x = juce::jmap((float) i, 0.0f, 180.0f, -1.2f, 1.2f);
            const auto lookup = x * driveGain + bias * 0.55f + phaseOffset;
            const auto y = sampleTable(tableIndex, framePosition, lookup, smooth, overflowMode);
            const auto px = drawX(x);
            const auto py = drawY(y);

            if (i == 0)
                curve.startNewSubPath(px, py);
            else
                curve.lineTo(px, py);
        }

        juce::Path fillPath(curve);
        fillPath.lineTo(drawX(1.2f), drawY(-1.25f));
        fillPath.lineTo(drawX(-1.2f), drawY(-1.25f));
        fillPath.closeSubPath();

        juce::ColourGradient areaFill(cyan().withAlpha(0.24f), graphArea.getX(), graphArea.getY(),
                                      coral().withAlpha(0.10f), graphArea.getRight(), graphArea.getBottom(), false);
        g.setGradientFill(areaFill);
        g.fillPath(fillPath);

        g.setColour(mint());
        g.strokePath(curve, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(panelOutline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 26.0f, 1.0f);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

WaveshaperControlsComponent::WaveshaperControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    badgeLabel.setText("3D MORPHING DISTORTION", juce::dontSendNotification);
    badgeLabel.setJustificationType(juce::Justification::centredLeft);
    badgeLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    badgeLabel.setColour(juce::Label::textColourId, cyan());
    addAndMakeVisible(badgeLabel);

    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setFont(juce::Font(13.0f));
    summaryLabel.setColour(juce::Label::textColourId, subtleText());
    addAndMakeVisible(summaryLabel);

    tableLabel.setText("Table", juce::dontSendNotification);
    tableLabel.setJustificationType(juce::Justification::centredLeft);
    tableLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    tableLabel.setColour(juce::Label::textColourId, subtleText());
    addAndMakeVisible(tableLabel);

    overflowLabel.setText("Overflow", juce::dontSendNotification);
    overflowLabel.setJustificationType(juce::Justification::centredLeft);
    overflowLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    overflowLabel.setColour(juce::Label::textColourId, subtleText());
    addAndMakeVisible(overflowLabel);

    tableBox.setJustificationType(juce::Justification::centredLeft);
    tableBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d151b));
    tableBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    tableBox.setColour(juce::ComboBox::textColourId, shellText());
    tableBox.addItemList(ssp::waveshaper::getTableNames(), 1);
    addAndMakeVisible(tableBox);

    overflowBox.setJustificationType(juce::Justification::centredLeft);
    overflowBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d151b));
    overflowBox.setColour(juce::ComboBox::outlineColourId, panelOutline());
    overflowBox.setColour(juce::ComboBox::textColourId, shellText());
    overflowBox.addItemList(ssp::waveshaper::getOverflowModeNames(), 1);
    addAndMakeVisible(overflowBox);

    dcFilterButton.setButtonText("DC Filter");
    dcFilterButton.setColour(juce::ToggleButton::textColourId, shellText());
    addAndMakeVisible(dcFilterButton);

    tableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "table", tableBox);
    overflowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "overflow", overflowBox);
    dcFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "dcfilter", dcFilterButton);

    tableDisplay = std::make_unique<TableStackDisplay>(apvts);
    curveDisplay = std::make_unique<CurveDisplay>(apvts);
    meterBridge = std::make_unique<MeterBridge>(processor);
    driveKnob = std::make_unique<WaveshaperKnob>(apvts, "drive", "Drive", "Input gain into the shaper", formatDrive, true);
    frameKnob = std::make_unique<WaveshaperKnob>(apvts, "frame", "Frame", "Morph position across the table", formatFrame, false);
    phaseKnob = std::make_unique<WaveshaperKnob>(apvts, "phase", "Phase", "Horizontal rotation of the curve", formatPhase, false);
    biasKnob = std::make_unique<WaveshaperKnob>(apvts, "bias", "Bias", "Offset the signal before shaping", formatSignedPercent, false);
    smoothKnob = std::make_unique<WaveshaperKnob>(apvts, "smooth", "Smooth", "Blur the contour for softer distortion", formatPercent, false);
    outputKnob = std::make_unique<WaveshaperKnob>(apvts, "output", "Output", "Level after the waveshaper", formatDecibels, false);
    mixKnob = std::make_unique<WaveshaperKnob>(apvts, "mix", "Mix", "Dry and wet balance", formatPercent, false);

    addAndMakeVisible(*tableDisplay);
    addAndMakeVisible(*curveDisplay);
    addAndMakeVisible(*meterBridge);
    addAndMakeVisible(*driveKnob);
    addAndMakeVisible(*frameKnob);
    addAndMakeVisible(*phaseKnob);
    addAndMakeVisible(*biasKnob);
    addAndMakeVisible(*smoothKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(*mixKnob);

    refreshSummary();
    startTimerHz(30);
}

WaveshaperControlsComponent::~WaveshaperControlsComponent() = default;

void WaveshaperControlsComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient fill(juce::Colour(0x2a0e1a22), bounds.getTopLeft(),
                              juce::Colour(0x12122635), bounds.getBottomRight(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 28.0f);

    g.setColour(panelOutline());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 28.0f, 1.0f);
}

void WaveshaperControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 18);

    auto topRow = area.removeFromTop(78);
    auto textArea = topRow.removeFromLeft(topRow.proportionOfWidth(0.45f));
    badgeLabel.setBounds(textArea.removeFromTop(20));
    summaryLabel.setBounds(textArea);

    auto selectorArea = topRow.reduced(0, 6);
    auto tableArea = selectorArea.removeFromLeft(selectorArea.proportionOfWidth(0.38f));
    tableLabel.setBounds(tableArea.removeFromTop(16));
    tableBox.setBounds(tableArea.removeFromTop(34));

    selectorArea.removeFromLeft(12);
    auto overflowArea = selectorArea.removeFromLeft(selectorArea.proportionOfWidth(0.42f));
    overflowLabel.setBounds(overflowArea.removeFromTop(16));
    overflowBox.setBounds(overflowArea.removeFromTop(34));

    selectorArea.removeFromLeft(12);
    dcFilterButton.setBounds(selectorArea.removeFromTop(34));

    area.removeFromTop(10);
    auto displayRow = area.removeFromTop(316);
    auto tableAreaDisplay = displayRow.removeFromLeft(displayRow.proportionOfWidth(0.54f));
    tableDisplay->setBounds(tableAreaDisplay);
    displayRow.removeFromLeft(16);
    curveDisplay->setBounds(displayRow);

    area.removeFromTop(16);
    meterBridge->setBounds(area.removeFromTop(116));

    area.removeFromTop(16);
    auto heroRow = area.removeFromTop(230);
    auto driveArea = heroRow.removeFromLeft(heroRow.proportionOfWidth(0.30f));
    driveKnob->setBounds(driveArea);

    heroRow.removeFromLeft(14);
    auto topKnobWidth = (heroRow.getWidth() - 28) / 3;
    frameKnob->setBounds(heroRow.removeFromLeft(topKnobWidth));
    heroRow.removeFromLeft(14);
    phaseKnob->setBounds(heroRow.removeFromLeft(topKnobWidth));
    heroRow.removeFromLeft(14);
    biasKnob->setBounds(heroRow);

    area.removeFromTop(14);
    auto bottomKnobWidth = (area.getWidth() - 28) / 3;
    smoothKnob->setBounds(area.removeFromLeft(bottomKnobWidth));
    area.removeFromLeft(14);
    outputKnob->setBounds(area.removeFromLeft(bottomKnobWidth));
    area.removeFromLeft(14);
    mixKnob->setBounds(area);
}

void WaveshaperControlsComponent::timerCallback()
{
    refreshSummary();
    tableDisplay->repaint();
    curveDisplay->repaint();
    meterBridge->repaint();
}

void WaveshaperControlsComponent::refreshSummary()
{
    using namespace ssp::waveshaper;

    const auto tableIndex = juce::jlimit(0, getTableNames().size() - 1, juce::roundToInt(apvts.getRawParameterValue("table")->load()));
    const auto framePosition = apvts.getRawParameterValue("frame")->load() / 100.0f;
    const auto frameNumber = juce::roundToInt(framePosition * (float) (frameCount - 1)) + 1;

    summaryLabel.setText(getTableDescription(tableIndex) + " Frame " + juce::String(frameNumber) + " of " + juce::String(frameCount) + ".",
                         juce::dontSendNotification);
}
