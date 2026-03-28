#include "OscillatorSectionComponent.h"
#include "ReactorUI.h"

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

enum DualWarpModeIndex
{
    warpModeOff = 0,
    warpModeFM,
    warpModeSync,
    warpModeBendPlus,
    warpModeBendMinus,
    warpModePhasePlus,
    warpModePhaseMinus,
    warpModeMirror,
    warpModeWrap,
    warpModePinch
};

juce::Rectangle<float> getPreviewBounds(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(14).toFloat();
    area.removeFromTop(78.0f);
    return area.removeFromTop(128.0f);
}

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    label.setFont(size >= 15.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
}

void styleCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111821));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffc86a));
}

void styleToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffc66d));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
    button.setClickingTogglesState(true);
}

float wrapPhase(float phase)
{
    phase -= std::floor(phase);
    if (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

float renderBaseWave(int waveIndex, float phase)
{
    switch (waveIndex)
    {
        case 0: return std::sin(twoPi * phase);
        case 1: return 2.0f * phase - 1.0f;
        case 2: return phase < 0.5f ? 1.0f : -1.0f;
        case 3: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        default: break;
    }

    return 0.0f;
}

float bendPreviewPhase(float phase, float amount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float wrappedPhase = wrapPhase(phase);

    if (clampedAmount <= 0.0001f)
        return wrappedPhase;

    const float pivot = juce::jmap(clampedAmount, 0.0f, 1.0f, 0.5f, 0.14f);
    if (wrappedPhase < pivot)
        return 0.5f * (wrappedPhase / juce::jmax(0.0001f, pivot));

    return 0.5f + 0.5f * ((wrappedPhase - pivot) / juce::jmax(0.0001f, 1.0f - pivot));
}

float renderPreviewSource(int sourceIndex, float phase, float mutateAmount);

float applyPreviewWarpMode(float phase, int modeIndex, float amount, int fmSourceIndex, float mutateAmount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (clampedAmount <= 0.0001f || modeIndex == warpModeOff)
        return phase;

    switch (modeIndex)
    {
        case warpModeFM:
        {
            const float modulator = renderPreviewSource(fmSourceIndex, phase, mutateAmount);
            const float fmDepth = clampedAmount * (0.18f + clampedAmount * (0.34f + mutateAmount * 0.22f));
            return wrapPhase(phase + fmDepth * modulator);
        }

        case warpModeSync:
            return wrapPhase(phase * (1.0f + clampedAmount * 7.0f));

        case warpModeBendPlus:
            return bendPreviewPhase(phase, clampedAmount);

        case warpModeBendMinus:
            return 1.0f - bendPreviewPhase(1.0f - phase, clampedAmount);

        case warpModePhasePlus:
            return wrapPhase(phase + clampedAmount * 0.35f);

        case warpModePhaseMinus:
            return wrapPhase(phase - clampedAmount * 0.35f);

        case warpModeMirror:
        {
            const float mirrored = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
            return wrapPhase(juce::jmap(clampedAmount, phase, mirrored));
        }

        case warpModeWrap:
            return wrapPhase((phase - 0.5f) * (1.0f + clampedAmount * 6.0f) + 0.5f);

        case warpModePinch:
        {
            const float exponent = 1.0f + clampedAmount * 5.0f;
            if (phase < 0.5f)
                return 0.5f * std::pow(phase * 2.0f, exponent);

            return 1.0f - 0.5f * std::pow((1.0f - phase) * 2.0f, exponent);
        }

        default:
            break;
    }

    return phase;
}

float renderPreviewSource(int sourceIndex, float phase, float mutateAmount)
{
    switch (sourceIndex)
    {
        case 0: return renderBaseWave(1, wrapPhase(phase * 1.00f));
        case 1: return renderBaseWave(2, wrapPhase(phase * 1.07f + 0.17f));
        case 2: return renderBaseWave(3, wrapPhase(phase * 0.53f + 0.29f));
        case 3: return std::sin(twoPi * phase * 7.0f) * 0.50f + std::sin(twoPi * phase * 11.0f) * 0.20f;
        case 4: return std::sin(twoPi * wrapPhase(phase * 0.5f));
        case 5: return 0.65f * std::sin(twoPi * phase * 0.18f) + 0.35f * std::sin(twoPi * phase * 1.15f);
        case 6: return std::sin(twoPi * wrapPhase(phase * (2.5f + mutateAmount * 2.0f)
                                                  + 0.20f * std::sin(twoPi * phase * 3.0f)));
        default: break;
    }

    return 0.0f;
}
}

OscillatorSectionComponent::OscillatorSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state, int oscillator)
    : oscIndex(oscillator),
      samplerComponent(processor, state, oscillator),
      wavetableComponent(processor, state, oscillator),
      octaveSlider(processor, "osc" + juce::String(oscillator) + "Octave",
                   reactormod::Destination::none, juce::Colour(0xffffb357), 96, 24),
      levelSlider(processor, "osc" + juce::String(oscillator) + "Level",
                  oscIndex == 1 ? reactormod::Destination::osc1Level
                                : oscIndex == 2 ? reactormod::Destination::osc2Level
                                                : reactormod::Destination::osc3Level,
                  juce::Colour(0xffffb357), 88, 26),
      coarseSlider(processor, "osc" + juce::String(oscillator) + "Coarse",
                   reactormod::Destination::none, juce::Colour(0xffffb357), 88, 26),
      detuneSlider(processor, "osc" + juce::String(oscillator) + "Detune",
                   oscIndex == 1 ? reactormod::Destination::osc1Detune
                                 : oscIndex == 2 ? reactormod::Destination::osc2Detune
                                                 : reactormod::Destination::osc3Detune,
                   juce::Colour(0xffffb357), 88, 26),
      panSlider(processor, "osc" + juce::String(oscillator) + "Pan",
                oscIndex == 1 ? reactormod::Destination::osc1Pan
                              : oscIndex == 2 ? reactormod::Destination::osc2Pan
                                              : reactormod::Destination::osc3Pan,
                juce::Colour(0xffffb357), 88, 26),
      unisonVoicesSlider(processor, "osc" + juce::String(oscillator) + "UnisonVoices",
                         reactormod::Destination::none, juce::Colour(0xffffb357), 88, 26),
      unisonDetuneSlider(processor, "osc" + juce::String(oscillator) + "Unison",
                         oscIndex == 1 ? reactormod::Destination::osc1Unison
                                       : oscIndex == 2 ? reactormod::Destination::osc2Unison
                                                       : reactormod::Destination::osc3Unison,
                         juce::Colour(0xffffb357), 88, 26)
{
    addAndMakeVisible(samplerComponent);
    addAndMakeVisible(wavetableComponent);
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(filterRouteButton);
    addAndMakeVisible(unisonButton);
    addAndMakeVisible(waveLabel);
    addAndMakeVisible(octaveLabel);
    addAndMakeVisible(levelLabel);
    addAndMakeVisible(coarseLabel);
    addAndMakeVisible(detuneLabel);
    addAndMakeVisible(panLabel);
    addAndMakeVisible(unisonVoicesLabel);
    addAndMakeVisible(unisonDetuneLabel);
    addAndMakeVisible(typeBox);
    addAndMakeVisible(waveBox);
    addAndMakeVisible(octaveSlider);
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(coarseSlider);
    addAndMakeVisible(detuneSlider);
    addAndMakeVisible(panSlider);
    addAndMakeVisible(unisonVoicesSlider);
    addAndMakeVisible(unisonDetuneSlider);

    titleLabel.setText("OSC " + juce::String(oscIndex), juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());

    filterRouteButton.setButtonText("Filter");
    filterRouteButton.setTooltip("Route this oscillator through the filter");
    styleToggle(filterRouteButton);

    unisonButton.setButtonText("Unison");
    unisonButton.setTooltip("Enable Serum-style unison for this oscillator");
    styleToggle(unisonButton);

    typeLabel.setText("Type", juce::dontSendNotification);
    waveLabel.setText("Wave", juce::dontSendNotification);
    octaveLabel.setText("Octave", juce::dontSendNotification);
    levelLabel.setText("Level", juce::dontSendNotification);
    coarseLabel.setText("Coarse", juce::dontSendNotification);
    detuneLabel.setText("Fine", juce::dontSendNotification);
    panLabel.setText("Pan", juce::dontSendNotification);
    unisonVoicesLabel.setText("Voices", juce::dontSendNotification);
    unisonDetuneLabel.setText("U Detune", juce::dontSendNotification);

    styleLabel(typeLabel, 11.5f, reactorui::textMuted());
    styleLabel(waveLabel, 11.5f, reactorui::textMuted());
    styleLabel(octaveLabel, 11.5f, reactorui::textMuted());
    styleLabel(levelLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(coarseLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(detuneLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(panLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(unisonVoicesLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(unisonDetuneLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);

    typeBox.addItemList(PluginProcessor::getOscillatorTypeNames(), 1);
    waveBox.addItemList(PluginProcessor::getWaveNames(), 1);
    styleCombo(typeBox);
    styleCombo(waveBox);
    octaveSlider.setRange(0.0, (double) (PluginProcessor::getOctaveNames().size() - 1), 1.0);
    octaveSlider.setNumDecimalPlacesToDisplay(0);
    octaveSlider.textFromValueFunction = [] (double value)
    {
        const auto index = juce::jlimit(0, PluginProcessor::getOctaveNames().size() - 1, juce::roundToInt((float) value));
        return PluginProcessor::getOctaveNames()[index];
    };
    octaveSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto names = PluginProcessor::getOctaveNames();
        for (int i = 0; i < names.size(); ++i)
            if (text.trim().equalsIgnoreCase(names[i]))
                return (double) i;

        return juce::jlimit(0.0, (double) (names.size() - 1),
                            (double) text.retainCharacters("0123456789-+").getIntValue());
    };
    levelSlider.setTextValueSuffix("");
    coarseSlider.setRange(-24.0, 24.0, 1.0);
    coarseSlider.setNumDecimalPlacesToDisplay(0);
    coarseSlider.setTextValueSuffix(" st");
    coarseSlider.textFromValueFunction = [] (double value)
    {
        const int semitones = juce::roundToInt((float) value);
        return juce::String(semitones > 0 ? "+" : "") + juce::String(semitones) + " st";
    };
    coarseSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit(-24.0, 24.0, (double) text.retainCharacters("0123456789-+").getIntValue());
    };
    detuneSlider.setTextValueSuffix(" ct");
    panSlider.setTextValueSuffix("");
    unisonVoicesSlider.setNumDecimalPlacesToDisplay(0);
    unisonVoicesSlider.setTextValueSuffix(" v");

    const auto prefix = "osc" + juce::String(oscIndex);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, prefix + "Type", typeBox);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, prefix + "Wave", waveBox);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Octave", octaveSlider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Level", levelSlider);
    coarseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Coarse", coarseSlider);
    detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Detune", detuneSlider);
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Pan", panSlider);
    filterRouteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, prefix + "ToFilter", filterRouteButton);
    unisonOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, prefix + "UnisonOn", unisonButton);
    unisonVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "UnisonVoices", unisonVoicesSlider);
    unisonDetuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, prefix + "Unison", unisonDetuneSlider);

    oscTypeParam = state.getRawParameterValue(prefix + "Type");
    warpFMSourceParam = state.getRawParameterValue(prefix + "WarpFMSource");
    warpModeParams[0] = state.getRawParameterValue(prefix + "Warp1Mode");
    warpAmountParams[0] = state.getRawParameterValue(prefix + "Warp1Amount");
    warpModeParams[1] = state.getRawParameterValue(prefix + "Warp2Mode");
    warpAmountParams[1] = state.getRawParameterValue(prefix + "Warp2Amount");
    mutateParam = state.getRawParameterValue("warpMutate");

    const int oscTypeIndex = oscTypeParam != nullptr ? juce::roundToInt(oscTypeParam->load()) : 0;
    const bool samplerSelected = oscTypeIndex == 1;
    const bool wavetableSelected = oscTypeIndex == 2;
    samplerComponent.setVisible(samplerSelected);
    wavetableComponent.setVisible(wavetableSelected);
    waveLabel.setVisible(!samplerSelected && !wavetableSelected);
    waveBox.setVisible(!samplerSelected && !wavetableSelected);
    octaveLabel.setVisible(!samplerSelected && !wavetableSelected);
    octaveSlider.setVisible(!samplerSelected && !wavetableSelected);

    startTimerHz(30);
}

void OscillatorSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const int oscTypeIndex = oscTypeParam != nullptr ? juce::roundToInt(oscTypeParam->load()) : 0;
    const bool samplerSelected = oscTypeIndex == 1;
    const bool wavetableSelected = oscTypeIndex == 2;
    const auto accent = samplerSelected ? juce::Colour(0xff6ce5ff)
                                        : (wavetableSelected ? juce::Colour(0xff78efb8)
                                                             : (unisonButton.getToggleState() ? juce::Colour(0xff96ff68)
                                                                                              : juce::Colour(0xffffbb63)));
    reactorui::drawPanelBackground(g, bounds, accent);

    if (samplerSelected || wavetableSelected)
        return;

    auto preview = getPreviewBounds(getLocalBounds());
    reactorui::drawDisplayPanel(g, preview, accent);

    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accent.withAlpha(0.12f), 4, 1);

    const int waveIndex = juce::jlimit(0, 3, waveBox.getSelectedItemIndex());
    const float level = (float) levelSlider.getValue();
    const float amplitude = preview.getHeight() * (0.15f + level * 0.30f);
    const float centreY = preview.getCentreY();
    const bool unisonEnabled = unisonButton.getToggleState();
    const float unisonDetune = (float) unisonDetuneSlider.getValue();
    const int unisonVoices = juce::jlimit(2, 7, juce::roundToInt((float) unisonVoicesSlider.getValue()));
    const int fmSourceIndex = warpFMSourceParam != nullptr ? juce::roundToInt(warpFMSourceParam->load()) : 0;
    const int warp1Mode = warpModeParams[0] != nullptr ? juce::roundToInt(warpModeParams[0]->load()) : 0;
    const float warp1Amount = warpAmountParams[0] != nullptr ? warpAmountParams[0]->load() : 0.0f;
    const int warp2Mode = warpModeParams[1] != nullptr ? juce::roundToInt(warpModeParams[1]->load()) : 0;
    const float warp2Amount = warpAmountParams[1] != nullptr ? warpAmountParams[1]->load() : 0.0f;
    const float mutateAmount = mutateParam != nullptr ? mutateParam->load() : 0.0f;

    auto sampleAt = [waveIndex, fmSourceIndex, warp1Mode, warp1Amount, warp2Mode, warp2Amount, mutateAmount, osc = oscIndex](float phase)
    {
        float warpedPhase = applyPreviewWarpMode(phase, warp1Mode, warp1Amount, fmSourceIndex, mutateAmount);
        warpedPhase = applyPreviewWarpMode(warpedPhase, warp2Mode, warp2Amount, fmSourceIndex, mutateAmount);
        float sample = renderBaseWave(waveIndex, warpedPhase);

        if (mutateAmount > 0.0001f)
        {
            const float harmonicPhase = wrapPhase(warpedPhase + 0.11f * mutateAmount
                                                                * std::sin(twoPi * warpedPhase * (2.0f + (float) osc)));
            const float harmonic = renderBaseWave(waveIndex, harmonicPhase);
            const float folded = std::sin((sample + harmonic * 0.45f) * (1.0f + mutateAmount * 4.5f));
            sample = juce::jlimit(-1.0f, 1.0f, juce::jmap(mutateAmount, sample, folded));
        }

        return sample;
    };

    auto drawWave = [&](float phaseOffset, float verticalScale, juce::Colour colour, float thickness)
    {
        juce::Path path;
        for (int x = 0; x < (int) preview.getWidth(); ++x)
        {
            const float phase = std::fmod(((float) x / juce::jmax(1.0f, preview.getWidth() - 1.0f)) + phaseOffset, 1.0f);
            const float y = centreY - sampleAt(phase) * amplitude * verticalScale;
            const float drawX = preview.getX() + (float) x;
            if (x == 0)
                path.startNewSubPath(drawX, y);
            else
                path.lineTo(drawX, y);
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (unisonEnabled)
    {
        const int ghosts = juce::jlimit(2, 4, unisonVoices);
        for (int i = 0; i < ghosts; ++i)
        {
            const float spread = juce::jmap((float) i, 0.0f, (float) juce::jmax(1, ghosts - 1), -1.0f, 1.0f);
            const float offset = spread * (0.010f + unisonDetune * 0.040f);
            drawWave(offset < 0.0f ? 1.0f + offset : offset,
                     0.90f,
                     accent.withAlpha(0.24f),
                     1.25f);
        }
    }

    drawWave(0.0f, 1.0f, accent, 2.4f);
}

void OscillatorSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto titleRow = area.removeFromTop(24);
    titleLabel.setBounds(titleRow.removeFromLeft(92));
    auto toggleArea = titleRow.removeFromRight(172);
    filterRouteButton.setBounds(toggleArea.removeFromLeft(78));
    toggleArea.removeFromLeft(8);
    unisonButton.setBounds(toggleArea.removeFromLeft(86));

    area.removeFromTop(10);

    auto typeRow = area.removeFromTop(44);
    typeLabel.setBounds(typeRow.removeFromLeft(40));
    typeRow.removeFromLeft(8);
    typeBox.setBounds(typeRow.removeFromLeft(130));

    const int oscTypeIndex = oscTypeParam != nullptr ? juce::roundToInt(oscTypeParam->load()) : 0;
    const bool samplerSelected = oscTypeIndex == 1;
    const bool wavetableSelected = oscTypeIndex == 2;
    const bool extendedSourcePage = samplerSelected || wavetableSelected;
    const int previewHeight = extendedSourcePage ? 112 : 116;
    const int comboHeight = extendedSourcePage ? 136 : 112;
    auto previewArea = area.removeFromTop(previewHeight);
    area.removeFromTop(8);

    auto comboRow = area.removeFromTop(comboHeight);
    auto left = comboRow.removeFromLeft(comboRow.getWidth() / 2 - 6);
    comboRow.removeFromLeft(12);
    auto right = comboRow;

    if (samplerSelected)
    {
        samplerComponent.setBounds(juce::Rectangle<int>(area.getX(), previewArea.getY(), area.getWidth(), comboRow.getBottom() - previewArea.getY()));
        wavetableComponent.setBounds({});
        waveLabel.setBounds({});
        waveBox.setBounds({});
        octaveLabel.setBounds({});
        octaveSlider.setBounds({});
    }
    else if (wavetableSelected)
    {
        wavetableComponent.setBounds(juce::Rectangle<int>(area.getX(), previewArea.getY(), area.getWidth(), comboRow.getBottom() - previewArea.getY()));
        samplerComponent.setBounds({});
        waveLabel.setBounds({});
        waveBox.setBounds({});
        octaveLabel.setBounds({});
        octaveSlider.setBounds({});
    }
    else
    {
        samplerComponent.setBounds({});
        wavetableComponent.setBounds({});
        waveLabel.setBounds(left.removeFromTop(16));
        waveBox.setBounds(left.removeFromTop(28));
        octaveLabel.setBounds(right.removeFromTop(16));
        right.removeFromTop(4);
        octaveSlider.setBounds(right.reduced(0, 2));
    }

    area.removeFromTop(extendedSourcePage ? 4 : 12);

    auto controlsArea = area;
    const int rowGap = 12;
    auto topControls = controlsArea.removeFromTop((controlsArea.getHeight() - rowGap) / 2);
    controlsArea.removeFromTop(rowGap);
    auto bottomControls = controlsArea;

    auto layoutRow = [] (juce::Rectangle<int> rowArea, int numCells, int gap)
    {
        std::vector<juce::Rectangle<int>> cells;
        if (numCells <= 0)
            return cells;

        const int cellWidth = (rowArea.getWidth() - gap * (numCells - 1)) / numCells;
        for (int i = 0; i < numCells; ++i)
        {
            cells.push_back(rowArea.removeFromLeft(cellWidth));
            if (i < numCells - 1)
                rowArea.removeFromLeft(gap);
        }

        return cells;
    };

    const auto topCells = layoutRow(topControls, 3, 10);
    const auto bottomCells = layoutRow(bottomControls, 3, 10);

    auto levelArea = topCells[(size_t) 0];
    auto fineArea = topCells[(size_t) 1];
    auto panArea = topCells[(size_t) 2];
    auto coarseArea = bottomCells[(size_t) 0];
    auto voicesArea = bottomCells[(size_t) 1];
    auto uDetuneArea = bottomCells[(size_t) 2];

    levelLabel.setBounds(levelArea.removeFromTop(16));
    levelSlider.setBounds(levelArea);
    detuneLabel.setBounds(fineArea.removeFromTop(16));
    detuneSlider.setBounds(fineArea);
    panLabel.setBounds(panArea.removeFromTop(16));
    panSlider.setBounds(panArea);
    coarseLabel.setBounds(coarseArea.removeFromTop(16));
    coarseSlider.setBounds(coarseArea);
    unisonVoicesLabel.setBounds(voicesArea.removeFromTop(16));
    unisonVoicesSlider.setBounds(voicesArea);
    unisonDetuneLabel.setBounds(uDetuneArea.removeFromTop(16));
    unisonDetuneSlider.setBounds(uDetuneArea);
}

void OscillatorSectionComponent::timerCallback()
{
    const bool enabled = unisonButton.getToggleState();
    const int oscTypeIndex = oscTypeParam != nullptr ? juce::roundToInt(oscTypeParam->load()) : 0;
    const bool samplerSelected = oscTypeIndex == 1;
    const bool wavetableSelected = oscTypeIndex == 2;
    if (samplerComponent.isVisible() != samplerSelected || wavetableComponent.isVisible() != wavetableSelected)
        resized();

    unisonVoicesSlider.setEnabled(enabled);
    unisonDetuneSlider.setEnabled(enabled);
    unisonVoicesLabel.setAlpha(enabled ? 1.0f : 0.55f);
    unisonDetuneLabel.setAlpha(enabled ? 1.0f : 0.55f);
    samplerComponent.setVisible(samplerSelected);
    wavetableComponent.setVisible(wavetableSelected);
    waveLabel.setVisible(!samplerSelected && !wavetableSelected);
    waveBox.setVisible(!samplerSelected && !wavetableSelected);
    octaveLabel.setVisible(!samplerSelected && !wavetableSelected);
    octaveSlider.setVisible(!samplerSelected && !wavetableSelected);
    repaint();
}
