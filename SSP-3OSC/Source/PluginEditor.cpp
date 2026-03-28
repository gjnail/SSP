#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 2260;
constexpr int editorHeight = 2050;
constexpr float defaultEditorScale = 0.64f;

bool matchesAnyThemeColour(juce::Colour colour, std::initializer_list<juce::uint32> values)
{
    for (auto value : values)
        if (colour == juce::Colour(value))
            return true;

    return false;
}

void refreshThemeLabelColours(juce::Component& component)
{
    if (auto* label = dynamic_cast<juce::Label*>(&component))
    {
        const auto textColour = label->findColour(juce::Label::textColourId);
        if (matchesAnyThemeColour(textColour, { 0xffede5d8, 0xff101216, 0xffeff8e4 }))
            label->setColour(juce::Label::textColourId, reactorui::textStrong());
        else if (matchesAnyThemeColour(textColour, { 0xff98a2b3, 0xff5a5d66, 0xffd8dfcc }))
            label->setColour(juce::Label::textColourId, reactorui::textMuted());
    }

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        refreshThemeLabelColours(*component.getChildComponent(i));
}

void applyKeyboardTheme(juce::MidiKeyboardComponent& keyboard)
{
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                       reactorui::isLightTheme() ? juce::Colour(0xfffffbf4) : juce::Colour(0xfff0f4fb));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                       reactorui::isLightTheme() ? juce::Colour(0xff17191d) : juce::Colour(0xff10141d));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                       reactorui::isLightTheme() ? juce::Colour(0xff746c61) : juce::Colour(0xff243248));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                       reactorui::brandGold().withAlpha(0.28f));
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                       reactorui::brandCyan().withAlpha(0.38f));
}
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      defaultLookAndFeelScope(lookAndFeel),
      presetBrowser(p),
      osc1Section(p, p.apvts, 1),
      osc2Section(p, p.apvts, 2),
      osc3Section(p, p.apvts, 3),
      filterSection(p, p.apvts),
      ampEnvelopeSection(p, p.apvts, "AMP ADSR", "amp", juce::Colour(0xff53d6be)),
      filterEnvelopeSection(p, p.apvts, "FILTER ADSR", "filter", juce::Colour(0xff58c7ff)),
      lfoPanelSection(p),
      noiseSection(p, p.apvts),
      subOscSection(p, p.apvts),
      voiceSection(p, p.apvts),
      performanceSection(p, p.apvts),
      oscillatorSettingsSection(p, p.apvts),
      warpSection(p, p.apvts),
      masterSection(p, p.apvts),
      effectsRackSection(p),
      modMatrixSection(p),
      presetLibrarySection(p),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      effectsKeyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    reactorui::setThemeMode(processor.isLightThemeEnabled() ? reactorui::ThemeMode::light
                                                            : reactorui::ThemeMode::dark);
    lookAndFeel.refreshPalette();

    setResizable(true, true);
    setResizeLimits((int) (editorWidth * 0.65f), (int) (editorHeight * 0.65f),
                    (int) (editorWidth * 1.8f), (int) (editorHeight * 1.8f));
    setSize(juce::roundToInt((float) editorWidth * defaultEditorScale),
            juce::roundToInt((float) editorHeight * defaultEditorScale));
    setWantsKeyboardFocus(true);
    addAndMakeVisible(uiRoot);
    uiRoot.setInterceptsMouseClicks(false, true);

    uiRoot.addAndMakeVisible(headerSection);
    uiRoot.addAndMakeVisible(presetBrowser);
    uiRoot.addAndMakeVisible(synthTabButton);
    uiRoot.addAndMakeVisible(effectsTabButton);
    uiRoot.addAndMakeVisible(modMatrixTabButton);
    uiRoot.addAndMakeVisible(settingsTabButton);
    uiRoot.addAndMakeVisible(browserTabButton);
    uiRoot.addAndMakeVisible(synthPage);
    uiRoot.addAndMakeVisible(effectsPage);
    uiRoot.addAndMakeVisible(modMatrixPage);
    uiRoot.addAndMakeVisible(settingsPage);
    uiRoot.addAndMakeVisible(browserPage);

    synthTabButton.setButtonText("SYNTH");
    effectsTabButton.setButtonText("EFFECTS");
    modMatrixTabButton.setButtonText("MOD MATRIX");
    settingsTabButton.setButtonText("SETTINGS");
    browserTabButton.setButtonText("BROWSER");
    synthTabButton.onClick = [this] { setCurrentPage(Page::synth); };
    effectsTabButton.onClick = [this] { setCurrentPage(Page::effects); };
    modMatrixTabButton.onClick = [this] { setCurrentPage(Page::modMatrix); };
    settingsTabButton.onClick = [this] { setCurrentPage(Page::settings); };
    browserTabButton.onClick = [this] { setCurrentPage(Page::browser); };

    settingsPage.addAndMakeVisible(settingsThemeLabel);
    settingsPage.addAndMakeVisible(settingsThemeButton);
    reactorui::styleTitle(settingsThemeLabel, 14.0f);
    settingsThemeLabel.setText("UI THEME", juce::dontSendNotification);
    settingsThemeButton.onClick = [this]
    {
        processor.setLightThemeEnabled(! processor.isLightThemeEnabled());
        applyTheme();
    };

    synthPage.addAndMakeVisible(osc1Section);
    synthPage.addAndMakeVisible(osc2Section);
    synthPage.addAndMakeVisible(osc3Section);
    synthPage.addAndMakeVisible(filterSection);
    synthPage.addAndMakeVisible(ampEnvelopeSection);
    synthPage.addAndMakeVisible(filterEnvelopeSection);
    synthPage.addAndMakeVisible(lfoPanelSection);
    synthPage.addAndMakeVisible(noiseSection);
    synthPage.addAndMakeVisible(subOscSection);
    synthPage.addAndMakeVisible(warpSection);
    settingsPage.addAndMakeVisible(voiceSection);
    settingsPage.addAndMakeVisible(performanceSection);
    settingsPage.addAndMakeVisible(oscillatorSettingsSection);
    settingsPage.addAndMakeVisible(masterSection);

    keyboardComponent.setKeyWidth(30.0f);
    keyboardComponent.setAvailableRange(24, 108);
    keyboardComponent.setScrollButtonsVisible(false);
    synthPage.addAndMakeVisible(keyboardComponent);

    effectsPage.addAndMakeVisible(effectsRackSection);
    effectsKeyboardComponent.setKeyWidth(30.0f);
    effectsKeyboardComponent.setAvailableRange(24, 108);
    effectsKeyboardComponent.setScrollButtonsVisible(false);
    effectsPage.addAndMakeVisible(effectsKeyboardComponent);
    modMatrixPage.addAndMakeVisible(modMatrixSection);
    browserPage.addAndMakeVisible(presetLibrarySection);

    applyKeyboardTheme(keyboardComponent);
    applyKeyboardTheme(effectsKeyboardComponent);
    updateThemeButton();
    setCurrentPage(Page::synth);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto panel = getScaledContentBounds();
    juce::DropShadow((reactorui::isLightTheme() ? juce::Colour(0xff62594d) : juce::Colours::black)
                         .withAlpha(reactorui::isLightTheme() ? 0.18f : 0.3f),
                     24, { 0, 10 }).drawForRectangle(g, panel.getSmallestIntegerContainer());

    reactorui::drawEditorBackdrop(g, panel);
    g.setColour(reactorui::outline());
    g.drawRoundedRectangle(panel.reduced(0.5f), 10.0f, 1.0f);
}

juce::Rectangle<float> PluginEditor::getScaledContentBounds() const
{
    auto available = getLocalBounds().toFloat().reduced(12.0f);
    const float scale = juce::jmin(available.getWidth() / (float) editorWidth,
                                   available.getHeight() / (float) editorHeight);
    return juce::Rectangle<float>((float) editorWidth * scale, (float) editorHeight * scale)
        .withCentre(available.getCentre());
}

void PluginEditor::layoutContent()
{
    auto area = uiRoot.getLocalBounds().reduced(18, 16);

    auto headerArea = area.removeFromTop(96);
    headerSection.setBounds(headerArea);

    auto tabArea = headerArea.reduced(106, 12);
    tabArea.removeFromRight(448);
    tabArea.removeFromTop(52);
    tabArea = tabArea.removeFromBottom(26);
    const int tabGap = 10;
    const int tabWidth = (tabArea.getWidth() - tabGap * 4) / 5;
    synthTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
    tabArea.removeFromLeft(tabGap);
    effectsTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
    tabArea.removeFromLeft(tabGap);
    modMatrixTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
    tabArea.removeFromLeft(tabGap);
    settingsTabButton.setBounds(tabArea.removeFromLeft(tabWidth));
    tabArea.removeFromLeft(tabGap);
    browserTabButton.setBounds(tabArea);

    area.removeFromTop(8);
    presetBrowser.setBounds(area.removeFromTop(64));
    area.removeFromTop(12);

    synthPage.setBounds(area);
    effectsPage.setBounds(area);
    modMatrixPage.setBounds(area);
    settingsPage.setBounds(area);
    browserPage.setBounds(area);
    layoutSynthPage();
    layoutEffectsPage();
    modMatrixSection.setBounds(modMatrixPage.getLocalBounds());
    layoutSettingsPage();
    layoutBrowserPage();
}

void PluginEditor::layoutSynthPage()
{
    auto area = synthPage.getLocalBounds();

    constexpr int minTopRowHeight = 530;
    constexpr int warpHeight = 324;
    constexpr int envRowHeight = 236;
    constexpr int lfoHeight = 540;
    constexpr int sourceColumnWidth = 292;
    constexpr int keyboardHeight = 60;
    constexpr int oscGap = 14;
    constexpr int utilityGap = 10;
    constexpr int sectionGap = 14;
    constexpr int upperBodyHeight = warpHeight
                                  + sectionGap + envRowHeight;
    const int fixedHeightBelowTopRow = sectionGap + upperBodyHeight
                                     + sectionGap + lfoHeight
                                     + utilityGap + keyboardHeight;
    auto topRow = area.removeFromTop(juce::jmax(minTopRowHeight, area.getHeight() - fixedHeightBelowTopRow));
    const int oscWidth = (topRow.getWidth() - oscGap * 2) / 3;
    osc1Section.setBounds(topRow.removeFromLeft(oscWidth));
    topRow.removeFromLeft(oscGap);
    osc2Section.setBounds(topRow.removeFromLeft(oscWidth));
    topRow.removeFromLeft(oscGap);
    osc3Section.setBounds(topRow);

    area.removeFromTop(sectionGap);
    auto keyboardArea = area.removeFromBottom(juce::jmin(keyboardHeight, area.getHeight()));
    area.removeFromBottom(utilityGap);

    auto bodyArea = area;
    auto upperBody = bodyArea.removeFromTop(juce::jmin(upperBodyHeight, bodyArea.getHeight()));
    bodyArea.removeFromTop(sectionGap);

    const int rightColumnWidth = juce::jlimit(720, 860, (upperBody.getWidth() * 40) / 100);
    auto leftColumn = upperBody.removeFromLeft(juce::jmax(540, upperBody.getWidth() - rightColumnWidth - sectionGap));
    upperBody.removeFromLeft(sectionGap);
    auto rightColumn = upperBody;

    warpSection.setBounds(leftColumn.removeFromTop(juce::jmin(warpHeight, leftColumn.getHeight())));
    leftColumn.removeFromTop(sectionGap);

    auto envRow = leftColumn.removeFromTop(juce::jmin(envRowHeight, leftColumn.getHeight()));
    auto ampColumn = envRow.removeFromLeft((envRow.getWidth() - sectionGap) / 2);
    envRow.removeFromLeft(sectionGap);
    ampEnvelopeSection.setBounds(ampColumn);
    filterEnvelopeSection.setBounds(envRow);

    auto sourceColumn = rightColumn.removeFromRight(juce::jmin(sourceColumnWidth, rightColumn.getWidth() / 2));
    rightColumn.removeFromRight(sectionGap);

    filterSection.setBounds(rightColumn);

    auto subHeight = (sourceColumn.getHeight() - sectionGap) / 2;
    subOscSection.setBounds(sourceColumn.removeFromTop(subHeight));
    sourceColumn.removeFromTop(sectionGap);
    noiseSection.setBounds(sourceColumn);

    auto lowerRow = bodyArea.removeFromTop(juce::jmin(lfoHeight, bodyArea.getHeight()));
    lfoPanelSection.setBounds(lowerRow);

    keyboardComponent.setBounds(keyboardArea);
}

void PluginEditor::layoutEffectsPage()
{
    auto area = effectsPage.getLocalBounds();
    constexpr int keyboardHeight = 60;
    constexpr int keyboardGap = 10;

    area.removeFromBottom(keyboardHeight + keyboardGap);
    effectsRackSection.setBounds(area);

    auto keyboardArea = effectsPage.getLocalBounds().removeFromBottom(keyboardHeight);
    effectsKeyboardComponent.setBounds(keyboardArea);
}

void PluginEditor::layoutSettingsPage()
{
    auto area = settingsPage.getLocalBounds().reduced(72, 52);
    const int sectionGap = 18;
    auto themeRow = area.removeFromTop(58);
    settingsThemeLabel.setBounds(themeRow.removeFromLeft(160));
    settingsThemeButton.setBounds(themeRow.removeFromLeft(220).reduced(0, 6));
    area.removeFromTop(sectionGap);

    const int topRowHeight = juce::jmin(260, area.getHeight() / 2);

    auto topRow = area.removeFromTop(topRowHeight);
    const int voiceWidth = juce::jlimit(420, 620, topRow.getWidth() / 3);
    voiceSection.setBounds(topRow.removeFromLeft(voiceWidth));
    topRow.removeFromLeft(sectionGap);
    performanceSection.setBounds(topRow);

    area.removeFromTop(sectionGap);
    auto bottomRow = area.removeFromTop(juce::jmin(252, area.getHeight()));
    const int leftWidth = (bottomRow.getWidth() - sectionGap) / 2;
    oscillatorSettingsSection.setBounds(bottomRow.removeFromLeft(leftWidth));
    bottomRow.removeFromLeft(sectionGap);
    masterSection.setBounds(bottomRow);
}

void PluginEditor::layoutBrowserPage()
{
    presetLibrarySection.setBounds(browserPage.getLocalBounds());
}

void PluginEditor::setCurrentPage(Page page)
{
    currentPage = page;
    const bool showSynth = currentPage == Page::synth;
    const bool showEffects = currentPage == Page::effects;
    const bool showModMatrix = currentPage == Page::modMatrix;
    const bool showSettings = currentPage == Page::settings;
    const bool showBrowser = currentPage == Page::browser;
    synthPage.setVisible(showSynth);
    effectsPage.setVisible(showEffects);
    modMatrixPage.setVisible(showModMatrix);
    settingsPage.setVisible(showSettings);
    browserPage.setVisible(showBrowser);
    synthTabButton.setToggleState(showSynth, juce::dontSendNotification);
    effectsTabButton.setToggleState(showEffects, juce::dontSendNotification);
    modMatrixTabButton.setToggleState(showModMatrix, juce::dontSendNotification);
    settingsTabButton.setToggleState(showSettings, juce::dontSendNotification);
    browserTabButton.setToggleState(showBrowser, juce::dontSendNotification);
}

void PluginEditor::applyTheme()
{
    reactorui::setThemeMode(processor.isLightThemeEnabled() ? reactorui::ThemeMode::light
                                                            : reactorui::ThemeMode::dark);
    lookAndFeel.refreshPalette();
    applyKeyboardTheme(keyboardComponent);
    applyKeyboardTheme(effectsKeyboardComponent);
    updateThemeButton();
    refreshThemeLabelColours(uiRoot);
    uiRoot.repaint();
    repaint();
}

void PluginEditor::updateThemeButton()
{
    const bool lightEnabled = processor.isLightThemeEnabled();
    settingsThemeButton.setButtonText(lightEnabled ? "LIGHT MODE" : "DARK MODE");
    settingsThemeButton.setToggleState(lightEnabled, juce::dontSendNotification);
}

void PluginEditor::resized()
{
    uiRoot.setBounds(0, 0, editorWidth, editorHeight);
    layoutContent();

    auto scaled = getScaledContentBounds();
    const float scale = scaled.getWidth() / (float) editorWidth;
    uiRoot.setTransform(juce::AffineTransform::scale(scale).translated(scaled.getX(), scaled.getY()));
}
