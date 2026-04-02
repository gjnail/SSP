#include "PluginEditor.h"

#include "BitcrusherModuleComponent.h"
#include "ChorusModuleComponent.h"
#include "CompressorModuleComponent.h"
#include "DelayModuleComponent.h"
#include "DistortionModuleComponent.h"
#include "EQModuleComponent.h"
#include "FXModuleComponent.h"
#include "FilterModuleComponent.h"
#include "FlangerModuleComponent.h"
#include "GateModuleComponent.h"
#include "ImagerModuleComponent.h"
#include "MultibandCompressorModuleComponent.h"
#include "PhaserModuleComponent.h"
#include "ReverbModuleComponent.h"
#include "ShiftModuleComponent.h"
#include "TremoloModuleComponent.h"

#include <limits>

namespace
{
constexpr int nominalEditorWidth = 1480;
constexpr int nominalEditorHeight = 1240;
constexpr float minimumEditorScale = 0.70f;
constexpr float maximumEditorScale = 1.45f;
constexpr float defaultEditorScale = 0.85f;
constexpr std::array<float, 6> editorScaleChoices{{ 0.70f, 0.85f, 1.00f, 1.15f, 1.30f, 1.45f }};

int roundScaledDimension(int dimension, float scale)
{
    return juce::roundToInt((float) dimension * scale);
}

juce::String formatEditorScaleLabel(float scale)
{
    return juce::String(juce::roundToInt(scale * 100.0f)) + "%";
}

int editorScaleToChoiceId(float scale)
{
    int bestIndex = 0;
    float bestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < (int) editorScaleChoices.size(); ++i)
    {
        const float distance = std::abs(editorScaleChoices[(size_t) i] - scale);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex + 1;
}

float choiceIdToEditorScale(int choiceId)
{
    if (! juce::isPositiveAndBelow(choiceId - 1, (int) editorScaleChoices.size()))
        return defaultEditorScale;

    return editorScaleChoices[(size_t) (choiceId - 1)];
}

juce::String getFXOrderParamID(int slotIndex)
{
    return "fxOrder" + juce::String(slotIndex + 1);
}

juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(size >= 14.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(just);
}

std::unique_ptr<FXModuleComponent> createFXComponent(PluginProcessor& processor,
                                                     juce::AudioProcessorValueTreeState& apvts,
                                                     int slotIndex,
                                                     int moduleType)
{
    switch (moduleType)
    {
        case 1: return std::make_unique<DistortionModuleComponent>(processor, apvts, slotIndex);
        case 2: return std::make_unique<FilterModuleComponent>(processor, apvts, slotIndex);
        case 3: return std::make_unique<ChorusModuleComponent>(processor, apvts, slotIndex);
        case 4: return std::make_unique<FlangerModuleComponent>(processor, apvts, slotIndex);
        case 5: return std::make_unique<PhaserModuleComponent>(processor, apvts, slotIndex);
        case 6: return std::make_unique<DelayModuleComponent>(processor, apvts, slotIndex);
        case 7: return std::make_unique<ReverbModuleComponent>(processor, apvts, slotIndex);
        case 8: return std::make_unique<CompressorModuleComponent>(processor, apvts, slotIndex);
        case 9: return std::make_unique<EQModuleComponent>(processor, apvts, slotIndex);
        case 10: return std::make_unique<MultibandCompressorModuleComponent>(processor, apvts, slotIndex);
        case 11: return std::make_unique<BitcrusherModuleComponent>(processor, apvts, slotIndex);
        case 12: return std::make_unique<ImagerModuleComponent>(processor, apvts, slotIndex);
        case 13: return std::make_unique<ShiftModuleComponent>(processor, apvts, slotIndex);
        case 14: return std::make_unique<TremoloModuleComponent>(processor, apvts, slotIndex);
        case 15: return std::make_unique<GateModuleComponent>(processor, apvts, slotIndex);
        default: return nullptr;
    }
}

class AttachedKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String(double)>;

    AttachedKnob(juce::AudioProcessorValueTreeState& state,
                 const juce::String& parameterId,
                 const juce::String& labelText,
                 juce::Colour accent,
                 Formatter formatter = {})
        : attachment(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, parameterId, slider)),
          textFormatter(std::move(formatter))
    {
        addAndMakeVisible(label);
        addAndMakeVisible(slider);

        styleLabel(label, 11.5f, reactorui::textMuted(), juce::Justification::centred);
        label.setText(labelText, juce::dontSendNotification);

        reactorui::styleKnobSlider(slider, accent, 74, 22);
        slider.setDoubleClickReturnValue(true, slider.getValue());

        if (textFormatter)
            slider.textFromValueFunction = [this](double value) { return textFormatter(value); };
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(18));
        slider.setBounds(area);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    Formatter textFormatter;
};

class ScaledModuleHost final : public juce::Component
{
public:
    void setModule(FXModuleComponent* moduleToShow)
    {
        if (module == moduleToShow)
            return;

        module = moduleToShow;
        resized();
    }

    void resized() override
    {
        if (module == nullptr)
            return;

        const auto bounds = getLocalBounds();
        const int viewportWidth = juce::jmax(220, bounds.getWidth());
        const int viewportHeight = juce::jmax(220, bounds.getHeight());
        const int preferredHeight = juce::jmax(220, module->getPreferredHeight());
        const float fitScale = juce::jmin(1.0f, (float) viewportHeight / (float) preferredHeight);
        const int logicalWidth = juce::jmax(viewportWidth, juce::roundToInt((float) viewportWidth / fitScale));
        const float scaledWidth = (float) logicalWidth * fitScale;
        const float scaledHeight = (float) preferredHeight * fitScale;
        const float offsetX = 0.5f * ((float) viewportWidth - scaledWidth);
        const float offsetY = 0.5f * ((float) viewportHeight - scaledHeight);

        module->setBounds(0, 0, logicalWidth, preferredHeight);
        module->setTransform(juce::AffineTransform::scale(fitScale).translated(offsetX, offsetY));
    }

private:
    FXModuleComponent* module = nullptr;
};

class TimeField final : public juce::Component
{
public:
    TimeField(juce::AudioProcessorValueTreeState& state,
              const juce::String& parameterId,
              const juce::String& labelText,
              juce::Colour accent)
        : attachment(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, parameterId, slider))
    {
        addAndMakeVisible(label);
        addAndMakeVisible(slider);

        styleLabel(label, 11.0f, reactorui::textMuted(), juce::Justification::centred);
        label.setText(labelText, juce::dontSendNotification);

        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff111821));
        slider.setColour(juce::Slider::trackColourId, accent.withAlpha(0.82f));
        slider.setColour(juce::Slider::thumbColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, reactorui::textStrong());
        slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
        slider.textFromValueFunction = [] (double value) { return juce::String(value, 2) + " ms"; };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.retainCharacters("0123456789.").getDoubleValue();
        };
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(18));
        slider.setBounds(area);
    }

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class MeterComponent final : public juce::Component,
                             private juce::Timer
{
public:
    using Getter = std::function<PluginProcessor::StereoMeterSnapshot()>;

    MeterComponent(Getter getterToUse, juce::Colour accentToUse, bool showRmsToUse)
        : getter(std::move(getterToUse)),
          accent(accentToUse),
          showRms(showRmsToUse)
    {
        startTimerHz(24);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reactorui::drawDisplayPanel(g, bounds, accent);
        auto plot = bounds.reduced(14.0f, 12.0f);
        const float gap = 10.0f;
        auto leftBar = plot.removeFromLeft((plot.getWidth() - gap) * 0.5f);
        plot.removeFromLeft(gap);
        auto rightBar = plot;

        auto drawBar = [&](juce::Rectangle<float> area, float peak, float rms)
        {
            g.setColour(juce::Colour(0xff0c131a));
            g.fillRoundedRectangle(area, 4.0f);

            const float peakHeight = area.getHeight() * juce::jlimit(0.0f, 1.1f, peak);
            const float rmsHeight = area.getHeight() * juce::jlimit(0.0f, 1.0f, rms);

            if (showRms)
            {
                g.setColour(accent.withAlpha(0.25f));
                g.fillRoundedRectangle(area.withTrimmedTop(area.getHeight() - rmsHeight), 4.0f);
            }

            juce::ColourGradient fill(accent.brighter(0.1f), area.getX(), area.getBottom(),
                                      accent.darker(0.4f), area.getX(), area.getY(), false);
            g.setGradientFill(fill);
            g.fillRoundedRectangle(area.withTrimmedTop(area.getHeight() - peakHeight), 4.0f);
            g.setColour(reactorui::outline());
            g.drawRoundedRectangle(area, 4.0f, 1.0f);
        };

        drawBar(leftBar, snapshot.leftPeak, snapshot.leftRms);
        drawBar(rightBar, snapshot.rightPeak, snapshot.rightRms);

        if (snapshot.clipped)
        {
            g.setColour(juce::Colour(0xffff5e4f));
            g.setFont(reactorui::sectionFont(11.0f));
            g.drawFittedText("CLIP", getLocalBounds().removeFromTop(18), juce::Justification::centred, 1);
        }
    }

private:
    void timerCallback() override
    {
        snapshot = getter();
        repaint();
    }

    Getter getter;
    juce::Colour accent;
    bool showRms = false;
    PluginProcessor::StereoMeterSnapshot snapshot;
};

class VisualizerComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit VisualizerComponent(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reactorui::drawDisplayPanel(g, bounds, juce::Colour(0xffff9b54));
        auto plot = bounds.reduced(16.0f, 14.0f);
        reactorui::drawSubtleGrid(g, plot, reactorui::displayLine().withAlpha(0.22f), 8, 4);

        juce::Path waveformPath;
        juce::Path transientPath;
        juce::Path bodyPath;

        for (int i = 0; i < PluginProcessor::visualizerHistorySize; ++i)
        {
            const int index = (writePosition + i) % PluginProcessor::visualizerHistorySize;
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (PluginProcessor::visualizerHistorySize - 1);
            const float waveformY = juce::jmap(waveform[(size_t) index], 0.0f, 1.0f, plot.getBottom(), plot.getY());
            const float transientY = juce::jmap(transient[(size_t) index], 0.0f, 1.0f, plot.getBottom(), plot.getY());
            const float bodyY = juce::jmap(body[(size_t) index], 0.0f, 1.0f, plot.getBottom(), plot.getY());

            if (i == 0)
            {
                waveformPath.startNewSubPath(x, waveformY);
                transientPath.startNewSubPath(x, transientY);
                bodyPath.startNewSubPath(x, bodyY);
            }
            else
            {
                waveformPath.lineTo(x, waveformY);
                transientPath.lineTo(x, transientY);
                bodyPath.lineTo(x, bodyY);
            }
        }

        g.setColour(juce::Colour(0xff9daab8).withAlpha(0.72f));
        g.strokePath(waveformPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffffaa45));
        g.strokePath(transientPath, juce::PathStrokeType(2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff65d5ff));
        g.strokePath(bodyPath, juce::PathStrokeType(2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void timerCallback() override
    {
        processor.getVisualizerData(waveform, transient, body, writePosition);
        repaint();
    }

    PluginProcessor& processor;
    std::array<float, PluginProcessor::visualizerHistorySize> waveform{};
    std::array<float, PluginProcessor::visualizerHistorySize> transient{};
    std::array<float, PluginProcessor::visualizerHistorySize> body{};
    int writePosition = 0;
};

class BeforeAfterVisualizerComponent final : public juce::Component,
                                            private juce::Timer
{
public:
    explicit BeforeAfterVisualizerComponent(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reactorui::drawDisplayPanel(g, bounds, juce::Colour(0xff65d5ff));
        auto plot = bounds.reduced(12.0f, 12.0f);
        auto beforePlot = plot.removeFromLeft((plot.getWidth() - 10.0f) * 0.5f);
        auto divider = plot.removeFromLeft(10.0f);
        auto afterPlot = plot;

        auto drawOne = [&g, this](juce::Rectangle<float> area,
                                  const std::array<float, PluginProcessor::visualizerHistorySize>& values,
                                  juce::Colour accent,
                                  const juce::String& label)
        {
            reactorui::drawSubtleGrid(g, area, reactorui::displayLine().withAlpha(0.18f), 4, 4);

            juce::Path path;
            for (int i = 0; i < PluginProcessor::visualizerHistorySize; ++i)
            {
                const int index = (writePosition + i) % PluginProcessor::visualizerHistorySize;
                const float x = area.getX() + area.getWidth() * (float) i / (float) (PluginProcessor::visualizerHistorySize - 1);
                const float y = juce::jmap(values[(size_t) index], 0.0f, 1.0f, area.getBottom(), area.getY());

                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            g.setColour(accent);
            g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            auto labelArea = area.withHeight(18.0f).translated(0.0f, -2.0f);
            g.setFont(reactorui::sectionFont(10.5f));
            g.setColour(accent);
            g.drawText(label, labelArea, juce::Justification::topLeft);
        };

        drawOne(beforePlot, before, juce::Colour(0xffaab6c4).withAlpha(0.90f), "BEFORE");
        drawOne(afterPlot, after, juce::Colour(0xff77e3ff), "AFTER");

        g.setColour(reactorui::displayLine().withAlpha(0.75f));
        g.drawLine(divider.getCentreX(), divider.getY(), divider.getCentreX(), divider.getBottom(), 1.5f);
    }

private:
    void timerCallback() override
    {
        processor.getBeforeAfterVisualizerData(before, after, writePosition);
        repaint();
    }

    PluginProcessor& processor;
    std::array<float, PluginProcessor::visualizerHistorySize> before{};
    std::array<float, PluginProcessor::visualizerHistorySize> after{};
    int writePosition = 0;
};

class TriggerChoice final : public juce::Component
{
public:
    TriggerChoice(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& labelText)
    {
        addAndMakeVisible(label);
        addAndMakeVisible(box);
        styleLabel(label, 11.5f, reactorui::textMuted(), juce::Justification::centred);
        label.setText(labelText, juce::dontSendNotification);

        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id)))
        {
            const auto choices = param->choices;
            for (int i = 0; i < choices.size(); ++i)
                box.addItem(choices[i], i + 1);
        }

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, box);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds(area.removeFromTop(18));
        box.setBounds(area.removeFromTop(30));
    }

private:
    juce::Label label;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class SlotHeaderButton final : public juce::TextButton
{
public:
    explicit SlotHeaderButton(const juce::String& text)
        : juce::TextButton(text)
    {
        setClickingTogglesState(false);
    }
};

class EffectSlotComponent final : public juce::Component
{
public:
    EffectSlotComponent(PluginProcessor& processorToUse, int pathToUse, int localSlotToUse, juce::Colour accentToUse)
        : processor(processorToUse),
          path(pathToUse),
          localSlot(localSlotToUse),
          globalSlot(processor.getGlobalSlotIndex(pathToUse, localSlotToUse)),
          accent(accentToUse)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(stateLabel);
        addAndMakeVisible(onButton);
        addAndMakeVisible(removeButton);

        styleLabel(titleLabel, 12.0f, reactorui::textStrong());
        styleLabel(stateLabel, 10.5f, reactorui::textMuted());
        titleLabel.setInterceptsMouseClicks(false, false);
        stateLabel.setInterceptsMouseClicks(false, false);

        onButton.setButtonText("On");
        onButton.setClickingTogglesState(true);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, getFXSlotOnParamID(globalSlot), onButton);
        onButton.onClick = [this]
        {
            if (onSelectRequested)
                onSelectRequested(localSlot);
        };

        removeButton.setButtonText("X");

        for (auto* button : { &removeButton })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141a20));
            button->setColour(juce::TextButton::buttonOnColourId, accent);
            button->setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
            button->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
        }

        removeButton.onClick = [this]
        {
            if (onSelectRequested)
                onSelectRequested(localSlot);
            processor.removeFXModuleFromPath(path, localSlot);
            if (onContentChanged)
                onContentChanged();
        };
    }

    void refresh()
    {
        const int moduleType = processor.getFXSlotType(globalSlot);
        const bool active = moduleType != 0;
        const bool enabled = processor.isFXSlotActive(globalSlot);

        titleLabel.setText("Slot " + juce::String(localSlot + 1) + "  " + PluginProcessor::getFXModuleNames()[moduleType],
                           juce::dontSendNotification);
        stateLabel.setText(active ? (enabled ? "Active Reactor module  |  Drag to reorder" : "Bypassed Reactor module  |  Drag to reorder")
                                  : "Empty slot",
                           juce::dontSendNotification);

        removeButton.setEnabled(active);
        onButton.setEnabled(active);
        currentModuleType = moduleType;

        repaint();
    }

    int getPreferredHeight() const
    {
        return 52;
    }

    void setSelected(bool shouldBeSelected)
    {
        if (selected != shouldBeSelected)
        {
            selected = shouldBeSelected;
            repaint();
        }
    }

    void setDropTarget(bool shouldBeDropTarget)
    {
        if (dropTarget != shouldBeDropTarget)
        {
            dropTarget = shouldBeDropTarget;
            repaint();
        }
    }

    int getModuleType() const noexcept
    {
        return currentModuleType;
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff111821));
        g.fillRoundedRectangle(bounds, 9.0f);
        g.setColour((selected || dropTarget) ? accent.withAlpha(dropTarget ? 1.0f : 0.92f) : reactorui::outline());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 9.0f, selected ? 1.6f : 1.0f);

        if (selected)
        {
            g.setColour(accent.withAlpha(0.10f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 8.5f);
        }

        if (dropTarget)
        {
            g.setColour(accent.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 8.5f);
        }

        if (currentModuleType != 0)
        {
            const auto handleArea = getLocalBounds().removeFromRight(16).reduced(4, 12).toFloat();
            g.setColour(reactorui::textMuted().withAlpha(0.68f));
            for (int row = 0; row < 3; ++row)
            {
                const float y = handleArea.getY() + 5.0f + row * 8.0f;
                g.fillEllipse(handleArea.getX() + 1.5f, y, 2.2f, 2.2f);
                g.fillEllipse(handleArea.getX() + 7.0f, y, 2.2f, 2.2f);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 8);
        auto buttons = area.removeFromRight(88);
        onButton.setBounds(buttons.removeFromLeft(42));
        buttons.removeFromLeft(6);
        removeButton.setBounds(buttons.removeFromLeft(28));

        titleLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        stateLabel.setBounds(area);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        dragging = false;
        if (onSelectRequested)
            onSelectRequested(localSlot);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (currentModuleType == 0)
            return;

        if (! dragging && event.getDistanceFromDragStart() > 4)
        {
            dragging = true;
            if (onDragStarted)
                onDragStarted(localSlot);
        }

        if (dragging && onDragMoved)
            onDragMoved(localSlot, event.getEventRelativeTo(getParentComponent()).position);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (dragging && onDragFinished)
            onDragFinished(localSlot, event.getEventRelativeTo(getParentComponent()).position);

        dragging = false;
    }

    std::function<void()> onContentChanged;
    std::function<void(int)> onSelectRequested;
    std::function<void(int)> onDragStarted;
    std::function<void(int, juce::Point<float>)> onDragMoved;
    std::function<void(int, juce::Point<float>)> onDragFinished;

private:
    PluginProcessor& processor;
    int path = 0;
    int localSlot = 0;
    int globalSlot = 0;
    juce::Colour accent;
    int currentModuleType = -1;
    bool selected = false;
    bool dropTarget = false;
    bool dragging = false;
    juce::Label titleLabel;
    juce::Label stateLabel;
    juce::ToggleButton onButton;
    SlotHeaderButton removeButton{"X"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
};

class EffectChainComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    EffectChainComponent(PluginProcessor& processorToUse,
                         int pathToUse,
                         juce::String titleText,
                         juce::Colour accentToUse,
                         const juce::String& levelParam,
                         const juce::String& panParam,
                         const juce::String& muteParam,
                         const juce::String& soloParam,
                         std::function<PluginProcessor::StereoMeterSnapshot()> meterGetter)
        : processor(processorToUse),
          path(pathToUse),
          title(std::move(titleText)),
          accent(accentToUse),
          levelKnob(processor.apvts, levelParam, "Level", accent, [] (double value)
          {
              if (value <= -99.0)
                  return juce::String("-inf");
              return juce::String(value, 1) + " dB";
          }),
          panKnob(processor.apvts, panParam, "Pan", accent, [] (double value)
          {
              const float pan = (float) value;
              if (std::abs(pan) < 0.001f)
                  return juce::String("C");
              return pan < 0.0f ? "L" + juce::String(juce::roundToInt(std::abs(pan) * 100.0f))
                                : "R" + juce::String(juce::roundToInt(std::abs(pan) * 100.0f));
          }),
          meter(std::move(meterGetter), accent, false),
          muteAttachment(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, muteParam, muteButton)),
          soloAttachment(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, soloParam, soloButton))
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(slotListLabel);
        addAndMakeVisible(slotListViewport);
        addAndMakeVisible(addBox);
        addAndMakeVisible(addButton);
        addAndMakeVisible(inspectorTitleLabel);
        addAndMakeVisible(inspectorHintLabel);
        addAndMakeVisible(inspectorViewport);
        addAndMakeVisible(levelKnob);
        addAndMakeVisible(panKnob);
        addAndMakeVisible(muteButton);
        addAndMakeVisible(soloButton);
        addAndMakeVisible(meter);

        titleLabel.setText(title, juce::dontSendNotification);
        subtitleLabel.setText("Eight serial Reactor slots dedicated to this side of the split.", juce::dontSendNotification);
        slotListLabel.setText("Chain Slots", juce::dontSendNotification);
        inspectorTitleLabel.setText("Selected Slot", juce::dontSendNotification);
        inspectorHintLabel.setText("Select a slot on the left to inspect or tweak its controls here.", juce::dontSendNotification);
        styleLabel(titleLabel, 14.0f, reactorui::textStrong());
        styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
        styleLabel(slotListLabel, 11.0f, reactorui::textMuted());
        styleLabel(inspectorTitleLabel, 12.5f, reactorui::textStrong());
        styleLabel(inspectorHintLabel, 10.5f, reactorui::textMuted());

        slotListViewport.setViewedComponent(&slotListContent, false);
        slotListViewport.setScrollBarsShown(true, false);
        slotListViewport.setScrollBarThickness(10);

        inspectorViewport.setViewedComponent(&inspectorContent, false);
        inspectorViewport.setScrollBarsShown(false, false);
        inspectorContent.addAndMakeVisible(inspectorHost);

        addBox.clear(juce::dontSendNotification);
        for (int moduleType = 1; moduleType < PluginProcessor::getFXModuleNames().size(); ++moduleType)
            addBox.addItem(PluginProcessor::getFXModuleNames()[moduleType], moduleType);
        addBox.setSelectedId(1, juce::dontSendNotification);

        addButton.setButtonText("Add");
        addButton.onClick = [this]
        {
            processor.addFXModule(path, addBox.getSelectedId());
            if (const int firstLoaded = findFirstLoadedSlot(); firstLoaded >= 0)
                selectedSlot = firstLoaded;
            rebuild();
        };

        muteButton.setButtonText("Mute");
        muteButton.setClickingTogglesState(true);
        soloButton.setButtonText("Solo");
        soloButton.setClickingTogglesState(true);

        for (int slot = 0; slot < PluginProcessor::slotsPerPath; ++slot)
        {
            auto slotComponent = std::make_unique<EffectSlotComponent>(processor, path, slot, accent);
            slotComponent->onContentChanged = [this] { rebuild(); };
            slotComponent->onSelectRequested = [this](int slotIndex)
            {
                selectedSlot = juce::jlimit(0, PluginProcessor::slotsPerPath - 1, slotIndex);
                rebuildInspector();
            };
            slotComponent->onDragStarted = [this](int slotIndex)
            {
                dragSourceSlot = slotIndex;
                dropTargetSlot = slotIndex;
                updateSlotDropTargets();
            };
            slotComponent->onDragMoved = [this](int, juce::Point<float> position)
            {
                updateDragTarget(position);
            };
            slotComponent->onDragFinished = [this](int, juce::Point<float> position)
            {
                finishDrag(position);
            };
            slotListContent.addAndMakeVisible(*slotComponent);
            slots.push_back(std::move(slotComponent));
        }

        startTimerHz(8);
        rebuild();
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 14.0f);

        for (const auto area : { slotListBounds, inspectorBounds, mixerBounds })
        {
            g.setColour(juce::Colour(0xff10161e));
            g.fillRoundedRectangle(area.toFloat(), 12.0f);
            g.setColour(reactorui::outline());
            g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 12.0f, 1.0f);
        }

        if (inspectorModule == nullptr)
        {
            auto placeholder = inspectorViewport.getBounds().reduced(18, 18);
            g.setColour(accent.withAlpha(0.12f));
            g.fillRoundedRectangle(placeholder.toFloat(), 12.0f);
            g.setColour(reactorui::textMuted());
            g.setFont(reactorui::bodyFont(12.0f));
            g.drawFittedText("Pick a slot to edit, or add a Reactor effect to this path.",
                             placeholder, juce::Justification::centred, 2);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(area.removeFromTop(20));
        subtitleLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);

        auto addRow = area.removeFromTop(34);
        addButton.setBounds(addRow.removeFromRight(74));
        addRow.removeFromRight(8);
        addBox.setBounds(addRow);

        area.removeFromTop(10);
        auto bottom = area.removeFromBottom(134);
        mixerBounds = bottom;

        auto contentArea = area;
        auto slotArea = contentArea.removeFromLeft(252);
        contentArea.removeFromLeft(12);
        auto inspectorArea = contentArea;

        slotListBounds = slotArea;
        inspectorBounds = inspectorArea;

        auto slotInner = slotArea.reduced(12, 12);
        slotListLabel.setBounds(slotInner.removeFromTop(18));
        slotInner.removeFromTop(8);
        slotListViewport.setBounds(slotInner);
        layoutSlots();

        auto inspectorInner = inspectorArea.reduced(12, 12);
        inspectorTitleLabel.setBounds(inspectorInner.removeFromTop(20));
        inspectorHintLabel.setBounds(inspectorInner.removeFromTop(18));
        inspectorInner.removeFromTop(8);
        inspectorViewport.setBounds(inspectorInner);

        auto mixerInner = bottom.reduced(14, 12);
        auto knobStrip = mixerInner.removeFromTop(86);
        levelKnob.setBounds(knobStrip.removeFromLeft(112));
        knobStrip.removeFromLeft(12);
        panKnob.setBounds(knobStrip.removeFromLeft(112));
        knobStrip.removeFromLeft(16);
        meter.setBounds(knobStrip.removeFromRight(96));

        auto buttons = mixerInner.removeFromTop(28);
        muteButton.setBounds(buttons.removeFromLeft(74));
        buttons.removeFromLeft(8);
        soloButton.setBounds(buttons.removeFromLeft(74));

        layoutInspector();
    }

private:
    void timerCallback() override
    {
        rebuild();
    }

    void rebuild()
    {
        for (auto& slot : slots)
            slot->refresh();

        selectedSlot = juce::jlimit(0, PluginProcessor::slotsPerPath - 1, selectedSlot);
        if (const int firstLoaded = findFirstLoadedSlot(); firstLoaded >= 0 && slots[(size_t) selectedSlot]->getModuleType() == 0)
            selectedSlot = firstLoaded;

        for (int slotIndex = 0; slotIndex < (int) slots.size(); ++slotIndex)
            slots[(size_t) slotIndex]->setSelected(slotIndex == selectedSlot);
        updateSlotDropTargets();

        layoutSlots();
        layoutInspector();
        rebuildInspector();
        repaint();
    }

    int findFirstLoadedSlot() const
    {
        for (int slotIndex = 0; slotIndex < (int) slots.size(); ++slotIndex)
        {
            if (slots[(size_t) slotIndex]->getModuleType() != 0)
                return slotIndex;
        }

        return -1;
    }

    void rebuildInspector()
    {
        if (! juce::isPositiveAndBelow(selectedSlot, PluginProcessor::slotsPerPath))
            selectedSlot = 0;

        for (int slotIndex = 0; slotIndex < (int) slots.size(); ++slotIndex)
            slots[(size_t) slotIndex]->setSelected(slotIndex == selectedSlot);

        const int globalSlot = processor.getGlobalSlotIndex(path, selectedSlot);
        const int moduleType = processor.getFXSlotType(globalSlot);
        const bool active = moduleType != 0;
        const bool enabled = processor.isFXSlotActive(globalSlot);

        inspectorTitleLabel.setText("Selected Slot  " + juce::String(selectedSlot + 1) + "  " + PluginProcessor::getFXModuleNames()[moduleType],
                                    juce::dontSendNotification);
        inspectorHintLabel.setText(active
                                       ? (enabled ? "Dial in the selected Reactor effect here. The list on the left controls order and bypass."
                                                  : "This slot is currently bypassed. You can still tweak it here, then turn it back on from the list.")
                                       : "This slot is empty. Use the Add menu above, or choose another slot with an effect loaded.",
                                   juce::dontSendNotification);

        if (moduleType != currentInspectorModuleType)
        {
            currentInspectorModuleType = moduleType;
            inspectorContent.removeAllChildren();
            inspectorContent.addAndMakeVisible(inspectorHost);
            inspectorModule.reset();

            if (active)
            {
                inspectorModule = createFXComponent(processor, processor.apvts, globalSlot, moduleType);
                if (inspectorModule)
                {
                    inspectorContent.addAndMakeVisible(*inspectorModule);
                    inspectorHost.setModule(inspectorModule.get());
                }
            }
        }
        else
        {
            inspectorHost.setModule(inspectorModule.get());
        }

        layoutInspector();
    }

    void layoutSlots()
    {
        if (slots.empty() || ! slotListViewport.isVisible())
            return;

        int y = 0;
        const int width = juce::jmax(180, slotListViewport.getWidth() - slotListViewport.getScrollBarThickness() - 4);
        for (auto& slot : slots)
        {
            const int height = slot->getPreferredHeight();
            slot->setBounds(0, y, width, height);
            y += height + 8;
        }

        slotListContent.setSize(width, juce::jmax(slotListViewport.getHeight(), y));
    }

    void layoutInspector()
    {
        if (! inspectorViewport.isVisible())
            return;

        const int width = juce::jmax(200, inspectorViewport.getWidth());
        const int height = juce::jmax(220, inspectorViewport.getHeight());
        inspectorHost.setBounds(0, 0, width, height);

        if (inspectorModule != nullptr)
        {
            inspectorHost.setModule(inspectorModule.get());
            inspectorContent.setSize(width, height);
        }
        else
        {
            inspectorHost.setModule(nullptr);
            inspectorContent.setSize(width, height);
        }
    }

    void updateDragTarget(juce::Point<float> position)
    {
        if (dragSourceSlot < 0)
            return;

        const int y = juce::roundToInt(position.y);
        int bestSlot = juce::jlimit(0, PluginProcessor::slotsPerPath - 1, dragSourceSlot);
        int bestDistance = std::numeric_limits<int>::max();

        for (int slotIndex = 0; slotIndex < (int) slots.size(); ++slotIndex)
        {
            const int centre = slots[(size_t) slotIndex]->getBounds().getCentreY();
            const int distance = std::abs(centre - y);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestSlot = slotIndex;
            }
        }

        if (dropTargetSlot != bestSlot)
        {
            dropTargetSlot = bestSlot;
            updateSlotDropTargets();
        }
    }

    void finishDrag(juce::Point<float> position)
    {
        updateDragTarget(position);

        if (dragSourceSlot >= 0 && dropTargetSlot >= 0 && dragSourceSlot != dropTargetSlot)
        {
            processor.moveFXModuleWithinPath(path, dragSourceSlot, dropTargetSlot);
            selectedSlot = dropTargetSlot;
        }

        dragSourceSlot = -1;
        dropTargetSlot = -1;
        rebuild();
    }

    void updateSlotDropTargets()
    {
        for (int slotIndex = 0; slotIndex < (int) slots.size(); ++slotIndex)
            slots[(size_t) slotIndex]->setDropTarget(slotIndex == dropTargetSlot && dragSourceSlot >= 0);
    }

    PluginProcessor& processor;
    int path = 0;
    juce::String title;
    juce::Colour accent;
    int selectedSlot = 0;
    int dragSourceSlot = -1;
    int dropTargetSlot = -1;
    int currentInspectorModuleType = -1;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label slotListLabel;
    juce::Viewport slotListViewport;
    juce::Component slotListContent;
    juce::ComboBox addBox;
    juce::TextButton addButton;
    std::vector<std::unique_ptr<EffectSlotComponent>> slots;
    juce::Label inspectorTitleLabel;
    juce::Label inspectorHintLabel;
    juce::Viewport inspectorViewport;
    juce::Component inspectorContent;
    ScaledModuleHost inspectorHost;
    std::unique_ptr<FXModuleComponent> inspectorModule;
    AttachedKnob levelKnob;
    AttachedKnob panKnob;
    juce::ToggleButton muteButton;
    juce::ToggleButton soloButton;
    MeterComponent meter;
    juce::Rectangle<int> slotListBounds;
    juce::Rectangle<int> inspectorBounds;
    juce::Rectangle<int> mixerBounds;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
};

class DetectionPanel final : public juce::Component
{
public:
    explicit DetectionPanel(PluginProcessor& processorToUse)
        : processor(processorToUse),
          sensitivity(processor.apvts, "sensitivity", "Sensitivity", juce::Colour(0xffffa648), [] (double value) { return juce::String(juce::roundToInt((float) value)) + "%"; }),
          speed(processor.apvts, "speedMs", "Speed", juce::Colour(0xffffa648), [] (double value) { return juce::String(value, 2) + " ms"; }),
          hold(processor.apvts, "holdMs", "Hold", juce::Colour(0xffffa648), [] (double value) { return juce::String(value, 1) + " ms"; }),
          release(processor.apvts, "releaseMs", "Release", juce::Colour(0xffffa648), [] (double value) { return juce::String(value, 1) + " ms"; }),
          lookahead(processor.apvts, "lookaheadMs", "Lookahead", juce::Colour(0xffffa648), [] (double value) { return juce::String(value, 1) + " ms"; }),
          focus(processor.apvts, "focus", "Focus", juce::Colour(0xffffa648), [] (double value) { return juce::String(juce::roundToInt((float) value)) + "%"; }),
          sidechainHpf(processor.apvts, "sidechainHPFHz", "SC HPF", juce::Colour(0xffffa648), [] (double value)
          {
              return value <= 21.0 ? juce::String("Off") : juce::String(value, 0) + " Hz";
          }),
          triggerChoice(processor.apvts, "triggerFilter", "Trigger Filter"),
          visualizer(processor),
          beforeAfterVisualizer(processor),
          activityMeter([this] { return makeDetectionSnapshot(); }, juce::Colour(0xffffa648), false)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(visualizer);
        addAndMakeVisible(beforeAfterVisualizer);
        addAndMakeVisible(activityMeter);
        addAndMakeVisible(sensitivity);
        addAndMakeVisible(speed);
        addAndMakeVisible(hold);
        addAndMakeVisible(release);
        addAndMakeVisible(lookahead);
        addAndMakeVisible(focus);
        addAndMakeVisible(sidechainHpf);
        addAndMakeVisible(triggerChoice);

        titleLabel.setText("Detection Engine", juce::dontSendNotification);
        subtitleLabel.setText("Differential envelope splitting with lookahead, focus control, and filtered triggering.", juce::dontSendNotification);
        styleLabel(titleLabel, 14.0f, reactorui::textStrong());
        styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffffa648), 14.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(area.removeFromTop(20));
        subtitleLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);

        auto top = area.removeFromTop(190);
        auto meterArea = top.removeFromRight(54);
        top.removeFromRight(10);
        auto compareArea = top.removeFromRight(250);
        top.removeFromRight(10);
        visualizer.setBounds(top);
        beforeAfterVisualizer.setBounds(compareArea);
        activityMeter.setBounds(meterArea);

        area.removeFromTop(10);
        auto row1 = area.removeFromTop(118);
        sensitivity.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        speed.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        hold.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        release.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        lookahead.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        focus.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        sidechainHpf.setBounds(row1.removeFromLeft(110));
        row1.removeFromLeft(8);
        triggerChoice.setBounds(row1);
    }

private:
    PluginProcessor::StereoMeterSnapshot makeDetectionSnapshot() const
    {
        const float activity = processor.getDetectionActivity();
        return { activity, activity, 0.0f, 0.0f, false };
    }

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    AttachedKnob sensitivity;
    AttachedKnob speed;
    AttachedKnob hold;
    AttachedKnob release;
    AttachedKnob lookahead;
    AttachedKnob focus;
    AttachedKnob sidechainHpf;
    TriggerChoice triggerChoice;
    VisualizerComponent visualizer;
    BeforeAfterVisualizerComponent beforeAfterVisualizer;
    MeterComponent activityMeter;
};

class ManualSplitWaveform final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit ManualSplitWaveform(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
        startTimerHz(24);
    }

    void setZoomFactor(float zoomToUse)
    {
        const float clamped = juce::jlimit(1.0f, 512.0f, zoomToUse);
        if (std::abs(clamped - zoomFactor) < 0.0001f)
            return;

        zoomFactor = clamped;
        if (onZoomChanged)
            onZoomChanged(zoomFactor);
        repaint();
    }

    void setScrollProportion(float scrollToUse)
    {
        const float clamped = juce::jlimit(0.0f, 1.0f, scrollToUse);
        if (std::abs(clamped - scrollProportion) < 0.0001f)
            return;

        scrollProportion = clamped;
        if (onScrollChanged)
            onScrollChanged(scrollProportion);
        repaint();
    }

    float getZoomFactor() const noexcept { return zoomFactor; }
    float getScrollProportion() const noexcept { return scrollProportion; }

    void resetView()
    {
        setZoomFactor(1.0f);
        setScrollProportion(0.0f);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reactorui::drawDisplayPanel(g, bounds, juce::Colour(0xffffa648));
        auto plot = bounds.reduced(14.0f, 12.0f);

        const float totalDurationMs = processor.getSplitEditorCaptureDurationMs();
        const float visibleDurationMs = juce::jmax(0.5f, totalDurationMs / zoomFactor);
        const float maxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - visibleDurationMs);
        const float visibleStartMs = scrollProportion * maxVisibleStartMs;
        const float visibleEndMs = visibleStartMs + visibleDurationMs;
        auto msToX = [plot, visibleStartMs, visibleDurationMs](float valueMs)
        {
            return plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, (valueMs - visibleStartMs) / juce::jmax(1.0f, visibleDurationMs));
        };

        reactorui::drawSubtleGrid(g, plot, reactorui::displayLine().withAlpha(0.18f), zoomFactor > 6.0f ? 16 : 10, 4);

        const auto splitMode = processor.getSplitModeIndex();
        if (splitMode != PluginProcessor::splitModeAuto)
        {
            const auto markerValues = processor.getManualMarkerValuesMs();
            const float transientStartX = msToX(markerValues[PluginProcessor::manualMarkerTransientStart]);
            const float transientEndX = msToX(markerValues[PluginProcessor::manualMarkerTransientEnd]);
            const float bodyStartX = msToX(markerValues[PluginProcessor::manualMarkerBodyStart]);
            const float bodyEndX = msToX(markerValues[PluginProcessor::manualMarkerBodyEnd]);

            g.setColour(juce::Colour(0xffffa648).withAlpha(0.16f));
            g.fillRect(juce::Rectangle<float>(transientStartX, plot.getY(), juce::jmax(0.0f, transientEndX - transientStartX), plot.getHeight()));

            juce::ColourGradient crossover(juce::Colour(0xffffa648).withAlpha(0.16f), transientEndX, plot.getY(),
                                           juce::Colour(0xff65d5ff).withAlpha(0.16f), bodyStartX, plot.getY(), false);
            g.setGradientFill(crossover);
            g.fillRect(juce::Rectangle<float>(transientEndX, plot.getY(), juce::jmax(0.0f, bodyStartX - transientEndX), plot.getHeight()));

            g.setColour(juce::Colour(0xff65d5ff).withAlpha(0.14f));
            g.fillRect(juce::Rectangle<float>(bodyStartX, plot.getY(), juce::jmax(0.0f, bodyEndX - bodyStartX), plot.getHeight()));
        }

        juce::Path waveformPath;
        const float centreY = plot.getCentreY();
        const float halfHeight = plot.getHeight() * 0.46f;
        constexpr int renderPointCount = 1024;
        const float startIndex = (visibleStartMs / juce::jmax(1.0f, totalDurationMs)) * (float) (PluginProcessor::splitEditorDisplayPointCount - 1);
        const float endIndex = (visibleEndMs / juce::jmax(1.0f, totalDurationMs)) * (float) (PluginProcessor::splitEditorDisplayPointCount - 1);

        auto sampleWaveformAt = [this](float index)
        {
            const float clamped = juce::jlimit(0.0f, (float) (PluginProcessor::splitEditorDisplayPointCount - 1), index);
            const int lowerIndex = juce::jlimit(0, PluginProcessor::splitEditorDisplayPointCount - 1, (int) std::floor(clamped));
            const int upperIndex = juce::jlimit(0, PluginProcessor::splitEditorDisplayPointCount - 1, lowerIndex + 1);
            const float alpha = clamped - (float) lowerIndex;
            return juce::jmap(alpha, waveform[(size_t) lowerIndex], waveform[(size_t) upperIndex]);
        };

        for (int i = 0; i < renderPointCount; ++i)
        {
            const float t = (float) i / (float) (renderPointCount - 1);
            const float x = plot.getX() + plot.getWidth() * t;
            const float y = centreY - sampleWaveformAt(juce::jmap(t, startIndex, endIndex)) * halfHeight;
            if (i == 0)
                waveformPath.startNewSubPath(x, y);
            else
                waveformPath.lineTo(x, y);
        }

        g.setColour(frozen ? juce::Colour(0xffebf2f8) : juce::Colour(0xffb5c3d0));
        g.strokePath(waveformPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(reactorui::displayLine().withAlpha(0.36f));
        g.drawLine(plot.getX(), centreY, plot.getRight(), centreY, 1.0f);

        if (splitMode != PluginProcessor::splitModeAuto)
        {
            const auto markerValues = processor.getManualMarkerValuesMs();
            const std::array<juce::Colour, PluginProcessor::manualMarkerCount> colours{
                juce::Colour(0xffffb35f),
                juce::Colour(0xffff8c3f),
                juce::Colour(0xff68d3ff),
                juce::Colour(0xff2fa4e7)
            };
            const std::array<juce::String, PluginProcessor::manualMarkerCount> labels{
                "T Start", "T End", "B Start", "B End"
            };

            for (int marker = 0; marker < PluginProcessor::manualMarkerCount; ++marker)
            {
                if (markerValues[(size_t) marker] < visibleStartMs || markerValues[(size_t) marker] > visibleEndMs)
                    continue;

                const float x = msToX(markerValues[(size_t) marker]);
                g.setColour(colours[(size_t) marker]);
                g.drawLine(x, plot.getY(), x, plot.getBottom(), marker == activeMarker ? 2.6f : 1.4f);

                if (zoomFactor >= 3.5f || marker == activeMarker)
                {
                    juce::Rectangle<float> chip(juce::jlimit(plot.getX(), plot.getRight() - 56.0f, x - 28.0f),
                                                plot.getY() + 6.0f + marker * 18.0f, 56.0f, 14.0f);
                    g.setColour(colours[(size_t) marker].withAlpha(0.18f));
                    g.fillRoundedRectangle(chip, 4.0f);
                    g.setColour(colours[(size_t) marker]);
                    g.drawRoundedRectangle(chip, 4.0f, 1.0f);
                    g.setColour(reactorui::textStrong());
                    g.setFont(reactorui::bodyFont(9.5f));
                    g.drawFittedText(labels[(size_t) marker], chip.getSmallestIntegerContainer(), juce::Justification::centred, 1);
                }
            }
        }

        g.setColour(reactorui::textMuted());
        g.setFont(reactorui::sectionFont(10.5f));
        g.drawFittedText(frozen ? "FROZEN HIT" : "LIVE INPUT", getLocalBounds().removeFromTop(16).reduced(8, 0), juce::Justification::right, 1);
        if (frozen && zoomFactor > 1.0f)
        {
            g.drawFittedText(juce::String(visibleStartMs, 1) + " ms - " + juce::String(visibleEndMs, 1) + " ms",
                             getLocalBounds().removeFromBottom(16).reduced(8, 0), juce::Justification::right, 1);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        activeMarker = -1;
        panningView = false;

        if (processor.getSplitModeIndex() != PluginProcessor::splitModeAuto)
            activeMarker = findClosestMarker(event.position.x);

        if (activeMarker < 0 && zoomFactor > 1.01f)
        {
            panningView = true;
            dragStartX = event.position.x;
            dragStartScroll = scrollProportion;
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (activeMarker >= 0)
        {
            const auto plot = getLocalBounds().toFloat().reduced(14.0f, 12.0f);
            const float totalDurationMs = processor.getSplitEditorCaptureDurationMs();
            const float visibleDurationMs = juce::jmax(0.5f, totalDurationMs / zoomFactor);
            const float maxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - visibleDurationMs);
            const float visibleStartMs = scrollProportion * maxVisibleStartMs;
            const float proportion = juce::jlimit(0.0f, 1.0f, (event.position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
            const float valueMs = visibleStartMs + proportion * visibleDurationMs;
            processor.setManualMarkerValueMs(activeMarker, valueMs);
            markers = processor.getManualMarkerValuesMs();
            repaint();
            return;
        }

        if (panningView)
        {
            const auto plot = getLocalBounds().toFloat().reduced(14.0f, 12.0f);
            const float totalDurationMs = processor.getSplitEditorCaptureDurationMs();
            const float visibleDurationMs = juce::jmax(0.5f, totalDurationMs / zoomFactor);
            const float maxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - visibleDurationMs);

            if (maxVisibleStartMs <= 0.0f)
            {
                setScrollProportion(0.0f);
                return;
            }

            const float pixelsPerMs = plot.getWidth() / juce::jmax(1.0f, visibleDurationMs);
            const float deltaMs = (dragStartX - event.position.x) / juce::jmax(1.0f, pixelsPerMs);
            const float startMs = juce::jlimit(0.0f, maxVisibleStartMs, dragStartScroll * maxVisibleStartMs + deltaMs);
            setScrollProportion(startMs / maxVisibleStartMs);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        activeMarker = -1;
        panningView = false;
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        resetView();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (std::abs(wheel.deltaY) < 0.0001f)
            return;

        const auto plot = getLocalBounds().toFloat().reduced(14.0f, 12.0f);
        const float totalDurationMs = processor.getSplitEditorCaptureDurationMs();
        const float oldVisibleDurationMs = juce::jmax(0.5f, totalDurationMs / zoomFactor);
        const float oldMaxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - oldVisibleDurationMs);
        const float oldVisibleStartMs = scrollProportion * oldMaxVisibleStartMs;
        const float anchor = juce::jlimit(0.0f, 1.0f, (event.position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
        const float anchorMs = oldVisibleStartMs + anchor * oldVisibleDurationMs;
        const float zoomMultiplier = std::pow(1.45f, wheel.deltaY * 2.4f);
        const float newZoom = juce::jlimit(1.0f, 512.0f, zoomFactor * zoomMultiplier);
        const float newVisibleDurationMs = juce::jmax(0.5f, totalDurationMs / newZoom);
        const float newMaxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - newVisibleDurationMs);
        const float newVisibleStartMs = juce::jlimit(0.0f, newMaxVisibleStartMs, anchorMs - anchor * newVisibleDurationMs);

        setZoomFactor(newZoom);
        setScrollProportion(newMaxVisibleStartMs > 0.0f ? newVisibleStartMs / newMaxVisibleStartMs : 0.0f);
    }

private:
    void timerCallback() override
    {
        processor.getSplitEditorWaveform(waveform, frozen);
        markers = processor.getManualMarkerValuesMs();
        repaint();
    }

    int findClosestMarker(float xPosition) const
    {
        auto plot = getLocalBounds().toFloat().reduced(14.0f, 12.0f);
        const float totalDurationMs = processor.getSplitEditorCaptureDurationMs();
        const float visibleDurationMs = juce::jmax(0.5f, totalDurationMs / zoomFactor);
        const float maxVisibleStartMs = juce::jmax(0.0f, totalDurationMs - visibleDurationMs);
        const float visibleStartMs = scrollProportion * maxVisibleStartMs;
        const float visibleEndMs = visibleStartMs + visibleDurationMs;
        int bestIndex = -1;
        float bestDistance = 18.0f;

        for (int marker = 0; marker < PluginProcessor::manualMarkerCount; ++marker)
        {
            if (markers[(size_t) marker] < visibleStartMs || markers[(size_t) marker] > visibleEndMs)
                continue;

            const float markerX = plot.getX() + plot.getWidth()
                                * juce::jlimit(0.0f, 1.0f, (markers[(size_t) marker] - visibleStartMs) / juce::jmax(1.0f, visibleDurationMs));
            const float distance = std::abs(markerX - xPosition);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = marker;
            }
        }

        return bestIndex;
    }

    PluginProcessor& processor;
    std::array<float, PluginProcessor::splitEditorDisplayPointCount> waveform{};
    std::array<float, PluginProcessor::manualMarkerCount> markers{};
    bool frozen = false;
    int activeMarker = -1;
    float zoomFactor = 1.0f;
    float scrollProportion = 0.0f;
    bool panningView = false;
    float dragStartX = 0.0f;
    float dragStartScroll = 0.0f;
public:
    std::function<void(float)> onZoomChanged;
    std::function<void(float)> onScrollChanged;
};

class ManualSplitPanel final : public juce::Component,
                               private juce::Timer
{
public:
    explicit ManualSplitPanel(PluginProcessor& processorToUse)
        : processor(processorToUse),
          modeChoice(processor.apvts, "splitMode", "Mode"),
          captureLengthChoice(processor.apvts, "captureLength", "Capture"),
          transientStartField(processor.apvts, "manualTransientStartMs", "Transient Start", juce::Colour(0xffffb35f)),
          transientEndField(processor.apvts, "manualTransientEndMs", "Transient End", juce::Colour(0xffff8c3f)),
          bodyStartField(processor.apvts, "manualBodyStartMs", "Body Start", juce::Colour(0xff68d3ff)),
          bodyEndField(processor.apvts, "manualBodyEndMs", "Body End", juce::Colour(0xff2fa4e7)),
          waveform(processor)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(modeChoice);
        addAndMakeVisible(captureLengthChoice);
        addAndMakeVisible(captureButton);
        addAndMakeVisible(liveButton);
        addAndMakeVisible(statusLabel);
        addAndMakeVisible(zoomLabel);
        addAndMakeVisible(zoomSlider);
        addAndMakeVisible(scrollLabel);
        addAndMakeVisible(scrollSlider);
        addAndMakeVisible(waveform);
        addAndMakeVisible(transientStartField);
        addAndMakeVisible(transientEndField);
        addAndMakeVisible(bodyStartField);
        addAndMakeVisible(bodyEndField);

        titleLabel.setText("Manual Split Editor", juce::dontSendNotification);
        subtitleLabel.setText("Capture a hit, drag the split markers, and switch between Auto, Assisted, and Manual routing.", juce::dontSendNotification);
        styleLabel(titleLabel, 14.0f, reactorui::textStrong());
        styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
        styleLabel(statusLabel, 11.0f, reactorui::textMuted(), juce::Justification::centredRight);
        styleLabel(zoomLabel, 11.0f, reactorui::textMuted());
        styleLabel(scrollLabel, 11.0f, reactorui::textMuted());
        zoomLabel.setText("Zoom", juce::dontSendNotification);
        scrollLabel.setText("Scroll", juce::dontSendNotification);

        captureButton.onClick = [this] { processor.armSplitEditorCapture(); refreshState(); };
        liveButton.onClick = [this] { processor.clearSplitEditorCapture(); refreshState(); };

        for (auto* button : { &captureButton, &liveButton })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff151d24));
            button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffa648));
            button->setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
            button->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
        }

        for (auto* slider : { &zoomSlider, &scrollSlider })
        {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 18);
            slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff111821));
            slider->setColour(juce::Slider::trackColourId, juce::Colour(0xff65d5ff).withAlpha(0.82f));
            slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xff65d5ff));
            slider->setColour(juce::Slider::textBoxTextColourId, reactorui::textStrong());
            slider->setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
        }

        zoomSlider.setRange(1.0, 512.0, 0.01);
        zoomSlider.setSkewFactor(0.28);
        zoomSlider.setValue(1.0, juce::dontSendNotification);
        zoomSlider.textFromValueFunction = [] (double value) { return juce::String(value, 1) + "x"; };
        zoomSlider.onValueChange = [this]
        {
            waveform.setZoomFactor((float) zoomSlider.getValue());
            refreshState();
        };

        scrollSlider.setRange(0.0, 100.0, 0.01);
        scrollSlider.setValue(0.0, juce::dontSendNotification);
        scrollSlider.textFromValueFunction = [] (double value) { return juce::String(value, 0) + "%"; };
        scrollSlider.onValueChange = [this]
        {
            waveform.setScrollProportion((float) scrollSlider.getValue() * 0.01f);
        };

        waveform.onZoomChanged = [this](float newZoom)
        {
            if (std::abs((float) zoomSlider.getValue() - newZoom) > 0.001f)
                zoomSlider.setValue(newZoom, juce::dontSendNotification);
            scrollSlider.setEnabled(newZoom > 1.01f);
        };

        waveform.onScrollChanged = [this](float newScroll)
        {
            const double scrollPercent = (double) newScroll * 100.0;
            if (std::abs(scrollSlider.getValue() - scrollPercent) > 0.05)
                scrollSlider.setValue(scrollPercent, juce::dontSendNotification);
        };

        startTimerHz(6);
        refreshState();
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff75d2ff), 14.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(area.removeFromTop(20));
        subtitleLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);

        auto controls = area.removeFromTop(34);
        modeChoice.setBounds(controls.removeFromLeft(136));
        controls.removeFromLeft(8);
        captureLengthChoice.setBounds(controls.removeFromLeft(146));
        controls.removeFromLeft(8);
        captureButton.setBounds(controls.removeFromLeft(132));
        controls.removeFromLeft(6);
        liveButton.setBounds(controls.removeFromLeft(76));
        statusLabel.setBounds(controls);

        area.removeFromTop(10);
        waveform.setBounds(area.removeFromTop(188));
        area.removeFromTop(10);

        auto zoomRow = area.removeFromTop(26);
        zoomLabel.setBounds(zoomRow.removeFromLeft(40));
        zoomSlider.setBounds(zoomRow.removeFromLeft(230));
        zoomRow.removeFromLeft(12);
        scrollLabel.setBounds(zoomRow.removeFromLeft(44));
        scrollSlider.setBounds(zoomRow.removeFromLeft(280));

        area.removeFromTop(10);
        auto values = area.removeFromTop(78);
        const int gap = 8;
        const int fieldWidth = (values.getWidth() - gap * 3) / 4;
        transientStartField.setBounds(values.removeFromLeft(fieldWidth));
        values.removeFromLeft(gap);
        transientEndField.setBounds(values.removeFromLeft(fieldWidth));
        values.removeFromLeft(gap);
        bodyStartField.setBounds(values.removeFromLeft(fieldWidth));
        values.removeFromLeft(gap);
        bodyEndField.setBounds(values);
    }

private:
    void timerCallback() override
    {
        if (processor.hasPendingSplitEditorSuggestedMarkers())
            processor.applyPendingSplitEditorSuggestedMarkers();

        refreshState();
    }

    void refreshState()
    {
        const bool frozen = processor.isSplitEditorCaptureFrozen();
        const float captureDurationMs = processor.getSplitEditorCaptureDurationMs();
        const juce::String durationLabel = captureDurationMs >= 1000.0f
                                               ? juce::String(captureDurationMs * 0.001f, 2) + " s"
                                               : juce::String(captureDurationMs, 0) + " ms";

        if (frozen && ! wasFrozen)
            waveform.resetView();
        else if (! frozen && wasFrozen)
            waveform.resetView();

        captureButton.setButtonText(processor.isSplitEditorCaptureArmed() ? "Waiting For Hit" : "Freeze / Capture");
        liveButton.setButtonText(frozen ? "Back To Live" : "Clear");
        scrollSlider.setEnabled(zoomSlider.getValue() > 1.01);

        if (! frozen && wasFrozen)
            waveform.setScrollProportion(0.0f);

        if (processor.isSplitEditorCaptureArmed())
            statusLabel.setText("Armed: the next detected hit will freeze a " + durationLabel + " capture into the waveform editor.",
                                juce::dontSendNotification);
        else if (frozen)
            statusLabel.setText("Frozen buffer ready (" + durationLabel + "). Wheel to zoom, drag to pan, drag markers, double-click to fit, or type exact times below.",
                                juce::dontSendNotification);
        else
            statusLabel.setText("Live waveform view. Capture up to " + durationLabel + " so long hits, loops, or bar-length phrases stay editable.",
                                juce::dontSendNotification);

        wasFrozen = frozen;
    }

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    TriggerChoice modeChoice;
    TriggerChoice captureLengthChoice;
    juce::TextButton captureButton{ "Freeze / Capture" };
    juce::TextButton liveButton{ "Back To Live" };
    juce::Label statusLabel;
    juce::Label zoomLabel;
    juce::Slider zoomSlider;
    juce::Label scrollLabel;
    juce::Slider scrollSlider;
    TimeField transientStartField;
    TimeField transientEndField;
    TimeField bodyStartField;
    TimeField bodyEndField;
    ManualSplitWaveform waveform;
    bool wasFrozen = false;
};

class SidebarPanel : public juce::Component
{
public:
    SidebarPanel(PluginProcessor& processorToUse,
                 juce::String titleText,
                 juce::Colour accentToUse,
                 std::vector<std::unique_ptr<AttachedKnob>> knobList,
                 std::function<PluginProcessor::StereoMeterSnapshot()> meterGetter,
                 bool showRms)
        : processor(processorToUse),
          title(std::move(titleText)),
          accent(accentToUse),
          knobs(std::move(knobList)),
          meter(std::move(meterGetter), accent, showRms)
    {
        addAndMakeVisible(titleLabel);
        styleLabel(titleLabel, 14.0f, reactorui::textStrong());
        titleLabel.setText(title, juce::dontSendNotification);

        for (auto& knob : knobs)
            addAndMakeVisible(*knob);

        addAndMakeVisible(meter);
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 14.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(10);

        meter.setBounds(area.removeFromBottom(170));
        area.removeFromBottom(10);

        for (auto& knob : knobs)
        {
            knob->setBounds(area.removeFromTop(110));
            area.removeFromTop(6);
        }
    }

protected:
    PluginProcessor& processor;
    juce::String title;
    juce::Colour accent;
    juce::Label titleLabel;
    std::vector<std::unique_ptr<AttachedKnob>> knobs;
    MeterComponent meter;
};

class OutputPanel final : public SidebarPanel
{
public:
    explicit OutputPanel(PluginProcessor& processorToUse)
        : SidebarPanel(processorToUse,
                       "Output",
                       juce::Colour(0xff75d2ff),
                       makeKnobs(processorToUse),
                       [&processorToUse] { return processorToUse.getOutputMeterSnapshot(); },
                       true)
    {
        addAndMakeVisible(clipButton);
        clipButton.setButtonText("Reset Clip");
        clipButton.onClick = [&processorToUse] { processorToUse.clearOutputClipLatch(); };
    }

    void resized() override
    {
        SidebarPanel::resized();
        auto area = getLocalBounds().reduced(14, 12);
        area.removeFromTop(20 + 10);
        const int meterTop = getHeight() - 14 - 170 - 10 - 30;
        clipButton.setBounds(getWidth() / 2 - 52, meterTop, 104, 26);
    }

private:
    static std::vector<std::unique_ptr<AttachedKnob>> makeKnobs(PluginProcessor& processorToUse)
    {
        std::vector<std::unique_ptr<AttachedKnob>> result;
        result.push_back(std::make_unique<AttachedKnob>(processorToUse.apvts, "dryWet", "Dry / Wet", juce::Colour(0xff75d2ff), [] (double value)
        {
            return juce::String(juce::roundToInt((float) value)) + "%";
        }));
        result.push_back(std::make_unique<AttachedKnob>(processorToUse.apvts, "pogoAmount", "POGO", juce::Colour(0xffffa648), [] (double value)
        {
            return juce::String(juce::roundToInt((float) value)) + "%";
        }));
        result.push_back(std::make_unique<AttachedKnob>(processorToUse.apvts, "outputGainDb", "Output Gain", juce::Colour(0xff75d2ff), [] (double value)
        {
            return juce::String(value, 1) + " dB";
        }));
        return result;
    }

    juce::TextButton clipButton;
};

class PresetBar final : public juce::Component,
                        private juce::Timer
{
public:
    explicit PresetBar(PluginProcessor& processorToUse,
                       float initialScale,
                       std::function<void(float)> scaleChangeCallback)
        : processor(processorToUse),
          onScaleChanged(std::move(scaleChangeCallback))
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(currentPresetLabel);
        addAndMakeVisible(currentPresetValue);
        addAndMakeVisible(currentPresetSummary);
        addAndMakeVisible(scaleLabel);
        addAndMakeVisible(scaleBox);
        addAndMakeVisible(factoryLabel);
        addAndMakeVisible(factoryCategoryBox);
        addAndMakeVisible(factoryBox);
        addAndMakeVisible(loadFactoryButton);
        addAndMakeVisible(userLabel);
        addAndMakeVisible(userBox);
        addAndMakeVisible(loadUserButton);
        addAndMakeVisible(saveUserButton);
        addAndMakeVisible(deleteUserButton);
        addAndMakeVisible(copyTransientToBodyButton);
        addAndMakeVisible(copyBodyToTransientButton);
        addAndMakeVisible(storeAButton);
        addAndMakeVisible(loadAButton);
        addAndMakeVisible(storeBButton);
        addAndMakeVisible(loadBButton);

        titleLabel.setText("SSP Transient Splitter", juce::dontSendNotification);
        currentPresetLabel.setText("Current", juce::dontSendNotification);
        factoryLabel.setText("Factory", juce::dontSendNotification);
        userLabel.setText("User", juce::dontSendNotification);
        scaleLabel.setText("Scale", juce::dontSendNotification);
        styleLabel(titleLabel, 16.0f, reactorui::textStrong());
        styleLabel(currentPresetLabel, 11.0f, reactorui::textMuted());
        styleLabel(factoryLabel, 11.0f, reactorui::textMuted());
        styleLabel(userLabel, 11.0f, reactorui::textMuted());
        styleLabel(scaleLabel, 11.0f, reactorui::textMuted());
        styleLabel(currentPresetValue, 12.5f, reactorui::textStrong(), juce::Justification::centredLeft);
        styleLabel(currentPresetSummary, 10.5f, reactorui::textMuted(), juce::Justification::centredLeft);

        const auto& factoryCategories = PluginProcessor::getFactoryPresetCategories();
        for (int i = 0; i < factoryCategories.size(); ++i)
            factoryCategoryBox.addItem(factoryCategories[i], i + 1);
        for (int i = 0; i < (int) editorScaleChoices.size(); ++i)
            scaleBox.addItem(formatEditorScaleLabel(editorScaleChoices[(size_t) i]), i + 1);
        scaleBox.setSelectedId(editorScaleToChoiceId(initialScale), juce::dontSendNotification);
        scaleBox.onChange = [this]
        {
            if (onScaleChanged)
                onScaleChanged(choiceIdToEditorScale(scaleBox.getSelectedId()));
        };
        userBox.setEditableText(true);
        userBox.setTextWhenNothingSelected("User preset name");

        loadFactoryButton.setButtonText("Load");
        loadUserButton.setButtonText("Load");
        saveUserButton.setButtonText("Save");
        deleteUserButton.setButtonText("Delete");
        copyTransientToBodyButton.setButtonText("T -> B");
        copyBodyToTransientButton.setButtonText("B -> T");
        storeAButton.setButtonText("Store A");
        loadAButton.setButtonText("Load A");
        storeBButton.setButtonText("Store B");
        loadBButton.setButtonText("Load B");

        for (auto* button : { &loadFactoryButton, &loadUserButton, &saveUserButton, &deleteUserButton,
                              &copyTransientToBodyButton, &copyBodyToTransientButton,
                              &storeAButton, &loadAButton, &storeBButton, &loadBButton })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff151d24));
            button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffa648));
            button->setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
            button->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
        }

        loadFactoryButton.onClick = [this]
        {
            const int selected = factoryBox.getSelectedId() - 1;
            if (selected >= 0 && processor.loadFactoryPreset(selected))
                refreshAll();
        };

        factoryCategoryBox.onChange = [this]
        {
            refreshFactoryPresetBox(factoryCategoryBox.getText(), 0);
        };

        loadUserButton.onClick = [this]
        {
            const auto presetName = userBox.getText().trim();
            if (presetName.isNotEmpty() && processor.loadUserPreset(presetName))
                refreshAll();
        };

        saveUserButton.onClick = [this]
        {
            const auto presetName = userBox.getText().trim();
            if (processor.saveUserPreset(presetName))
                refreshAll(presetName);
            else
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Preset Save Failed",
                                                       "Type a preset name in the User field, then try saving again.");
        };

        deleteUserButton.onClick = [this]
        {
            const auto presetName = userBox.getText().trim();
            if (presetName.isEmpty())
                return;

            if (processor.deleteUserPreset(presetName))
                refreshAll();
            else
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Preset Delete Failed",
                                                       "The preset file could not be removed.");
        };

        copyTransientToBodyButton.onClick = [this] { processor.copyPathChain(PluginProcessor::transientPath, PluginProcessor::bodyPath); };
        copyBodyToTransientButton.onClick = [this] { processor.copyPathChain(PluginProcessor::bodyPath, PluginProcessor::transientPath); };
        storeAButton.onClick = [this] { processor.storeABState(0); refreshButtons(); };
        storeBButton.onClick = [this] { processor.storeABState(1); refreshButtons(); };
        loadAButton.onClick = [this]
        {
            if (processor.recallABState(0))
                refreshAll();
        };
        loadBButton.onClick = [this]
        {
            if (processor.recallABState(1))
                refreshAll();
        };

        refreshAll();
        startTimerHz(2);
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffffa648), 14.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);
        auto top = area.removeFromTop(24);
        auto scaleArea = top.removeFromRight(126);
        scaleBox.setBounds(scaleArea.removeFromRight(82));
        scaleArea.removeFromRight(6);
        scaleLabel.setBounds(scaleArea);
        titleLabel.setBounds(top.removeFromLeft(260));
        currentPresetLabel.setBounds(top.removeFromLeft(56));
        currentPresetValue.setBounds(top.removeFromLeft(230));
        currentPresetSummary.setBounds(top);

        area.removeFromTop(10);
        auto row = area.removeFromTop(34);
        factoryLabel.setBounds(row.removeFromLeft(50));
        factoryCategoryBox.setBounds(row.removeFromLeft(112));
        row.removeFromLeft(8);
        factoryBox.setBounds(row.removeFromLeft(210));
        row.removeFromLeft(8);
        loadFactoryButton.setBounds(row.removeFromLeft(64));
        row.removeFromLeft(14);
        userLabel.setBounds(row.removeFromLeft(38));
        userBox.setBounds(row.removeFromLeft(220));
        row.removeFromLeft(8);
        loadUserButton.setBounds(row.removeFromLeft(64));
        row.removeFromLeft(6);
        saveUserButton.setBounds(row.removeFromLeft(64));
        row.removeFromLeft(6);
        deleteUserButton.setBounds(row.removeFromLeft(70));
        row.removeFromLeft(16);
        copyTransientToBodyButton.setBounds(row.removeFromLeft(68));
        row.removeFromLeft(6);
        copyBodyToTransientButton.setBounds(row.removeFromLeft(68));
        row.removeFromLeft(16);
        storeAButton.setBounds(row.removeFromLeft(72));
        row.removeFromLeft(6);
        loadAButton.setBounds(row.removeFromLeft(68));
        row.removeFromLeft(6);
        storeBButton.setBounds(row.removeFromLeft(72));
        row.removeFromLeft(6);
        loadBButton.setBounds(row.removeFromLeft(68));
    }

private:
    void timerCallback() override
    {
        refreshButtons();
        refreshCurrentPreset();
    }

    void refreshAll(const juce::String& preferredUserPreset = {})
    {
        refreshCurrentPreset();
        refreshFactorySelection();
        refreshUserPresets(preferredUserPreset.isNotEmpty() ? preferredUserPreset : userBox.getText());
        refreshButtons();
    }

    void refreshCurrentPreset()
    {
        currentPresetValue.setText(processor.getCurrentPresetName(), juce::dontSendNotification);
        currentPresetSummary.setText(processor.getCurrentPresetDescription(), juce::dontSendNotification);
    }

    void refreshFactorySelection()
    {
        const auto& factoryNames = PluginProcessor::getFactoryPresetNames();
        const int currentIndex = factoryNames.indexOf(processor.getCurrentPresetName());
        const auto& factoryCategories = PluginProcessor::getFactoryPresetCategories();

        juce::String targetCategory = factoryCategories.isEmpty() ? juce::String() : factoryCategories[0];
        if (currentIndex >= 0)
            targetCategory = PluginProcessor::getFactoryPresetCategory(currentIndex);
        else if (factoryCategoryBox.getSelectedId() > 0)
            targetCategory = factoryCategoryBox.getText();

        if (targetCategory.isNotEmpty())
        {
            const int categoryIndex = factoryCategories.indexOf(targetCategory);
            if (categoryIndex >= 0)
                factoryCategoryBox.setSelectedId(categoryIndex + 1, juce::dontSendNotification);
        }

        refreshFactoryPresetBox(targetCategory, currentIndex >= 0 ? currentIndex + 1 : 0);
    }

    void refreshFactoryPresetBox(const juce::String& category, int preferredPresetId)
    {
        const int currentSelection = factoryBox.getSelectedId();
        factoryBox.clear(juce::dontSendNotification);

        const auto& factoryNames = PluginProcessor::getFactoryPresetNames();
        for (int i = 0; i < factoryNames.size(); ++i)
        {
            if (category.isNotEmpty() && PluginProcessor::getFactoryPresetCategory(i) != category)
                continue;

            factoryBox.addItem(factoryNames[i], i + 1);
        }

        const int desiredSelection = preferredPresetId > 0 && factoryBox.indexOfItemId(preferredPresetId) >= 0
                                       ? preferredPresetId
                                       : (currentSelection > 0 && factoryBox.indexOfItemId(currentSelection) >= 0 ? currentSelection : 0);

        if (desiredSelection > 0)
            factoryBox.setSelectedId(desiredSelection, juce::dontSendNotification);
        else if (factoryBox.getNumItems() > 0)
            factoryBox.setSelectedItemIndex(0, juce::dontSendNotification);
    }

    void refreshUserPresets(const juce::String& preferredSelection)
    {
        const auto names = processor.getUserPresetNames();
        userBox.clear(juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i)
            userBox.addItem(names[i], i + 1);

        if (const int preferredIndex = names.indexOf(preferredSelection); preferredIndex >= 0)
            userBox.setSelectedItemIndex(preferredIndex, juce::dontSendNotification);
        else if (const int currentPresetIndex = names.indexOf(processor.getCurrentPresetName()); currentPresetIndex >= 0)
            userBox.setSelectedItemIndex(currentPresetIndex, juce::dontSendNotification);
        else if (names.size() > 0)
            userBox.setSelectedId(1, juce::dontSendNotification);
    }

    void refreshButtons()
    {
        loadAButton.setEnabled(processor.isABStateValid(0));
        loadBButton.setEnabled(processor.isABStateValid(1));
        const auto currentAB = processor.getActiveABSlot();
        loadAButton.setColour(juce::TextButton::buttonColourId, currentAB == 0 ? juce::Colour(0xffffa648) : juce::Colour(0xff151d24));
        loadBButton.setColour(juce::TextButton::buttonColourId, currentAB == 1 ? juce::Colour(0xff65d5ff) : juce::Colour(0xff151d24));
    }

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label currentPresetLabel;
    juce::Label currentPresetValue;
    juce::Label currentPresetSummary;
    juce::Label scaleLabel;
    juce::ComboBox scaleBox;
    juce::Label factoryLabel;
    juce::ComboBox factoryCategoryBox;
    juce::ComboBox factoryBox;
    juce::TextButton loadFactoryButton;
    juce::Label userLabel;
    juce::ComboBox userBox;
    juce::TextButton loadUserButton;
    juce::TextButton saveUserButton;
    juce::TextButton deleteUserButton;
    juce::TextButton copyTransientToBodyButton;
    juce::TextButton copyBodyToTransientButton;
    juce::TextButton storeAButton;
    juce::TextButton loadAButton;
    juce::TextButton storeBButton;
    juce::TextButton loadBButton;
    std::function<void(float)> onScaleChanged;
};

class RootComponent final : public juce::Component
{
public:
    explicit RootComponent(PluginProcessor& processorToUse,
                           float initialScale,
                           std::function<void(float)> scaleChangeCallback)
        : processor(processorToUse),
          presetBar(processor, initialScale, std::move(scaleChangeCallback)),
          detection(processor),
          manualSplit(processor),
          transientChain(processor, PluginProcessor::transientPath, "Transient Chain", juce::Colour(0xffffa648),
                         "transientLevelDb", "transientPan", "transientMute", "transientSolo",
                         [&processorToUse] { return processorToUse.getTransientMeterSnapshot(); }),
          bodyChain(processor, PluginProcessor::bodyPath, "Body Chain", juce::Colour(0xff65d5ff),
                    "bodyLevelDb", "bodyPan", "bodyMute", "bodySolo",
                    [&processorToUse] { return processorToUse.getBodyMeterSnapshot(); }),
          inputPanel(processor,
                     "Input",
                     juce::Colour(0xffffc95a),
                     makeInputKnobs(processor),
                     [&processorToUse] { return processorToUse.getInputMeterSnapshot(); },
                     true),
          outputPanel(processor)
    {
        addAndMakeVisible(presetBar);
        addAndMakeVisible(detection);
        addAndMakeVisible(manualSplit);
        addAndMakeVisible(transientChain);
        addAndMakeVisible(bodyChain);
        addAndMakeVisible(inputPanel);
        addAndMakeVisible(outputPanel);
    }

    void paint(juce::Graphics& g) override
    {
        reactorui::drawEditorBackdrop(g, getLocalBounds().toFloat());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        presetBar.setBounds(area.removeFromTop(82));
        area.removeFromTop(14);

        const int topHeight = juce::jlimit(500, 620, juce::roundToInt((float) area.getHeight() * 0.48f));
        auto top = area.removeFromTop(topHeight);
        auto left = top.removeFromLeft(220);
        auto right = top.removeFromRight(220);
        top.removeFromLeft(12);
        top.removeFromRight(12);

        inputPanel.setBounds(left);
        outputPanel.setBounds(right);
        auto detectionArea = top.removeFromTop(250);
        top.removeFromTop(12);
        detection.setBounds(detectionArea);
        manualSplit.setBounds(top);

        area.removeFromTop(14);
        auto chains = area;
        auto transientArea = chains.removeFromLeft((chains.getWidth() - 16) / 2);
        chains.removeFromLeft(16);
        transientChain.setBounds(transientArea);
        bodyChain.setBounds(chains);
    }

private:
    static std::vector<std::unique_ptr<AttachedKnob>> makeInputKnobs(PluginProcessor& processor)
    {
        std::vector<std::unique_ptr<AttachedKnob>> result;
        result.push_back(std::make_unique<AttachedKnob>(processor.apvts, "inputGainDb", "Input Gain", juce::Colour(0xffffc95a), [] (double value)
        {
            return juce::String(value, 1) + " dB";
        }));
        return result;
    }

    PluginProcessor& processor;
    PresetBar presetBar;
    DetectionPanel detection;
    ManualSplitPanel manualSplit;
    EffectChainComponent transientChain;
    EffectChainComponent bodyChain;
    SidebarPanel inputPanel;
    OutputPanel outputPanel;
};
} // namespace

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      pluginProcessor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(roundScaledDimension(nominalEditorWidth, minimumEditorScale),
                    roundScaledDimension(nominalEditorHeight, minimumEditorScale),
                    roundScaledDimension(nominalEditorWidth, maximumEditorScale),
                    roundScaledDimension(nominalEditorHeight, maximumEditorScale));
    setSize(roundScaledDimension(nominalEditorWidth, defaultEditorScale),
            roundScaledDimension(nominalEditorHeight, defaultEditorScale));

    content = std::make_unique<RootComponent>(pluginProcessor,
                                              defaultEditorScale,
                                              [this](float newScale) { setEditorScale(newScale); });
    addAndMakeVisible(*content);
    content->setBounds(0, 0, nominalEditorWidth, nominalEditorHeight);
    updateContentScale();
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

void PluginEditor::paint(juce::Graphics& g)
{
    reactorui::drawEditorBackdrop(g, getLocalBounds().toFloat());
}

void PluginEditor::resized()
{
    updateContentScale();
}

void PluginEditor::setEditorScale(float newScale)
{
    editorScale = juce::jlimit(minimumEditorScale, maximumEditorScale, newScale);
    setSize(roundScaledDimension(nominalEditorWidth, editorScale),
            roundScaledDimension(nominalEditorHeight, editorScale));
    updateContentScale();
}

void PluginEditor::updateContentScale()
{
    if (content == nullptr)
        return;

    const float scaleX = (float) getWidth() / (float) nominalEditorWidth;
    const float scaleY = (float) getHeight() / (float) nominalEditorHeight;
    const float fitScale = juce::jlimit(minimumEditorScale, maximumEditorScale, juce::jmin(scaleX, scaleY));
    const int scaledWidth = roundScaledDimension(nominalEditorWidth, fitScale);
    const int scaledHeight = roundScaledDimension(nominalEditorHeight, fitScale);
    const float offsetX = 0.5f * ((float) getWidth() - (float) scaledWidth);
    const float offsetY = 0.5f * ((float) getHeight() - (float) scaledHeight);

    content->setBounds(0, 0, nominalEditorWidth, nominalEditorHeight);
    content->setTransform(juce::AffineTransform::scale(fitScale).translated(offsetX, offsetY));
}
