#include "SaturateControlsComponent.h"

namespace
{
using Formatter = std::function<juce::String(double)>;

juce::Colour backgroundA() { return juce::Colour(0xff0a1014); }
juce::Colour textBright() { return juce::Colour(0xffeff7fb); }
juce::Colour textSoft() { return juce::Colour(0xff9eb5c2); }
juce::Colour accentSky() { return juce::Colour(0xff7fd0ff); }
juce::Colour accentMint() { return juce::Colour(0xff88f4c1); }
juce::Colour accentPeach() { return juce::Colour(0xffffb97c); }
juce::Colour accentRose() { return juce::Colour(0xffff7d8e); }

const std::array<juce::Colour, PluginProcessor::maxBands> bandColours{
    juce::Colour(0xff51b8ff),
    juce::Colour(0xff4fe2d0),
    juce::Colour(0xff8ef087),
    juce::Colour(0xffffd56c),
    juce::Colour(0xffffa45d),
    juce::Colour(0xffff6d7d)
};

const std::array<const char*, PluginProcessor::maxBands> bandNames{
    "Sub", "Low", "Low Mid", "Upper Mid", "Presence", "Air"
};

juce::String formatDb(double value)
{
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt((float) value)) + "%";
}

juce::String formatSignedPercent(double value)
{
    const auto rounded = juce::roundToInt((float) value);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + "%";
}

juce::String styleCaptionForBand(int bandIndex)
{
    static const std::array<const char*, PluginProcessor::maxBands> captions{
        "Low-end weight and transformer bloom.",
        "Tape-like glue for bass fundamentals.",
        "Midrange push with expressive drive.",
        "Edge, bite, and feedback motion.",
        "Presence control for sparkle and attack.",
        "Air-band excitement and broken edges."
    };

    return captions[(size_t) juce::jlimit(0, PluginProcessor::maxBands - 1, bandIndex)];
}

float applyPreviewDynamics(float sample, float amount) noexcept
{
    const float clippedAmount = juce::jlimit(-1.0f, 1.0f, amount);

    if (clippedAmount >= 0.0f)
    {
        const float strength = 1.0f + clippedAmount * 2.4f;
        return std::tanh(sample * strength) / std::max(0.5f, strength * 0.72f);
    }

    const float expand = 1.0f + (-clippedAmount) * 1.6f;
    return juce::jlimit(-3.0f, 3.0f, sample * (0.85f + std::abs(sample) * expand));
}

float shapePreviewSample(float sample, int styleIndex, float tone, float dynamics) noexcept
{
    const float clippedTone = juce::jlimit(0.0f, 1.0f, tone);
    const float spectralTilt = juce::jmap(clippedTone, 0.0f, 1.0f, 0.82f, 1.42f);
    const float dynamicSample = applyPreviewDynamics(sample, dynamics);

    switch (styleIndex)
    {
        case 0: return std::tanh(dynamicSample * juce::jmap(clippedTone, 0.0f, 1.0f, 0.9f, 1.6f)) * 0.92f;
        case 1:
        {
            const float even = dynamicSample / (1.0f + std::abs(dynamicSample) * (0.55f + clippedTone * 0.8f));
            const float soft = std::tanh(dynamicSample * (1.1f + clippedTone * 1.3f));
            return juce::jmap(0.36f + clippedTone * 0.34f, even, soft);
        }
        case 2:
        {
            const float compressed = std::atan(dynamicSample * (1.2f + clippedTone * 2.0f)) / 1.3f;
            const float rounded = std::tanh((dynamicSample - 0.08f * dynamicSample * dynamicSample * dynamicSample) * 0.94f);
            return juce::jmap(clippedTone * 0.65f, rounded, compressed);
        }
        case 3: return juce::jlimit(-1.25f, 1.25f, dynamicSample - 0.18f * std::pow(dynamicSample, 3.0f));
        case 4: return juce::jlimit(-0.94f, 0.94f, dynamicSample * juce::jmap(clippedTone, 0.0f, 1.0f, 1.2f, 2.6f));
        case 5:
        {
            const float pre = juce::jlimit(-3.0f, 3.0f, dynamicSample * (1.25f + clippedTone * 2.5f));
            const float odd = std::tanh(pre) + 0.18f * std::sin(pre * 2.5f);
            return juce::jlimit(-1.25f, 1.25f, odd * (0.86f + clippedTone * 0.18f));
        }
        case 6:
        {
            const float asym = dynamicSample + 0.12f * std::abs(dynamicSample);
            const float fold = asym / (1.0f + std::abs(asym) * (1.1f + clippedTone * 1.7f));
            return juce::jlimit(-1.15f, 1.15f, fold * spectralTilt);
        }
        case 7:
        {
            const float squareish = std::tanh(dynamicSample * (2.4f + clippedTone * 3.2f));
            return juce::jlimit(-1.2f, 1.2f, squareish + 0.12f * std::sin(squareish * 10.0f));
        }
        case 8:
        {
            const float rectified = std::copysign(std::pow(std::abs(dynamicSample), 0.82f), dynamicSample);
            const float diode = std::max(0.0f, rectified) - 0.58f * std::max(0.0f, -rectified);
            return juce::jlimit(-1.1f, 1.1f, std::tanh(diode * (1.8f + clippedTone)));
        }
        case 9:
        {
            const float crushed = juce::jlimit(-1.0f, 1.0f, dynamicSample * (1.6f + clippedTone * 2.6f));
            const float splat = crushed - 0.34f * std::pow(crushed, 3.0f) + 0.05f * std::sin(crushed * 22.0f);
            return juce::jlimit(-1.15f, 1.15f, splat);
        }
        default: return std::tanh(dynamicSample);
    }
}
} // namespace

class SaturateControlsComponent::SaturateKnob final : public juce::Component
{
public:
    SaturateKnob(juce::AudioProcessorValueTreeState& state,
                 const juce::String& paramId,
                 juce::Colour accentColour,
                 const juce::String& title,
                 Formatter formatterToUse)
        : attachment(state, paramId, slider)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(title, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, textSoft());
        titleLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setColour(juce::Slider::thumbColourId, accentColour.brighter(0.3f));
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, slider.getValue());
        slider.setName(title);
        slider.textFromValueFunction = [formatterToUse](double value)
        {
            return formatterToUse != nullptr ? formatterToUse(value) : juce::String(value, 2);
        };
        slider.valueFromTextFunction = [this](const juce::String& text)
        {
            const auto sanitized = text.retainCharacters("0123456789+-.");
            const auto parsed = sanitized.isNotEmpty() ? sanitized.getDoubleValue() : slider.getValue();
            return juce::jlimit(slider.getMinimum(), slider.getMaximum(), parsed);
        };
    }

    ~SaturateKnob() override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds();
        titleLabel.setBounds(area.removeFromTop(18));
        slider.setBounds(area.reduced(2, 0));
    }

private:
    juce::Label titleLabel;
    ssp::ui::SSPKnob slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class SaturateControlsComponent::GlobalPanel final : public juce::Component
{
public:
    explicit GlobalPanel(juce::AudioProcessorValueTreeState& state)
    {
        addAndMakeVisible(sectionLabel);
        addAndMakeVisible(summaryLabel);
        addAndMakeVisible(bandsLabel);
        addAndMakeVisible(crossoverLabel);
        addAndMakeVisible(stereoLabel);
        addAndMakeVisible(oversamplingLabel);
        addAndMakeVisible(bandsBox);
        addAndMakeVisible(crossoverModeBox);
        addAndMakeVisible(stereoModeBox);
        addAndMakeVisible(oversamplingBox);
        addAndMakeVisible(hqButton);
        addAndMakeVisible(autoGainButton);

        setupLabel(sectionLabel, "GLOBAL ENGINE", 12.0f, accentMint());
        setupLabel(summaryLabel, "Multiband drive, crossover topology, stereo mode, and render quality.", 13.0f, textSoft());
        summaryLabel.setJustificationType(juce::Justification::topLeft);

        setupLabel(bandsLabel, "Bands", 12.0f, textSoft());
        setupLabel(crossoverLabel, "Crossovers", 12.0f, textSoft());
        setupLabel(stereoLabel, "Stereo", 12.0f, textSoft());
        setupLabel(oversamplingLabel, "Oversampling", 12.0f, textSoft());

        setupCombo(bandsBox, {"1 Band", "2 Bands", "3 Bands", "4 Bands", "5 Bands", "6 Bands"});
        setupCombo(crossoverModeBox, {"Minimum Phase", "Linear Phase"});
        setupCombo(stereoModeBox, {"Stereo", "Mid/Side"});
        setupCombo(oversamplingBox, {"1x", "2x", "4x", "8x"});

        setupToggle(hqButton, "HQ Render");
        setupToggle(autoGainButton, "Auto Gain");

        bandsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "bands", bandsBox);
        crossoverModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "crossoverMode", crossoverModeBox);
        stereoModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "stereoMode", stereoModeBox);
        oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "oversampling", oversamplingBox);
        hqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "hqRender", hqButton);
        autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "autoGain", autoGainButton);

        driveKnob = std::make_unique<SaturateKnob>(state, "globalDrive", accentSky(), "Drive", formatDb);
        mixKnob = std::make_unique<SaturateKnob>(state, "globalMix", accentMint(), "Mix", formatPercent);
        outputKnob = std::make_unique<SaturateKnob>(state, "globalOutput", accentPeach(), "Output", formatDb);

        addAndMakeVisible(*driveKnob);
        addAndMakeVisible(*mixKnob);
        addAndMakeVisible(*outputKnob);
    }

    void paint(juce::Graphics& g) override
    {
        ssp::ui::drawPanelBackground(g, getLocalBounds().toFloat(), accentMint(), 18.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        sectionLabel.setBounds(area.removeFromTop(18));
        summaryLabel.setBounds(area.removeFromTop(36));
        area.removeFromTop(6);

        layoutLabeledRow(area, bandsLabel, bandsBox);
        layoutLabeledRow(area, crossoverLabel, crossoverModeBox);
        layoutLabeledRow(area, stereoLabel, stereoModeBox);
        layoutLabeledRow(area, oversamplingLabel, oversamplingBox);

        auto toggleRow = area.removeFromTop(30);
        hqButton.setBounds(toggleRow.removeFromLeft(toggleRow.getWidth() / 2).reduced(0, 2));
        autoGainButton.setBounds(toggleRow.reduced(6, 2));

        area.removeFromTop(14);
        auto knobArea = area.reduced(0, 2);
        const int gap = 8;
        const int knobWidth = (knobArea.getWidth() - gap * 2) / 3;
        driveKnob->setBounds(knobArea.removeFromLeft(knobWidth));
        knobArea.removeFromLeft(gap);
        mixKnob->setBounds(knobArea.removeFromLeft(knobWidth));
        knobArea.removeFromLeft(gap);
        outputKnob->setBounds(knobArea);
    }

private:
    static void setupLabel(juce::Label& label, const juce::String& text, float size, juce::Colour colour)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, colour);
        label.setFont(juce::Font(size, size <= 12.0f ? juce::Font::bold : juce::Font::plain));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    static void setupCombo(juce::ComboBox& box, const juce::StringArray& items)
    {
        box.addItemList(items, 1);
        box.setJustificationType(juce::Justification::centred);
    }

    static void setupToggle(juce::ToggleButton& button, const juce::String& text)
    {
        button.setButtonText(text);
        button.setColour(juce::ToggleButton::textColourId, textBright());
        button.setColour(juce::ToggleButton::tickColourId, accentMint());
        button.setColour(juce::ToggleButton::tickDisabledColourId, textSoft().withAlpha(0.6f));
    }

    static void layoutLabeledRow(juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& box)
    {
        auto row = area.removeFromTop(34);
        label.setBounds(row.removeFromLeft(96));
        box.setBounds(row.reduced(0, 3));
    }

    juce::Label sectionLabel;
    juce::Label summaryLabel;
    juce::Label bandsLabel;
    juce::Label crossoverLabel;
    juce::Label stereoLabel;
    juce::Label oversamplingLabel;
    ssp::ui::SSPDropdown bandsBox;
    ssp::ui::SSPDropdown crossoverModeBox;
    ssp::ui::SSPDropdown stereoModeBox;
    ssp::ui::SSPDropdown oversamplingBox;
    ssp::ui::SSPToggle hqButton;
    ssp::ui::SSPToggle autoGainButton;

    std::unique_ptr<SaturateKnob> driveKnob;
    std::unique_ptr<SaturateKnob> mixKnob;
    std::unique_ptr<SaturateKnob> outputKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> crossoverModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
};

class SaturateControlsComponent::BandPanel final : public juce::Component
{
public:
    BandPanel(PluginProcessor& processorToUse, juce::AudioProcessorValueTreeState& state, int bandIndexToUse)
        : processor(processorToUse),
          apvts(state),
          bandIndex(bandIndexToUse)
    {
        addAndMakeVisible(sectionLabel);
        addAndMakeVisible(summaryLabel);
        addAndMakeVisible(styleLabel);
        addAndMakeVisible(styleBox);
        addAndMakeVisible(soloButton);

        setupLabel(sectionLabel, juce::String("BAND ") + juce::String(bandIndex + 1) + "  " + bandNames[(size_t) bandIndex], bandColours[(size_t) bandIndex]);
        setupLabel(summaryLabel, styleCaptionForBand(bandIndex), textSoft());
        summaryLabel.setJustificationType(juce::Justification::topLeft);
        setupLabel(styleLabel, "Saturation Style", textSoft());

        styleBox.addItemList({"Gentle", "Clean Tube", "Warm Tape", "Soft Clip", "Hard Clip",
                              "Amp", "Transformer", "Fuzz", "Rectify", "Broken Speaker"}, 1);
        styleBox.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(styleBox);
        styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,
                                                                                                    PluginProcessor::bandParamId(bandIndex, "style"),
                                                                                                    styleBox);

        soloButton.setClickingTogglesState(true);
        soloButton.onClick = [this]
        {
            processor.setSoloBand(soloButton.getToggleState() ? bandIndex : -1);
            refreshState();
        };

        driveKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "drive"), bandColours[(size_t) bandIndex], "Drive", formatDb);
        toneKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "tone"), bandColours[(size_t) bandIndex], "Tone", formatSignedPercent);
        dynamicsKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "dynamics"), bandColours[(size_t) bandIndex], "Dynamics", formatSignedPercent);
        feedbackKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "feedback"), bandColours[(size_t) bandIndex], "Feedback", formatPercent);
        mixKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "mix"), bandColours[(size_t) bandIndex], "Mix", formatPercent);
        levelKnob = std::make_unique<SaturateKnob>(state, PluginProcessor::bandParamId(bandIndex, "level"), bandColours[(size_t) bandIndex], "Level", formatDb);

        for (auto* knob : { driveKnob.get(), toneKnob.get(), dynamicsKnob.get(), feedbackKnob.get(), mixKnob.get(), levelKnob.get() })
            addAndMakeVisible(*knob);

        refreshState();
    }

    void refreshState()
    {
        soloButton.setToggleState(processor.getSoloBand() == bandIndex, juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto accent = bandColours[(size_t) bandIndex];
        ssp::ui::drawPanelBackground(g, bounds, accent, 18.0f);

        if (! shapeBounds.isEmpty())
            drawSelectedBandShape(g, shapeBounds.toFloat(), accent);

    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        sectionLabel.setBounds(area.removeFromTop(18));
        auto infoRow = area.removeFromTop(78);
        auto summaryArea = infoRow.removeFromLeft((int) std::round(infoRow.getWidth() * 0.56f));
        summaryLabel.setBounds(summaryArea.removeFromTop(34));
        shapeBounds = infoRow.reduced(0, 2);

        auto styleRow = area.removeFromTop(34);
        styleLabel.setBounds(styleRow.removeFromLeft(110));
        auto soloArea = styleRow.removeFromRight(112);
        styleBox.setBounds(styleRow.reduced(0, 1));
        soloButton.setBounds(soloArea.reduced(0, 2));

        area.removeFromTop(10);
        auto topKnobs = area.removeFromTop((area.getHeight() - 18) / 2);
        auto bottomKnobs = area;

        layoutThree(topKnobs, *driveKnob, *toneKnob, *dynamicsKnob);
        bottomKnobs.removeFromTop(18);
        layoutThree(bottomKnobs, *feedbackKnob, *mixKnob, *levelKnob);
    }

private:
    static void setupLabel(juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, colour);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
    }

    static void layoutThree(juce::Rectangle<int> area, juce::Component& a, juce::Component& b, juce::Component& c)
    {
        auto itemWidth = (area.getWidth() - 20) / 3;
        a.setBounds(area.removeFromLeft(itemWidth));
        area.removeFromLeft(10);
        b.setBounds(area.removeFromLeft(itemWidth));
        area.removeFromLeft(10);
        c.setBounds(area);
    }

    void drawSelectedBandShape(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent) const
    {
        g.setColour(juce::Colour(0xcc0b1218));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(accent.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

        auto header = bounds.removeFromTop(18.0f);
        g.setFont(11.0f);
        g.setColour(textSoft());
        g.drawText("Selected Band Shape", header.toNearestInt(), juce::Justification::centredLeft, false);

        auto graph = bounds.reduced(10.0f, 8.0f);
        g.setColour(juce::Colour(0x20ffffff));
        g.drawHorizontalLine((int) std::round(graph.getCentreY()), graph.getX(), graph.getRight());
        g.drawVerticalLine((int) std::round(graph.getCentreX()), graph.getY(), graph.getBottom());

        const float globalDriveDb = apvts.getRawParameterValue("globalDrive")->load();
        const float driveDb = globalDriveDb + apvts.getRawParameterValue(PluginProcessor::bandParamId(bandIndex, "drive"))->load();
        const int styleIndex = juce::roundToInt(apvts.getRawParameterValue(PluginProcessor::bandParamId(bandIndex, "style"))->load());
        const float tone = 0.5f + 0.5f * (apvts.getRawParameterValue(PluginProcessor::bandParamId(bandIndex, "tone"))->load() / 100.0f);
        const float dynamics = apvts.getRawParameterValue(PluginProcessor::bandParamId(bandIndex, "dynamics"))->load() / 100.0f;
        const float driveGain = juce::Decibels::decibelsToGain(driveDb);

        juce::Path transfer;
        for (int i = 0; i <= 80; ++i)
        {
            const float xNorm = juce::jmap((float) i, 0.0f, 80.0f, -1.0f, 1.0f);
            const float yNorm = juce::jlimit(-1.15f, 1.15f, shapePreviewSample(xNorm * driveGain, styleIndex, tone, dynamics));
            const float x = juce::jmap(xNorm, -1.0f, 1.0f, graph.getX(), graph.getRight());
            const float y = juce::jmap(yNorm, -1.15f, 1.15f, graph.getBottom(), graph.getY());
            if (i == 0)
                transfer.startNewSubPath(x, y);
            else
                transfer.lineTo(x, y);
        }

        g.setColour(textBright().withAlpha(0.25f));
        g.drawLine(graph.getX(), graph.getBottom(), graph.getRight(), graph.getY(), 1.0f);
        g.setColour(accent);
        g.strokePath(transfer, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    int bandIndex = 0;
    juce::Label sectionLabel;
    juce::Label summaryLabel;
    juce::Label styleLabel;
    ssp::ui::SSPDropdown styleBox;
    ssp::ui::SSPButton soloButton{"SOLO"};
    juce::Rectangle<int> shapeBounds;

    std::unique_ptr<SaturateKnob> driveKnob;
    std::unique_ptr<SaturateKnob> toneKnob;
    std::unique_ptr<SaturateKnob> dynamicsKnob;
    std::unique_ptr<SaturateKnob> feedbackKnob;
    std::unique_ptr<SaturateKnob> mixKnob;
    std::unique_ptr<SaturateKnob> levelKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;
};

class SaturateControlsComponent::SpectrumDisplay final : public juce::Component
{
public:
    SpectrumDisplay(PluginProcessor& processorToUse,
                    juce::AudioProcessorValueTreeState& state,
                    std::function<void(int)> onBandSelectedToUse,
                    std::function<int()> selectedBandGetterToUse)
        : processor(processorToUse),
          apvts(state),
          onBandSelected(std::move(onBandSelectedToUse)),
          selectedBandGetter(std::move(selectedBandGetterToUse))
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        ssp::ui::drawPanelBackground(g, bounds, accentSky(), 20.0f);

        auto plot = bounds.reduced(18.0f, 18.0f);
        auto titleArea = plot.removeFromTop(22.0f);
        g.setColour(textBright());
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("PROCESSED OUTPUT SPECTRUM", titleArea.toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(textSoft());
        g.setFont(juce::Font(12.0f));
        g.drawText("Shows the post-saturation output spectrum. Click a band to focus controls and drag crossovers to reshape it.",
                   juce::Rectangle<int>((int) plot.getRight() - 360, (int) titleArea.getY(), 360, (int) titleArea.getHeight()),
                   juce::Justification::centredRight,
                   false);

        const auto activeBands = processor.getActiveBandCount();
        const auto crossovers = getCrossoverFrequencies();
        const auto spectrum = processor.getSpectrumData();

        for (float hz : {40.0f, 80.0f, 160.0f, 320.0f, 640.0f, 1250.0f, 2500.0f, 5000.0f, 10000.0f, 16000.0f})
        {
            const float x = frequencyToX(hz, plot);
            g.setColour(juce::Colour(0x20ffffff));
            g.drawVerticalLine((int) std::round(x), plot.getY(), plot.getBottom());
            g.setColour(textSoft().withAlpha(0.65f));
            g.setFont(10.0f);
            g.drawText(hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k" : juce::String((int) hz),
                       juce::Rectangle<int>((int) x - 24, (int) plot.getBottom() - 18, 48, 16),
                       juce::Justification::centred,
                       false);
        }

        const auto focusedBand = selectedBandGetter();
        float left = plot.getX();
        for (int band = 0; band < activeBands; ++band)
        {
            const float right = band < activeBands - 1 ? frequencyToX(crossovers[(size_t) band], plot) : plot.getRight();
            auto bandArea = juce::Rectangle<float>(left, plot.getY(), right - left, plot.getHeight());
            auto accent = bandColours[(size_t) band];
            const float energy = juce::jlimit(0.0f, 1.0f, processor.getBandMeterLevel(band));
            g.setColour(accent.withAlpha(band == focusedBand ? 0.18f + energy * 0.10f : 0.07f + energy * 0.06f));
            g.fillRoundedRectangle(bandArea.reduced(1.0f, 24.0f), 18.0f);

            g.setColour(accent.withAlpha(0.55f));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText(juce::String(band + 1) + "  " + bandNames[(size_t) band],
                       juce::Rectangle<int>((int) bandArea.getX() + 10, (int) bandArea.getY() + 8, (int) bandArea.getWidth() - 20, 18),
                       juce::Justification::centredLeft,
                       false);

            left = right;
        }

        drawSpectrumLine(g, plot, spectrum);
        drawCrossovers(g, plot, crossovers, activeBands);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        auto plot = getPlotBounds();
        if (! plot.contains(event.position))
            return;

        const auto activeBands = processor.getActiveBandCount();
        const auto crossovers = getCrossoverFrequencies();

        draggedHandle = -1;
        for (int i = 0; i < activeBands - 1; ++i)
        {
            const float x = frequencyToX(crossovers[(size_t) i], plot);
            if (std::abs(event.position.x - x) < 14.0f)
            {
                draggedHandle = i;
                if (auto* parameter = apvts.getParameter(PluginProcessor::crossoverParamId(i)))
                    parameter->beginChangeGesture();
                return;
            }
        }

        onBandSelected(getBandForX(event.position.x, plot, crossovers, activeBands));
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (draggedHandle < 0)
            return;

        auto plot = getPlotBounds();
        const auto activeBands = processor.getActiveBandCount();
        const auto crossovers = getCrossoverFrequencies();

        const float minHz = draggedHandle == 0 ? 40.0f : crossovers[(size_t) draggedHandle - 1] * 1.12f;
        const float maxHz = draggedHandle == activeBands - 2 ? 18000.0f : crossovers[(size_t) draggedHandle + 1] / 1.12f;
        const float hz = juce::jlimit(minHz, maxHz, xToFrequency(event.position.x, plot));

        if (auto* parameter = apvts.getParameter(PluginProcessor::crossoverParamId(draggedHandle)))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(hz));
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (draggedHandle >= 0)
            if (auto* parameter = apvts.getParameter(PluginProcessor::crossoverParamId(draggedHandle)))
                parameter->endChangeGesture();

        draggedHandle = -1;
    }

private:
    juce::Rectangle<float> getPlotBounds() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(18.0f, 18.0f);
        bounds.removeFromTop(22.0f);
        return bounds;
    }

    std::array<float, PluginProcessor::maxCrossovers> getCrossoverFrequencies() const
    {
        std::array<float, PluginProcessor::maxCrossovers> values{};
        for (int i = 0; i < PluginProcessor::maxCrossovers; ++i)
            values[(size_t) i] = apvts.getRawParameterValue(PluginProcessor::crossoverParamId(i))->load();
        return values;
    }

    static float frequencyToX(float frequency, juce::Rectangle<float> plot)
    {
        const float minLog = std::log10(40.0f);
        const float maxLog = std::log10(18000.0f);
        const float norm = (std::log10(frequency) - minLog) / (maxLog - minLog);
        return plot.getX() + norm * plot.getWidth();
    }

    static float xToFrequency(float x, juce::Rectangle<float> plot)
    {
        const float minLog = std::log10(40.0f);
        const float maxLog = std::log10(18000.0f);
        const float norm = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        return std::pow(10.0f, minLog + norm * (maxLog - minLog));
    }

    static int getBandForX(float x, juce::Rectangle<float> plot,
                           const std::array<float, PluginProcessor::maxCrossovers>& crossovers,
                           int activeBands)
    {
        float left = plot.getX();
        for (int i = 0; i < activeBands; ++i)
        {
            const float right = i < activeBands - 1 ? frequencyToX(crossovers[(size_t) i], plot) : plot.getRight();
            if (x >= left && x <= right)
                return i;
            left = right;
        }
        return juce::jlimit(0, activeBands - 1, activeBands - 1);
    }

    void drawSpectrumLine(juce::Graphics& g,
                          juce::Rectangle<float> plot,
                          const std::array<float, PluginProcessor::analyzerBinCount>& spectrum) const
    {
        juce::Path path;
        juce::Path fill;

        for (int i = 0; i < PluginProcessor::analyzerBinCount; ++i)
        {
            const float x = juce::jmap((float) i, 0.0f, (float) (PluginProcessor::analyzerBinCount - 1), plot.getX(), plot.getRight());
            const float y = juce::jmap(spectrum[(size_t) i], 0.0f, 1.0f, plot.getBottom() - 2.0f, plot.getY() + 24.0f);

            if (i == 0)
            {
                path.startNewSubPath(x, y);
                fill.startNewSubPath(x, plot.getBottom());
                fill.lineTo(x, y);
            }
            else
            {
                path.lineTo(x, y);
                fill.lineTo(x, y);
            }
        }
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        juce::ColourGradient fillGradient(accentSky().withAlpha(0.22f), plot.getX(), plot.getY(),
                                          accentRose().withAlpha(0.03f), plot.getRight(), plot.getBottom(), false);
        g.setGradientFill(fillGradient);
        g.fillPath(fill);

        juce::ColourGradient gradient(accentSky(), plot.getX(), plot.getCentreY(),
                                      accentPeach(), plot.getRight(), plot.getY(), false);
        g.setGradientFill(gradient);
        g.strokePath(path, juce::PathStrokeType(2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawCrossovers(juce::Graphics& g,
                        juce::Rectangle<float> plot,
                        const std::array<float, PluginProcessor::maxCrossovers>& crossovers,
                        int activeBands) const
    {
        for (int i = 0; i < activeBands - 1; ++i)
        {
            const float x = frequencyToX(crossovers[(size_t) i], plot);
            g.setColour(textBright().withAlpha(0.45f));
            g.drawVerticalLine((int) std::round(x), plot.getY() + 10.0f, plot.getBottom() - 22.0f);

            auto handle = juce::Rectangle<float>(16.0f, 16.0f).withCentre({x, plot.getBottom() - 34.0f});
            g.setColour(bandColours[(size_t) juce::jmin(i + 1, PluginProcessor::maxBands - 1)]);
            g.fillEllipse(handle);
            g.setColour(backgroundA());
            g.drawEllipse(handle, 2.0f);
        }
    }

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    std::function<void(int)> onBandSelected;
    std::function<int()> selectedBandGetter;
    int draggedHandle = -1;
};

SaturateControlsComponent::SaturateControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state)
{
    for (auto* label : { &eyebrowLabel, &titleLabel, &subtitleLabel, &presetLabel, &searchLabel, &selectedBandLabel })
        addAndMakeVisible(*label);
    for (auto* component : { static_cast<juce::Component*>(&previousPresetButton),
                             static_cast<juce::Component*>(&presetButton),
                             static_cast<juce::Component*>(&nextPresetButton) })
        addAndMakeVisible(*component);

    eyebrowLabel.setText("SATURATION SYSTEM", juce::dontSendNotification);
    eyebrowLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    eyebrowLabel.setColour(juce::Label::textColourId, accentMint());
    eyebrowLabel.setJustificationType(juce::Justification::centredLeft);

    titleLabel.setText("SSP Saturate", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(34.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, textBright());
    titleLabel.setJustificationType(juce::Justification::centredLeft);

    subtitleLabel.setText("Saturn-inspired multiband saturation workflow with spectrum-driven band shaping, per-band drive/tone/dynamics/feedback, and global stereo/render controls.",
                          juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, textSoft());
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);

    presetLabel.setText("PRESET BROWSER", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(11.5f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, textBright());
    presetLabel.setJustificationType(juce::Justification::centredLeft);

    searchLabel.setText({}, juce::dontSendNotification);
    searchLabel.setFont(juce::Font(11.0f));
    searchLabel.setColour(juce::Label::textColourId, textSoft());
    searchLabel.setJustificationType(juce::Justification::centredLeft);

    selectedBandLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    selectedBandLabel.setColour(juce::Label::textColourId, textBright());
    selectedBandLabel.setJustificationType(juce::Justification::centredRight);

    presetButton.onClick = [this] { showPresetBrowser(); };
    previousPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };

    spectrumDisplay = std::make_unique<SpectrumDisplay>(processor, apvts,
                                                        [this](int bandIndex) { setSelectedBand(bandIndex); },
                                                        [this]() { return selectedBand; });
    addAndMakeVisible(*spectrumDisplay);

    globalPanel = std::make_unique<GlobalPanel>(apvts);
    addAndMakeVisible(*globalPanel);

    for (int i = 0; i < PluginProcessor::maxBands; ++i)
    {
        bandPanels[(size_t) i] = std::make_unique<BandPanel>(processor, apvts, i);
        addAndMakeVisible(*bandPanels[(size_t) i]);
    }

    currentPresetIndex = processor.getCurrentFactoryPresetIndex();
    presetButton.setButtonText(PluginProcessor::getFactoryPresetName(currentPresetIndex));
    setSelectedBand(0);
    updateHeaderText();
    startTimerHz(30);
}

SaturateControlsComponent::~SaturateControlsComponent()
{
    stopTimer();
}

void SaturateControlsComponent::timerCallback()
{
    for (auto& panel : bandPanels)
        panel->refreshState();
    updateHeaderText();
    if (auto* selectedPanel = bandPanels[(size_t) selectedBand].get())
        selectedPanel->repaint();
    spectrumDisplay->repaint();
}

void SaturateControlsComponent::setSelectedBand(int newBandIndex)
{
    selectedBand = juce::jlimit(0, PluginProcessor::maxBands - 1, newBandIndex);

    for (int i = 0; i < PluginProcessor::maxBands; ++i)
        bandPanels[(size_t) i]->setVisible(i == selectedBand);

    updateHeaderText();
    repaint();
}

void SaturateControlsComponent::updateHeaderText()
{
    const auto energy = juce::roundToInt(processor.getBandMeterLevel(selectedBand) * 100.0f);
    selectedBandLabel.setText("FOCUS  " + juce::String(selectedBand + 1) + " / " + juce::String(processor.getActiveBandCount())
                              + "  " + bandNames[(size_t) selectedBand] + "  ACTIVITY " + juce::String(energy) + "%",
                              juce::dontSendNotification);
    searchLabel.setText("CATEGORY  " + PluginProcessor::getFactoryPresetCategory(currentPresetIndex)
                            + "    TAGS  " + PluginProcessor::getFactoryPresetTags(currentPresetIndex),
                        juce::dontSendNotification);
}

void SaturateControlsComponent::applyPreset(int presetIndex)
{
    currentPresetIndex = juce::jlimit(0, PluginProcessor::getFactoryPresetNames().size() - 1, presetIndex);
    processor.applyFactoryPreset(currentPresetIndex);
    presetButton.setButtonText(PluginProcessor::getFactoryPresetName(currentPresetIndex));
    updateHeaderText();
    repaint();
}

void SaturateControlsComponent::stepPreset(int delta)
{
    const int total = PluginProcessor::getFactoryPresetNames().size();
    applyPreset((currentPresetIndex + delta + total) % total);
}

void SaturateControlsComponent::showPresetBrowser()
{
    juce::PopupMenu menu;
    const auto names = PluginProcessor::getFactoryPresetNames();
    juce::StringArray categories;

    for (int i = 0; i < names.size(); ++i)
        categories.addIfNotAlreadyThere(PluginProcessor::getFactoryPresetCategory(i));

    for (const auto& category : categories)
    {
        juce::PopupMenu submenu;
        for (int i = 0; i < names.size(); ++i)
            if (PluginProcessor::getFactoryPresetCategory(i) == category)
                submenu.addItem(1000 + i, names[i], true, i == currentPresetIndex);
        menu.addSubMenu(category, submenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
                       [this](int result)
                       {
                           if (result >= 1000)
                               applyPreset(result - 1000);
                       });
}

void SaturateControlsComponent::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void SaturateControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(24, 20);

    auto header = area.removeFromTop(74);
    auto headerLeft = header.removeFromLeft((int) std::round(header.getWidth() * 0.62f));
    eyebrowLabel.setBounds(headerLeft.removeFromTop(16));
    titleLabel.setBounds(headerLeft.removeFromTop(36));
    subtitleLabel.setBounds(headerLeft);

    auto headerRight = header.reduced(0, 4);
    auto presetTitle = headerRight.removeFromTop(16);
    presetLabel.setBounds(presetTitle);
    auto presetRow = headerRight.removeFromTop(32);
    previousPresetButton.setBounds(presetRow.removeFromLeft(34).reduced(0, 2));
    presetRow.removeFromLeft(6);
    nextPresetButton.setBounds(presetRow.removeFromRight(34).reduced(0, 2));
    presetRow.removeFromRight(6);
    presetButton.setBounds(presetRow.reduced(0, 2));
    searchLabel.setBounds(headerRight.removeFromTop(18));
    selectedBandLabel.setBounds(headerRight.removeFromTop(18));

    area.removeFromTop(10);
    spectrumDisplay->setBounds(area.removeFromTop(294));

    area.removeFromTop(14);
    auto bottom = area;
    auto left = bottom.removeFromLeft(392);
    globalPanel->setBounds(left);
    bottom.removeFromLeft(14);

    for (auto& panel : bandPanels)
        panel->setBounds(bottom);
}
