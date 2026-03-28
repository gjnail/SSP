#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OscillatorSectionComponent.h"
#include "FilterSectionComponent.h"
#include "EnvelopeSectionComponent.h"
#include "LfoPanelComponent.h"
#include "NoiseSectionComponent.h"
#include "SubOscSectionComponent.h"
#include "VoiceSectionComponent.h"
#include "PerformanceSectionComponent.h"
#include "OscillatorSettingsComponent.h"
#include "WarpSectionComponent.h"
#include "MasterSectionComponent.h"
#include "ModMatrixComponent.h"
#include "PresetBrowserComponent.h"
#include "PresetLibraryComponent.h"
#include "HeaderSectionComponent.h"
#include "EffectsRackComponent.h"
#include "ReactorUI.h"

class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct ScopedDefaultLookAndFeel
    {
        explicit ScopedDefaultLookAndFeel(juce::LookAndFeel& lf) : lookAndFeel(lf)
        {
            juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        }

        ~ScopedDefaultLookAndFeel()
        {
            if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lookAndFeel)
                juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        }

        juce::LookAndFeel& lookAndFeel;
    };

    enum class Page
    {
        synth = 0,
        effects,
        modMatrix,
        settings,
        browser
    };

    juce::Rectangle<float> getScaledContentBounds() const;
    void layoutContent();
    void layoutSynthPage();
    void layoutEffectsPage();
    void layoutSettingsPage();
    void layoutBrowserPage();
    void setCurrentPage(Page page);
    void applyTheme();
    void updateThemeButton();

    PluginProcessor& processor;
    reactorui::LookAndFeel lookAndFeel;
    ScopedDefaultLookAndFeel defaultLookAndFeelScope;
    juce::Component uiRoot;
    HeaderSectionComponent headerSection;
    juce::TextButton synthTabButton;
    juce::TextButton effectsTabButton;
    juce::TextButton modMatrixTabButton;
    juce::TextButton settingsTabButton;
    juce::TextButton browserTabButton;
    PresetBrowserComponent presetBrowser;
    juce::Component synthPage;
    juce::Component effectsPage;
    juce::Component modMatrixPage;
    juce::Component settingsPage;
    juce::Component browserPage;
    juce::Label settingsThemeLabel;
    juce::TextButton settingsThemeButton;
    OscillatorSectionComponent osc1Section;
    OscillatorSectionComponent osc2Section;
    OscillatorSectionComponent osc3Section;
    FilterSectionComponent filterSection;
    EnvelopeSectionComponent ampEnvelopeSection;
    EnvelopeSectionComponent filterEnvelopeSection;
    LfoPanelComponent lfoPanelSection;
    NoiseSectionComponent noiseSection;
    SubOscSectionComponent subOscSection;
    VoiceSectionComponent voiceSection;
    PerformanceSectionComponent performanceSection;
    OscillatorSettingsComponent oscillatorSettingsSection;
    WarpSectionComponent warpSection;
    MasterSectionComponent masterSection;
    EffectsRackComponent effectsRackSection;
    ModMatrixComponent modMatrixSection;
    PresetLibraryComponent presetLibrarySection;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::MidiKeyboardComponent effectsKeyboardComponent;
    Page currentPage = Page::synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
