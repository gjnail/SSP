#include "EffectsRackComponent.h"

#include "BitcrusherModuleComponent.h"
#include "ChorusModuleComponent.h"
#include "CompressorModuleComponent.h"
#include "DelayModuleComponent.h"
#include "DistortionModuleComponent.h"
#include "EQModuleComponent.h"
#include "FilterModuleComponent.h"
#include "FlangerModuleComponent.h"
#include "GateModuleComponent.h"
#include "ImagerModuleComponent.h"
#include "MultibandCompressorModuleComponent.h"
#include "PhaserModuleComponent.h"
#include "ReverbModuleComponent.h"
#include "ReactorUI.h"
#include "ShiftModuleComponent.h"
#include "TremoloModuleComponent.h"

namespace
{
void styleRackLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(size >= 14.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(just);
}

juce::Colour getModuleAccent(int moduleType)
{
    switch (moduleType)
    {
        case 1: return juce::Colour(0xffff9e45);
        case 2: return juce::Colour(0xff7fe17b);
        case 3: return juce::Colour(0xff6ef0d6);
        case 4: return juce::Colour(0xffff6fae);
        case 5: return juce::Colour(0xff4d8dff);
        case 6: return juce::Colour(0xffd6a3ff);
        case 7: return juce::Colour(0xff7fd8ff);
        case 8: return juce::Colour(0xffff6f5f);
        case 10:return juce::Colour(0xffffdf63);
        case 11:return juce::Colour(0xffc3ff5b);
        case 12:return juce::Colour(0xffb38cff);
        case 13:return juce::Colour(0xffa7b8ff);
        case 14:return juce::Colour(0xff95ffb3);
        case 15:return juce::Colour(0xfff9a56c);
        case 16:return juce::Colour(0xfff26a7d);
        default: break;
    }

    return reactorui::textMuted();
}

juce::String getFXSlotOnParamIDForRow(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotParallelParamIDForRow(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Parallel";
}

std::unique_ptr<FXModuleComponent> createFXComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts, int slotIndex, int moduleType)
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
        case 10:return std::make_unique<EQModuleComponent>(processor, apvts, slotIndex);
        case 11:return std::make_unique<MultibandCompressorModuleComponent>(processor, apvts, slotIndex);
        case 12:return std::make_unique<BitcrusherModuleComponent>(processor, apvts, slotIndex);
        case 13:return std::make_unique<ImagerModuleComponent>(processor, apvts, slotIndex);
        case 14:return std::make_unique<ShiftModuleComponent>(processor, apvts, slotIndex);
        case 15:return std::make_unique<TremoloModuleComponent>(processor, apvts, slotIndex);
        case 16:return std::make_unique<GateModuleComponent>(processor, apvts, slotIndex);
        default: break;
    }

    return nullptr;
}

class FXSlotRackItem final : public juce::Component
{
public:
    FXSlotRackItem(PluginProcessor& p, int slot) : processor(p), slotIndex(slot) {}

    void setModuleType(int moduleType)
    {
        if (moduleType == currentModuleType)
            return;

        currentModuleType = moduleType;
        moduleComponent.reset();

        if (PluginProcessor::isSupportedFXModuleType(currentModuleType) && currentModuleType != 0)
        {
            moduleComponent = createFXComponent(processor, processor.apvts, slotIndex, currentModuleType);
            if (moduleComponent != nullptr)
                addAndMakeVisible(*moduleComponent);
        }

        resized();
    }

    int getPreferredHeight() const
    {
        return moduleComponent != nullptr ? moduleComponent->getPreferredHeight() : 0;
    }

    void resized() override
    {
        if (moduleComponent != nullptr)
            moduleComponent->setBounds(getLocalBounds());
    }

private:
    PluginProcessor& processor;
    const int slotIndex;
    int currentModuleType = -1;
    std::unique_ptr<FXModuleComponent> moduleComponent;
};

class FXOrderRow final : public juce::Component
{
public:
    FXOrderRow(juce::AudioProcessorValueTreeState& state, int slot)
        : slotIndex(slot)
    {
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(routeButton);
        addAndMakeVisible(powerButton);
        addAndMakeVisible(removeButton);
        styleRackLabel(nameLabel, 11.5f, reactorui::textStrong(), juce::Justification::centredLeft);
        routeButton.setButtonText("SER");
        routeButton.setClickingTogglesState(true);
        powerButton.setButtonText("ON");
        powerButton.setClickingTogglesState(true);
        removeButton.setButtonText("X");

        parallelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, getFXSlotParallelParamIDForRow(slotIndex), routeButton);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, getFXSlotOnParamIDForRow(slotIndex), powerButton);
        parallelParam = state.getRawParameterValue(getFXSlotParallelParamIDForRow(slotIndex));
        enabledParam = state.getRawParameterValue(getFXSlotOnParamIDForRow(slotIndex));
        routeButton.onClick = [this] { refreshRoutePresentation(); };
        powerButton.onClick = [this] { refreshEnabledPresentation(); };
        refreshRoutePresentation();
        refreshEnabledPresentation();
    }

    void setRowData(int slot, int display, const juce::String& moduleName, int moduleType)
    {
        juce::ignoreUnused(slot);
        displayIndex = display;
        dragging = false;
        nameLabel.setText(juce::String(display + 1) + ". " + moduleName, juce::dontSendNotification);
        accent = getModuleAccent(moduleType);
        refreshRoutePresentation();
        refreshEnabledPresentation();
    }

    int getSlotIndex() const noexcept { return slotIndex; }
    int getDisplayIndex() const noexcept { return displayIndex; }
    bool isDragging() const noexcept { return dragging; }

    std::function<void(int, int)> onDropRequest;
    std::function<void(bool)> onDragStateChange;

    juce::TextButton removeButton;
    juce::TextButton routeButton;
    juce::TextButton powerButton;

    void paint(juce::Graphics& g) override
    {
        const bool enabled = isEffectEnabled();
        const bool parallel = isParallelRouted();
        const auto routeAccent = parallel ? reactorui::brandCyan() : accent;
        const auto rowAccent = enabled ? routeAccent : juce::Colour(0xff8f9399);
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient fill(enabled ? juce::Colour(0xff23272c) : juce::Colour(0xff27292c), bounds.getTopLeft(),
                                  enabled ? juce::Colour(0xff16191d) : juce::Colour(0xff1c1e21), bounds.getBottomLeft(), false);
        fill.addColour(0.08, rowAccent.withAlpha(dragging ? 0.22f : 0.10f));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff090b0d));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.3f);
        g.setColour(reactorui::outlineSoft().withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.reduced(1.7f), 5.0f, 1.0f);
        g.setColour(rowAccent.withAlpha(enabled ? 0.95f : 0.72f));
        g.fillRoundedRectangle(bounds.removeFromLeft(4.0f), 2.0f);

        auto handleArea = bounds.removeFromLeft(18.0f).reduced(4.0f, 7.0f);
        g.setColour((enabled ? juce::Colour(0xffaeb6c0) : juce::Colour(0xff8f9399)).withAlpha(0.75f));
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle(handleArea.withY(handleArea.getY() + i * 4.0f).withHeight(2.0f), 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6, 4);
        area.removeFromLeft(20);
        routeButton.setBounds(area.removeFromRight(52));
        area.removeFromRight(6);
        powerButton.setBounds(area.removeFromRight(54));
        area.removeFromRight(6);
        removeButton.setBounds(area.removeFromRight(26));
        area.removeFromRight(6);
        nameLabel.setBounds(area);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.eventComponent == &removeButton
            || event.eventComponent == &powerButton
            || event.eventComponent == &routeButton)
            return;

        dragStartBounds = getBounds();
        if (auto* parent = getParentComponent())
        {
            auto parentEvent = event.getEventRelativeTo(parent);
            dragCursorOffsetY = juce::roundToInt(parentEvent.position.y) - getY();
        }
        else
        {
            dragCursorOffsetY = event.getPosition().y;
        }

        dragging = false;
        pendingDrag = true;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (pendingDrag && event.getDistanceFromDragStartY() != 0)
        {
            if (std::abs(event.getDistanceFromDragStartY()) < 4)
                return;

            pendingDrag = false;
            dragging = true;
            if (onDragStateChange)
                onDragStateChange(true);
            toFront(false);
            repaint();
        }

        if (! dragging)
            return;

        int newY = dragStartBounds.getY() + event.getDistanceFromDragStartY();
        if (auto* parent = getParentComponent())
        {
            auto parentEvent = event.getEventRelativeTo(parent);
            newY = juce::roundToInt(parentEvent.position.y) - dragCursorOffsetY;
        }

        setTopLeftPosition(dragStartBounds.getX(), newY);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging && onDropRequest)
            onDropRequest(slotIndex, getBounds().getCentreY());

        pendingDrag = false;
        dragging = false;
        if (onDragStateChange)
            onDragStateChange(false);
        repaint();
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            dragging = false;
    }

    juce::Label nameLabel;

private:
    bool isEffectEnabled() const noexcept
    {
        return enabledParam == nullptr || enabledParam->load() >= 0.5f;
    }

    bool isParallelRouted() const noexcept
    {
        return parallelParam != nullptr && parallelParam->load() >= 0.5f;
    }

    void refreshRoutePresentation()
    {
        const bool parallel = isParallelRouted();
        routeButton.setButtonText(parallel ? "PAR" : "SER");
        routeButton.setToggleState(parallel, juce::dontSendNotification);
        repaint();
    }

    void refreshEnabledPresentation()
    {
        const bool enabled = isEffectEnabled();
        powerButton.setButtonText(enabled ? "ON" : "OFF");
        powerButton.setToggleState(enabled, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, enabled ? reactorui::textStrong()
                                                               : reactorui::textMuted().withAlpha(0.92f));
        repaint();
    }

    const int slotIndex;
    int displayIndex = -1;
    bool dragging = false;
    bool pendingDrag = false;
    juce::Rectangle<int> dragStartBounds;
    int dragCursorOffsetY = 0;
    juce::Colour accent = reactorui::textMuted();
    std::atomic<float>* parallelParam = nullptr;
    std::atomic<float>* enabledParam = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> parallelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
};

class FXLfoDragBadge final : public juce::Component
{
public:
    void setSelectedSourceIndex(int index)
    {
        selectedSourceIndex = juce::jmax(0, index);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0b1016));
        g.fillRoundedRectangle(bounds, 9.0f);

        const bool active = selectedSourceIndex > 0;
        const auto accent = active ? reactorui::modulationSourceColour(selectedSourceIndex)
                                   : reactorui::textMuted();
        g.setColour(accent.withAlpha(isMouseOverOrDragging() ? 0.95f : 0.72f));
        g.drawRoundedRectangle(bounds, 9.0f, 1.4f);
        g.setColour(active ? reactorui::textStrong() : reactorui::textMuted());
        g.setFont(reactorui::sectionFont(10.8f));
        g.drawFittedText(active ? "DRAG LFO " + juce::String(selectedSourceIndex) : "SELECT LFO",
                         getLocalBounds(),
                         juce::Justification::centred,
                         1);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (selectedSourceIndex <= 0 || ! event.mouseWasDraggedSinceMouseDown())
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            if (! container->isDragAndDropActive())
                container->startDragging("LFO:" + juce::String(selectedSourceIndex), this);
    }

private:
    int selectedSourceIndex = 0;
};
}

EffectsRackComponent::EffectsRackComponent(PluginProcessor& p)
    : processor(p)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(rackLabel);
    addAndMakeVisible(rackViewport);
    addAndMakeVisible(addModuleBox);
    addAndMakeVisible(addButton);
    addAndMakeVisible(lfoLabel);
    addAndMakeVisible(lfoSelector);
    lfoDragBadge = std::make_unique<FXLfoDragBadge>();
    addAndMakeVisible(*lfoDragBadge);

    titleLabel.setText("EFFECTS RACK", juce::dontSendNotification);
    subtitleLabel.setText("Build the rack in series or switch rows to parallel on the right. Drag the order into place, bypass rows with ON/OFF, and use SER/PAR to branch effects from the current chain.", juce::dontSendNotification);
    rackLabel.setText("Effect Modules", juce::dontSendNotification);
    styleRackLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleRackLabel(subtitleLabel, 11.5f, reactorui::textMuted());
    styleRackLabel(rackLabel, 13.0f, reactorui::textStrong());
    styleRackLabel(lfoLabel, 12.0f, reactorui::textStrong());
    lfoLabel.setText("FX LFO", juce::dontSendNotification);

    rackViewport.setViewedComponent(&rackContent, false);
    rackViewport.setScrollBarsShown(true, false);
    rackViewport.setScrollBarThickness(12);
    rackViewport.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff8f9399));
    rackViewport.setColour(juce::ScrollBar::trackColourId, juce::Colour(0xff13171c));

    for (int slot = 0; slot < PluginProcessor::maxFXSlots; ++slot)
    {
        auto* item = rackItems.add(new FXSlotRackItem(processor, slot));
        rackContent.addAndMakeVisible(item);

        auto* row = dynamic_cast<FXOrderRow*>(orderItems.add(new FXOrderRow(processor.apvts, slot)));
        addAndMakeVisible(row);
        if (row != nullptr)
        {
            styleActionButton(row->routeButton, reactorui::brandCyan());
            styleActionButton(row->powerButton, juce::Colour(0xff9ae074));
            styleActionButton(row->removeButton, juce::Colour(0xffff6767));
            row->removeButton.onClick = [this, slot]
            {
                processor.removeFXModuleFromOrder(slot);
                refreshRack();
            };
            row->onDragStateChange = [this](bool dragging)
            {
                orderDragInProgress = dragging;
                if (! dragging)
                    refreshRack();
            };
            row->onDropRequest = [this](int fromSlot, int dropY)
            {
                int targetSlot = 0;
                int activeCount = 0;
                for (int i = 0; i < orderItems.size(); ++i)
                {
                    auto* candidate = dynamic_cast<FXOrderRow*>(orderItems[i]);
                    if (candidate == nullptr || ! candidate->isVisible())
                        continue;

                    if (candidate->getSlotIndex() != fromSlot && dropY > candidate->getBounds().getCentreY())
                        ++targetSlot;
                    ++activeCount;
                }

                processor.moveFXModule(fromSlot, juce::jlimit(0, juce::jmax(0, activeCount - 1), targetSlot));
                refreshRack();
            };
        }
    }

    rebuildAddMenu();
    addModuleBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11161c));
    addModuleBox.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    addModuleBox.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    addModuleBox.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff98ff62));
    lfoSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11161c));
    lfoSelector.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    lfoSelector.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    lfoSelector.setColour(juce::ComboBox::arrowColourId, reactorui::brandCyan());
    lfoSelector.onChange = [this]
    {
        if (auto* badge = dynamic_cast<FXLfoDragBadge*>(lfoDragBadge.get()))
            badge->setSelectedSourceIndex(juce::jmax(0, lfoSelector.getSelectedId() - 1));
    };

    addButton.setButtonText("Add Module");
    styleActionButton(addButton, juce::Colour(0xffa7ff6b));
    addButton.onClick = [this]
    {
        processor.addFXModule(addModuleBox.getSelectedId());
        refreshRack();
    };

    refreshRack();
    refreshLfoSelector();
    startTimerHz(10);
}

void EffectsRackComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), juce::Colour(0xff98ff62), 14.0f);

    if (! rackPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, rackPanelBounds.toFloat(), juce::Colour(0xff75c6ff), 12.0f);

    if (! orderPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, orderPanelBounds.toFloat(), juce::Colour(0xffff785d), 12.0f);

    if (! lfoPanelBounds.isEmpty())
        reactorui::drawPanelBackground(g, lfoPanelBounds.toFloat(), reactorui::brandCyan(), 10.0f);

    const bool hasStages = std::any_of(cachedOrder.begin(), cachedOrder.end(), [] (int moduleType)
    {
        return PluginProcessor::isSupportedFXModuleType(moduleType) && moduleType != 0;
    });

    if (! hasStages && ! rackPanelBounds.isEmpty())
    {
        g.setColour(reactorui::textMuted());
        g.setFont(reactorui::bodyFont(13.0f));
        g.drawFittedText("No effect modules yet. Add a distortion, filter, chorus, flanger, phaser, delay, reverb, compressor, EQ, multiband comp, or bitcrusher stage from the right to start building the rack.",
                         rackPanelBounds.reduced(28, 24),
                         juce::Justification::centred,
                         2);
    }
}

void EffectsRackComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);
    titleLabel.setBounds(area.removeFromTop(22));
    subtitleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(12);

    orderPanelBounds = area.removeFromRight(324);
    area.removeFromRight(16);
    rackPanelBounds = area;

    auto rackArea = rackPanelBounds.reduced(14, 12);
    rackLabel.setBounds(rackArea.removeFromTop(20));
    rackArea.removeFromTop(8);
    rackViewport.setBounds(rackArea);

    auto orderArea = orderPanelBounds.reduced(14, 12);
    orderArea.removeFromTop(10);
    addModuleBox.setBounds(orderArea.removeFromTop(34));
    orderArea.removeFromTop(10);
    addButton.setBounds(orderArea.removeFromTop(34));
    orderArea.removeFromTop(14);
    lfoPanelBounds = orderArea.removeFromBottom(112);
    orderRowsBounds = orderArea;

    auto lfoArea = lfoPanelBounds.reduced(12, 10);
    lfoLabel.setBounds(lfoArea.removeFromTop(18));
    lfoArea.removeFromTop(8);
    lfoSelector.setBounds(lfoArea.removeFromTop(30));
    lfoArea.removeFromTop(10);
    if (lfoDragBadge != nullptr)
        lfoDragBadge->setBounds(lfoArea.removeFromTop(32));

    refreshRack();
}

void EffectsRackComponent::refreshRack()
{
    if (orderDragInProgress)
        return;

    const auto order = processor.getFXOrder();
    cachedOrder = order;
    addButton.setEnabled(order.indexOf(0) >= 0 && addModuleBox.getSelectedId() > 0);

    constexpr int stripGap = 12;
    const int contentWidth = juce::jmax(420, rackViewport.getWidth() - rackViewport.getScrollBarThickness() - 2);

    int y = 0;
    int visibleRows = 0;
    for (int slot = 0; slot < PluginProcessor::maxFXSlots; ++slot)
    {
        const bool active = processor.isFXSlotActive(slot);
        auto* item = dynamic_cast<FXSlotRackItem*>(rackItems[slot]);
        auto* row = dynamic_cast<FXOrderRow*>(orderItems[slot]);
        if (item == nullptr || row == nullptr)
            continue;

        item->setModuleType(order[slot]);
        item->setVisible(active);
        row->setVisible(active);

        if (active)
        {
            const int stripHeight = juce::jmax(188, item->getPreferredHeight());
            item->setBounds(0, y, contentWidth, stripHeight);
            y += stripHeight + stripGap;

            row->setRowData(slot, visibleRows, PluginProcessor::getFXModuleNames()[order[slot]], order[slot]);
            ++visibleRows;
        }
    }

    rackContent.setSize(contentWidth, juce::jmax(rackViewport.getHeight(), y > 0 ? y - stripGap : 80));
    layoutOrderRows();
}

void EffectsRackComponent::styleActionButton(juce::Button& button, juce::Colour accent) const
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141a20));
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.setColour(juce::TextButton::textColourOffId, reactorui::textStrong());
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0c1116));
}

void EffectsRackComponent::rebuildAddMenu()
{
    addModuleBox.clear(juce::dontSendNotification);

    for (int moduleType = 1; moduleType < PluginProcessor::getFXModuleNames().size(); ++moduleType)
    {
        if (! PluginProcessor::isSupportedFXModuleType(moduleType))
            continue;

        addModuleBox.addItem(PluginProcessor::getFXModuleNames()[moduleType], moduleType);
    }

    if (addModuleBox.getNumItems() > 0)
        addModuleBox.setSelectedItemIndex(0, juce::dontSendNotification);
}

void EffectsRackComponent::layoutOrderRows()
{
    if (orderRowsBounds.isEmpty())
        return;

    auto area = orderRowsBounds;
    const int rowHeight = 40;
    const int rowGap = 10;

    for (int i = 0; i < orderItems.size(); ++i)
    {
        auto* row = dynamic_cast<FXOrderRow*>(orderItems[i]);
        if (row == nullptr || ! row->isVisible())
            continue;

        auto targetBounds = area.removeFromTop(rowHeight);
        area.removeFromTop(rowGap);

        if (! row->isDragging())
            row->setBounds(targetBounds);
    }
}

void EffectsRackComponent::timerCallback()
{
    refreshLfoSelector();
    if (! orderDragInProgress)
        refreshRack();
}

void EffectsRackComponent::refreshLfoSelector()
{
    const auto names = processor.getModulationLfoNames();
    if (names != cachedLfoNames)
    {
        const int previousSourceIndex = juce::jmax(0, lfoSelector.getSelectedId() - 1);
        cachedLfoNames = names;
        lfoSelector.clear(juce::dontSendNotification);

        for (int i = 0; i < names.size(); ++i)
            lfoSelector.addItem(names[i], i + 1);

        int selectedSourceIndex = previousSourceIndex;
        if (selectedSourceIndex <= 0 && names.size() > 1)
            selectedSourceIndex = 1;
        selectedSourceIndex = juce::jlimit(0, juce::jmax(0, names.size() - 1), selectedSourceIndex);
        lfoSelector.setSelectedId(selectedSourceIndex + 1, juce::dontSendNotification);
    }

    if (auto* badge = dynamic_cast<FXLfoDragBadge*>(lfoDragBadge.get()))
        badge->setSelectedSourceIndex(juce::jmax(0, lfoSelector.getSelectedId() - 1));
}
