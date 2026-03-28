#include "PerformanceSectionComponent.h"
#include "ReactorUI.h"

namespace
{
void stylePerfLabel(juce::Label& label, float size, juce::Colour colour,
                    juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}
}

class PerformanceSectionComponent::DragValueBox final : public juce::Component,
                                                        private juce::Timer
{
public:
    DragValueBox(PluginProcessor& processorToUse,
                 juce::String parameterToUse,
                 juce::String suffixToUse,
                 juce::Colour accentToUse)
        : processor(processorToUse),
          parameterID(std::move(parameterToUse)),
          suffix(std::move(suffixToUse)),
          accent(accentToUse)
    {
        parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.apvts.getParameter(parameterID));
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        startTimerHz(12);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0d1218));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(accent.withAlpha(isDragging ? 0.95f : (isMouseOverOrDragging() ? 0.80f : 0.62f)));
        g.drawRoundedRectangle(bounds, 6.0f, isDragging ? 1.5f : 1.0f);

        g.setColour(reactorui::textStrong());
        g.setFont(reactorui::bodyFont(11.5f));
        g.drawFittedText(getDisplayText(), getLocalBounds().reduced(8, 2), juce::Justification::centred, 1);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStart = event.position;
        dragStartValue = getPlainValue();
        isDragging = true;
        if (parameter != nullptr)
            parameter->beginChangeGesture();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (parameter == nullptr)
            return;

        const float delta = (event.position.x - dragStart.x) * 0.20f;
        setPlainValue(dragStartValue + delta);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (parameter != nullptr)
            parameter->endChangeGesture();
        isDragging = false;
        repaint();
    }

private:
    float getPlainValue() const
    {
        return parameter != nullptr ? parameter->convertFrom0to1(parameter->getValue()) : 0.0f;
    }

    void setPlainValue(float plainValue)
    {
        if (parameter == nullptr)
            return;

        const auto range = parameter->getNormalisableRange();
        const float clamped = juce::jlimit(range.start, range.end, plainValue);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(clamped));
    }

    juce::String getDisplayText() const
    {
        if (parameter == nullptr)
            return "--";

        const auto value = getPlainValue();
        return juce::String(juce::roundToInt(value)) + suffix;
    }

    PluginProcessor& processor;
    juce::String parameterID;
    juce::String suffix;
    juce::Colour accent;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::Point<float> dragStart;
    float dragStartValue = 0.0f;
    bool isDragging = false;

    void timerCallback() override
    {
        repaint();
    }
};

PerformanceSectionComponent::PerformanceSectionComponent(PluginProcessor& processor,
                                                         juce::AudioProcessorValueTreeState&)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(bendTitleLabel);
    addAndMakeVisible(bendUpLabel);
    addAndMakeVisible(bendDownLabel);

    bendUpBox = std::make_unique<DragValueBox>(processor, "pitchBendUp", " st", juce::Colour(0xffb97dff));
    bendDownBox = std::make_unique<DragValueBox>(processor, "pitchBendDown", " st", juce::Colour(0xffb97dff));
    addAndMakeVisible(*bendUpBox);
    addAndMakeVisible(*bendDownBox);

    titleLabel.setText("PERFORMANCE", juce::dontSendNotification);
    bendTitleLabel.setText("Pitch Bend Range", juce::dontSendNotification);
    bendUpLabel.setText("Up", juce::dontSendNotification);
    bendDownLabel.setText("Down", juce::dontSendNotification);

    stylePerfLabel(titleLabel, 15.0f, reactorui::textStrong());
    stylePerfLabel(bendTitleLabel, 12.5f, juce::Colour(0xffdcb6ff), juce::Justification::centredLeft);
    stylePerfLabel(bendUpLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    stylePerfLabel(bendDownLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
}

PerformanceSectionComponent::~PerformanceSectionComponent() = default;

void PerformanceSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xffaf86ff));
}

void PerformanceSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);
    bendTitleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    const int gap = 14;
    const int cellWidth = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto right = area;

    bendUpLabel.setBounds(left.removeFromTop(16));
    bendUpBox->setBounds(left.removeFromTop(34));
    bendDownLabel.setBounds(right.removeFromTop(16));
    bendDownBox->setBounds(right.removeFromTop(34));
}
