#include "ModMatrixComponent.h"
#include "ReactorUI.h"

namespace
{
juce::Colour matrixAccent()
{
    return juce::Colour(0xff89ff8d);
}

void styleMatrixCombo(juce::ComboBox& box)
{
    box.setJustificationType(juce::Justification::centredLeft);
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10151c));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::arrowColourId, matrixAccent());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
}
}

class ModMatrixComponent::MatrixRow final : public juce::Component,
                                            public juce::DragAndDropTarget
{
public:
    MatrixRow(PluginProcessor& p, int slot)
        : processor(p), slotIndex(slot)
    {
        addAndMakeVisible(slotLabel);
        addAndMakeVisible(sourceBox);
        addAndMakeVisible(destinationBox);
        addAndMakeVisible(amountSlider);
        addAndMakeVisible(clearButton);

        slotLabel.setText(juce::String(slotIndex), juce::dontSendNotification);
        slotLabel.setFont(reactorui::sectionFont(12.0f));
        slotLabel.setColour(juce::Label::textColourId, reactorui::textStrong());
        slotLabel.setJustificationType(juce::Justification::centred);

        styleMatrixCombo(sourceBox);
        styleMatrixCombo(destinationBox);
        destinationBox.addItemList(reactormod::getDestinationNames(), 1);

        amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
        amountSlider.setColour(juce::Slider::trackColourId, matrixAccent());
        amountSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a2220));
        amountSlider.setColour(juce::Slider::thumbColourId, matrixAccent().brighter(0.18f));
        amountSlider.setColour(juce::Slider::textBoxTextColourId, reactorui::textStrong());
        amountSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0b1110));
        amountSlider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());

        clearButton.setButtonText("CLR");
        clearButton.onClick = [this]
        {
            processor.setMatrixSourceForSlot(slotIndex - 1, 0);
            sourceBox.setSelectedId(1, juce::dontSendNotification);
            destinationBox.setSelectedItemIndex(0, juce::sendNotificationSync);
            amountSlider.setValue(0.0, juce::sendNotificationSync);
        };

        sourceBox.onChange = [this]
        {
            if (! syncingSource)
                processor.setMatrixSourceForSlot(slotIndex - 1, juce::jmax(0, sourceBox.getSelectedId() - 1));
        };

        destinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.apvts, reactormod::getMatrixDestinationParamID(slotIndex), destinationBox);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, reactormod::getMatrixAmountParamID(slotIndex), amountSlider);

        refreshSourceItems();
        syncSourceFromProcessor();
    }

    void refreshSourceItems()
    {
        const auto currentSources = processor.getAvailableModulationSources();
        juce::StringArray currentNames;
        currentNames.add("Off");
        for (const auto& source : currentSources)
            currentNames.add(source.name);

        if (currentNames == cachedSourceNames)
            return;

        cachedSourceNames = currentNames;
        syncingSource = true;
        sourceBox.clear(juce::dontSendNotification);
        sourceBox.addItem("Off", 1);
        for (const auto& source : currentSources)
            sourceBox.addItem(source.name, source.sourceIndex + 1);
        syncSourceFromProcessor();
        syncingSource = false;
    }

    void syncSourceFromProcessor()
    {
        syncingSource = true;
        sourceBox.setSelectedId(processor.getMatrixSourceForSlot(slotIndex - 1) + 1, juce::dontSendNotification);
        syncingSource = false;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0d1212));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(dropActive ? matrixAccent() : reactorui::outline());
        g.drawRoundedRectangle(bounds, 10.0f, dropActive ? 1.6f : 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 8);
        slotLabel.setBounds(area.removeFromLeft(28));
        area.removeFromLeft(8);
        sourceBox.setBounds(area.removeFromLeft(160));
        area.removeFromLeft(10);
        destinationBox.setBounds(area.removeFromLeft(280));
        area.removeFromLeft(10);
        amountSlider.setBounds(area.removeFromLeft(220));
        area.removeFromLeft(10);
        clearButton.setBounds(area.removeFromLeft(56));
    }

    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override
    {
        return reactormod::isModulationSourceDragDescription(dragSourceDetails.description.toString());
    }

    void itemDragEnter(const SourceDetails&) override
    {
        dropActive = true;
        repaint();
    }

    void itemDragExit(const SourceDetails&) override
    {
        dropActive = false;
        repaint();
    }

    void itemDropped(const SourceDetails& dragSourceDetails) override
    {
        dropActive = false;
        const int sourceIndex = reactormod::sourceIndexFromDragDescription(dragSourceDetails.description.toString());
        if (sourceIndex <= 0)
            return;

        processor.setMatrixSourceForSlot(slotIndex - 1, sourceIndex);
        syncSourceFromProcessor();

        if (destinationBox.getSelectedItemIndex() <= 0)
            destinationBox.setSelectedItemIndex(1, juce::sendNotificationSync);

        if (std::abs((float) amountSlider.getValue()) < 0.0001f)
            amountSlider.setValue(0.5, juce::sendNotificationSync);

        repaint();
    }

private:
    PluginProcessor& processor;
    int slotIndex = 1;
    juce::Label slotLabel;
    juce::ComboBox sourceBox;
    juce::ComboBox destinationBox;
    juce::Slider amountSlider;
    juce::TextButton clearButton;
    juce::StringArray cachedSourceNames;
    bool syncingSource = false;
    bool dropActive = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> destinationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

ModMatrixComponent::ModMatrixComponent(PluginProcessor& p)
    : processor(p)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subLabel);
    addAndMakeVisible(dragLabel);
    addAndMakeVisible(rowsViewport);
    rowsViewport.setViewedComponent(&rowsContent, false);
    rowsViewport.setScrollBarsShown(true, false);

    titleLabel.setText("MOD MATRIX", juce::dontSendNotification);
    reactorui::styleTitle(titleLabel, 15.0f);

    subLabel.setText("Route LFOs and MOD knobs from the source bank into the synth or FX with per-slot destination and bipolar amount.",
                     juce::dontSendNotification);
    reactorui::styleMeta(subLabel, 11.2f);

    dragLabel.setText("Drag an LFO or MOD source into a row, or pick it from the Source menu.", juce::dontSendNotification);
    dragLabel.setFont(reactorui::bodyFont(11.0f));
    dragLabel.setColour(juce::Label::textColourId, matrixAccent().withAlpha(0.82f));
    dragLabel.setJustificationType(juce::Justification::centredLeft);

    rows.reserve((size_t) reactormod::matrixSlotCount);
    for (int slot = 1; slot <= reactormod::matrixSlotCount; ++slot)
    {
        auto row = std::make_unique<MatrixRow>(processor, slot);
        rowsContent.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    startTimerHz(4);
}

ModMatrixComponent::~ModMatrixComponent() = default;

void ModMatrixComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), matrixAccent(), 18.0f);
}

void ModMatrixComponent::resized()
{
    auto area = getLocalBounds().reduced(16, 14);
    titleLabel.setBounds(area.removeFromTop(22));
    subLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);
    dragLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(12);

    const int rowGap = 8;
    const int rowHeight = 48;
    rowsViewport.setBounds(area);

    auto contentArea = rowsViewport.getLocalBounds();
    const int contentHeight = juce::jmax(contentArea.getHeight(),
                                         (int) rows.size() * rowHeight + juce::jmax(0, (int) rows.size() - 1) * rowGap + 16);
    rowsContent.setSize(contentArea.getWidth(), contentHeight);

    auto rowsArea = rowsContent.getLocalBounds().reduced(0, 8);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        rows[i]->setBounds(rowsArea.removeFromTop(rowHeight));
        rows[i]->refreshSourceItems();
        rows[i]->syncSourceFromProcessor();
        if (i + 1 < rows.size())
            rowsArea.removeFromTop(rowGap);
    }
}

void ModMatrixComponent::timerCallback()
{
    for (auto& row : rows)
    {
        row->refreshSourceItems();
        row->syncSourceFromProcessor();
    }
}
