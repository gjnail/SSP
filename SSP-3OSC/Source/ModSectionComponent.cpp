#include "ModSectionComponent.h"
#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
juce::Colour modAccent()
{
    return juce::Colour(0xffff8b3d);
}

class MacroDragBadge final : public juce::Component
{
public:
    void setSourceIndex(int newSourceIndex)
    {
        sourceIndex = juce::jlimit(0, reactormod::maxModulationSourceCount, newSourceIndex);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const auto accent = sourceIndex > 0 ? reactorui::modulationSourceColour(sourceIndex)
                                            : modAccent();

        g.setColour(juce::Colour(0xff10151b));
        g.fillRoundedRectangle(bounds, 7.0f);
        g.setColour(accent.withAlpha(isMouseOverOrDragging() ? 0.96f : 0.74f));
        g.drawRoundedRectangle(bounds, 7.0f, 1.2f);
        g.setColour(reactorui::textStrong());
        g.setFont(reactorui::sectionFont(9.8f));

        juce::String text = "DRAG";
        if (const int macroNumber = reactormod::macroNumberForSourceIndex(sourceIndex); macroNumber > 0)
            text << " MOD " << juce::String(macroNumber);

        g.drawFittedText(text, getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (sourceIndex <= 0 || ! event.mouseWasDraggedSinceMouseDown())
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            if (! container->isDragAndDropActive())
                container->startDragging(reactormod::getSourceDragDescription(sourceIndex), this);
    }

private:
    int sourceIndex = 0;
};
}

ModSectionComponent::ModSectionComponent(PluginProcessor& p,
                                         juce::AudioProcessorValueTreeState& apvts,
                                         juce::String title,
                                         juce::String subtitle)
    : processor(p), titleText(std::move(title)), subtitleText(std::move(subtitle))
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subLabel);

    titleLabel.setText(titleText, juce::dontSendNotification);
    titleLabel.setFont(reactorui::sectionFont(14.0f));
    titleLabel.setColour(juce::Label::textColourId, reactorui::textStrong());
    titleLabel.setJustificationType(juce::Justification::centredLeft);

    subLabel.setText(subtitleText, juce::dontSendNotification);
    subLabel.setFont(reactorui::bodyFont(10.8f));
    subLabel.setColour(juce::Label::textColourId, reactorui::textMuted());
    subLabel.setJustificationType(juce::Justification::centredLeft);

    for (int i = 0; i < reactormod::macroSourceCount; ++i)
    {
        const int macroNumber = i + 1;
        macroLabels[(size_t) i].setText("MOD " + juce::String(macroNumber), juce::dontSendNotification);
        macroLabels[(size_t) i].setFont(reactorui::sectionFont(10.8f));
        macroLabels[(size_t) i].setColour(juce::Label::textColourId, reactorui::textStrong());
        macroLabels[(size_t) i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(macroLabels[(size_t) i]);

        macroKnobs[(size_t) i].setupModulation(processor,
                                               reactormod::getMacroParamID(macroNumber),
                                               reactormod::Destination::none,
                                               reactorui::modulationSourceColour(reactormod::sourceIndexForMacro(macroNumber)),
                                               62,
                                               22);
        macroKnobs[(size_t) i].onPrimaryInteract = [this, macroNumber]
        {
            processor.setSelectedModulationSourceIndex(reactormod::sourceIndexForMacro(macroNumber));
        };
        addAndMakeVisible(macroKnobs[(size_t) i]);
        macroAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, reactormod::getMacroParamID(macroNumber), macroKnobs[(size_t) i]);

        auto badge = std::make_unique<MacroDragBadge>();
        badge->setSourceIndex(reactormod::sourceIndexForMacro(macroNumber));
        addAndMakeVisible(*badge);
        dragBadges[(size_t) i] = std::move(badge);
    }
}

void ModSectionComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), modAccent(), 14.0f);
}

void ModSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    titleLabel.setBounds(area.removeFromTop(20));

    if (! subtitleText.isEmpty())
    {
        subLabel.setVisible(true);
        subLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
    }
    else
    {
        subLabel.setVisible(false);
        area.removeFromTop(6);
    }

    const bool useThreeColumns = area.getWidth() >= 330 || area.getHeight() < 300;
    const int columns = useThreeColumns ? 3 : 2;
    const int rows = (reactormod::macroSourceCount + columns - 1) / columns;
    const int columnGap = 10;
    const int rowGap = 10;
    const int cellWidth = (area.getWidth() - columnGap * (columns - 1)) / columns;
    const int cellHeight = (area.getHeight() - rowGap * (rows - 1)) / rows;

    for (int i = 0; i < reactormod::macroSourceCount; ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        auto cell = juce::Rectangle<int>(area.getX() + column * (cellWidth + columnGap),
                                         area.getY() + row * (cellHeight + rowGap),
                                         cellWidth,
                                         cellHeight);

        macroLabels[(size_t) i].setBounds(cell.removeFromTop(18));
        cell.removeFromTop(2);
        const auto badgeBounds = cell.removeFromBottom(24);
        if (dragBadges[(size_t) i] != nullptr)
            dragBadges[(size_t) i]->setBounds(badgeBounds);
        cell.removeFromBottom(2);
        macroKnobs[(size_t) i].setBounds(cell);
    }
}
