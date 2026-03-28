#include "ControlsSectionComponent.h"

namespace
{
constexpr int sectionGap = 12;
constexpr int mixHeaderHeight = 18;
constexpr int mixRowHeight = 54;
constexpr int bottomPadding = 14;
constexpr int mixSliderGap = 10;

juce::Colour colourForRole(const juce::String& role)
{
    if (role == "blue")
        return juce::Colour(0xff63b9ff);
    if (role == "green")
        return juce::Colour(0xff74e7b3);
    if (role == "orange")
        return juce::Colour(0xffffb35b);
    if (role == "pink")
        return juce::Colour(0xffff6f92);
    return juce::Colour(0xff9ba8b5);
}
} // namespace

class ControlsSectionComponent::ParameterSlider final : public juce::Component
{
public:
    ParameterSlider(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
                    const juce::String& roleColour)
        : accent(colourForRole(roleColour)), attachment(state, paramId, slider)
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(slider);

        titleLabel.setText(title, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffedf3f8));

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
        slider.setColour(juce::Slider::trackColourId, accent.withAlpha(0.9f));
        slider.setColour(juce::Slider::thumbColourId, accent.brighter(0.15f));
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff202833));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffeef4fa));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d1218));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff27313c));
        slider.setColour(juce::Slider::textBoxHighlightColourId, accent.withAlpha(0.25f));
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        juce::ColourGradient fill(accent.withAlpha(0.08f), area.getTopLeft(), juce::Colour(0xff121820), area.getBottomRight(), false);
        fill.addColour(0.8, accent.withAlpha(0.02f));
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area, 12.0f);

        g.setColour(juce::Colour(0xff26313a));
        g.drawRoundedRectangle(area.reduced(0.5f), 12.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 8);
        titleLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(8);
        slider.setBounds(area.removeFromTop(20));
    }

private:
    juce::Colour accent;
    juce::Label titleLabel;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

ControlsSectionComponent::ControlsSectionComponent(PluginProcessor& processor, juce::AudioProcessorValueTreeState& state)
    : apvts(state),
      triggerControls(processor, state)
{
    addAndMakeVisible(triggerControls);

    auto addSlider = [this](std::unique_ptr<ParameterSlider>& slider, const juce::String& paramId, const juce::String& title,
                            const juce::String& roleColour)
    {
        slider = std::make_unique<ParameterSlider>(apvts, paramId, title, roleColour);
        addAndMakeVisible(*slider);
    };

    addSlider(lowMixSlider, "dryWetBandLow", "Low", "blue");
    addSlider(midMixSlider, "dryWetBandMid", "Mid", "green");
    addSlider(highMixSlider, "dryWetBandHigh", "High", "orange");
    addSlider(masterMixSlider, "dryWetOverall", "Master", "pink");
}

ControlsSectionComponent::~ControlsSectionComponent() = default;

void ControlsSectionComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient shell(juce::Colour(0xff151b22), bounds.getTopLeft(), juce::Colour(0xff0f1318), bounds.getBottomRight(), false);
    shell.addColour(0.55, juce::Colour(0xff121a23));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(bounds, 18.0f);

    g.setColour(juce::Colour(0xff28323d));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 18.0f, 1.0f);

    auto top = bounds.reduced(14.0f, 12.0f);
    top.removeFromTop(86.0f + (float)sectionGap);
    auto mixHeader = top.removeFromTop((float)mixHeaderHeight);

    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("Band Mix", mixHeader.removeFromLeft(140.0f), juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8693a0));
    g.setFont(juce::Font(10.5f));
    g.drawText("Balance the processed bands and final blend.", mixHeader, juce::Justification::centredRight);
}

void ControlsSectionComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);

    triggerControls.setBounds(area.removeFromTop(86));
    area.removeFromTop(sectionGap);
    area.removeFromTop(mixHeaderHeight);
    area.removeFromBottom(bottomPadding);

    const int count = 4;
    const int sliderWidth = (area.getWidth() - mixSliderGap * (count - 1)) / count;
    int x = area.getX();

    for (auto* slider : {lowMixSlider.get(), midMixSlider.get(), highMixSlider.get(), masterMixSlider.get()})
    {
        slider->setBounds(x, area.getY(), sliderWidth, mixRowHeight);
        x += sliderWidth + mixSliderGap;
    }
}
