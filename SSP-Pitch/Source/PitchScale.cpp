#include "PitchScale.h"

namespace
{
using IntervalArray = juce::Array<int>;

const juce::StringArray scaleKeys{
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

const juce::StringArray scaleModes{
    "Major", "Minor", "Dorian", "Mixolydian", "Pentatonic", "Blues",
    "Harmonic Minor", "Melodic Minor", "Chromatic", "Custom"
};

const juce::StringArray snapModes{
    "Chromatic", "Scale", "Free"
};

IntervalArray makeIntervals(std::initializer_list<int> values)
{
    IntervalArray intervals;
    for (const auto value : values)
        intervals.add(value);
    return intervals;
}

const std::map<juce::String, IntervalArray>& intervalMap()
{
    static const std::map<juce::String, IntervalArray> intervals{
        {"Major", makeIntervals({0, 2, 4, 5, 7, 9, 11})},
        {"Minor", makeIntervals({0, 2, 3, 5, 7, 8, 10})},
        {"Dorian", makeIntervals({0, 2, 3, 5, 7, 9, 10})},
        {"Mixolydian", makeIntervals({0, 2, 4, 5, 7, 9, 10})},
        {"Pentatonic", makeIntervals({0, 3, 5, 7, 10})},
        {"Blues", makeIntervals({0, 3, 5, 6, 7, 10})},
        {"Harmonic Minor", makeIntervals({0, 2, 3, 5, 7, 8, 11})},
        {"Melodic Minor", makeIntervals({0, 2, 3, 5, 7, 9, 11})},
        {"Chromatic", makeIntervals({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11})},
        {"Custom", makeIntervals({0, 2, 4, 7, 9})},
    };
    return intervals;
}
} // namespace

namespace ssp::pitch
{
const juce::StringArray& getScaleKeyNames()
{
    return scaleKeys;
}

const juce::StringArray& getScaleModeNames()
{
    return scaleModes;
}

const juce::StringArray& getSnapModeNames()
{
    return snapModes;
}

const juce::Array<int>& getScaleIntervals(const juce::String& modeName)
{
    const auto mapIt = intervalMap().find(modeName);
    if (mapIt != intervalMap().end())
        return mapIt->second;

    return intervalMap().find("Major")->second;
}

bool isMidiNoteInScale(int midiNote, int keyRoot, const juce::String& modeName)
{
    const int wrapped = (midiNote - keyRoot) % 12;
    const int degree = (wrapped + 12) % 12;
    return getScaleIntervals(modeName).contains(degree);
}

float snapMidiNote(float midiNote, int keyRoot, const juce::String& modeName, SnapMode snapMode)
{
    if (snapMode == SnapMode::free)
        return midiNote;

    if (snapMode == SnapMode::chromatic)
        return std::round(midiNote);

    const auto& intervals = getScaleIntervals(modeName);
    if (intervals.isEmpty())
        return std::round(midiNote);

    float bestNote = midiNote;
    float bestDistance = std::numeric_limits<float>::max();

    for (int octave = -2; octave <= 10; ++octave)
    {
        for (auto interval : intervals)
        {
            const float candidate = (float) (keyRoot + interval + octave * 12);
            const float distance = std::abs(candidate - midiNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestNote = candidate;
            }
        }
    }

    return bestNote;
}

juce::String formatMidiNote(float midiNote, bool includeCents)
{
    const int rounded = juce::roundToInt(midiNote);
    const int noteIndex = (rounded % 12 + 12) % 12;
    const int octave = (rounded / 12) - 1;
    const int cents = juce::roundToInt((midiNote - (float) rounded) * 100.0f);
    juce::String label = scaleKeys[noteIndex] + juce::String(octave);

    if (includeCents && cents != 0)
        label << " " << (cents > 0 ? "+" : "") << cents << "c";

    return label;
}
} // namespace ssp::pitch
