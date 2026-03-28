#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ModMatrixComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit ModMatrixComponent(PluginProcessor&);
    ~ModMatrixComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class MatrixRow;

    PluginProcessor& processor;
    juce::Label titleLabel;
    juce::Label subLabel;
    juce::Label dragLabel;
    juce::Viewport rowsViewport;
    juce::Component rowsContent;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixComponent)
};
