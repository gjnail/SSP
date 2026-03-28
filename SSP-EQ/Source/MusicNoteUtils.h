#pragma once

#include <JuceHeader.h>

namespace ssp::notes
{
struct NoteInfo
{
    juce::String name;
    int octave = 4;
    int midiNote = 69;
    double cents = 0.0;
    double frequency = 440.0;
};

NoteInfo frequencyToNote(double frequency, bool preferFlats = false);
juce::String formatFrequencyWithNote(double frequency, bool preferFlats = false);
juce::String formatNoteWithCents(const NoteInfo& noteInfo);
bool tryParseFrequencyInput(const juce::String& text, double& frequencyOut);
juce::StringArray buildNoteSuggestions(const juce::String& query, int maxSuggestions = 8, bool preferFlats = false);
} // namespace ssp::notes
