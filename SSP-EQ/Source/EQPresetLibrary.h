#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace sspeq::presets
{
const juce::Array<PluginProcessor::PresetRecord>& getFactoryPresetLibrary();
}
