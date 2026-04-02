#include "PluginEditor.h"

#include "PitchScale.h"
#include "SSPVectorUI.h"

namespace
{
using namespace ssp::pitch;
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

constexpr int editorWidth = 1220;
constexpr int editorHeight = 760;

juce::Colour accentTeal() { return juce::Colour(0xff35d4c9); }
juce::Colour accentCyan() { return juce::Colour(0xff34c6ff); }
juce::Colour accentAmber() { return juce::Colour(0xffffc15a); }
juce::Colour accentCoral() { return juce::Colour(0xffff6d6b); }
juce::Colour strongText() { return juce::Colour(0xffedf6ff); }
juce::Colour softText() { return juce::Colour(0xff91a8ba); }

juce::String formatPercent(double value)
{
    return juce::String((int) std::round(value)) + "%";
}

const juce::StringArray& getInputRangeNames()
{
    static const juce::StringArray names{ "Soprano", "Alto/Tenor", "Low Male", "Instrument" };
    return names;
}

class KnobField final : public juce::Component
{
public:
    KnobField(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& titleText)
        : attachment(state, paramId, knob)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(knob);
        title.setText(titleText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, softText());
        title.setFont(juce::Font(12.0f, juce::Font::bold));
        knob.setColour(juce::Slider::rotarySliderFillColourId, accentCyan());
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    }

    void setFormatting(std::function<juce::String(double)> formatter)
    {
        knob.textFromValueFunction = std::move(formatter);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds(area.removeFromTop(18));
        knob.setBounds(area);
    }

private:
    juce::Label title;
    ssp::ui::SSPKnob knob;
    SliderAttachment attachment;
};

class LiveMeterComponent final : public juce::Component
{
public:
    explicit LiveMeterComponent(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        ssp::ui::drawPanelBackground(g, bounds, accentCyan(), 18.0f);

        auto area = bounds.reduced(28.0f);
        auto header = area.removeFromTop(34.0f);
        const bool pitched = processor.isLivePitchDetected();
        const float detectedMidi = processor.getLiveDetectedMidi();
        const float targetMidi = processor.getLiveTargetMidi();
        const float correctionCents = processor.getLiveCorrectionCents();
        const float inputLevel = processor.getLiveInputLevel();
        const juce::String displayedNote = pitched ? formatMidiNote(targetMidi, false) : juce::String("--");
        const juce::String detectedText = pitched ? "Detected " + formatMidiNote(detectedMidi, true) : "Waiting for pitched input";

        g.setColour(strongText());
        g.setFont(juce::Font(17.0f, juce::Font::bold));
        g.drawText("Live Pitch Correction", header.removeFromLeft(260).toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(softText());
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(detectedText, header.toNearestInt(), juce::Justification::centredRight, false);

        auto lower = area.removeFromBottom(56.0f);
        auto meter = area.withSizeKeepingCentre(360.0f, 360.0f);

        g.setColour(juce::Colour(0xff10161d));
        g.fillEllipse(meter);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(meter, 2.0f);

        const float startAngle = juce::MathConstants<float>::pi * 1.15f;
        const float endAngle = juce::MathConstants<float>::twoPi + juce::MathConstants<float>::pi * 0.85f;
        const float centsNormalised = juce::jlimit(-1.0f, 1.0f, correctionCents / 100.0f);
        const float angle = juce::jmap(centsNormalised, -1.0f, 1.0f, startAngle, endAngle);
        const float radius = meter.getWidth() * 0.39f;

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(meter.getCentreX(), meter.getCentreY(), radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.strokePath(backgroundArc, juce::PathStrokeType(18.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(meter.getCentreX(), meter.getCentreY(), radius, radius, 0.0f,
                               juce::MathConstants<float>::pi * 1.5f, angle, true);
        g.setColour(! pitched ? softText() : (std::abs(correctionCents) > 30.0f ? accentCoral() : accentCyan()));
        g.strokePath(valueArc, juce::PathStrokeType(18.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(strongText());
        g.setFont(juce::Font(78.0f, juce::Font::bold));
        g.drawText(displayedNote, meter.toNearestInt(), juce::Justification::centred, false);

        auto centreInfo = meter.toNearestInt().translated(0, 74);
        g.setColour(softText());
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        const juce::String centerLabel = pitched
            ? (std::abs(correctionCents) < 1.0f ? "In tune" : juce::String(correctionCents > 0.0f ? "+" : "") + juce::String(correctionCents, 1) + " cents")
            : "Sing or play a monophonic line";
        g.drawText(centerLabel, centreInfo, juce::Justification::centred, false);

        auto levelArea = lower.removeFromLeft(240.0f);
        g.setColour(softText());
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("Input Level", levelArea.removeFromTop(20).toNearestInt(), juce::Justification::centredLeft, false);
        auto bar = levelArea.toFloat().reduced(0.0f, 10.0f);
        g.setColour(juce::Colour(0xff0d1319));
        g.fillRoundedRectangle(bar, 8.0f);
        g.setColour(accentTeal().withAlpha(0.85f));
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, inputLevel * 3.0f)), 8.0f);

        auto rightInfo = lower;
        g.setColour(strongText());
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("Target Note", rightInfo.removeFromTop(18).toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(softText());
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(pitched ? formatMidiNote(targetMidi, true) : "--", rightInfo.removeFromTop(22).toNearestInt(),
                   juce::Justification::centredLeft, false);
        g.drawText("Live mode is optimized for vocals, bass, and other monophonic sources.",
                   rightInfo.toNearestInt(), juce::Justification::centredLeft, true);
    }

private:
    PluginProcessor& processor;
};

class KeyboardStripComponent final : public juce::Component
{
public:
    explicit KeyboardStripComponent(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        ssp::ui::drawPanelBackground(g, bounds, accentAmber(), 16.0f);

        auto area = bounds.reduced(18.0f, 16.0f);
        const int startMidi = 36;
        const int endMidi = 84;
        const int activeDetected = juce::roundToInt(processor.getLiveDetectedMidi());
        const int activeTarget = juce::roundToInt(processor.getLiveTargetMidi());
        const int keyRoot = (int) processor.apvts.getRawParameterValue("scaleKey")->load();
        const auto modeName = getScaleModeNames()[juce::jlimit(0, getScaleModeNames().size() - 1,
                                                               (int) processor.apvts.getRawParameterValue("scaleMode")->load())];
        const juce::StringArray names{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        juce::Array<int> whiteNotes;
        for (int midi = startMidi; midi < endMidi; ++midi)
        {
            const int noteClass = ((midi % 12) + 12) % 12;
            const bool black = noteClass == 1 || noteClass == 3 || noteClass == 6 || noteClass == 8 || noteClass == 10;
            if (! black)
                whiteNotes.add(midi);
        }

        const float whiteWidth = area.getWidth() / (float) juce::jmax(1, whiteNotes.size());
        std::map<int, juce::Rectangle<float>> whiteBounds;
        for (int i = 0; i < whiteNotes.size(); ++i)
        {
            const int midi = whiteNotes[i];
            auto key = juce::Rectangle<float>(area.getX() + whiteWidth * (float) i, area.getY(), whiteWidth + 1.0f, area.getHeight());
            whiteBounds[midi] = key;
            const bool inScale = isMidiNoteInScale(midi, keyRoot, modeName);
            const bool detected = midi == activeDetected;
            const bool target = midi == activeTarget;
            juce::Colour fill = inScale ? juce::Colour(0xffe8eef4) : juce::Colour(0xffc8d0d8);
            if (target)
                fill = accentAmber();
            else if (detected)
                fill = accentCyan();
            g.setColour(fill);
            g.fillRoundedRectangle(key, 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.drawRect(key);
            if (((midi % 12) + 12) % 12 == 0)
            {
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.setFont(juce::Font(10.0f, juce::Font::bold));
                g.drawText(names[0] + juce::String((midi / 12) - 1), key.toNearestInt().removeFromBottom(16), juce::Justification::centred, false);
            }
        }

        for (int midi = startMidi; midi < endMidi; ++midi)
        {
            const int noteClass = ((midi % 12) + 12) % 12;
            const bool black = noteClass == 1 || noteClass == 3 || noteClass == 6 || noteClass == 8 || noteClass == 10;
            if (! black)
                continue;

            const int previousWhite = midi - 1;
            if (whiteBounds.find(previousWhite) == whiteBounds.end())
                continue;

            auto previous = whiteBounds[previousWhite];
            auto key = juce::Rectangle<float>(previous.getRight() - whiteWidth * 0.28f, area.getY(),
                                              whiteWidth * 0.56f, area.getHeight() * 0.62f);
            const bool inScale = isMidiNoteInScale(midi, keyRoot, modeName);
            const bool detected = midi == activeDetected;
            const bool target = midi == activeTarget;
            juce::Colour fill = inScale ? juce::Colour(0xff295c86) : juce::Colour(0xff182029);
            if (target)
                fill = accentAmber().darker(0.18f);
            else if (detected)
                fill = accentCyan().darker(0.18f);
            g.setColour(fill);
            g.fillRoundedRectangle(key, 2.0f);
        }
    }

private:
    PluginProcessor& processor;
};

class LiveAutoPitchRootComponent final : public juce::Component
{
public:
    explicit LiveAutoPitchRootComponent(PluginProcessor& processorToUse)
        : processor(processorToUse),
          meter(processorToUse),
          keyboard(processorToUse),
          amount(processor.apvts, "correctionAmount", "Amount"),
          retune(processor.apvts, "retuneSpeed", "Retune Speed"),
          humanize(processor.apvts, "humanize", "Humanize"),
          mix(processor.apvts, "mix", "Mix"),
          liveAttachment(processor.apvts, "liveEnabled", liveToggle),
          keyAttachment(processor.apvts, "scaleKey", keyBox),
          modeAttachment(processor.apvts, "scaleMode", modeBox),
          inputAttachment(processor.apvts, "inputRange", inputBox),
          referenceAttachment(processor.apvts, "referencePitchHz", referenceSlider)
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setText("SSP Auto Pitch", juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, strongText());
        titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));

        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText("Live monophonic pitch correction for vocals, bass, and solo instruments.", juce::dontSendNotification);
        subtitleLabel.setColour(juce::Label::textColourId, softText());
        subtitleLabel.setFont(juce::Font(13.0f));

        addAndMakeVisible(statusLabel);
        statusLabel.setColour(juce::Label::textColourId, softText());
        statusLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        statusLabel.setJustificationType(juce::Justification::centredRight);

        for (auto* component : { static_cast<juce::Component*>(&liveToggle), static_cast<juce::Component*>(&keyBox),
                                 static_cast<juce::Component*>(&modeBox), static_cast<juce::Component*>(&inputBox),
                                 static_cast<juce::Component*>(&referenceSlider), static_cast<juce::Component*>(&meter),
                                 static_cast<juce::Component*>(&keyboard), static_cast<juce::Component*>(&amount),
                                 static_cast<juce::Component*>(&retune), static_cast<juce::Component*>(&humanize),
                                 static_cast<juce::Component*>(&mix) })
            addAndMakeVisible(*component);

        liveToggle.setButtonText("Live");

        for (int i = 0; i < getScaleKeyNames().size(); ++i)
            keyBox.addItem(getScaleKeyNames()[i], i + 1);
        for (int i = 0; i < getScaleModeNames().size(); ++i)
            modeBox.addItem(getScaleModeNames()[i], i + 1);
        for (int i = 0; i < getInputRangeNames().size(); ++i)
            inputBox.addItem(getInputRangeNames()[i], i + 1);

        keyBox.setTextWhenNothingSelected("Key");
        modeBox.setTextWhenNothingSelected("Scale");
        inputBox.setTextWhenNothingSelected("Input");

        amount.setFormatting(formatPercent);
        retune.setFormatting(formatPercent);
        humanize.setFormatting(formatPercent);
        mix.setFormatting(formatPercent);

        referenceSlider.setLookAndFeel(&ssp::ui::getVectorLookAndFeel());
        referenceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        referenceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
        referenceSlider.setColour(juce::Slider::trackColourId, accentAmber());
        referenceSlider.textFromValueFunction = [](double value) { return juce::String(value, 2) + " Hz"; };
    }

    void refresh()
    {
        const bool pitched = processor.isLivePitchDetected();
        const auto keyText = keyBox.getText().isNotEmpty() ? keyBox.getText() : juce::String("C");
        const auto scaleText = modeBox.getText().isNotEmpty() ? modeBox.getText() : juce::String("Major");
        statusLabel.setText(pitched
                                ? "Target " + formatMidiNote(processor.getLiveTargetMidi(), false) + "  |  " + keyText + " " + scaleText
                                : "Live input ready  |  " + keyText + " " + scaleText,
                            juce::dontSendNotification);
        meter.repaint();
        keyboard.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        auto header = area.removeFromTop(88);

        auto titleRow = header.removeFromTop(34);
        titleLabel.setBounds(titleRow.removeFromLeft(260));
        statusLabel.setBounds(titleRow);
        subtitleLabel.setBounds(header.removeFromTop(22));

        auto controls = area.removeFromTop(88);
        auto controlsPanel = controls.reduced(0, 6);
        auto cell = controlsPanel.getWidth() / 5;
        liveToggle.setBounds(controlsPanel.removeFromLeft(86).removeFromTop(34));
        controlsPanel.removeFromLeft(10);
        inputBox.setBounds(controlsPanel.removeFromLeft(cell - 20).removeFromTop(34));
        controlsPanel.removeFromLeft(8);
        keyBox.setBounds(controlsPanel.removeFromLeft(90).removeFromTop(34));
        controlsPanel.removeFromLeft(8);
        modeBox.setBounds(controlsPanel.removeFromLeft(130).removeFromTop(34));
        controlsPanel.removeFromLeft(12);
        referenceSlider.setBounds(controlsPanel.removeFromLeft(240).removeFromTop(34));

        auto bottom = area.removeFromBottom(210);
        bottom.removeFromTop(8);
        keyboard.setBounds(bottom.removeFromBottom(120));

        auto knobArea = bottom.reduced(4, 0);
        const int knobWidth = knobArea.getWidth() / 4;
        amount.setBounds(knobArea.removeFromLeft(knobWidth));
        retune.setBounds(knobArea.removeFromLeft(knobWidth));
        humanize.setBounds(knobArea.removeFromLeft(knobWidth));
        mix.setBounds(knobArea);

        meter.setBounds(area.reduced(0, 8));
    }

private:
    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    ssp::ui::SSPToggle liveToggle{"Live"};
    ssp::ui::SSPDropdown keyBox;
    ssp::ui::SSPDropdown modeBox;
    ssp::ui::SSPDropdown inputBox;
    juce::Slider referenceSlider;
    LiveMeterComponent meter;
    KeyboardStripComponent keyboard;
    KnobField amount;
    KnobField retune;
    KnobField humanize;
    KnobField mix;
    ButtonAttachment liveAttachment;
    ComboBoxAttachment keyAttachment;
    ComboBoxAttachment modeAttachment;
    ComboBoxAttachment inputAttachment;
    SliderAttachment referenceAttachment;
};

} // namespace

PluginEditor::PluginEditor(PluginProcessor& processorToUse)
    : juce::AudioProcessorEditor(&processorToUse),
      root(std::make_unique<LiveAutoPitchRootComponent>(processorToUse))
{
    setSize(editorWidth, editorHeight);
    addAndMakeVisible(*root);
    startTimerHz(30);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint(juce::Graphics&)
{
}

void PluginEditor::resized()
{
    root->setBounds(getLocalBounds());
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    juce::ignoreUnused(key);
    return false;
}

bool PluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
    return false;
}

void PluginEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);
}

void PluginEditor::timerCallback()
{
    if (auto* liveRoot = dynamic_cast<LiveAutoPitchRootComponent*>(root.get()))
        liveRoot->refresh();
}
