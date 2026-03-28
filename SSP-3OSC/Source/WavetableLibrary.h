#pragma once

#include <JuceHeader.h>

namespace wavetable
{
constexpr int frameSize = 2048;
constexpr int frameCount = 8;

const juce::StringArray& getTableNames();
float renderSample(int tableIndex, float position, float phase);
}
