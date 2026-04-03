#pragma once

#include "PluginProcessor.h"

namespace sspecho::presets
{
const juce::Array<PluginProcessor::PresetRecord>& getFactoryPresetLibrary();
}
