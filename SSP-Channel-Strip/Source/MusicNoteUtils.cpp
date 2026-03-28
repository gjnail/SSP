#include "MusicNoteUtils.h"

namespace
{
constexpr const char* sharpNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr const char* flatNames[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

int noteNameToSemitone(const juce::String& name)
{
    const juce::String upper = name.toUpperCase();
    if (upper == "C") return 0;
    if (upper == "C#" || upper == "DB") return 1;
    if (upper == "D") return 2;
    if (upper == "D#" || upper == "EB") return 3;
    if (upper == "E") return 4;
    if (upper == "F") return 5;
    if (upper == "F#" || upper == "GB") return 6;
    if (upper == "G") return 7;
    if (upper == "G#" || upper == "AB") return 8;
    if (upper == "A") return 9;
    if (upper == "A#" || upper == "BB") return 10;
    if (upper == "B") return 11;
    return -1;
}
} // namespace

namespace ssp::notes
{
NoteInfo frequencyToNote(double frequency, bool preferFlats)
{
    const double clampedFrequency = juce::jmax(1.0e-4, frequency);
    const double noteNumber = 12.0 * std::log2(clampedFrequency / 440.0) + 69.0;
    const int roundedNote = juce::roundToInt(noteNumber);
    const int noteIndex = ((roundedNote % 12) + 12) % 12;

    NoteInfo noteInfo;
    noteInfo.frequency = clampedFrequency;
    noteInfo.midiNote = roundedNote;
    noteInfo.octave = (roundedNote / 12) - 1;
    noteInfo.cents = (noteNumber - (double) roundedNote) * 100.0;
    noteInfo.name = preferFlats ? flatNames[noteIndex] : sharpNames[noteIndex];
    return noteInfo;
}

juce::String formatNoteWithCents(const NoteInfo& noteInfo)
{
    juce::String result = noteInfo.name + juce::String(noteInfo.octave);
    const int roundedCents = juce::roundToInt(noteInfo.cents);
    if (std::abs(roundedCents) > 0)
        result << " " << (roundedCents > 0 ? "+" : "") << roundedCents << " cents";
    return result;
}

juce::String formatFrequencyWithNote(double frequency, bool preferFlats)
{
    const auto noteInfo = frequencyToNote(frequency, preferFlats);
    return juce::String(frequency, frequency >= 1000.0 ? 2 : 1) + " Hz — " + formatNoteWithCents(noteInfo);
}

bool tryParseFrequencyInput(const juce::String& text, double& frequencyOut)
{
    const juce::String trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    const double numericValue = trimmed.getDoubleValue();
    if (numericValue > 0.0)
    {
        frequencyOut = numericValue;
        return true;
    }

    juce::String notePart;
    juce::String octavePart;

    for (juce::juce_wchar character : trimmed)
    {
        if (juce::CharacterFunctions::isDigit(character) || character == '-')
            octavePart << juce::String::charToString(character);
        else if (character != ' ')
            notePart << juce::String::charToString(character);
    }

    if (notePart.isEmpty() || octavePart.isEmpty())
        return false;

    const int semitone = noteNameToSemitone(notePart);
    if (semitone < 0)
        return false;

    const int octave = octavePart.getIntValue();
    const int midiNote = (octave + 1) * 12 + semitone;
    frequencyOut = 440.0 * std::pow(2.0, ((double) midiNote - 69.0) / 12.0);
    return std::isfinite(frequencyOut) && frequencyOut > 0.0;
}

juce::StringArray buildNoteSuggestions(const juce::String& query, int maxSuggestions, bool preferFlats)
{
    juce::StringArray suggestions;
    const juce::String upperQuery = query.trim().toUpperCase();

    for (int octave = 0; octave <= 8; ++octave)
    {
        for (int noteIndex = 0; noteIndex < 12; ++noteIndex)
        {
            const juce::String noteName = preferFlats ? flatNames[noteIndex] : sharpNames[noteIndex];
            const juce::String noteToken = noteName + juce::String(octave);

            if (!upperQuery.isEmpty() && !noteToken.toUpperCase().startsWith(upperQuery))
                continue;

            const int midiNote = (octave + 1) * 12 + noteIndex;
            const double frequency = 440.0 * std::pow(2.0, ((double) midiNote - 69.0) / 12.0);
            suggestions.add(noteToken + " (" + juce::String(frequency, frequency >= 1000.0 ? 2 : 1) + " Hz)");

            if (suggestions.size() >= maxSuggestions)
                return suggestions;
        }
    }

    return suggestions;
}
} // namespace ssp::notes
