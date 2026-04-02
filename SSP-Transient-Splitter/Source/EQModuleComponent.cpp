#include "EQModuleComponent.h"

#include "PluginProcessor.h"
#include "ReactorUI.h"

namespace
{
constexpr int eqBandCount = 4;
constexpr int eqTypeCount = 5;
constexpr int eqTargetCount = 3;
constexpr int eqVariantMax = 0xfffff;

enum EQBandTarget
{
    eqTargetStereo = 0,
    eqTargetMid,
    eqTargetSide
};

juce::String getFXSlotOnParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "On";
}

juce::String getFXSlotFloatParamID(int slotIndex, int parameterIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Param" + juce::String(parameterIndex + 1);
}

juce::String getFXSlotVariantParamID(int slotIndex)
{
    return "fxSlot" + juce::String(slotIndex + 1) + "Variant";
}

int getEQFrequencyParamIndex(int bandIndex)
{
    return bandIndex * 2;
}

int getEQGainParamIndex(int bandIndex)
{
    return bandIndex * 2 + 1;
}

int getEQQParamIndex(int bandIndex)
{
    return 8 + bandIndex;
}

void styleLabel(juce::Label& label, float size, juce::Colour colour, juce::Justification just = juce::Justification::centredLeft)
{
    label.setFont(size >= 14.0f ? reactorui::sectionFont(size) : reactorui::bodyFont(size));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(just);
}

void styleToggle(juce::ToggleButton& button, juce::Colour accent)
{
    button.setButtonText("On");
    button.setClickingTogglesState(true);
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, accent);
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff59606a));
}

void styleSlider(juce::Slider& slider, juce::Colour accent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.45f));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, reactorui::outline());
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f151c));
    slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
}

void styleSelector(juce::ComboBox& combo, juce::Colour accent)
{
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff11161c));
    combo.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    combo.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    combo.setColour(juce::ComboBox::arrowColourId, accent);
}

void drawDeck(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    juce::ColourGradient fill(juce::Colour(0xff12161c), bounds.getTopLeft(),
                              juce::Colour(0xff090d12), bounds.getBottomLeft(), false);
    fill.addColour(0.12, accent.withAlpha(0.10f));
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(juce::Colour(0xff050608));
    g.drawRoundedRectangle(bounds, 7.0f, 1.2f);
    g.setColour(reactorui::outlineSoft().withAlpha(0.95f));
    g.drawRoundedRectangle(bounds.reduced(1.2f), 6.0f, 1.0f);
}

float normalToFrequency(float value)
{
    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 20000.0f;
    return minFrequency * std::pow(maxFrequency / minFrequency, juce::jlimit(0.0f, 1.0f, value));
}

float frequencyToNormal(float frequency)
{
    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 20000.0f;
    return juce::jlimit(0.0f, 1.0f, (float) juce::mapFromLog10(juce::jlimit(minFrequency, maxFrequency, frequency), minFrequency, maxFrequency));
}

float normalToGainDb(float value)
{
    return juce::jmap(juce::jlimit(0.0f, 1.0f, value), -18.0f, 18.0f);
}

float gainDbToNormal(float gainDb)
{
    return juce::jlimit(0.0f, 1.0f, juce::jmap(gainDb, -18.0f, 18.0f, 0.0f, 1.0f));
}

float normalToQ(float value)
{
    return 0.35f + juce::jlimit(0.0f, 1.0f, value) * 11.65f;
}

int getBandTypeFromVariant(int variant, int bandIndex)
{
    return (variant >> (4 + bandIndex * 2)) & 0x3;
}

int getBandTargetFromVariant(int variant, int bandIndex)
{
    return (variant >> (12 + bandIndex * 2)) & 0x3;
}

bool getBandActiveFromVariant(int variant, int bandIndex)
{
    return ((variant >> bandIndex) & 0x1) != 0;
}

int withBandType(int variant, int bandIndex, int typeIndex)
{
    const int shift = 4 + bandIndex * 2;
    variant &= ~(0x3 << shift);
    variant |= (juce::jlimit(0, eqTypeCount - 1, typeIndex) << shift);
    return variant;
}

int withBandTarget(int variant, int bandIndex, int targetIndex)
{
    const int shift = 12 + bandIndex * 2;
    variant &= ~(0x3 << shift);
    variant |= (juce::jlimit(0, eqTargetCount - 1, targetIndex) << shift);
    return variant;
}

int withBandActive(int variant, int bandIndex, bool active)
{
    const int mask = 1 << bandIndex;
    if (active)
        return variant | mask;

    return variant & ~mask;
}

void setFloatParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1(juce::jlimit(0.0f, 1.0f, value)));
}

void setChoiceParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int value)
{
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        ranged->setValueNotifyingHost(ranged->convertTo0to1((float) juce::jmax(0, value)));
}

juce::IIRCoefficients makeEQCoefficients(int type, double sampleRate, float frequency, float q, float gainDb)
{
    const float safeFrequency = juce::jlimit(20.0f, (float) sampleRate * 0.45f, frequency);
    const float safeQ = juce::jlimit(0.35f, 12.0f, q);
    const float gain = juce::Decibels::decibelsToGain(gainDb);

    switch (type)
    {
        case 1: return juce::IIRCoefficients::makeLowShelf(sampleRate, safeFrequency, juce::jmax(0.4f, safeQ * 0.45f), gain);
        case 2: return juce::IIRCoefficients::makeHighShelf(sampleRate, safeFrequency, juce::jmax(0.4f, safeQ * 0.45f), gain);
        case 3: return juce::IIRCoefficients::makeHighPass(sampleRate, safeFrequency, safeQ);
        case 4: return juce::IIRCoefficients::makeLowPass(sampleRate, safeFrequency, safeQ);
        default:return juce::IIRCoefficients::makePeakFilter(sampleRate, safeFrequency, safeQ, gain);
    }
}

float evaluateMagnitude(const juce::IIRCoefficients& coefficients, float frequency, double sampleRate)
{
    const auto omega = juce::MathConstants<double>::twoPi * (double) frequency / juce::jmax(1.0, sampleRate);
    const std::complex<double> z1 = std::polar(1.0, -omega);
    const std::complex<double> z2 = std::polar(1.0, -2.0 * omega);
    const auto numerator = (double) coefficients.coefficients[0]
                         + (double) coefficients.coefficients[1] * z1
                         + (double) coefficients.coefficients[2] * z2;
    const auto denominator = 1.0
                           + (double) coefficients.coefficients[3] * z1
                           + (double) coefficients.coefficients[4] * z2;
    return (float) std::abs(numerator / denominator);
}

juce::Colour getTargetColour(int target, juce::Colour accent)
{
    switch (target)
    {
        case eqTargetMid: return juce::Colour(0xffffb46b);
        case eqTargetSide: return juce::Colour(0xff79d7ff);
        default: return accent.brighter(0.08f);
    }
}
}

EQModuleComponent::EQModuleComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state, int slot)
    : processor(p),
      apvts(state),
      slotIndex(slot)
{
    const auto accent = juce::Colour(0xffffdf63);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    addAndMakeVisible(onButton);

    titleLabel.setText("EQ", juce::dontSendNotification);
    subtitleLabel.setText("Click in the graph to add up to four bands, drag points to shape them, and route each band to Stereo, Mid, or Side.", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());
    styleLabel(subtitleLabel, 11.0f, reactorui::textMuted());
    styleToggle(onButton, accent);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts,
                                                                                           getFXSlotOnParamID(slotIndex),
                                                                                           onButton);

    for (int band = 0; band < eqBandCount; ++band)
    {
        addAndMakeVisible(bandLabels[(size_t) band]);
        addAndMakeVisible(qLabels[(size_t) band]);
        addAndMakeVisible(targetSelectors[(size_t) band]);
        addAndMakeVisible(typeSelectors[(size_t) band]);
        addAndMakeVisible(qKnobs[(size_t) band]);

        bandLabels[(size_t) band].setText("Band " + juce::String(band + 1), juce::dontSendNotification);
        qLabels[(size_t) band].setText("Q", juce::dontSendNotification);
        styleLabel(bandLabels[(size_t) band], 10.5f, reactorui::textMuted());
        styleLabel(qLabels[(size_t) band], 10.0f, reactorui::textMuted(), juce::Justification::centred);
        styleSelector(targetSelectors[(size_t) band], accent);
        styleSelector(typeSelectors[(size_t) band], accent);
        const auto parameterID = getFXSlotFloatParamID(slotIndex, getEQQParamIndex(band));
        qKnobs[(size_t) band].setupModulation(processor, parameterID, reactormod::Destination::none, accent);

        qKnobs[(size_t) band].textFromValueFunction = [] (double value)
        {
            return juce::String(normalToQ((float) value), 2);
        };
        qKnobs[(size_t) band].valueFromTextFunction = [] (const juce::String& text)
        {
            const float q = juce::jlimit(0.35f, 12.0f, (float) text.getDoubleValue());
            return juce::jlimit(0.0f, 1.0f, (q - 0.35f) / 11.65f);
        };

        targetSelectors[(size_t) band].addItem("Stereo", 1);
        targetSelectors[(size_t) band].addItem("Mid", 2);
        targetSelectors[(size_t) band].addItem("Side", 3);
        targetSelectors[(size_t) band].onChange = [this, band]
        {
            setBandTarget(band, juce::jlimit(0, eqTargetCount - 1, targetSelectors[(size_t) band].getSelectedId() - 1));
        };

        typeSelectors[(size_t) band].addItemList(PluginProcessor::getFXEQTypeNames(), 1);
        typeSelectors[(size_t) band].onChange = [this, band]
        {
            setBandType(band, juce::jlimit(0, eqTypeCount - 1, typeSelectors[(size_t) band].getSelectedId() - 1));
        };

        qAttachments[(size_t) band] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                                                                                                              parameterID,
                                                                                                              qKnobs[(size_t) band]);
    }

    syncTypeSelectors();
    syncControlState();
    startTimerHz(24);
}

int EQModuleComponent::getPreferredHeight() const
{
    return 516;
}

void EQModuleComponent::paint(juce::Graphics& g)
{
    const auto accent = juce::Colour(0xffffdf63);

    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), accent, 12.0f);
    reactorui::drawDisplayPanel(g, graphBounds.toFloat(), accent);
    reactorui::drawSubtleGrid(g, graphBounds.reduced(1).toFloat(), accent.withAlpha(0.12f), 6, 4);
    drawDeck(g, controlDeckBounds.toFloat(), accent);

    for (int band = 0; band < eqBandCount; ++band)
    {
        if (! bandCellBounds[(size_t) band].isEmpty())
            drawDeck(g, bandCellBounds[(size_t) band].toFloat(), accent.withAlpha(isBandActive(band) ? 0.74f : 0.22f));
    }

    if (! graphBounds.isEmpty())
    {
        const float centreY = graphBounds.getCentreY();
        g.setColour(reactorui::textMuted().withAlpha(0.28f));
        g.drawHorizontalLine((int) std::round(centreY), (float) graphBounds.getX(), (float) graphBounds.getRight());

        std::array<float, PluginProcessor::fxEQSpectrumBinCount> spectrum{};
        processor.getFXEQSpectrum(slotIndex, spectrum);

        juce::Path midResponse;
        juce::Path sideResponse;
        bool midStarted = false;
        bool sideStarted = false;
        juce::Path spectrumPath;
        bool spectrumStarted = false;
        int activeBands = 0;
        for (int band = 0; band < eqBandCount; ++band)
            activeBands += isBandActive(band) ? 1 : 0;

        for (int x = 0; x < graphBounds.getWidth(); ++x)
        {
            const float normX = (float) x / juce::jmax(1.0f, (float) graphBounds.getWidth() - 1.0f);
            const float frequency = normalToFrequency(normX);
            float midMagnitude = 1.0f;
            float sideMagnitude = 1.0f;

            for (int band = 0; band < eqBandCount; ++band)
            {
                if (! isBandActive(band))
                    continue;

                const int type = getBandType(band);
                const int target = getBandTarget(band);
                const float bandFrequency = normalToFrequency(getBandFrequencyNormal(band));
                const float gainDb = normalToGainDb(getBandGainNormal(band));
                const float q = normalToQ((float) qKnobs[(size_t) band].getValue());
                const float bandMagnitude = evaluateMagnitude(makeEQCoefficients(type, 48000.0, bandFrequency, q, gainDb), frequency, 48000.0);

                if (target != eqTargetSide)
                    midMagnitude *= bandMagnitude;
                if (target != eqTargetMid)
                    sideMagnitude *= bandMagnitude;
            }

            const float drawX = (float) graphBounds.getX() + (float) x;
            const auto dbToY = [this] (float magnitude)
            {
                const float responseDb = juce::Decibels::gainToDecibels(magnitude, -24.0f);
                const float mappedDb = juce::jlimit(-18.0f, 18.0f, responseDb);
                return juce::jmap(mappedDb, 18.0f, -18.0f, (float) graphBounds.getY() + 8.0f, (float) graphBounds.getBottom() - 8.0f);
            };

            const float spectrumIndex = juce::jlimit(0.0f, (float) PluginProcessor::fxEQSpectrumBinCount - 1.001f,
                                                     normX * (float) (PluginProcessor::fxEQSpectrumBinCount - 1));
            const int lowerSpectrumIndex = juce::jlimit(0, PluginProcessor::fxEQSpectrumBinCount - 1, (int) std::floor(spectrumIndex));
            const int upperSpectrumIndex = juce::jlimit(0, PluginProcessor::fxEQSpectrumBinCount - 1, lowerSpectrumIndex + 1);
            const float spectrumAlpha = spectrumIndex - (float) lowerSpectrumIndex;
            const float spectrumValue = juce::jmap(spectrumAlpha,
                                                   spectrum[(size_t) lowerSpectrumIndex],
                                                   spectrum[(size_t) upperSpectrumIndex]);
            const float spectrumY = juce::jmap(spectrumValue, 0.0f, 1.0f,
                                               (float) graphBounds.getBottom() - 10.0f,
                                               (float) graphBounds.getY() + graphBounds.getHeight() * 0.20f);
            const float midY = dbToY(midMagnitude);
            const float sideY = dbToY(sideMagnitude);

            if (! spectrumStarted)
            {
                spectrumPath.startNewSubPath(drawX, spectrumY);
                spectrumStarted = true;
            }
            else
            {
                spectrumPath.lineTo(drawX, spectrumY);
            }

            if (! midStarted)
            {
                midResponse.startNewSubPath(drawX, midY);
                midStarted = true;
            }
            else
            {
                midResponse.lineTo(drawX, midY);
            }

            if (! sideStarted)
            {
                sideResponse.startNewSubPath(drawX, sideY);
                sideStarted = true;
            }
            else
            {
                sideResponse.lineTo(drawX, sideY);
            }
        }

        g.setColour(juce::Colour(0xffb8fff0).withAlpha(0.07f));
        juce::Path spectrumFill(spectrumPath);
        spectrumFill.lineTo((float) graphBounds.getRight(), (float) graphBounds.getBottom() - 8.0f);
        spectrumFill.lineTo((float) graphBounds.getX(), (float) graphBounds.getBottom() - 8.0f);
        spectrumFill.closeSubPath();
        g.fillPath(spectrumFill);

        g.setColour(juce::Colour(0xffb8fff0).withAlpha(0.42f));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(accent.withAlpha(0.10f));
        juce::Path midFill(midResponse);
        midFill.lineTo((float) graphBounds.getRight(), centreY);
        midFill.lineTo((float) graphBounds.getX(), centreY);
        midFill.closeSubPath();
        g.fillPath(midFill);

        g.setColour(juce::Colour(0xff79d7ff).withAlpha(0.08f));
        juce::Path sideFill(sideResponse);
        sideFill.lineTo((float) graphBounds.getRight(), centreY);
        sideFill.lineTo((float) graphBounds.getX(), centreY);
        sideFill.closeSubPath();
        g.fillPath(sideFill);

        g.setColour(juce::Colour(0xffffb46b));
        g.strokePath(midResponse, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff79d7ff));
        g.strokePath(sideResponse, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto legendArea = juce::Rectangle<float>((float) graphBounds.getRight() - 120.0f, (float) graphBounds.getY() + 10.0f, 110.0f, 18.0f);
        g.setFont(reactorui::bodyFont(10.0f));
        g.setColour(juce::Colour(0xffffb46b));
        g.drawText("Mid", legendArea.removeFromLeft(48.0f), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff79d7ff));
        g.drawText("Side", legendArea.removeFromLeft(40.0f), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffb8fff0));
        g.drawText("Post", legendArea, juce::Justification::centredLeft);

        if (activeBands == 0)
        {
            g.setColour(reactorui::textMuted());
            g.setFont(reactorui::bodyFont(12.5f));
            g.drawFittedText("Click to add a band", graphBounds.reduced(16, 12), juce::Justification::centred, 1);
        }

        for (int band = 0; band < eqBandCount; ++band)
        {
            if (! isBandActive(band))
                continue;

            const auto point = getBandPosition(band);
            const float radius = band == selectedBand ? 8.5f : 7.0f;
            const int target = getBandTarget(band);
            const auto pointColour = getTargetColour(target, accent);
            g.setColour(juce::Colour(0xff10151b));
            g.fillEllipse(point.x - radius - 1.0f, point.y - radius - 1.0f, (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);
            g.setColour(pointColour.brighter(band == selectedBand ? 0.2f : 0.05f));
            g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colour(0xff0f1419));
            g.drawEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
            g.setColour(juce::Colour(0xff11161c));
            g.setFont(reactorui::bodyFont(10.0f));
            g.drawFittedText(juce::String(band + 1),
                             juce::Rectangle<int>((int) std::round(point.x - 8.0f), (int) std::round(point.y - 8.0f), 16, 16),
                             juce::Justification::centred,
                             1);

            if (target != eqTargetStereo)
            {
                auto tagBounds = juce::Rectangle<float>(point.x + radius - 2.0f, point.y - radius - 10.0f, 18.0f, 12.0f);
                g.setColour(pointColour.withAlpha(0.95f));
                g.fillRoundedRectangle(tagBounds, 4.0f);
                g.setColour(juce::Colour(0xff0f1419));
                g.setFont(reactorui::bodyFont(9.0f));
                g.drawText(target == eqTargetMid ? "M" : "S", tagBounds, juce::Justification::centred);
            }
        }
    }
}

void EQModuleComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 12);
    auto header = area.removeFromTop(22);
    titleLabel.setBounds(header.removeFromLeft(160));
    onButton.setBounds(header.removeFromRight(56));

    subtitleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    graphBounds = area.removeFromTop(196);
    area.removeFromTop(12);

    controlDeckBounds = area;
    auto controlArea = controlDeckBounds.reduced(10, 8);
    const int columnGap = 12;
    const int cellWidth = (controlArea.getWidth() - columnGap * 3) / 4;

    for (int band = 0; band < eqBandCount; ++band)
    {
        auto cell = controlArea.removeFromLeft(cellWidth);
        if (band < eqBandCount - 1)
            controlArea.removeFromLeft(columnGap);

        bandCellBounds[(size_t) band] = cell;
        auto content = cell.reduced(6, 6);
        bandLabels[(size_t) band].setBounds(content.removeFromTop(14));
        content.removeFromTop(4);
        targetSelectors[(size_t) band].setBounds(content.removeFromTop(24));
        content.removeFromTop(4);
        typeSelectors[(size_t) band].setBounds(content.removeFromTop(26));
        content.removeFromTop(4);
        qLabels[(size_t) band].setBounds(content.removeFromTop(14));
        content.removeFromTop(2);
        qKnobs[(size_t) band].setBounds(content);
    }
}

void EQModuleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! graphBounds.contains(event.getPosition()))
        return;

    const auto position = event.position;
    const int hitBand = findBandAt(position);

    if (event.mods.isRightButtonDown())
    {
        if (hitBand >= 0)
        {
            setBandActive(hitBand, false);
            if (selectedBand == hitBand)
                selectedBand = -1;
            repaint();
        }
        return;
    }

    if (hitBand >= 0)
    {
        selectedBand = hitBand;
        draggingBand = hitBand;
        repaint();
        return;
    }

    const int inactiveBand = findFirstInactiveBand();
    if (inactiveBand < 0)
        return;

    const float normalX = juce::jlimit(0.0f, 1.0f, (position.x - (float) graphBounds.getX()) / juce::jmax(1.0f, (float) graphBounds.getWidth()));
    const float normalY = juce::jlimit(0.0f, 1.0f, 1.0f - ((position.y - (float) graphBounds.getY()) / juce::jmax(1.0f, (float) graphBounds.getHeight())));
    setBandPosition(inactiveBand, normalX, normalY);
    setBandType(inactiveBand, 0);
    setBandActive(inactiveBand, true);
    selectedBand = inactiveBand;
    draggingBand = inactiveBand;
    repaint();
}

void EQModuleComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingBand < 0)
        return;

    const float normalX = juce::jlimit(0.0f, 1.0f, (event.position.x - (float) graphBounds.getX()) / juce::jmax(1.0f, (float) graphBounds.getWidth()));
    const float normalY = juce::jlimit(0.0f, 1.0f, 1.0f - ((event.position.y - (float) graphBounds.getY()) / juce::jmax(1.0f, (float) graphBounds.getHeight())));
    setBandPosition(draggingBand, normalX, normalY);
    repaint();
}

void EQModuleComponent::mouseUp(const juce::MouseEvent&)
{
    draggingBand = -1;
}

void EQModuleComponent::timerCallback()
{
    syncTypeSelectors();
    syncControlState();
    repaint();
}

int EQModuleComponent::getPackedVariant() const
{
    if (auto* raw = apvts.getRawParameterValue(getFXSlotVariantParamID(slotIndex)))
        return juce::jmax(0, (int) std::round(raw->load()));

    return 0;
}

void EQModuleComponent::setPackedVariant(int variant)
{
    setChoiceParameter(apvts, getFXSlotVariantParamID(slotIndex), juce::jlimit(0, eqVariantMax, variant));
}

bool EQModuleComponent::isBandActive(int bandIndex) const
{
    return juce::isPositiveAndBelow(bandIndex, eqBandCount) && getBandActiveFromVariant(getPackedVariant(), bandIndex);
}

void EQModuleComponent::setBandActive(int bandIndex, bool shouldBeActive)
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return;

    setPackedVariant(withBandActive(getPackedVariant(), bandIndex, shouldBeActive));
}

int EQModuleComponent::getBandType(int bandIndex) const
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return 0;

    return juce::jlimit(0, eqTypeCount - 1, getBandTypeFromVariant(getPackedVariant(), bandIndex));
}

void EQModuleComponent::setBandType(int bandIndex, int typeIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return;

    setPackedVariant(withBandType(getPackedVariant(), bandIndex, typeIndex));
}

int EQModuleComponent::getBandTarget(int bandIndex) const
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return eqTargetStereo;

    return juce::jlimit(0, eqTargetCount - 1, getBandTargetFromVariant(getPackedVariant(), bandIndex));
}

void EQModuleComponent::setBandTarget(int bandIndex, int targetIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return;

    setPackedVariant(withBandTarget(getPackedVariant(), bandIndex, targetIndex));
}

float EQModuleComponent::getBandFrequencyNormal(int bandIndex) const
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return 0.5f;

    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, getEQFrequencyParamIndex(bandIndex))))
        return juce::jlimit(0.0f, 1.0f, raw->load());

    return 0.5f;
}

float EQModuleComponent::getBandGainNormal(int bandIndex) const
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return 0.5f;

    if (auto* raw = apvts.getRawParameterValue(getFXSlotFloatParamID(slotIndex, getEQGainParamIndex(bandIndex))))
        return juce::jlimit(0.0f, 1.0f, raw->load());

    return 0.5f;
}

void EQModuleComponent::setBandPosition(int bandIndex, float frequencyNormal, float gainNormal)
{
    if (! juce::isPositiveAndBelow(bandIndex, eqBandCount))
        return;

    setFloatParameter(apvts, getFXSlotFloatParamID(slotIndex, getEQFrequencyParamIndex(bandIndex)), frequencyNormal);
    setFloatParameter(apvts, getFXSlotFloatParamID(slotIndex, getEQGainParamIndex(bandIndex)), gainNormal);
}

int EQModuleComponent::findBandAt(juce::Point<float> position) const
{
    for (int band = eqBandCount - 1; band >= 0; --band)
    {
        if (! isBandActive(band))
            continue;

        if (getBandPosition(band).getDistanceFrom(position) <= 11.0f)
            return band;
    }

    return -1;
}

int EQModuleComponent::findFirstInactiveBand() const
{
    for (int band = 0; band < eqBandCount; ++band)
        if (! isBandActive(band))
            return band;

    return -1;
}

juce::Point<float> EQModuleComponent::getBandPosition(int bandIndex) const
{
    const float x = (float) graphBounds.getX() + getBandFrequencyNormal(bandIndex) * (float) graphBounds.getWidth();
    const float y = (float) graphBounds.getBottom() - getBandGainNormal(bandIndex) * (float) graphBounds.getHeight();
    return { x, y };
}

void EQModuleComponent::syncTypeSelectors()
{
    for (int band = 0; band < eqBandCount; ++band)
    {
        const int targetIndex = getBandTarget(band);
        if (targetSelectors[(size_t) band].getSelectedId() != targetIndex + 1)
            targetSelectors[(size_t) band].setSelectedId(targetIndex + 1, juce::dontSendNotification);

        const int typeIndex = getBandType(band);
        if (typeSelectors[(size_t) band].getSelectedId() != typeIndex + 1)
            typeSelectors[(size_t) band].setSelectedId(typeIndex + 1, juce::dontSendNotification);
    }
}

void EQModuleComponent::syncControlState()
{
    const bool enabled = onButton.getToggleState();
    for (int band = 0; band < eqBandCount; ++band)
    {
        const bool bandEnabled = enabled && isBandActive(band);
        targetSelectors[(size_t) band].setEnabled(bandEnabled);
        typeSelectors[(size_t) band].setEnabled(bandEnabled);
        qKnobs[(size_t) band].setEnabled(bandEnabled);
        const auto labelColour = bandEnabled ? reactorui::textStrong() : reactorui::textMuted().withAlpha(0.6f);
        bandLabels[(size_t) band].setColour(juce::Label::textColourId, labelColour);
        qLabels[(size_t) band].setColour(juce::Label::textColourId, labelColour.withAlpha(0.85f));
    }
}
