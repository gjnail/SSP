#include "PluginEditor.h"

#include "EqControlsComponent.h"
#include "EqGraphComponent.h"
#include "MusicNoteUtils.h"
#include "SSPVectorUI.h"

namespace
{
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

constexpr int editorWidth = 392;
constexpr int editorHeight = 1100;
constexpr int outputRailWidth = 88;
constexpr int sectionHeaderHeight = 20;
constexpr float meterFloorDb = -60.0f;

juce::Colour backgroundTop() { return juce::Colour(0xff0a1118); }
juce::Colour backgroundBottom() { return juce::Colour(0xff12171d); }
juce::Colour panelFill() { return juce::Colour(0xff12161c); }
juce::Colour panelFillAlt() { return juce::Colour(0xff181e25); }
juce::Colour panelOutline() { return juce::Colour(0xff95a0af); }
juce::Colour panelInnerOutline() { return juce::Colour(0xff2c3642); }
juce::Colour accent() { return juce::Colour(0xff63d0ff); }
juce::Colour accentSoft() { return juce::Colour(0xff9de7ff); }
juce::Colour gateColour() { return juce::Colour(0xff36ffb0); }
juce::Colour amber() { return juce::Colour(0xfff0b661); }
juce::Colour purple() { return juce::Colour(0xffa58dff); }
juce::Colour subtleText() { return juce::Colour(0xff98a2b3); }
juce::Colour brightText() { return juce::Colour(0xffede5d8); }
float panelRadius() { return 7.0f; }

juce::String formatDb(double value)
{
    return juce::String(value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

juce::String formatPlain(double value, int decimals = 1)
{
    return juce::String(value, decimals);
}

juce::String formatPercentFromUnit(double value)
{
    return juce::String((int) std::round(value * 100.0)) + "%";
}

juce::String formatPercent(double value)
{
    return juce::String((int) std::round(value)) + "%";
}

juce::String formatPan(double value)
{
    const int rounded = juce::roundToInt((float) value);
    if (rounded == 0)
        return "C";
    return juce::String(std::abs(rounded)) + (rounded < 0 ? "L" : "R");
}

juce::String formatMs(double value)
{
    return value >= 1000.0 ? juce::String(value * 0.001, 2) + " s" : juce::String(value, 1) + " ms";
}

float dbToNorm(float db)
{
    return juce::jlimit(0.0f, 1.0f, (db - meterFloorDb) / -meterFloorDb);
}

void fillPanel(juce::Graphics& g, juce::Rectangle<float> bounds, juce::String title, bool bypassed, bool clipped, juce::Colour tint = accent())
{
    juce::ColourGradient fill(panelFillAlt(), bounds.getX(), bounds.getY(),
                              panelFill(), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, panelRadius());
    g.setColour(juce::Colour(0xff07090c));
    g.drawRoundedRectangle(bounds.reduced(0.5f), panelRadius(), 1.3f);
    g.setColour(panelInnerOutline().withAlpha(0.92f));
    g.drawRoundedRectangle(bounds.reduced(1.6f), panelRadius() - 1.0f, 1.0f);
    g.setColour(tint.withAlpha(0.9f));
    g.fillRoundedRectangle(bounds.removeFromLeft(4.0f), 2.0f);

    auto header = bounds.removeFromTop((float) sectionHeaderHeight);
    juce::ColourGradient headerFill(juce::Colour(0xff1b2026), header.getX(), header.getY(),
                                    juce::Colour(0xff11161c), header.getRight(), header.getBottom(), false);
    g.setGradientFill(headerFill);
    g.fillRoundedRectangle(header, panelRadius());
    g.setColour(brightText());
    g.setFont(juce::Font(10.4f, juce::Font::bold));
    g.drawText(title.toUpperCase(), header.toNearestInt().reduced(10, 0), juce::Justification::centredLeft, false);

    if (clipped)
    {
        auto clip = juce::Rectangle<float>(bounds.getRight() - 26.0f, bounds.getY() + 6.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0xfff45d5d));
        g.fillEllipse(clip);
    }

    if (bypassed)
    {
        g.setColour(juce::Colour(0x8f0a0d11));
        g.fillRoundedRectangle(bounds.reduced(1.0f), panelRadius());
    }
}

struct FrequencyFieldConfig
{
    static std::function<juce::String(double)> formatter()
    {
        return [](double value) { return ssp::notes::formatFrequencyWithNote(value); };
    }

    static std::function<double(const juce::String&)> parser()
    {
        return [](const juce::String& text)
        {
            double frequency = 0.0;
            if (ssp::notes::tryParseFrequencyInput(text, frequency))
                return frequency;
            return text.getDoubleValue();
        };
    }
};

class KnobField final : public juce::Component
{
public:
    KnobField(juce::AudioProcessorValueTreeState& state,
              const juce::String& paramId,
              const juce::String& titleText)
        : attachment(state, paramId, knob)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(knob);

        title.setText(titleText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, subtleText());
        title.setFont(juce::Font(9.0f, juce::Font::bold));

        knob.setColour(juce::Slider::rotarySliderFillColourId, accent());
        knob.setColour(juce::Slider::thumbColourId, accentSoft());
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 15);
        knob.setColour(juce::Slider::textBoxTextColourId, brightText());
        knob.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0b0f14));
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob.setMouseDragSensitivity(155);

        if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(paramId)))
            knob.setDoubleClickReturnValue(true, param->convertFrom0to1(state.getParameter(paramId)->getDefaultValue()));
    }

    void setTextFormatting(std::function<juce::String(double)> formatter,
                           std::function<double(const juce::String&)> parser = {})
    {
        knob.textFromValueFunction = std::move(formatter);
        if (parser)
            knob.valueFromTextFunction = std::move(parser);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds(area.removeFromTop(12));
        knob.setBounds(area);
    }

private:
    juce::Label title;
    ssp::ui::SSPKnob knob;
    SliderAttachment attachment;
};

class DropdownField final : public juce::Component
{
public:
    DropdownField(juce::AudioProcessorValueTreeState& state,
                  const juce::String& paramId,
                  const juce::String& titleText,
                  const juce::StringArray& items)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(box);

        title.setText(titleText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setColour(juce::Label::textColourId, subtleText());
        title.setFont(juce::Font(9.5f, juce::Font::bold));

        for (int i = 0; i < items.size(); ++i)
            box.addItem(items[i], i + 1);

        attachment = std::make_unique<ComboBoxAttachment>(state, paramId, box);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds(area.removeFromTop(12));
        box.setBounds(area.removeFromTop(24));
    }

private:
    juce::Label title;
    ssp::ui::SSPDropdown box;
    std::unique_ptr<ComboBoxAttachment> attachment;
};

class ToggleField final : public juce::Component
{
public:
    ToggleField(juce::AudioProcessorValueTreeState& state,
                const juce::String& paramId,
                const juce::String& labelText)
        : attachment(state, paramId, toggle)
    {
        addAndMakeVisible(toggle);
        toggle.setButtonText(labelText);
    }

    void resized() override
    {
        toggle.setBounds(getLocalBounds());
    }

private:
    ssp::ui::SSPToggle toggle;
    ButtonAttachment attachment;
};

class FaderField final : public juce::Component
{
public:
    FaderField(juce::AudioProcessorValueTreeState& state,
               const juce::String& paramId,
               const juce::String& titleText)
        : attachment(state, paramId, slider)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(slider);
        title.setText(titleText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, subtleText());
        title.setFont(juce::Font(10.0f, juce::Font::bold));
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
        slider.textFromValueFunction = [](double value) { return formatDb(value); };
        slider.setDoubleClickReturnValue(true, 0.0);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds(area.removeFromTop(14));
        slider.setBounds(area);
    }

private:
    juce::Label title;
    ssp::ui::SSPSlider slider;
    SliderAttachment attachment;
};

class CompactStageMeter final : public juce::Component
{
public:
    CompactStageMeter(PluginProcessor& processorToUse, int stageIndexToUse, const juce::String& labelToUse)
        : processor(processorToUse), stageIndex(stageIndexToUse), label(labelToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        const auto snapshot = processor.getStageMeterSnapshot(stageIndex);
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0b131a));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(panelOutline().withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        auto meter = bounds.reduced(6.0f, 16.0f);
        g.setColour(juce::Colour(0x1829d6a5));
        g.fillRoundedRectangle(meter, 5.0f);

        const float rmsNorm = dbToNorm(snapshot.rmsDb);
        const float peakNorm = dbToNorm(snapshot.peakDb);
        auto rmsBar = meter.withWidth(meter.getWidth() * rmsNorm);
        auto peakBar = meter.withWidth(meter.getWidth() * peakNorm);
        g.setColour(juce::Colour(0x559de7ff));
        g.fillRoundedRectangle(rmsBar, 5.0f);
        juce::ColourGradient peakFill(juce::Colour(0xff29d6a5), peakBar.getX(), peakBar.getBottom(),
                                      juce::Colour(0xffff5d5d), peakBar.getRight(), peakBar.getY(), false);
        g.setGradientFill(peakFill);
        g.fillRoundedRectangle(peakBar, 5.0f);

        g.setColour(brightText());
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(label, 6, 2, getWidth() - 12, 12, juce::Justification::centredLeft, false);
        if (snapshot.clipped)
        {
            g.setColour(juce::Colour(0xfff45d5d));
            g.fillEllipse((float) getWidth() - 14.0f, 4.0f, 6.0f, 6.0f);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        processor.clearClipLatch(stageIndex);
    }

private:
    PluginProcessor& processor;
    int stageIndex = 0;
    juce::String label;
};

class InputFilterDisplay final : public juce::Component
{
public:
    explicit InputFilterDisplay(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c141d));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(panelOutline().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto plot = bounds.reduced(10.0f, 8.0f);
        g.setColour(juce::Colour(0x22ffffff));
        for (int i = 0; i < 4; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, 3.0f, plot.getY(), plot.getBottom());
            g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        }

        const bool hpfOn = processor.apvts.getRawParameterValue("inputHpfOn")->load() >= 0.5f;
        const bool lpfOn = processor.apvts.getRawParameterValue("inputLpfOn")->load() >= 0.5f;
        const float hpf = processor.apvts.getRawParameterValue("inputHpfFreq")->load();
        const float lpf = processor.apvts.getRawParameterValue("inputLpfFreq")->load();

        auto magnitudeForFrequency = [&](float frequency)
        {
            double gain = 1.0;
            if (hpfOn)
                gain *= 1.0 / std::sqrt(1.0 + std::pow((double) juce::jmax(1.0f, hpf) / frequency, 4.0));
            if (lpfOn)
                gain *= 1.0 / std::sqrt(1.0 + std::pow(frequency / (double) juce::jmax(1000.0f, lpf), 4.0));
            return juce::Decibels::gainToDecibels((float) gain, -36.0f);
        };

        juce::Path response;
        for (int i = 0; i <= 96; ++i)
        {
            const float norm = (float) i / 96.0f;
            const float frequency = std::pow(10.0f, juce::jmap(norm, std::log10(20.0f), std::log10(20000.0f)));
            const float db = magnitudeForFrequency(frequency);
            const float x = juce::jmap(norm, 0.0f, 1.0f, plot.getX(), plot.getRight());
            const float y = juce::jmap(db, -24.0f, 0.0f, plot.getBottom(), plot.getY());
            if (i == 0)
                response.startNewSubPath(x, y);
            else
                response.lineTo(x, y);
        }

        g.setColour(accent());
        g.strokePath(response, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    PluginProcessor& processor;
};

class HistoryGraphBase : public juce::Component,
                         private juce::Timer
{
public:
    explicit HistoryGraphBase(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
        history.fill(0.0f);
        startTimerHz(30);
    }

protected:
    void pushValue(float value)
    {
        std::rotate(history.begin(), history.begin() + 1, history.end());
        history.back() = value;
    }

    std::array<float, 96> history{};
    PluginProcessor& processor;

private:
    void timerCallback() override
    {
        tick();
        repaint();
    }

    virtual void tick() = 0;
};

class CompressorDisplay final : public HistoryGraphBase
{
public:
    explicit CompressorDisplay(PluginProcessor& processorToUse)
        : HistoryGraphBase(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c141d));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(panelOutline().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto plot = bounds.reduced(10.0f);
        auto traceArea = plot.removeFromBottom(22.0f);
        plot.removeFromBottom(6.0f);

        g.setColour(juce::Colour(0x25ffffff));
        for (int i = 0; i < 4; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, 3.0f, plot.getX(), plot.getRight());
            const float y = juce::jmap((float) i, 0.0f, 3.0f, plot.getBottom(), plot.getY());
            g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
            g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        }

        const float threshold = processor.apvts.getRawParameterValue("compThreshold")->load();
        const float ratio = processor.apvts.getRawParameterValue("compRatio")->load();
        const float knee = processor.apvts.getRawParameterValue("compKnee")->load();
        const float makeup = processor.apvts.getRawParameterValue("compMakeup")->load();
        const float currentGr = processor.getStageMeterSnapshot(PluginProcessor::moduleCompressor).auxDb;
        const float detectorDb = processor.getStageMeterSnapshot(PluginProcessor::moduleInput).rmsDb;

        juce::Path curve;
        auto transfer = [&](float inputDb)
        {
            float outputDb = inputDb;
            const float overshoot = inputDb - threshold;
            if (overshoot > 0.0f)
                outputDb = threshold + overshoot / juce::jmax(1.0f, ratio);
            else if (knee > 0.0f && std::abs(overshoot) < knee * 0.5f)
                outputDb = inputDb - (1.0f - 1.0f / juce::jmax(1.0f, ratio)) * std::pow(overshoot + knee * 0.5f, 2.0f) / juce::jmax(0.1f, knee * 2.0f);
            return outputDb + makeup;
        };

        for (int i = 0; i <= 120; ++i)
        {
            const float inputDb = juce::jmap((float) i, 0.0f, 120.0f, -48.0f, 12.0f);
            const float outputDb = transfer(inputDb);
            const float x = juce::jmap(inputDb, -48.0f, 12.0f, plot.getX(), plot.getRight());
            const float y = juce::jmap(outputDb, -48.0f, 12.0f, plot.getBottom(), plot.getY());
            if (i == 0)
                curve.startNewSubPath(x, y);
            else
                curve.lineTo(x, y);
        }

        g.setColour(juce::Colour(0x45ffffff));
        g.drawLine(plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);
        g.setColour(accent());
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float dotInput = juce::jlimit(-48.0f, 12.0f, detectorDb);
        const float dotOutput = transfer(dotInput) - currentGr;
        const float dotX = juce::jmap(dotInput, -48.0f, 12.0f, plot.getX(), plot.getRight());
        const float dotY = juce::jmap(dotOutput, -48.0f, 12.0f, plot.getBottom(), plot.getY());
        g.setColour(amber());
        g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

        juce::Path trace;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) history.size() - 1.0f, traceArea.getX(), traceArea.getRight());
            const float y = juce::jmap(history[i], 0.0f, 24.0f, traceArea.getY(), traceArea.getBottom());
            if (i == 0)
                trace.startNewSubPath(x, y);
            else
                trace.lineTo(x, y);
        }

        g.setColour(juce::Colour(0x2040baff));
        g.fillRoundedRectangle(traceArea, 6.0f);
        g.setColour(accentSoft());
        g.strokePath(trace, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void tick() override
    {
        pushValue(juce::jlimit(0.0f, 24.0f, processor.getStageMeterSnapshot(PluginProcessor::moduleCompressor).auxDb));
    }
};

class GateDisplay final : public HistoryGraphBase
{
public:
    explicit GateDisplay(PluginProcessor& processorToUse)
        : HistoryGraphBase(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c141d));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(panelOutline().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto area = bounds.reduced(10.0f);
        auto top = area.removeFromTop(24.0f);
        const auto snapshot = processor.getStageMeterSnapshot(PluginProcessor::moduleGate);
        const float openAmount = 1.0f - juce::jlimit(0.0f, 1.0f, snapshot.auxDb / 36.0f);
        auto door = top.removeFromLeft(44.0f).reduced(2.0f);
        g.setColour(juce::Colour(0xff13202b));
        g.fillRoundedRectangle(door, 6.0f);
        auto openBar = door.reduced(4.0f).withHeight((door.getHeight() - 8.0f) * juce::jlimit(0.08f, 1.0f, openAmount));
        openBar.setY(door.getBottom() - 4.0f - openBar.getHeight());
        g.setColour(gateColour());
        g.fillRoundedRectangle(openBar, 4.0f);

        g.setColour(brightText());
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(openAmount > 0.7f ? "OPEN" : openAmount > 0.25f ? "TRANS" : "CLOSED",
                   top.toNearestInt().reduced(6, 0), juce::Justification::centredLeft, false);

        auto scArea = area.removeFromTop(22.0f);
        const auto frame = processor.getAnalyzerFrameCopy();
        g.setColour(juce::Colour(0x1829d6a5));
        g.fillRoundedRectangle(scArea, 5.0f);
        const int barCount = 16;
        for (int i = 0; i < barCount; ++i)
        {
            const int bin = juce::jlimit(1, (int) frame.sidechain.size() - 1, i * 8 + 1);
            const float db = frame.sidechain[(size_t) bin];
            const float norm = dbToNorm(db);
            auto bar = juce::Rectangle<float>(scArea.getX() + i * (scArea.getWidth() / (float) barCount) + 1.0f,
                                              scArea.getBottom() - norm * scArea.getHeight(),
                                              scArea.getWidth() / (float) barCount - 2.0f,
                                              norm * scArea.getHeight());
            g.setColour(accentSoft().withAlpha(0.7f));
            g.fillRoundedRectangle(bar, 2.0f);
        }

        auto traceArea = area.removeFromBottom(24.0f);
        juce::Path trace;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) history.size() - 1.0f, traceArea.getX(), traceArea.getRight());
            const float y = juce::jmap(history[i], 0.0f, 36.0f, traceArea.getY(), traceArea.getBottom());
            if (i == 0)
                trace.startNewSubPath(x, y);
            else
                trace.lineTo(x, y);
        }
        g.setColour(juce::Colour(0x1829d6a5));
        g.fillRoundedRectangle(traceArea, 5.0f);
        g.setColour(gateColour());
        g.strokePath(trace, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void tick() override
    {
        pushValue(juce::jlimit(0.0f, 36.0f, processor.getStageMeterSnapshot(PluginProcessor::moduleGate).auxDb));
    }
};

class HarmonicsDisplay final : public juce::Component
{
public:
    explicit HarmonicsDisplay(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c141d));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(panelOutline().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto plot = bounds.reduced(10.0f);
        const float drive = processor.apvts.getRawParameterValue("satDrive")->load() / 24.0f;
        const float mix = processor.apvts.getRawParameterValue("satMix")->load() / 100.0f;
        const int character = (int) processor.apvts.getRawParameterValue("satCharacter")->load();
        const float inputNorm = dbToNorm(processor.getStageMeterSnapshot(PluginProcessor::moduleSaturation).rmsDb);

        for (int i = 0; i < 8; ++i)
        {
            const float harmonic = (float) (i + 2);
            const float weight = character == 0 ? (1.0f / harmonic)
                                : character == 1 ? ((i % 2 == 0 ? 1.18f : 0.62f) / harmonic)
                                                 : ((i < 3 ? 1.08f : 0.52f) / harmonic);
            const float amount = juce::jlimit(0.0f, 1.0f, weight * (0.4f + drive * 1.2f) * (0.3f + mix * 0.9f) * (0.45f + inputNorm));
            auto bar = juce::Rectangle<float>(plot.getX() + i * (plot.getWidth() / 8.0f) + 3.0f,
                                              plot.getBottom() - amount * plot.getHeight(),
                                              plot.getWidth() / 8.0f - 6.0f,
                                              amount * plot.getHeight());
            juce::ColourGradient fill(character == 1 ? amber() : accent(), bar.getCentreX(), bar.getBottom(),
                                      character == 2 ? purple() : gateColour(), bar.getCentreX(), bar.getY(), false);
            g.setGradientFill(fill);
            g.fillRoundedRectangle(bar, 3.0f);
        }
    }

private:
    PluginProcessor& processor;
};

class LargeOutputMeter final : public juce::Component
{
public:
    explicit LargeOutputMeter(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c141d));
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(panelOutline().withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

        const auto snapshot = processor.getStageMeterSnapshot(PluginProcessor::moduleOutput);
        const bool vuMode = processor.apvts.getRawParameterValue("outputMeterMode")->load() >= 0.5f;
        const float ballisticsDb = vuMode ? snapshot.rmsDb : snapshot.peakDb;
        const float meterNorm = dbToNorm(ballisticsDb);
        const float rmsNorm = dbToNorm(snapshot.rmsDb);

        auto meter = bounds.reduced(12.0f, 14.0f);
        auto rightText = meter.removeFromBottom(46.0f);
        auto peakBar = meter.removeFromLeft(meter.getWidth() * 0.52f).reduced(10.0f, 0.0f);
        auto rmsBar = meter.reduced(10.0f, 0.0f);

        auto drawBar = [&](juce::Rectangle<float> area, float norm, juce::Colour base)
        {
            g.setColour(juce::Colour(0xff101821));
            g.fillRoundedRectangle(area, 8.0f);
            const int segments = 24;
            for (int i = 0; i < segments; ++i)
            {
                const float segmentHeight = area.getHeight() / (float) segments;
                auto seg = juce::Rectangle<float>(area.getX() + 2.0f,
                                                  area.getBottom() - (i + 1) * segmentHeight + 1.0f,
                                                  area.getWidth() - 4.0f,
                                                  segmentHeight - 2.0f);
                const float threshold = (float) (i + 1) / (float) segments;
                juce::Colour colour = threshold > 0.84f ? juce::Colour(0xfff45d5d)
                                     : threshold > 0.64f ? amber()
                                                         : base;
                g.setColour(threshold <= norm ? colour : colour.withAlpha(0.12f));
                g.fillRoundedRectangle(seg, 2.0f);
            }
        };

        drawBar(peakBar, meterNorm, accent());
        drawBar(rmsBar, rmsNorm, gateColour());

        g.setColour(brightText());
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(vuMode ? "VU" : "PK", peakBar.toNearestInt().removeFromTop(14), juce::Justification::centred, false);
        g.drawText("RMS", rmsBar.toNearestInt().removeFromTop(14), juce::Justification::centred, false);

        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("PEAK " + juce::String(snapshot.peakDb, 1), rightText.removeFromTop(14), juce::Justification::centredLeft, false);
        g.drawText("RMS  " + juce::String(snapshot.rmsDb, 1), rightText.removeFromTop(14), juce::Justification::centredLeft, false);
        g.drawText("LUFS " + juce::String(snapshot.rmsDb - 1.5f, 1), rightText.removeFromTop(14), juce::Justification::centredLeft, false);
    }

private:
    PluginProcessor& processor;
};

class SignalFlowComponent final : public juce::Component
{
public:
    explicit SignalFlowComponent(PluginProcessor& processorToUse)
        : processor(processorToUse),
          inputMeter(processorToUse, PluginProcessor::moduleInput, "IN"),
          gateMeter(processorToUse, PluginProcessor::moduleGate, "GATE"),
          eqMeter(processorToUse, PluginProcessor::moduleEq, "EQ"),
          compMeter(processorToUse, PluginProcessor::moduleCompressor, "COMP"),
          satMeter(processorToUse, PluginProcessor::moduleSaturation, "SAT"),
          outputMeter(processorToUse, PluginProcessor::moduleOutput, "OUT")
    {
        for (auto* meter : {&inputMeter, &gateMeter, &eqMeter, &compMeter, &satMeter, &outputMeter})
            addAndMakeVisible(*meter);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        fillPanel(g, bounds, "Signal Flow", false, false, accent());

        auto blocks = getLocalBounds().reduced(10, 8);
        blocks.removeFromTop(sectionHeaderHeight);
        auto diagram = blocks.removeFromTop(26);

        std::vector<juce::String> labels{"Input"};
        for (int module : processor.getModuleOrder())
            labels.push_back(PluginProcessor::getModuleNames()[module]);
        labels.push_back("Output");

        const int blockWidth = diagram.getWidth() / juce::jmax(1, (int) labels.size());
        for (size_t i = 0; i < labels.size(); ++i)
        {
            auto box = diagram.removeFromLeft(blockWidth).reduced(2, 2).toFloat();
            auto tint = i == 0 ? accent()
                        : labels[i] == "Gate" ? gateColour()
                        : labels[i] == "Compressor" ? amber()
                        : labels[i] == "EQ" ? purple()
                        : labels[i] == "Saturation" ? juce::Colour(0xffff8f65)
                                                    : accentSoft();
            g.setColour(juce::Colour(0xff121c27));
            g.fillRoundedRectangle(box, 7.0f);
            g.setColour(tint.withAlpha(0.85f));
            g.drawRoundedRectangle(box, 7.0f, 1.0f);
            g.setColour(brightText());
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(labels[i].substring(0, 6).toUpperCase(), box.toNearestInt(), juce::Justification::centred, false);

            if (i + 1 < labels.size())
            {
                const float x1 = box.getRight() + 2.0f;
                const float x2 = box.getRight() + 10.0f;
                const float y = box.getCentreY();
                g.setColour(subtleText());
                g.drawLine(x1, y, x2, y, 1.2f);
                g.drawLine(x2 - 3.0f, y - 3.0f, x2, y, 1.2f);
                g.drawLine(x2 - 3.0f, y + 3.0f, x2, y, 1.2f);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight + 28);
        const int width = area.getWidth() / 6;
        inputMeter.setBounds(area.removeFromLeft(width).reduced(2));
        gateMeter.setBounds(area.removeFromLeft(width).reduced(2));
        eqMeter.setBounds(area.removeFromLeft(width).reduced(2));
        compMeter.setBounds(area.removeFromLeft(width).reduced(2));
        satMeter.setBounds(area.removeFromLeft(width).reduced(2));
        outputMeter.setBounds(area.reduced(2));
    }

private:
    PluginProcessor& processor;
    CompactStageMeter inputMeter;
    CompactStageMeter gateMeter;
    CompactStageMeter eqMeter;
    CompactStageMeter compMeter;
    CompactStageMeter satMeter;
    CompactStageMeter outputMeter;
};

class EqPopupContent final : public juce::Component
{
public:
    explicit EqPopupContent(PluginProcessor& processor)
        : graph(processor), controls(processor)
    {
        addAndMakeVisible(graph);
        addAndMakeVisible(controls);
        graph.onSelectionChanged = [this](int index) { controls.setSelectedPoint(index); };
        controls.onSelectionChanged = [this](int index) { graph.setSelectedPoint(index); };
        setSize(980, 760);
    }

    void paint(juce::Graphics& g) override
    {
        juce::ColourGradient background(backgroundTop(), 0.0f, 0.0f,
                                        backgroundBottom(), 0.0f, (float) getHeight(), false);
        background.addColour(0.55, juce::Colour(0xff0e1824));
        g.setGradientFill(background);
        g.fillAll();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        graph.setBounds(area.removeFromTop(420));
        area.removeFromTop(12);
        controls.setBounds(area);
    }

private:
    EqGraphComponent graph;
    EqControlsComponent controls;
};

class InputSection final : public juce::Component
{
public:
    explicit InputSection(PluginProcessor& processorToUse)
        : processor(processorToUse),
          trim(processor.apvts, "inputTrim", "Trim"),
          hpf(processor.apvts, "inputHpfFreq", "HPF"),
          lpf(processor.apvts, "inputLpfFreq", "LPF"),
          hpfOn(processor.apvts, "inputHpfOn", "HPF In"),
          lpfOn(processor.apvts, "inputLpfOn", "LPF In"),
          phase(processor.apvts, "inputPhase", "Phase"),
          response(processor)
    {
        for (auto* control : {&trim, &hpf, &lpf})
            addAndMakeVisible(*control);
        for (auto* toggle : {&hpfOn, &lpfOn, &phase})
            addAndMakeVisible(*toggle);
        addAndMakeVisible(response);

        trim.setTextFormatting([](double value) { return formatDb(value); });
        hpf.setTextFormatting(FrequencyFieldConfig::formatter(), FrequencyFieldConfig::parser());
        lpf.setTextFormatting(FrequencyFieldConfig::formatter(), FrequencyFieldConfig::parser());
    }

    void paint(juce::Graphics& g) override
    {
        fillPanel(g,
                  getLocalBounds().toFloat(),
                  "Input",
                  processor.apvts.getRawParameterValue("inputBypass")->load() >= 0.5f,
                  processor.getStageMeterSnapshot(PluginProcessor::moduleInput).clipped,
                  accent());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight);
        auto knobs = area.removeFromTop(82);
        const int knobWidth = knobs.getWidth() / 3;
        trim.setBounds(knobs.removeFromLeft(knobWidth).reduced(2));
        hpf.setBounds(knobs.removeFromLeft(knobWidth).reduced(2));
        lpf.setBounds(knobs.reduced(2));

        auto toggles = area.removeFromTop(28);
        const int toggleWidth = toggles.getWidth() / 3;
        hpfOn.setBounds(toggles.removeFromLeft(toggleWidth).reduced(2));
        lpfOn.setBounds(toggles.removeFromLeft(toggleWidth).reduced(2));
        phase.setBounds(toggles.reduced(2));
        area.removeFromTop(4);
        response.setBounds(area);
    }

private:
    PluginProcessor& processor;
    KnobField trim;
    KnobField hpf;
    KnobField lpf;
    ToggleField hpfOn;
    ToggleField lpfOn;
    ToggleField phase;
    InputFilterDisplay response;
};

class DynamicsSection final : public juce::Component
{
public:
    explicit DynamicsSection(PluginProcessor& processorToUse)
        : processor(processorToUse),
          dynBypass(processor.apvts, "dynamicsBypass", "Dyn In"),
          order(processor.apvts, "compGateOrder", "Comp > Gate"),
          link(processor.apvts, "dynamicsStereoLink", "Link"),
          compThreshold(processor.apvts, "compThreshold", "Thresh"),
          compRatio(processor.apvts, "compRatio", "Ratio"),
          compAttack(processor.apvts, "compAttack", "Attack"),
          compRelease(processor.apvts, "compRelease", "Release"),
          compMakeup(processor.apvts, "compMakeup", "Makeup"),
          compMix(processor.apvts, "compMix", "Mix"),
          compKnee(processor.apvts, "compKnee", "Knee"),
          compCharacter(processor.apvts, "compCharacter", "Mode", PluginProcessor::getCompressorCharacterNames()),
          compAttackMode(processor.apvts, "compAttackMode", "Attack", juce::StringArray{"Fast", "Slow"}),
          compAutoRelease(processor.apvts, "compAutoRelease", "Auto Rel"),
          compListen(processor.apvts, "compScListen", "SC Listen"),
          compDisplay(processor),
          gateThreshold(processor.apvts, "gateThreshold", "Thresh"),
          gateRange(processor.apvts, "gateRange", "Range"),
          gateAttack(processor.apvts, "gateAttack", "Attack"),
          gateHold(processor.apvts, "gateHold", "Hold"),
          gateRelease(processor.apvts, "gateRelease", "Release"),
          gateScHp(processor.apvts, "gateScHpFreq", "SC HP"),
          gateScLp(processor.apvts, "gateScLpFreq", "SC LP"),
          gateMode(processor.apvts, "gateMode", "Mode", PluginProcessor::getGateModeNames()),
          gateKey(processor.apvts, "gateKeyInput", "Key", juce::StringArray{"Internal", "External"}),
          gateListen(processor.apvts, "gateScListen", "SC Listen"),
          gateDisplay(processor)
    {
        for (auto* toggle : {&dynBypass, &order, &link, &compAutoRelease, &compListen, &gateListen})
            addAndMakeVisible(*toggle);
        for (auto* knob : {&compThreshold, &compRatio, &compAttack, &compRelease, &compMakeup, &compMix, &compKnee,
                           &gateThreshold, &gateRange, &gateAttack, &gateHold, &gateRelease, &gateScHp, &gateScLp})
            addAndMakeVisible(*knob);
        for (auto* dropdown : {&compCharacter, &compAttackMode, &gateMode, &gateKey})
            addAndMakeVisible(*dropdown);
        addAndMakeVisible(compDisplay);
        addAndMakeVisible(gateDisplay);

        compThreshold.setTextFormatting([](double value) { return juce::String(value, 1) + " dB"; });
        compRatio.setTextFormatting([](double value) { return juce::String(value, 2) + ":1"; });
        compAttack.setTextFormatting([](double value) { return formatMs(value); });
        compRelease.setTextFormatting([](double value) { return formatMs(value); });
        compMakeup.setTextFormatting([](double value) { return formatDb(value); });
        compMix.setTextFormatting([](double value) { return formatPercent(value); });
        compKnee.setTextFormatting([](double value) { return juce::String(value, 1) + " dB"; });

        gateThreshold.setTextFormatting([](double value) { return juce::String(value, 1) + " dB"; });
        gateRange.setTextFormatting([](double value) { return juce::String(value, 1) + " dB"; });
        gateAttack.setTextFormatting([](double value) { return formatMs(value); });
        gateHold.setTextFormatting([](double value) { return formatMs(value); });
        gateRelease.setTextFormatting([](double value) { return formatMs(value); });
        gateScHp.setTextFormatting(FrequencyFieldConfig::formatter(), FrequencyFieldConfig::parser());
        gateScLp.setTextFormatting(FrequencyFieldConfig::formatter(), FrequencyFieldConfig::parser());
    }

    void paint(juce::Graphics& g) override
    {
        const auto gateClip = processor.getStageMeterSnapshot(PluginProcessor::moduleGate).clipped;
        const auto compClip = processor.getStageMeterSnapshot(PluginProcessor::moduleCompressor).clipped;
        fillPanel(g,
                  getLocalBounds().toFloat(),
                  "Dynamics",
                  processor.apvts.getRawParameterValue("dynamicsBypass")->load() >= 0.5f,
                  gateClip || compClip,
                  amber());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight);
        auto toggles = area.removeFromTop(28);
        const int toggleWidth = toggles.getWidth() / 3;
        dynBypass.setBounds(toggles.removeFromLeft(toggleWidth).reduced(2));
        order.setBounds(toggles.removeFromLeft(toggleWidth).reduced(2));
        link.setBounds(toggles.reduced(2));

        auto compArea = area.removeFromTop(174);
        auto compHeader = compArea.removeFromTop(16);
        juce::ignoreUnused(compHeader);
        auto compTop = compArea.removeFromTop(76);
        const int compKnobWidth = compTop.getWidth() / 4;
        compThreshold.setBounds(compTop.removeFromLeft(compKnobWidth).reduced(2));
        compRatio.setBounds(compTop.removeFromLeft(compKnobWidth).reduced(2));
        compAttack.setBounds(compTop.removeFromLeft(compKnobWidth).reduced(2));
        compRelease.setBounds(compTop.reduced(2));

        auto compMid = compArea.removeFromTop(76);
        compMakeup.setBounds(compMid.removeFromLeft(compKnobWidth).reduced(2));
        compMix.setBounds(compMid.removeFromLeft(compKnobWidth).reduced(2));
        compKnee.setBounds(compMid.removeFromLeft(compKnobWidth).reduced(2));
        auto compSide = compMid.reduced(2);
        auto compSideTop = compSide.removeFromTop(36);
        compAttackMode.setBounds(compSideTop.removeFromLeft(compSideTop.getWidth() / 2).reduced(1));
        compAutoRelease.setBounds(compSideTop.reduced(1));
        auto compSideBottom = compSide.removeFromTop(36);
        compCharacter.setBounds(compSideBottom.removeFromLeft(compSideBottom.getWidth() / 2).reduced(1));
        compListen.setBounds(compSideBottom.reduced(1));

        compDisplay.setBounds(compArea.reduced(2));

        area.removeFromTop(4);
        auto gateArea = area;
        auto gateTop = gateArea.removeFromTop(76);
        const int gateKnobWidth = gateTop.getWidth() / 5;
        gateThreshold.setBounds(gateTop.removeFromLeft(gateKnobWidth).reduced(2));
        gateRange.setBounds(gateTop.removeFromLeft(gateKnobWidth).reduced(2));
        gateAttack.setBounds(gateTop.removeFromLeft(gateKnobWidth).reduced(2));
        gateHold.setBounds(gateTop.removeFromLeft(gateKnobWidth).reduced(2));
        gateRelease.setBounds(gateTop.reduced(2));

        auto gateMid = gateArea.removeFromTop(76);
        gateScHp.setBounds(gateMid.removeFromLeft(gateKnobWidth).reduced(2));
        gateScLp.setBounds(gateMid.removeFromLeft(gateKnobWidth).reduced(2));
        auto gateRight = gateMid.reduced(2);
        auto gateRightTop = gateRight.removeFromTop(36);
        gateMode.setBounds(gateRightTop.removeFromLeft(gateRightTop.getWidth() / 2).reduced(1));
        gateKey.setBounds(gateRightTop.reduced(1));
        gateListen.setBounds(gateRight.removeFromTop(28).reduced(1));

        gateDisplay.setBounds(gateArea.reduced(2));
    }

private:
    PluginProcessor& processor;
    ToggleField dynBypass;
    ToggleField order;
    ToggleField link;
    KnobField compThreshold;
    KnobField compRatio;
    KnobField compAttack;
    KnobField compRelease;
    KnobField compMakeup;
    KnobField compMix;
    KnobField compKnee;
    DropdownField compCharacter;
    DropdownField compAttackMode;
    ToggleField compAutoRelease;
    ToggleField compListen;
    CompressorDisplay compDisplay;
    KnobField gateThreshold;
    KnobField gateRange;
    KnobField gateAttack;
    KnobField gateHold;
    KnobField gateRelease;
    KnobField gateScHp;
    KnobField gateScLp;
    DropdownField gateMode;
    DropdownField gateKey;
    ToggleField gateListen;
    GateDisplay gateDisplay;
};

class BasicEqDisplay final : public juce::Component
{
public:
    explicit BasicEqDisplay(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a1015));
        g.fillRoundedRectangle(bounds, panelRadius() - 1.0f);
        g.setColour(panelInnerOutline().withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), panelRadius() - 1.0f, 1.0f);

        auto plot = bounds.reduced(10.0f, 8.0f);
        g.setColour(juce::Colour(0x20ffffff));
        for (int i = 0; i < 4; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, 3.0f, plot.getY(), plot.getBottom());
            g.drawHorizontalLine((int) std::round(y), plot.getX(), plot.getRight());
        }
        for (float norm : { 0.12f, 0.32f, 0.54f, 0.76f })
        {
            const float x = juce::jmap(norm, 0.0f, 1.0f, plot.getX(), plot.getRight());
            g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
        }

        juce::Path combined;
        juce::Path lowBand;
        juce::Path midBand;
        juce::Path highBand;
        auto frequencyToX = [&](float frequency)
        {
            const float norm = juce::jmap(std::log10(frequency), std::log10(20.0f), std::log10(20000.0f), 0.0f, 1.0f);
            return juce::jmap(norm, 0.0f, 1.0f, plot.getX(), plot.getRight());
        };
        auto dbToY = [&](float db) { return juce::jmap(db, -18.0f, 18.0f, plot.getBottom(), plot.getY()); };

        for (int i = 0; i <= 120; ++i)
        {
            const float norm = (float) i / 120.0f;
            const float frequency = std::pow(10.0f, juce::jmap(norm, std::log10(20.0f), std::log10(20000.0f)));
            const float low = processor.getBandResponseForFrequency(0, frequency);
            const float mid = processor.getBandResponseForFrequency(1, frequency);
            const float high = processor.getBandResponseForFrequency(2, frequency);
            const float total = low + mid + high;
            const float x = frequencyToX(frequency);

            auto addPoint = [&](juce::Path& path, float db)
            {
                const float y = dbToY(db);
                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            };

            addPoint(lowBand, low);
            addPoint(midBand, mid);
            addPoint(highBand, high);
            addPoint(combined, total);
        }

        g.setColour(accent().withAlpha(0.28f));
        g.strokePath(lowBand, juce::PathStrokeType(1.0f));
        g.setColour(amber().withAlpha(0.32f));
        g.strokePath(midBand, juce::PathStrokeType(1.0f));
        g.setColour(purple().withAlpha(0.28f));
        g.strokePath(highBand, juce::PathStrokeType(1.0f));
        g.setColour(accentSoft());
        g.strokePath(combined, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (int index = 0; index < 3; ++index)
        {
            const auto point = processor.getPoint(index);
            if (!point.enabled)
                continue;

            const float x = frequencyToX(juce::jlimit(20.0f, 20000.0f, point.frequency));
            const float y = dbToY(point.gainDb);
            const auto colour = index == 0 ? accent() : (index == 1 ? amber() : purple());
            g.setColour(juce::Colour(0xff071016));
            g.fillEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f);
            g.setColour(colour);
            g.drawEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.6f);
        }
    }

private:
    PluginProcessor& processor;
};

class EqSection final : public juce::Component,
                        private juce::Timer
{
public:
    explicit EqSection(PluginProcessor& processorToUse)
        : processor(processorToUse),
          eqBypass(processor.apvts, "eqBypass", "EQ In"),
          order(processor.apvts, "eqPreDynamics", "EQ > Dyn"),
          visualizer(processor),
          lowFreq(processor.apvts, "p0Freq", "Low Freq"),
          lowGain(processor.apvts, "p0Gain", "Low Gain"),
          midFreq(processor.apvts, "p1Freq", "Mid Freq"),
          midGain(processor.apvts, "p1Gain", "Mid Gain"),
          midQ(processor.apvts, "p1Q", "Mid Q"),
          highFreq(processor.apvts, "p2Freq", "High Freq"),
          highGain(processor.apvts, "p2Gain", "High Gain")
    {
        addAndMakeVisible(eqBypass);
        addAndMakeVisible(order);
        addAndMakeVisible(visualizer);

        for (auto* component : {&lowFreq, &lowGain, &midFreq, &midGain, &midQ, &highFreq, &highGain})
            addAndMakeVisible(*component);

        for (auto* field : {&lowFreq, &midFreq, &highFreq})
            field->setTextFormatting(FrequencyFieldConfig::formatter(), FrequencyFieldConfig::parser());
        for (auto* field : {&lowGain, &midGain, &highGain})
            field->setTextFormatting([](double value) { return formatDb(value); });
        midQ.setTextFormatting([](double value) { return formatPlain(value, 2); });

        syncBands();
        startTimerHz(8);
    }

    void paint(juce::Graphics& g) override
    {
        fillPanel(g,
                  getLocalBounds().toFloat(),
                  "3-Band EQ",
                  processor.apvts.getRawParameterValue("eqBypass")->load() >= 0.5f,
                  processor.getStageMeterSnapshot(PluginProcessor::moduleEq).clipped,
                  purple());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight);
        auto topRow = area.removeFromTop(28);
        const int block = topRow.getWidth() / 2;
        eqBypass.setBounds(topRow.removeFromLeft(block).reduced(2));
        order.setBounds(topRow.reduced(2));

        visualizer.setBounds(area.removeFromTop(104).reduced(2));
        area.removeFromTop(4);

        auto freqRow = area.removeFromTop(72);
        const int third = freqRow.getWidth() / 3;
        lowFreq.setBounds(freqRow.removeFromLeft(third).reduced(2));
        midFreq.setBounds(freqRow.removeFromLeft(third).reduced(2));
        highFreq.setBounds(freqRow.reduced(2));

        auto gainRow = area.removeFromTop(72);
        lowGain.setBounds(gainRow.removeFromLeft(third).reduced(2));
        midGain.setBounds(gainRow.removeFromLeft(third).reduced(2));
        highGain.setBounds(gainRow.reduced(2));

        auto qRow = area.removeFromTop(72);
        qRow.removeFromLeft(qRow.getWidth() / 3);
        midQ.setBounds(qRow.removeFromLeft(qRow.getWidth() / 2).reduced(2));
    }

private:
    void timerCallback() override
    {
        syncBands();
        repaint();
    }

    void syncBands()
    {
        updateBand(0, 90.0f, PluginProcessor::lowShelf, 0.7f);
        updateBand(1, 1400.0f, PluginProcessor::bell, 1.0f);
        updateBand(2, 9000.0f, PluginProcessor::highShelf, 0.7f);

        for (int i = 3; i < PluginProcessor::maxPoints; ++i)
        {
            auto point = processor.getPoint(i);
            if (point.enabled)
            {
                point.enabled = false;
                processor.setPoint(i, point);
            }
        }
    }

    void updateBand(int index, float defaultFrequency, int type, float defaultQ)
    {
        auto point = processor.getPoint(index);
        point.enabled = true;
        if (point.frequency <= 0.0f)
            point.frequency = defaultFrequency;
        if (!std::isfinite(point.frequency))
            point.frequency = defaultFrequency;
        point.type = type;
        point.stereoMode = PluginProcessor::stereo;
        point.slopeIndex = 1;
        if (point.q < 0.2f || !std::isfinite(point.q))
            point.q = defaultQ;
        processor.setPoint(index, point);
    }

    PluginProcessor& processor;
    ToggleField eqBypass;
    ToggleField order;
    BasicEqDisplay visualizer;
    KnobField lowFreq;
    KnobField lowGain;
    KnobField midFreq;
    KnobField midGain;
    KnobField midQ;
    KnobField highFreq;
    KnobField highGain;
};

class SaturationSection final : public juce::Component
{
public:
    explicit SaturationSection(PluginProcessor& processorToUse)
        : processor(processorToUse),
          bypass(processor.apvts, "satBypass", "Sat In"),
          position(processor.apvts, "satPosition", "Place", juce::StringArray{"Pre EQ", "Post EQ", "Post Dyn"}),
          drive(processor.apvts, "satDrive", "Drive"),
          mix(processor.apvts, "satMix", "Mix"),
          output(processor.apvts, "satOutput", "Trim"),
          character(processor.apvts, "satCharacter", "Mode", PluginProcessor::getSaturationCharacterNames()),
          harmonics(processor)
    {
        addAndMakeVisible(bypass);
        addAndMakeVisible(position);
        addAndMakeVisible(drive);
        addAndMakeVisible(mix);
        addAndMakeVisible(output);
        addAndMakeVisible(character);
        addAndMakeVisible(harmonics);

        drive.setTextFormatting([](double value) { return formatDb(value); });
        mix.setTextFormatting([](double value) { return formatPercent(value); });
        output.setTextFormatting([](double value) { return formatDb(value); });
    }

    void paint(juce::Graphics& g) override
    {
        fillPanel(g,
                  getLocalBounds().toFloat(),
                  "Saturation",
                  processor.apvts.getRawParameterValue("satBypass")->load() >= 0.5f,
                  processor.getStageMeterSnapshot(PluginProcessor::moduleSaturation).clipped,
                  juce::Colour(0xffff8f65));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight);
        auto top = area.removeFromTop(28);
        bypass.setBounds(top.removeFromLeft(top.getWidth() / 3).reduced(2));
        position.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(2));
        character.setBounds(top.reduced(2));

        auto knobs = area.removeFromTop(76);
        const int width = knobs.getWidth() / 3;
        drive.setBounds(knobs.removeFromLeft(width).reduced(2));
        mix.setBounds(knobs.removeFromLeft(width).reduced(2));
        output.setBounds(knobs.reduced(2));
        harmonics.setBounds(area.reduced(2));
    }

private:
    PluginProcessor& processor;
    ToggleField bypass;
    DropdownField position;
    KnobField drive;
    KnobField mix;
    KnobField output;
    DropdownField character;
    HarmonicsDisplay harmonics;
};

class OutputRail final : public juce::Component
{
public:
    explicit OutputRail(PluginProcessor& processorToUse)
        : processor(processorToUse),
          bypass(processor.apvts, "outputBypass", "Out In"),
          meterMode(processor.apvts, "outputMeterMode", "Meter", juce::StringArray{"Peak", "VU"}),
          fader(processor.apvts, "outputGain", "Fader"),
          pan(processor.apvts, "outputBalance", "Pan"),
          width(processor.apvts, "outputWidth", "Width"),
          ceiling(processor.apvts, "outputCeiling", "Ceil"),
          limiter(processor.apvts, "outputLimiter", "Clip"),
          meter(processor)
    {
        addAndMakeVisible(bypass);
        addAndMakeVisible(meterMode);
        addAndMakeVisible(fader);
        addAndMakeVisible(meter);
        addAndMakeVisible(pan);
        addAndMakeVisible(width);
        addAndMakeVisible(ceiling);
        addAndMakeVisible(limiter);

        pan.setTextFormatting([](double value) { return formatPan(value); });
        width.setTextFormatting([](double value) { return formatPercentFromUnit(value); });
        ceiling.setTextFormatting([](double value) { return formatDb(value); });
    }

    void paint(juce::Graphics& g) override
    {
        fillPanel(g,
                  getLocalBounds().toFloat(),
                  "Output",
                  processor.apvts.getRawParameterValue("outputBypass")->load() >= 0.5f,
                  processor.getStageMeterSnapshot(PluginProcessor::moduleOutput).clipped,
                  accentSoft());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(sectionHeaderHeight);
        bypass.setBounds(area.removeFromTop(28).reduced(2));
        meterMode.setBounds(area.removeFromTop(38).reduced(2));
        area.removeFromTop(2);
        fader.setBounds(area.removeFromTop(310).reduced(2));
        meter.setBounds(area.removeFromTop(280).reduced(2));
        pan.setBounds(area.removeFromTop(76).reduced(2));
        width.setBounds(area.removeFromTop(76).reduced(2));
        ceiling.setBounds(area.removeFromTop(76).reduced(2));
        limiter.setBounds(area.removeFromTop(28).reduced(2));
    }

private:
    PluginProcessor& processor;
    ToggleField bypass;
    DropdownField meterMode;
    FaderField fader;
    KnobField pan;
    KnobField width;
    KnobField ceiling;
    ToggleField limiter;
    LargeOutputMeter meter;
};
} // namespace

class ChannelStripRootComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    explicit ChannelStripRootComponent(PluginProcessor& processorToUse)
        : processor(processorToUse),
          flow(processor),
          input(processor),
          dynamics(processor),
          eq(processor),
          saturation(processor),
          output(processor),
          presetLabel("Preset", "Preset"),
          globalBypassAttachment(processor.apvts, "globalBypass", globalBypass)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(presetBox);
        addAndMakeVisible(undoButton);
        addAndMakeVisible(redoButton);
        addAndMakeVisible(globalBypass);
        addAndMakeVisible(settingsButton);
        addAndMakeVisible(flow);
        addAndMakeVisible(input);
        addAndMakeVisible(dynamics);
        addAndMakeVisible(eq);
        addAndMakeVisible(saturation);
        addAndMakeVisible(output);

        titleLabel.setText("SSP CHANNEL STRIP", juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, brightText());
        titleLabel.setFont(juce::Font(15.5f, juce::Font::bold));

        subtitleLabel.setText("reactor-style strip  /  fast channel workflow", juce::dontSendNotification);
        subtitleLabel.setColour(juce::Label::textColourId, subtleText());
        subtitleLabel.setFont(juce::Font(9.0f));

        presetBox.addItem("Factory Preset", 1);
        int presetId = 2;
        for (const auto& preset : PluginProcessor::getFactoryPresetNames())
            presetBox.addItem(preset, presetId++);
        presetBox.onChange = [this]
        {
            const int index = presetBox.getSelectedItemIndex() - 1;
            if (index >= 0 && index < PluginProcessor::getFactoryPresetNames().size())
                processor.applyFactoryPreset(PluginProcessor::getFactoryPresetNames()[index]);
        };

        undoButton.setButtonText("Undo");
        redoButton.setButtonText("Redo");
        settingsButton.setButtonText("Link");
        undoButton.setEnabled(false);
        redoButton.setEnabled(false);

        globalBypass.setButtonText("Power");

        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        juce::ColourGradient background(backgroundTop(), 0.0f, 0.0f,
                                        backgroundBottom(), 0.0f, (float) getHeight(), false);
        background.addColour(0.58, juce::Colour(0xff0d1217));
        g.setGradientFill(background);
        g.fillAll();

        g.setColour(juce::Colour(0x08ffffff));
        for (int y = 0; y < getHeight(); y += 24)
            g.drawHorizontalLine(y, 0.0f, (float) getWidth());
        g.setColour(juce::Colour(0x10ffffff));
        for (int x = 0; x < getWidth(); x += 24)
            g.drawVerticalLine(x, 0.0f, (float) getHeight());

        auto header = getLocalBounds().reduced(10).removeFromTop(64).toFloat();
        juce::ColourGradient headerFill(juce::Colour(0x22000000), header.getTopLeft(),
                                        juce::Colour(0x11000000), header.getBottomLeft(), false);
        g.setGradientFill(headerFill);
        g.fillRoundedRectangle(header, panelRadius());
        g.setColour(panelInnerOutline().withAlpha(0.85f));
        g.drawRoundedRectangle(header.reduced(0.5f), panelRadius(), 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto topBar = area.removeFromTop(64);
        auto leftTop = topBar.removeFromLeft(188);
        titleLabel.setBounds(leftTop.removeFromTop(26));
        subtitleLabel.setBounds(leftTop);

        auto rightTop = topBar;
        settingsButton.setBounds(rightTop.removeFromRight(54));
        rightTop.removeFromRight(4);
        globalBypass.setBounds(rightTop.removeFromRight(98));
        rightTop.removeFromRight(4);
        redoButton.setBounds(rightTop.removeFromRight(52));
        rightTop.removeFromRight(4);
        undoButton.setBounds(rightTop.removeFromRight(52));
        rightTop.removeFromRight(4);
        presetBox.setBounds(rightTop);

        flow.setBounds(area.removeFromTop(90));
        area.removeFromTop(6);

        auto rightRail = area.removeFromRight(outputRailWidth);
        output.setBounds(rightRail);
        area.removeFromRight(6);

        input.setBounds(area.removeFromTop(154));
        area.removeFromTop(6);
        dynamics.setBounds(area.removeFromTop(398));
        area.removeFromTop(6);
        eq.setBounds(area.removeFromTop(286));
        area.removeFromTop(6);
        saturation.setBounds(area);
    }

private:
    void timerCallback() override
    {
        repaint();
        resized();
    }

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    ssp::ui::SSPDropdown presetBox;
    ssp::ui::SSPButton undoButton;
    ssp::ui::SSPButton redoButton;
    ssp::ui::SSPToggle globalBypass;
    ssp::ui::SSPButton settingsButton;
    SignalFlowComponent flow;
    InputSection input;
    DynamicsSection dynamics;
    EqSection eq;
    SaturationSection saturation;
    OutputRail output;
    juce::Label presetLabel;
    ButtonAttachment globalBypassAttachment;
};

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      root(std::make_unique<ChannelStripRootComponent>(p))
{
    setResizable(true, true);
    setResizeLimits(360, 980, 520, 1320);
    setSize(editorWidth, editorHeight);
    addAndMakeVisible(*root);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint(juce::Graphics&) {}

void PluginEditor::resized()
{
    root->setBounds(getLocalBounds());
}
