#include "WarpSectionComponent.h"
#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
void styleWarpLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleWarpButton(juce::TextButton& button)
{
    button.setClickingTogglesState(true);
    button.setRadioGroupId(8803);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141b22));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff95ff62));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd7e4ee));
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff10200c));
}

void styleWarpCombo(juce::ComboBox& box, juce::Colour accent)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10161d));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, accent);
}
}

WarpSectionComponent::WarpSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state)
    : apvts(state),
      saturatorSlider(processor, "warpSaturator", reactormod::Destination::warpSaturator, juce::Colour(0xfff4c24d), 64, 22),
      mutateSlider(processor, "warpMutate", reactormod::Destination::warpMutate, juce::Colour(0xff95ff62), 64, 22)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(saturatorLabel);
    addAndMakeVisible(mutateLabel);
    addAndMakeVisible(saturatorSlider);
    addAndMakeVisible(mutateSlider);
    addAndMakeVisible(beforeFilterButton);
    addAndMakeVisible(afterFilterButton);

    titleLabel.setText("WARP", juce::dontSendNotification);
    saturatorLabel.setText("Saturator", juce::dontSendNotification);
    mutateLabel.setText("Mutate", juce::dontSendNotification);
    beforeFilterButton.setButtonText("Before Filter");
    afterFilterButton.setButtonText("After Filter");

    styleWarpLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleWarpLabel(saturatorLabel, 10.5f, reactorui::textMuted(), juce::Justification::centred);
    styleWarpLabel(mutateLabel, 10.5f, reactorui::textMuted(), juce::Justification::centred);
    styleWarpButton(beforeFilterButton);
    styleWarpButton(afterFilterButton);

    saturatorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "warpSaturator", saturatorSlider);
    mutateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "warpMutate", mutateSlider);

    beforeFilterButton.onClick = [this] { setSaturatorPlacement(false); };
    afterFilterButton.onClick = [this] { setSaturatorPlacement(true); };

    const std::array<juce::Colour, 2> slotAccents{ juce::Colour(0xff68f0d0), juce::Colour(0xfff6ca62) };
    const std::array<const char*, 3> legacyWarpNames{ "FM", "Sync", "Bend" };
    const std::array<juce::Colour, 3> legacyWarpAccents{ juce::Colour(0xff59d7ff), juce::Colour(0xffb7ff70), juce::Colour(0xffffca6a) };
    const std::array<const char*, 3> legacyWarpParamSuffixes{ "WarpFM", "WarpSync", "WarpBend" };

    for (int osc = 0; osc < 3; ++osc)
    {
        auto& oscTitle = oscTitleLabels[(size_t) osc];
        auto& fmSourceLabel = fmSourceLabels[(size_t) osc];
        auto& fmSourceBox = fmSourceBoxes[(size_t) osc];

        addAndMakeVisible(oscTitle);
        addAndMakeVisible(fmSourceLabel);
        addAndMakeVisible(fmSourceBox);

        oscTitle.setText("OSC " + juce::String(osc + 1), juce::dontSendNotification);
        fmSourceLabel.setText("FM Source", juce::dontSendNotification);

        styleWarpLabel(oscTitle, 12.5f, reactorui::textStrong(), juce::Justification::centred);
        styleWarpLabel(fmSourceLabel, 11.5f, reactorui::textMuted());
        fmSourceBox.addItemList(PluginProcessor::getWarpFMSourceNames(), 1);
        styleWarpCombo(fmSourceBox, juce::Colour(0xff68f0d0));

        const auto prefix = "osc" + juce::String(osc + 1);
        fmSourceAttachments[(size_t) osc] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "WarpFMSource", fmSourceBox);

        for (int legacyIndex = 0; legacyIndex < 3; ++legacyIndex)
        {
            auto& legacyLabel = legacyWarpLabels[(size_t) osc][(size_t) legacyIndex];
            auto& legacySlider = legacyWarpSliders[(size_t) osc][(size_t) legacyIndex];

            addAndMakeVisible(legacyLabel);
            legacyLabel.setText(legacyWarpNames[(size_t) legacyIndex], juce::dontSendNotification);
            styleWarpLabel(legacyLabel, 10.0f, reactorui::textMuted(), juce::Justification::centred);

            const auto parameterID = prefix + legacyWarpParamSuffixes[(size_t) legacyIndex];
            legacySlider = std::make_unique<ModulationKnob>(processor, parameterID, reactormod::Destination::none,
                                                            legacyWarpAccents[(size_t) legacyIndex], 54, 18);
            addAndMakeVisible(*legacySlider);

            legacyWarpAttachments[(size_t) osc][(size_t) legacyIndex] =
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, parameterID, *legacySlider);
        }

        for (int slot = 0; slot < 2; ++slot)
        {
            auto& slotLabel = warpSlotLabels[(size_t) osc][(size_t) slot];
            auto& modeBox = warpModeBoxes[(size_t) osc][(size_t) slot];
            auto& amountSlider = warpAmountSliders[(size_t) osc][(size_t) slot];

            addAndMakeVisible(modeBox);

            slotLabel.setText(slot == 0 ? "Warp A" : "Warp B", juce::dontSendNotification);
            styleWarpLabel(slotLabel, 10.0f, reactorui::textMuted(), juce::Justification::centred);
            modeBox.addItemList(PluginProcessor::getWarpModeNames(), 1);
            styleWarpCombo(modeBox, slotAccents[(size_t) slot]);

            const auto parameterID = prefix + "Warp" + juce::String(slot + 1) + "Amount";
            const auto destination = reactormod::destinationForParameterID(parameterID);
            amountSlider = std::make_unique<ModulationKnob>(processor, parameterID, destination,
                                                            slotAccents[(size_t) slot], 64, 22);
            addAndMakeVisible(*amountSlider);

            warpModeAttachments[(size_t) osc][(size_t) slot] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, prefix + "Warp" + juce::String(slot + 1) + "Mode", modeBox);
            warpAmountAttachments[(size_t) osc][(size_t) slot] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, prefix + "Warp" + juce::String(slot + 1) + "Amount", *amountSlider);
        }
    }

    startTimerHz(24);
}

void WarpSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    reactorui::drawPanelBackground(g, bounds, juce::Colour(0xff91ff63), 12.0f);

    auto content = getLocalBounds().reduced(14);
    content.removeFromTop(28);

    auto globalRow = content.removeFromTop(98);
    auto leftCard = globalRow.removeFromLeft(164).toFloat();
    globalRow.removeFromLeft(14);
    auto midCard = globalRow.removeFromLeft(164).toFloat();
    globalRow.removeFromLeft(18);
    auto rightCard = globalRow.removeFromRight(326).toFloat();

    reactorui::drawPanelBackground(g, leftCard, juce::Colour(0xfff4c24d), 10.0f);
    reactorui::drawPanelBackground(g, midCard, juce::Colour(0xff91ff63), 10.0f);
    reactorui::drawPanelBackground(g, rightCard, juce::Colour(0xff6ce5ff), 10.0f);

    content.removeFromTop(6);
    const int gap = 12;
    const int groupWidth = (content.getWidth() - gap * 2) / 3;
    for (int osc = 0; osc < 3; ++osc)
    {
        auto group = content.removeFromLeft(groupWidth).toFloat();
        reactorui::drawPanelBackground(g, group, juce::Colour(0xff4fff9c), 10.0f);

        if (osc < 2)
            content.removeFromLeft(gap);
    }
}

void WarpSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    auto globalRow = area.removeFromTop(98);
    auto leftCard = globalRow.removeFromLeft(164);
    globalRow.removeFromLeft(14);
    auto midCard = globalRow.removeFromLeft(164);
    globalRow.removeFromLeft(18);
    auto rightCard = globalRow.removeFromRight(326);

    auto leftContent = leftCard.reduced(18, 8);
    saturatorLabel.setBounds(leftContent.removeFromTop(14));
    leftContent.removeFromTop(2);
    saturatorSlider.setBounds(leftContent);

    auto midContent = midCard.reduced(18, 8);
    mutateLabel.setBounds(midContent.removeFromTop(14));
    midContent.removeFromTop(2);
    mutateSlider.setBounds(midContent);

    auto placementArea = rightCard.reduced(18, 10);
    placementArea.removeFromTop(2);
    auto buttons = placementArea.removeFromTop(26);
    beforeFilterButton.setBounds(buttons.removeFromLeft((buttons.getWidth() - 10) / 2));
    buttons.removeFromLeft(10);
    afterFilterButton.setBounds(buttons);

    area.removeFromTop(6);
    const int gap = 12;
    const int groupWidth = (area.getWidth() - gap * 2) / 3;

    for (int osc = 0; osc < 3; ++osc)
    {
        auto group = area.removeFromLeft(groupWidth).reduced(6, 1);
        if (osc < 2)
            area.removeFromLeft(gap);

        oscTitleLabels[(size_t) osc].setBounds(group.removeFromTop(12));
        group.removeFromTop(2);

        auto fmSourceRow = group.removeFromTop(22);
        fmSourceLabels[(size_t) osc].setBounds(fmSourceRow.removeFromLeft(56));
        fmSourceBoxes[(size_t) osc].setBounds(fmSourceRow);
        group.removeFromTop(6);

        auto legacyRow = group.removeFromTop(70);
        const int legacyGap = 8;
        const int legacyWidth = (legacyRow.getWidth() - legacyGap * 2) / 3;
        for (int legacyIndex = 0; legacyIndex < 3; ++legacyIndex)
        {
            auto legacyArea = legacyRow.removeFromLeft(legacyWidth);
            if (legacyIndex < 2)
                legacyRow.removeFromLeft(legacyGap);

            legacyWarpLabels[(size_t) osc][(size_t) legacyIndex].setBounds(legacyArea.removeFromTop(14));
            legacyArea.removeFromTop(2);
            if (legacyWarpSliders[(size_t) osc][(size_t) legacyIndex] != nullptr)
                legacyWarpSliders[(size_t) osc][(size_t) legacyIndex]->setBounds(legacyArea);
        }

        group.removeFromTop(6);
        const int slotGap = 12;
        const int slotWidth = (group.getWidth() - slotGap) / 2;

        for (int slot = 0; slot < 2; ++slot)
        {
            auto slotArea = group.removeFromLeft(slotWidth);
            if (slot == 0)
                group.removeFromLeft(slotGap);

            warpModeBoxes[(size_t) osc][(size_t) slot].setBounds(slotArea.removeFromTop(24));
            warpSlotLabels[(size_t) osc][(size_t) slot].setBounds({});
            slotArea.removeFromTop(4);
            if (warpAmountSliders[(size_t) osc][(size_t) slot] != nullptr)
                warpAmountSliders[(size_t) osc][(size_t) slot]->setBounds(slotArea);
        }
    }
}

void WarpSectionComponent::timerCallback()
{
    const bool afterFilter = apvts.getRawParameterValue("warpSaturatorPostFilter")->load() >= 0.5f;
    beforeFilterButton.setToggleState(!afterFilter, juce::dontSendNotification);
    afterFilterButton.setToggleState(afterFilter, juce::dontSendNotification);

    for (int osc = 0; osc < 3; ++osc)
    {
        bool usesFMSource = false;
        for (int slot = 0; slot < 2; ++slot)
        {
            const int modeIndex = warpModeBoxes[(size_t) osc][(size_t) slot].getSelectedItemIndex();
            if (warpAmountSliders[(size_t) osc][(size_t) slot] != nullptr)
                warpAmountSliders[(size_t) osc][(size_t) slot]->setEnabled(modeIndex > 0);
            usesFMSource = usesFMSource || modeIndex == 1;
        }

        fmSourceBoxes[(size_t) osc].setEnabled(usesFMSource);
        fmSourceBoxes[(size_t) osc].setAlpha(usesFMSource ? 1.0f : 0.55f);
        fmSourceLabels[(size_t) osc].setAlpha(usesFMSource ? 1.0f : 0.6f);
    }
}

void WarpSectionComponent::setSaturatorPlacement(bool afterFilter)
{
    if (auto* parameter = apvts.getParameter("warpSaturatorPostFilter"))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(afterFilter ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    }
}
