#pragma once

#include <JuceHeader.h>
#include "FactoryPresetBank.h"

namespace factorypresetloader
{
void applyPreset(juce::AudioProcessorValueTreeState& apvts, const factorypresetbank::PresetSeed& seed);
}
