#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer,
                           private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    enum class ProgressionView
    {
        progression,
        advanced
    };

    void timerCallback() override;
    void selectProgressionSlot(int slotIndex);
    void showChordPickerMenu(int slotIndex);
    void refreshScaleDegreeBox();
    void refreshStyleBox();
    void refreshSelectedSlotControls();

    PluginProcessor& pluginProcessor;

    juce::Label titleLabel;
    juce::Label tagLabel;
    juce::Label helperLabel;
    juce::Label settingsSummaryLabel;
    juce::Label progressionSummaryLabel;
    juce::Label exportStatusLabel;

    juce::Label modeCaptionLabel;
    juce::Label keyCaptionLabel;
    juce::Label scaleCaptionLabel;
    juce::Label styleCaptionLabel;
    juce::Label countCaptionLabel;
    juce::Label durationCaptionLabel;
    juce::Label playbackCaptionLabel;
    juce::Label rateCaptionLabel;
    juce::Label octaveCaptionLabel;
    juce::Label registerCaptionLabel;
    juce::Label rangeCaptionLabel;
    juce::Label spreadCaptionLabel;
    juce::Label velocityCaptionLabel;
    juce::Label gateCaptionLabel;
    juce::Label strumCaptionLabel;
    juce::Label swingCaptionLabel;
    juce::Label progressionCaptionLabel;
    juce::Label dragCaptionLabel;
    juce::Label slotCaptionLabel;
    juce::Label qualityCaptionLabel;
    juce::Label inversionCaptionLabel;
    juce::Label advancedHintLabel;

    juce::ComboBox modeBox;
    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::ComboBox styleBox;
    juce::ComboBox chordCountBox;
    juce::ComboBox chordDurationBox;
    juce::ComboBox playbackBox;
    juce::ComboBox rateBox;
    juce::ComboBox octaveBox;
    juce::ComboBox registerBox;
    juce::ComboBox rangeBox;
    juce::ComboBox spreadBox;
    juce::ComboBox slotRootBox;
    juce::ComboBox slotQualityBox;
    juce::Slider velocitySlider;
    juce::Slider gateSlider;
    juce::Slider strumSlider;
    juce::Slider swingSlider;

    juce::TextButton randomizeButton;
    juce::TextButton bakeButton;
    juce::TextButton exportButton;
    juce::TextButton progressionTabButton;
    juce::TextButton advancedTabButton;
    juce::TextButton inversionButton;

    std::array<std::unique_ptr<juce::Button>, 8> progressionButtons;
    std::array<juce::Rectangle<int>, 8> progressionCardBounds;

    std::unique_ptr<juce::Component> dragTile;
    std::unique_ptr<juce::Component> chordEditor;
    std::unique_ptr<juce::FileChooser> exportChooser;

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::unique_ptr<ComboBoxAttachment> keyAttachment;
    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<ComboBoxAttachment> styleAttachment;
    std::unique_ptr<ComboBoxAttachment> chordCountAttachment;
    std::unique_ptr<ComboBoxAttachment> chordDurationAttachment;
    std::unique_ptr<ComboBoxAttachment> playbackAttachment;
    std::unique_ptr<ComboBoxAttachment> rateAttachment;
    std::unique_ptr<ComboBoxAttachment> octaveAttachment;
    std::unique_ptr<ComboBoxAttachment> registerAttachment;
    std::unique_ptr<ComboBoxAttachment> rangeAttachment;
    std::unique_ptr<ComboBoxAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;
    std::unique_ptr<SliderAttachment> strumAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;

    juce::Rectangle<int> generationArea;
    juce::Rectangle<int> performanceArea;
    juce::Rectangle<int> progressionArea;
    juce::Rectangle<int> exportArea;
    ProgressionView progressionView = ProgressionView::progression;
    int selectedProgressionSlot = 0;
    juce::StringArray lastScaleDegreeItems;
    juce::StringArray lastStyleItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
