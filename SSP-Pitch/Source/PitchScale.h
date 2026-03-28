#pragma once

#include <JuceHeader.h>

namespace ssp::pitch
{
enum class SnapMode
{
    chromatic = 0,
    scale,
    free
};

const juce::StringArray& getScaleKeyNames();
const juce::StringArray& getScaleModeNames();
const juce::StringArray& getSnapModeNames();
const juce::Array<int>& getScaleIntervals(const juce::String& modeName);
bool isMidiNoteInScale(int midiNote, int keyRoot, const juce::String& modeName);
float snapMidiNote(float midiNote, int keyRoot, const juce::String& modeName, SnapMode snapMode);
juce::String formatMidiNote(float midiNote, bool includeCents = true);
} // namespace ssp::pitch
