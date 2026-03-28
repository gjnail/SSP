#include "FilterSectionComponent.h"
#include "ReactorUI.h"

namespace
{
juce::Rectangle<float> getFilterPreviewBounds(juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced(14).toFloat();
    area.removeFromTop(36.0f);
    return area.removeFromTop(48.0f);
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
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff8ed2ff));
}

void styleToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff8ed2ff));
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
    button.setClickingTogglesState(true);
}
}

FilterSectionComponent::FilterSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    : cutoffSlider(processor, "filterCutoff", reactormod::Destination::filterCutoff, juce::Colour(0xff45b8ff), 66, 16),
      resonanceSlider(processor, "filterResonance", reactormod::Destination::filterResonance, juce::Colour(0xff45b8ff), 66, 16),
      driveSlider(processor, "filterDrive", reactormod::Destination::filterDrive, juce::Colour(0xff45b8ff), 66, 16),
      envSlider(processor, "filterEnvAmount", reactormod::Destination::filterEnvAmount, juce::Colour(0xff45b8ff), 66, 16)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(filterOnButton);
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeBox);
    addAndMakeVisible(cutoffLabel);
    addAndMakeVisible(resonanceLabel);
    addAndMakeVisible(driveLabel);
    addAndMakeVisible(envLabel);
    addAndMakeVisible(cutoffSlider);
    addAndMakeVisible(resonanceSlider);
    addAndMakeVisible(driveSlider);
    addAndMakeVisible(envSlider);

    titleLabel.setText("FILTER", juce::dontSendNotification);
    styleLabel(titleLabel, 15.0f, reactorui::textStrong());

    filterOnButton.setButtonText("On");
    filterOnButton.setTooltip("Enable or bypass the filter");
    styleToggle(filterOnButton);

    modeLabel.setText("Mode (26 types)", juce::dontSendNotification);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    resonanceLabel.setText("Res / Q", juce::dontSendNotification);
    driveLabel.setText("Drive", juce::dontSendNotification);
    envLabel.setText("Env Amt", juce::dontSendNotification);

    styleLabel(modeLabel, 11.5f, reactorui::textMuted());
    styleLabel(cutoffLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(resonanceLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(driveLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);
    styleLabel(envLabel, 11.5f, reactorui::textMuted(), juce::Justification::centred);

    modeBox.addItemList(PluginProcessor::getFilterModeNames(), 1);
    styleCombo(modeBox);
    cutoffSlider.setTextValueSuffix(" Hz");

    filterOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "filterOn", filterOnButton);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "filterMode", modeBox);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterCutoff", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterResonance", resonanceSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterDrive", driveSlider);
    envAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "filterEnvAmount", envSlider);

    startTimerHz(30);
}

void FilterSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto accent = filterOnButton.getToggleState() ? juce::Colour(0xff59c9ff) : juce::Colour(0xff7f8ea1);
    reactorui::drawPanelBackground(g, bounds, accent);

    auto preview = getFilterPreviewBounds(getLocalBounds());
    reactorui::drawDisplayPanel(g, preview, accent);
    reactorui::drawSubtleGrid(g, preview.reduced(1.0f), accent.withAlpha(0.12f), 4, 3);

    const bool filterEnabled = filterOnButton.getToggleState();
    const int modeIndex = juce::jmax(0, modeBox.getSelectedItemIndex());
    const float cutoffValue = juce::jlimit(20.0f, 20000.0f, (float) cutoffSlider.getValue());
    const float cutoffLog = (float) juce::mapFromLog10(cutoffValue, 20.0f, 20000.0f);
    const float resonance = (float) resonanceSlider.getValue();
    const float resonanceNorm = juce::jlimit(0.0f, 1.0f, (resonance - 0.2f) / 17.8f);
    const float envAmount = (float) envSlider.getValue();
    const float drive = (float) driveSlider.getValue();

    juce::Path response;
    for (int x = 0; x < (int) preview.getWidth(); ++x)
    {
        const float normX = (float) x / juce::jmax(1.0f, preview.getWidth() - 1.0f);
        const float frequency = (float) juce::mapToLog10(normX, 20.0f, 20000.0f);
        float magnitude = 0.55f;

        if (filterEnabled)
        {
            switch (modeIndex)
            {
                case 0:
                    magnitude = 1.0f / (1.0f + std::exp((normX - cutoffLog) * 13.0f));
                    break;
                case 1:
                    magnitude = 1.0f / (1.0f + std::exp((normX - cutoffLog) * 18.0f));
                    break;
                case 2:
                    magnitude = 1.0f - (1.0f / (1.0f + std::exp((normX - cutoffLog) * 13.0f)));
                    break;
                case 3:
                    magnitude = 1.0f - (1.0f / (1.0f + std::exp((normX - cutoffLog) * 18.0f)));
                    break;
                case 4:
                case 5:
                {
                    const float width = modeIndex == 4 ? 0.18f : 0.12f;
                    const float distance = (normX - cutoffLog) / width;
                    magnitude = std::exp(-distance * distance * (1.8f + resonanceNorm * 4.0f));
                    break;
                }
                case 6:
                {
                    const float distance = (normX - cutoffLog) / (0.08f + resonanceNorm * 0.08f);
                    magnitude = 1.0f - std::exp(-distance * distance * 1.8f);
                    break;
                }
                case 7:
                {
                    const float distance = (normX - cutoffLog) / 0.09f;
                    magnitude = 0.45f + std::exp(-distance * distance * 2.5f) * (0.22f + resonanceNorm * 0.5f);
                    break;
                }
                case 8:
                    magnitude = 0.38f + (1.0f / (1.0f + std::exp((normX - cutoffLog) * 13.0f))) * (0.3f + resonanceNorm * 0.45f);
                    break;
                case 9:
                    magnitude = 0.38f + (1.0f - (1.0f / (1.0f + std::exp((normX - cutoffLog) * 13.0f)))) * (0.3f + resonanceNorm * 0.45f);
                    break;
                case 10:
                    magnitude = 0.55f;
                    break;
                case 11:
                case 12:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const float feedForward = 0.85f;
                    const float feedback = juce::jlimit(0.1f, 0.95f, 0.18f + resonanceNorm * 0.72f);
                    const float delaySamples = juce::jlimit(18.0f, 3000.0f, visualSampleRate / juce::jlimit(40.0f, 12000.0f, cutoffValue));
                    const float omega = juce::MathConstants<float>::twoPi * frequency / visualSampleRate;
                    const float phase = omega * delaySamples;
                    const float cosine = std::cos(phase);
                    const bool invert = modeIndex == 12;
                    const float numerator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedForward * feedForward + (invert ? -2.0f : 2.0f) * feedForward * cosine));
                    const float denominator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedback * feedback + (invert ? 2.0f : -2.0f) * feedback * cosine));
                    const float responseDb = juce::Decibels::gainToDecibels(numerator / denominator, -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.18f, 0.95f);
                    break;
                }
                case 13:
                {
                    const float c1 = juce::jlimit(0.12f, 0.55f, cutoffLog * 0.75f);
                    const float c2 = juce::jlimit(0.28f, 0.82f, c1 + 0.18f);
                    const float d1 = (normX - c1) / 0.05f;
                    const float d2 = (normX - c2) / 0.06f;
                    magnitude = 0.16f + std::exp(-d1 * d1 * 1.8f) * 0.48f + std::exp(-d2 * d2 * 1.8f) * 0.42f;
                    break;
                }
                case 14:
                case 15:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const float feedForward = 0.80f;
                    const float feedback = juce::jlimit(0.12f, 0.85f, 0.18f + resonanceNorm * 0.52f);
                    const float delaySamples = juce::jmap(cutoffLog, 180.0f, 6.0f);
                    const float omega = juce::MathConstants<float>::twoPi * frequency / visualSampleRate;
                    const float phase = omega * delaySamples;
                    const float cosine = std::cos(phase);
                    const bool invert = modeIndex == 15;
                    const float numerator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedForward * feedForward + (invert ? -2.0f : 2.0f) * feedForward * cosine));
                    const float denominator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedback * feedback + (invert ? 2.0f : -2.0f) * feedback * cosine));
                    const float responseDb = juce::Decibels::gainToDecibels(numerator / denominator, -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.22f, 0.92f);
                    break;
                }
                case 16:
                case 17:
                case 18:
                case 19:
                case 20:
                {
                    float c1 = 0.34f, c2 = 0.58f;
                    switch (modeIndex)
                    {
                        case 16: c1 = 0.42f; c2 = 0.58f; break;
                        case 17: c1 = 0.24f; c2 = 0.70f; break;
                        case 18: c1 = 0.18f; c2 = 0.78f; break;
                        case 19: c1 = 0.30f; c2 = 0.48f; break;
                        case 20: c1 = 0.22f; c2 = 0.40f; break;
                        default: break;
                    }

                    const float sweep = (cutoffLog - 0.5f) * 0.22f;
                    c1 = juce::jlimit(0.05f, 0.92f, c1 + sweep);
                    c2 = juce::jlimit(0.08f, 0.96f, c2 + sweep);
                    const float d1 = (normX - c1) / 0.045f;
                    const float d2 = (normX - c2) / 0.060f;
                    magnitude = 0.16f + std::exp(-d1 * d1 * 1.9f) * 0.50f + std::exp(-d2 * d2 * 1.9f) * 0.44f;
                    break;
                }
                case 21:
                {
                    const float slope = (normX - cutoffLog) * (0.36f + resonanceNorm * 0.34f);
                    magnitude = 0.55f + slope;
                    break;
                }
                case 22:
                {
                    const float d1 = (normX - cutoffLog) / 0.05f;
                    const float d2 = (normX - juce::jlimit(0.0f, 1.0f, cutoffLog * 1.35f)) / 0.07f;
                    magnitude = 0.14f + std::exp(-d1 * d1 * 1.8f) * 0.56f + std::exp(-d2 * d2 * 1.8f) * 0.38f;
                    break;
                }
                case 23:
                {
                    const float base = 1.0f / (1.0f + std::exp((normX - cutoffLog) * 19.0f));
                    const float bump = std::exp(-std::pow((normX - cutoffLog) / 0.055f, 2.0f) * 2.0f) * resonanceNorm * 0.46f;
                    magnitude = base + bump + drive * 0.08f;
                    break;
                }
                case 24:
                {
                    constexpr float visualSampleRate = 48000.0f;
                    const float delaySamples = juce::jmap(cutoffLog, 160.0f, 4.0f);
                    const float feedback = juce::jlimit(0.24f, 0.96f, 0.28f + resonanceNorm * 0.62f);
                    const float omega = juce::MathConstants<float>::twoPi * frequency / visualSampleRate;
                    const float phase = omega * delaySamples;
                    const float numerator = std::sqrt(juce::jmax(0.0001f, 1.0f + 0.9f * 0.9f + 1.8f * std::cos(phase)));
                    const float denominator = std::sqrt(juce::jmax(0.0001f, 1.0f + feedback * feedback - 2.0f * feedback * std::cos(phase)));
                    const float responseDb = juce::Decibels::gainToDecibels(numerator / denominator, -24.0f);
                    magnitude = juce::jmap(responseDb, -18.0f, 18.0f, 0.16f, 0.98f);
                    break;
                }
                case 25:
                    magnitude = 0.50f + std::sin((normX - cutoffLog) * 22.0f) * (0.06f + resonanceNorm * 0.12f);
                    break;
                default:
                    break;
            }

            magnitude = juce::jlimit(0.05f, 1.0f, magnitude + envAmount * 0.08f + drive * 0.05f);
        }

        const float y = preview.getBottom() - magnitude * (preview.getHeight() - 10.0f) - 5.0f;
        const float drawX = preview.getX() + (float) x;
        if (x == 0)
            response.startNewSubPath(drawX, y);
        else
            response.lineTo(drawX, y);
    }

    juce::Path filled(response);
    filled.lineTo(preview.getRight() - 6.0f, preview.getBottom() - 6.0f);
    filled.lineTo(preview.getX() + 6.0f, preview.getBottom() - 6.0f);
    filled.closeSubPath();
    g.setColour((filterEnabled ? accent : juce::Colour(0xff6e7a89)).withAlpha(0.10f));
    g.fillPath(filled);
    g.setColour(filterEnabled ? accent : juce::Colour(0xff6e7a89));
    g.strokePath(response, juce::PathStrokeType(2.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void FilterSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto titleRow = area.removeFromTop(24);
    titleLabel.setBounds(titleRow.removeFromLeft(110));
    filterOnButton.setBounds(titleRow.removeFromRight(62));
    area.removeFromTop(56);
    area.removeFromTop(6);

    auto topRow = area.removeFromTop(38);
    modeLabel.setBounds(topRow.removeFromTop(16));
    modeBox.setBounds(topRow.removeFromTop(22));

    area.removeFromTop(8);
    const int gap = 10;
    const int cellWidth = (area.getWidth() - gap * 3) / 4;
    auto cutoffArea = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto resArea = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto driveArea = area.removeFromLeft(cellWidth);
    area.removeFromLeft(gap);
    auto envArea = area;

    cutoffLabel.setBounds(cutoffArea.removeFromTop(16));
    cutoffSlider.setBounds(cutoffArea);
    resonanceLabel.setBounds(resArea.removeFromTop(16));
    resonanceSlider.setBounds(resArea);
    driveLabel.setBounds(driveArea.removeFromTop(16));
    driveSlider.setBounds(driveArea);
    envLabel.setBounds(envArea.removeFromTop(16));
    envSlider.setBounds(envArea);
}

void FilterSectionComponent::timerCallback()
{
    const bool enabled = filterOnButton.getToggleState();
    modeBox.setEnabled(enabled);
    cutoffSlider.setEnabled(enabled);
    resonanceSlider.setEnabled(enabled);
    driveSlider.setEnabled(enabled);
    envSlider.setEnabled(enabled);
    repaint();
}
