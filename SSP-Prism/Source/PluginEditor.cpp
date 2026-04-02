#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1280;
constexpr int editorHeight = 860;
const juce::Colour coralAccent(0xffff8f88);
const juce::Colour lilacAccent(0xffb38fff);

std::unique_ptr<juce::Drawable> createDrawableFromSvg(const char* svgText)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(svgText));
    return xml != nullptr ? juce::Drawable::createFromSVG(*xml) : nullptr;
}

int getChoiceIndex(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return juce::roundToInt(raw->load());

    return 0;
}

float noteNumberToHz(float noteNumber)
{
    return 440.0f * std::pow(2.0f, (noteNumber - 69.0f) / 12.0f);
}

struct ChordDefinition
{
    std::array<int, 5> intervals;
    int count = 3;
};

const std::array<ChordDefinition, 8>& getChordDefinitions()
{
    static const std::array<ChordDefinition, 8> chords {{
        { { 0, 4, 7, 0, 0 }, 3 },
        { { 0, 3, 7, 0, 0 }, 3 },
        { { 0, 2, 7, 0, 0 }, 3 },
        { { 0, 5, 7, 0, 0 }, 3 },
        { { 0, 4, 7, 11, 0 }, 4 },
        { { 0, 3, 7, 10, 0 }, 4 },
        { { 0, 7, 12, 0, 0 }, 3 },
        { { 0, 2, 4, 7, 9 }, 5 }
    }};

    return chords;
}

float mapFrequencyToX(float frequency, juce::Rectangle<float> bounds)
{
    const float minLog = std::log(40.0f);
    const float maxLog = std::log(3000.0f);
    const float norm = (std::log(juce::jlimit(40.0f, 3000.0f, frequency)) - minLog) / (maxLog - minLog);
    return bounds.getX() + norm * bounds.getWidth();
}
} // namespace

class PluginEditor::MiniKnob final : public juce::Component
{
public:
    MiniKnob(juce::AudioProcessorValueTreeState& state,
             const juce::String& parameterID,
             const juce::String& labelText,
             juce::Colour accent,
             std::function<juce::String(double)> formatterToUse)
        : attachment(state, parameterID, slider),
          formatter(std::move(formatterToUse))
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);

        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setMouseDragSensitivity(180);

        if (auto* parameter = state.getParameter(parameterID))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider.setDoubleClickReturnValue(true, ranged->convertFrom0to1(ranged->getDefaultValue()));

        slider.onValueChange = [this] { refreshValue(); };

        label.setText(labelText, juce::dontSendNotification);
        label.setFont(reverbui::smallCapsFont(10.5f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, reverbui::textStrong());
        label.setInterceptsMouseClicks(false, false);

        valueLabel.setFont(reverbui::bodyFont(9.0f));
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setColour(juce::Label::textColourId, accent.brighter(0.14f));
        valueLabel.setInterceptsMouseClicks(false, false);

        refreshValue();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(34);
        label.setBounds(footer.removeFromTop(16));
        valueLabel.setBounds(footer);
        slider.setBounds(area);
    }

private:
    void refreshValue()
    {
        valueLabel.setText(formatter(slider.getValue()), juce::dontSendNotification);
    }

    reverbui::SSPKnob slider;
    juce::Label label;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    std::function<juce::String(double)> formatter;
};

class PluginEditor::HeroDisplay final : public juce::Component,
                                        private juce::Timer
{
public:
    explicit HeroDisplay(PluginProcessor& processorToUse)
        : pluginProcessor(processorToUse)
    {
        crystalDrawable = createDrawableFromSvg(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 420">
  <defs>
    <radialGradient id="coreGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#ffffff" stop-opacity="1.0"/>
      <stop offset="42%" stop-color="#ffe2f6" stop-opacity="0.95"/>
      <stop offset="100%" stop-color="#ffe2f6" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="pinkFacet" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#ffd0d1"/>
      <stop offset="35%" stop-color="#ff9fa8"/>
      <stop offset="100%" stop-color="#8d5fff"/>
    </linearGradient>
    <linearGradient id="violetFacet" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#f8d8ff"/>
      <stop offset="45%" stop-color="#b58cff"/>
      <stop offset="100%" stop-color="#6b58ff"/>
    </linearGradient>
    <linearGradient id="amberFacet" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#fff0d1"/>
      <stop offset="40%" stop-color="#ffcf9c"/>
      <stop offset="100%" stop-color="#8f66ff"/>
    </linearGradient>
  </defs>
  <circle cx="210" cy="210" r="110" fill="url(#coreGlow)"/>
  <g opacity="0.96">
    <path d="M208 38 L258 118 L214 150 L168 111 Z" fill="url(#violetFacet)"/>
    <path d="M129 95 L204 139 L182 188 L108 160 Z" fill="url(#pinkFacet)"/>
    <path d="M74 181 L162 185 L176 240 L100 250 Z" fill="url(#amberFacet)"/>
    <path d="M106 272 L178 239 L208 286 L152 343 Z" fill="url(#violetFacet)"/>
    <path d="M208 274 L262 240 L318 294 L214 354 Z" fill="url(#pinkFacet)"/>
    <path d="M242 185 L336 188 L348 252 L260 238 Z" fill="url(#violetFacet)"/>
    <path d="M214 146 L310 103 L340 156 L246 187 Z" fill="url(#amberFacet)"/>
    <path d="M190 149 L214 150 L208 286 L178 239 Z" fill="#ffffff" fill-opacity="0.18"/>
    <path d="M214 150 L242 185 L208 286 L208 150 Z" fill="#ffffff" fill-opacity="0.22"/>
  </g>
  <circle cx="210" cy="210" r="18" fill="#ffffff"/>
</svg>
)svg");

        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reverbui::drawPanelBackground(g, bounds, coralAccent, 15.0f);

        auto display = bounds.reduced(26.0f, 18.0f);
        auto midY = display.getCentreY();

        juce::ColourGradient floorGlow(juce::Colour(0x18ff8f88), display.getCentreX(), display.getBottom(),
                                       juce::Colour(0x00b38fff), display.getCentreX(), display.getY(), false);
        g.setGradientFill(floorGlow);
        g.fillEllipse(display.withTrimmedTop(display.getHeight() * 0.35f));

        g.setColour(reverbui::outlineSoft().withAlpha(0.55f));
        for (int i = 0; i < 5; ++i)
        {
            const float y = juce::jmap((float) i, 0.0f, 4.0f, display.getY() + 20.0f, display.getBottom() - 20.0f);
            g.drawHorizontalLine((int) y, display.getX(), display.getRight());
        }

        const float inputLevel = pluginProcessor.getInputMeterLevel();
        const float resonatorLevel = pluginProcessor.getResonatorMeterLevel();
        const float outputLevel = pluginProcessor.getOutputMeterLevel();
        const float largeAmplitude = 8.0f + resonatorLevel * 22.0f;
        const float fineAmplitude = 3.0f + inputLevel * 8.0f;

        juce::Path wave;
        for (int i = 0; i <= (int) display.getWidth(); ++i)
        {
            const float x = display.getX() + (float) i;
            const float norm = (x - display.getX()) / display.getWidth();
            const float y = midY
                            + std::sin(animationPhase + norm * 8.0f) * largeAmplitude
                            + std::sin(animationPhase * 0.62f + norm * 28.0f) * fineAmplitude;

            if (i == 0)
                wave.startNewSubPath(x, y);
            else
                wave.lineTo(x, y);
        }

        juce::ColourGradient waveGlow(coralAccent.withAlpha(0.98f), { display.getX(), midY },
                                      lilacAccent.withAlpha(0.88f), { display.getRight(), midY }, false);
        g.setGradientFill(waveGlow);
        g.strokePath(wave, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xfffff4fd).withAlpha(0.78f + resonatorLevel * 0.2f));
        g.drawLine(display.getX(), midY, display.getRight(), midY, 1.6f);

        const auto& apvts = pluginProcessor.apvts;
        const int chordIndex = juce::jlimit(0, (int) getChordDefinitions().size() - 1, getChoiceIndex(apvts, "chord"));
        const int rootIndex = juce::jlimit(0, 11, getChoiceIndex(apvts, "rootNote"));
        const int octaveIndex = juce::jlimit(0, 4, getChoiceIndex(apvts, "octave"));
        const int spreadIndex = juce::jlimit(0, 3, getChoiceIndex(apvts, "spread"));
        const auto& chord = getChordDefinitions()[(size_t) chordIndex];
        const float spreadSemitones = (float) (spreadIndex * 12);

        for (int i = 0; i < chord.count; ++i)
        {
            const float spreadOffset = chord.count > 1 ? spreadSemitones * ((float) i / (float) (chord.count - 1)) : 0.0f;
            const float noteNumber = 36.0f + (float) octaveIndex * 12.0f + (float) rootIndex + (float) chord.intervals[(size_t) i] + spreadOffset;
            const float x = mapFrequencyToX(noteNumberToHz(noteNumber), display);
            auto marker = juce::Rectangle<float>(12.0f, 12.0f).withCentre({ x, midY });
            g.setColour(i == 0 ? coralAccent : lilacAccent);
            g.fillEllipse(marker);
            g.setColour(juce::Colours::white.withAlpha(0.28f));
            g.drawEllipse(marker, 1.0f);
        }

        auto crystalBounds = juce::Rectangle<float>(220.0f, 220.0f).withCentre(display.getCentre());
        juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(0.94f + resonatorLevel * 0.10f);
        if (crystalDrawable != nullptr)
            crystalDrawable->drawWithin(g, crystalBounds, juce::RectanglePlacement::stretchToFit, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.06f + outputLevel * 0.18f));
        g.fillEllipse(crystalBounds.expanded(24.0f));

        auto footer = getLocalBounds().reduced(24, 10).removeFromBottom(22);
        g.setFont(reverbui::smallCapsFont(10.2f));
        g.setColour(reverbui::textMuted());
        g.drawText("INPUT", footer.removeFromLeft(80), juce::Justification::centredLeft, false);
        g.setColour(coralAccent.brighter(0.14f));
        g.drawText("COLOR", footer.removeFromLeft(80), juce::Justification::centredLeft, false);
        g.setColour(lilacAccent.brighter(0.14f));
        g.drawText("OUT", footer.removeFromLeft(60), juce::Justification::centredLeft, false);
    }

private:
    void timerCallback() override
    {
        animationPhase = std::fmod(animationPhase + 0.055f + pluginProcessor.getOutputMeterLevel() * 0.08f,
                                   juce::MathConstants<float>::twoPi);
        repaint();
    }

    PluginProcessor& pluginProcessor;
    std::unique_ptr<juce::Drawable> crystalDrawable;
    float animationPhase = 0.0f;
};

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      pluginProcessor(p),
      controls(p.apvts)
{
    titleLabel.setText("SSP Prism", juce::dontSendNotification);
    titleLabel.setFont(reverbui::titleFont(24.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, reverbui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Iridescent harmonic resonator inspired by chord-tuned color, warped overtones, animated motion, and focused filtering.",
                      juce::dontSendNotification);
    hintLabel.setFont(reverbui::bodyFont(9.8f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, reverbui::textMuted());
    addAndMakeVisible(hintLabel);

    heroDisplay = std::make_unique<HeroDisplay>(pluginProcessor);
    addAndMakeVisible(*heroDisplay);

    mixKnob = std::make_unique<MiniKnob>(pluginProcessor.apvts, "mix", "Mix", coralAccent,
                                         [](double value) { return juce::String(juce::roundToInt((float) value * 100.0f)) + "%"; });
    outputKnob = std::make_unique<MiniKnob>(pluginProcessor.apvts, "outputDb", "Output", lilacAccent,
                                            [](double value)
                                            {
                                                if (std::abs(value) < 0.05)
                                                    return juce::String("0.0 dB");

                                                return juce::String(value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
                                            });

    addAndMakeVisible(*mixKnob);
    addAndMakeVisible(*outputKnob);
    addAndMakeVisible(controls);

    setSize(editorWidth, editorHeight);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint(juce::Graphics& g)
{
    reverbui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(2.0f));
}

void PluginEditor::resized()
{
    if (mixKnob == nullptr || outputKnob == nullptr || heroDisplay == nullptr)
        return;

    auto area = getLocalBounds().reduced(20, 18);

    auto header = area.removeFromTop(72);
    auto utility = header.removeFromRight(190);
    auto titleArea = header;
    titleLabel.setBounds(titleArea.removeFromTop(30));
    hintLabel.setBounds(titleArea.removeFromTop(18));

    auto knobWidth = utility.getWidth() / 2;
    mixKnob->setBounds(utility.removeFromLeft(knobWidth).reduced(4, 0));
    outputKnob->setBounds(utility.reduced(4, 0));

    heroDisplay->setBounds(area.removeFromTop(322));
    area.removeFromTop(14);
    controls.setBounds(area);
}
