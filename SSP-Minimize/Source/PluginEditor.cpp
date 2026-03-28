#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 980;
constexpr int editorHeight = 660;

juce::Path createBrandGlyph(juce::Rectangle<float> area)
{
    juce::Path glyph;
    auto left = area.removeFromLeft(area.getWidth() * 0.36f);
    auto mid = area.removeFromLeft(area.getWidth() * 0.34f);
    auto right = area;

    auto makeS = [](juce::Rectangle<float> box)
    {
        juce::Path p;
        auto top = box.removeFromTop(box.getHeight() * 0.46f);
        auto bottom = box;
        p.addRoundedRectangle(top.withTrimmedRight(top.getWidth() * 0.10f), 8.0f);
        p.addRoundedRectangle(bottom.withTrimmedLeft(bottom.getWidth() * 0.10f), 8.0f);
        p.addRectangle(juce::Rectangle<float>(box.getCentreX() - box.getWidth() * 0.16f, top.getBottom() - 6.0f,
                                              box.getWidth() * 0.32f, bottom.getY() - top.getBottom() + 12.0f));
        return p;
    };

    glyph.addPath(makeS(left.reduced(2.0f)));
    glyph.addPath(makeS(mid.reduced(2.0f)));

    juce::Path pLetter;
    auto stem = right.reduced(4.0f);
    pLetter.addRoundedRectangle(stem.removeFromLeft(stem.getWidth() * 0.34f), 8.0f);
    auto bowl = stem.removeFromTop(stem.getHeight() * 0.52f);
    pLetter.addRoundedRectangle(bowl.withTrimmedLeft(bowl.getWidth() * 0.10f), 9.0f);
    glyph.addPath(pLetter);

    return glyph;
}
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      response(p),
      controls(p.apvts)
{
    setLookAndFeel(&lookAndFeel);
    setSize(editorWidth, editorHeight);

    titleLabel.setText("SSP Minimize", juce::dontSendNotification);
    titleLabel.setFont(minimizeui::headerFont(33.0f));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, minimizeui::textStrong());
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Adaptive resonance control for vocals, synths, guitars, and harsh buses.", juce::dontSendNotification);
    hintLabel.setFont(minimizeui::bodyFont(13.0f));
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, minimizeui::textMuted());
    addAndMakeVisible(hintLabel);

    addAndMakeVisible(response);
    addAndMakeVisible(controls);
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    minimizeui::drawEditorBackdrop(g, bounds);

    auto content = bounds.reduced(18.0f);
    auto header = content.removeFromTop(96.0f);

    auto logoArea = header.removeFromLeft(86.0f).reduced(6.0f);
    juce::ColourGradient logoFill(juce::Colour(0xff1c242d), logoArea.getTopLeft(),
                                  juce::Colour(0xff0f1318), logoArea.getBottomLeft(), false);
    g.setGradientFill(logoFill);
    g.fillRoundedRectangle(logoArea, 20.0f);
    g.setColour(minimizeui::outline());
    g.drawRoundedRectangle(logoArea, 20.0f, 1.5f);

    auto glyphArea = logoArea.reduced(18.0f);
    g.setColour(minimizeui::brandCream());
    g.fillPath(createBrandGlyph(glyphArea));

    auto textBlock = header.removeFromLeft(520.0f).reduced(12.0f, 4.0f);
    auto eyebrow = textBlock.removeFromTop(18.0f);
    g.setColour(minimizeui::brandGold());
    g.setFont(minimizeui::bodyFont(12.0f));
    g.drawText("DYNAMIC RESONANCE SUPPRESSION", eyebrow.toNearestInt(), juce::Justification::centredLeft, false);

    auto statusBlock = header.reduced(18.0f, 8.0f);
    auto statusPanel = statusBlock.removeFromRight(248.0f);
    juce::ColourGradient statusFill(juce::Colour(0xfffffbf3), statusPanel.getTopLeft(),
                                    juce::Colour(0xffede0cb), statusPanel.getBottomLeft(), false);
    g.setGradientFill(statusFill);
    g.fillRoundedRectangle(statusPanel, 22.0f);
    g.setColour(minimizeui::outline());
    g.drawRoundedRectangle(statusPanel, 22.0f, 1.3f);

    auto leftTag = statusPanel.reduced(18.0f, 18.0f).removeFromLeft(96.0f);
    auto rightText = statusPanel.reduced(18.0f, 18.0f);
    rightText.removeFromLeft(108.0f);

    g.setColour(minimizeui::brandCyan().withAlpha(0.18f));
    g.fillRoundedRectangle(leftTag, 14.0f);
    g.setColour(minimizeui::brandCyan());
    g.setFont(minimizeui::sectionFont(15.0f));
    g.drawText("MIN", leftTag.toNearestInt(), juce::Justification::centred, false);

    g.setColour(minimizeui::textMuted());
    g.setFont(minimizeui::bodyFont(12.0f));
    g.drawText("Suppresses harsh build-up without flattening the source.", rightText.toNearestInt(),
               juce::Justification::centredLeft, true);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(30, 30);
    auto header = area.removeFromTop(86);

    auto titleArea = header.removeFromLeft(600).reduced(112, 0);
    titleArea.removeFromTop(18);
    titleLabel.setBounds(titleArea.removeFromTop(38));
    hintLabel.setBounds(titleArea.removeFromTop(20));

    area.removeFromTop(10);
    response.setBounds(area.removeFromTop(270));
    area.removeFromTop(16);
    controls.setBounds(area);
}
