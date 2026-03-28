#include "VintageCompControlsComponent.h"

namespace
{
juce::Colour rackShadow() { return juce::Colour(0xff030405); }
juce::Colour rackRail() { return juce::Colour(0xff080a0d); }
juce::Colour faceBlue() { return juce::Colour(0xff355d8d); }
juce::Colour faceBlueDark() { return juce::Colour(0xff1a3757); }
juce::Colour faceHighlight() { return juce::Colour(0xff5b82b0); }
juce::Colour lineSteel() { return juce::Colour(0xffa4b0bb); }
juce::Colour textWarm() { return juce::Colour(0xfff3d889); }
juce::Colour textCool() { return juce::Colour(0xffd8e4ef); }

juce::String formatVintageLevelReadout(double value, double minValue, double maxValue)
{
    const auto normalised = juce::jlimit(0.0, 1.0, (value - minValue) / (maxValue - minValue));
    if (normalised <= 0.0005)
        return juce::String(CharPointer_UTF8("\xE2\x88\x9E"));

    const int scaled = juce::roundToInt((float) juce::jmap(normalised, 0.0, 1.0, 48.0, 0.0));
    return juce::String(scaled);
}
}

VintageCompControlsComponent::VintageCompControlsComponent(PluginProcessor& p, juce::AudioProcessorValueTreeState& state)
    : processor(p),
      apvts(state),
      meter(p)
{
    inputKnob.setValueFormatter([](double value) { return formatVintageLevelReadout(value, 0.0, 36.0); });
    attackKnob.setValueFormatter([](double value) { return juce::String(value, 1); });
    releaseKnob.setValueFormatter([](double value) { return juce::String(value, 1); });
    mixKnob.setValueFormatter([](double value) { return juce::String(juce::roundToInt((float) value)) + "%"; });
    outputKnob.setValueFormatter([](double value) { return formatVintageLevelReadout(value, -18.0, 18.0); });

    addAndMakeVisible(inputKnob);
    addAndMakeVisible(attackKnob);
    addAndMakeVisible(releaseKnob);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(outputKnob);
    addAndMakeVisible(meter);

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "input", inputKnob.getSlider());
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackKnob.getSlider());
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseKnob.getSlider());
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixKnob.getSlider());
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "output", outputKnob.getSlider());

    auto setupRatioButton = [this](juce::TextButton& button, int index)
    {
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff111720));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0bf57));
        button.onClick = [this, index] { selectRatio(index); };
        addAndMakeVisible(button);
    };

    setupRatioButton(ratio4Button, 0);
    setupRatioButton(ratio8Button, 1);
    setupRatioButton(ratio12Button, 2);
    setupRatioButton(ratio20Button, 3);
    setupRatioButton(ratioAllButton, 4);

    auto setupMeterButton = [this](juce::TextButton& button, int index)
    {
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f141c));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd8e4ef));
        button.onClick = [this, index] { selectMeterMode(index); };
        addAndMakeVisible(button);
    };

    setupMeterButton(meterGrButton, 0);
    setupMeterButton(meter8Button, 1);
    setupMeterButton(meter4Button, 2);
    setupMeterButton(meterOffButton, 3);

    startTimerHz(24);
    updateRatioButtons();
    updateMeterModeButtons();
}

void VintageCompControlsComponent::paint(juce::Graphics& g)
{
    g.fillAll(rackShadow());

    auto bounds = getLocalBounds();
    g.setColour(rackRail());
    g.fillRect(bounds.removeFromTop(30));
    g.fillRect(getLocalBounds().removeFromBottom(30));

    auto faceplate = getLocalBounds().reduced(28, 30).toFloat();
    drawFaceplate(g, faceplate);

    auto header = juce::Rectangle<float>(faceplate.getX() + 26.0f, faceplate.getY() + 18.0f, faceplate.getWidth() - 52.0f, 88.0f);
    g.setColour(juce::Colour(0xcc07101d));
    g.fillRoundedRectangle(header, 12.0f);
    g.setColour(textWarm());
    g.setFont(juce::Font("Arial", 33.0f, juce::Font::bold));
    g.drawText("SSP VINTAGE COMPRESS", header.toNearestInt(), juce::Justification::centred, false);

    auto sub = juce::Rectangle<float>(header.getX(), header.getBottom() + 8.0f, header.getWidth(), 20.0f);
    g.setColour(juce::Colour(0xaa0f2239));
    g.fillRoundedRectangle(sub, 6.0f);
    g.setColour(textCool());
    g.setFont(12.5f);
    g.drawText("1176-inspired FET compression with classic ratio buttons, all-buttons smash, and parallel blend.",
               sub.toNearestInt(), juce::Justification::centred, false);

    drawSectionCaption(g, juce::Rectangle<int>(inputKnob.getX(), inputKnob.getY() - 24, inputKnob.getWidth(), 18), "INPUT STAGE");
    drawSectionCaption(g, juce::Rectangle<int>(attackKnob.getX(), attackKnob.getY() - 24, releaseKnob.getRight() - attackKnob.getX(), 18), "TIMING");
    drawSectionCaption(g, juce::Rectangle<int>(mixKnob.getX(), mixKnob.getY() - 24, mixKnob.getWidth(), 18), "BLEND");
    drawSectionCaption(g, juce::Rectangle<int>(outputKnob.getX(), outputKnob.getY() - 24, outputKnob.getWidth(), 18), "OUTPUT STAGE");
    drawSectionCaption(g, juce::Rectangle<int>(ratio4Button.getX(), ratio4Button.getY() - 26, ratioAllButton.getRight() - ratio4Button.getX(), 18), "RATIO SELECT");
    drawSectionCaption(g, juce::Rectangle<int>(meterGrButton.getX(), meterGrButton.getY() - 26, meterGrButton.getWidth(), 18), "METER");

    drawRackScrews(g, getLocalBounds());
}

void VintageCompControlsComponent::resized()
{
    const int topKnobY = 214;
    const int bigKnobW = 240;
    const int bigKnobH = 298;
    const int smallKnobW = 156;
    const int smallKnobH = 192;
    const int meterWidth = 446;
    const int meterHeight = 256;
    const int meterY = 162;
    const int meterSwitchWidth = 56;
    const int meterSwitchHeight = 30;
    const int meterSwitchGap = 8;

    inputKnob.setBounds(58, topKnobY, bigKnobW, bigKnobH);
    outputKnob.setBounds(getWidth() - 58 - bigKnobW, topKnobY, bigKnobW, bigKnobH);

    meter.setBounds(getWidth() / 2 - meterWidth / 2, meterY, meterWidth, meterHeight);

    const int meterSwitchX = meter.getRight() + 24;
    int meterSwitchY = meter.getY() + 52;
    meterGrButton.setBounds(meterSwitchX, meterSwitchY, meterSwitchWidth, meterSwitchHeight); meterSwitchY += meterSwitchHeight + meterSwitchGap;
    meter8Button.setBounds(meterSwitchX, meterSwitchY, meterSwitchWidth, meterSwitchHeight); meterSwitchY += meterSwitchHeight + meterSwitchGap;
    meter4Button.setBounds(meterSwitchX, meterSwitchY, meterSwitchWidth, meterSwitchHeight); meterSwitchY += meterSwitchHeight + meterSwitchGap;
    meterOffButton.setBounds(meterSwitchX, meterSwitchY, meterSwitchWidth, meterSwitchHeight);

    const int buttonY = meter.getBottom() + 36;
    const int lowerKnobY = buttonY + 68;
    attackKnob.setBounds(getWidth() / 2 - 272, lowerKnobY, smallKnobW, smallKnobH);
    releaseKnob.setBounds(getWidth() / 2 - 78, lowerKnobY, smallKnobW, smallKnobH);
    mixKnob.setBounds(getWidth() / 2 + 116, lowerKnobY, smallKnobW, smallKnobH);

    const int buttonWidth = 66;
    const int buttonHeight = 40;
    const int gap = 12;
    const int allWidth = 78;
    const int totalWidth = buttonWidth * 4 + allWidth + gap * 4;
    int x = getWidth() / 2 - totalWidth / 2;

    ratio4Button.setBounds(x, buttonY, buttonWidth, buttonHeight); x += buttonWidth + gap;
    ratio8Button.setBounds(x, buttonY, buttonWidth, buttonHeight); x += buttonWidth + gap;
    ratio12Button.setBounds(x, buttonY, buttonWidth, buttonHeight); x += buttonWidth + gap;
    ratio20Button.setBounds(x, buttonY, buttonWidth, buttonHeight); x += buttonWidth + gap;
    ratioAllButton.setBounds(x, buttonY, allWidth, buttonHeight);
}

void VintageCompControlsComponent::timerCallback()
{
    updateRatioButtons();
    updateMeterModeButtons();
    repaint();
}

void VintageCompControlsComponent::updateRatioButtons()
{
    const int index = processor.getCurrentRatioIndex();
    ratio4Button.setToggleState(index == 0, juce::dontSendNotification);
    ratio8Button.setToggleState(index == 1, juce::dontSendNotification);
    ratio12Button.setToggleState(index == 2, juce::dontSendNotification);
    ratio20Button.setToggleState(index == 3, juce::dontSendNotification);
    ratioAllButton.setToggleState(index == 4, juce::dontSendNotification);
}

void VintageCompControlsComponent::selectRatio(int index)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("ratio")))
    {
        const float normalised = parameter->getNormalisableRange().convertTo0to1((float) index);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalised);
        parameter->endChangeGesture();
    }
}

void VintageCompControlsComponent::updateMeterModeButtons()
{
    const int index = processor.getCurrentMeterModeIndex();
    meterGrButton.setToggleState(index == 0, juce::dontSendNotification);
    meter8Button.setToggleState(index == 1, juce::dontSendNotification);
    meter4Button.setToggleState(index == 2, juce::dontSendNotification);
    meterOffButton.setToggleState(index == 3, juce::dontSendNotification);
}

void VintageCompControlsComponent::selectMeterMode(int index)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("meterMode")))
    {
        const float normalised = parameter->getNormalisableRange().convertTo0to1((float) index);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalised);
        parameter->endChangeGesture();
    }
}

void VintageCompControlsComponent::drawFaceplate(juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour(juce::Colour(0xff020305));
    g.fillRoundedRectangle(area.expanded(7.0f, 7.0f), 18.0f);

    juce::ColourGradient plate(faceHighlight(), area.getCentreX(), area.getY(),
                               faceBlueDark(), area.getCentreX(), area.getBottom(), false);
    plate.addColour(0.2, faceBlue());
    plate.addColour(0.62, faceBlueDark().brighter(0.08f));
    g.setGradientFill(plate);
    g.fillRoundedRectangle(area, 15.0f);

    g.setColour(lineSteel().withAlpha(0.7f));
    g.drawRoundedRectangle(area.reduced(1.0f), 15.0f, 1.3f);

    auto inset = area.reduced(24.0f, 120.0f);
    inset.removeFromLeft(248.0f);
    inset.removeFromRight(248.0f);
    g.setColour(juce::Colour(0x16000000));
    g.fillRoundedRectangle(inset, 24.0f);
}

void VintageCompControlsComponent::drawRackScrews(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const std::array<juce::Point<int>, 4> points{{
        {22, 22},
        {area.getWidth() - 22, 22},
        {22, area.getHeight() - 22},
        {area.getWidth() - 22, area.getHeight() - 22}
    }};

    for (const auto& point : points)
    {
        g.setColour(juce::Colour(0xff171a1f));
        g.fillEllipse((float) point.x - 8.0f, (float) point.y - 8.0f, 16.0f, 16.0f);
        g.setColour(juce::Colour(0xffadb6bf));
        g.drawLine((float) point.x - 4.5f, (float) point.y, (float) point.x + 4.5f, (float) point.y, 1.2f);
        g.drawLine((float) point.x, (float) point.y - 4.5f, (float) point.x, (float) point.y + 4.5f, 1.2f);
    }
}

void VintageCompControlsComponent::drawSectionCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text) const
{
    g.setColour(juce::Colour(0xffdfe8f1).withAlpha(0.9f));
    g.setFont(juce::Font("Arial", 13.0f, juce::Font::bold));
    g.drawText(text, area, juce::Justification::centred, false);
}
