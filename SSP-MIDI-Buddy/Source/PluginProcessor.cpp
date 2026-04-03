#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <limits>
#include <map>
#include <numeric>

namespace
{
enum StyleFeel
{
    houseFeel = 0,
    deepHouseFeel,
    technoFeel,
    tranceFeel,
    psytranceFeel,
    dubstepFeel,
    raveFeel,
    melodicFeel,
    futureBassFeel
};

struct RhythmHit
{
    double position = 0.0;
    double length = 0.25;
};

float readParameterValue(const juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& parameterID,
                         float fallback = 0.0f)
{
    if (const auto* value = apvts.getRawParameterValue(parameterID))
        return value->load();

    return fallback;
}

const std::vector<std::vector<int>>& getStyleTemplates(int styleIndex)
{
    static const std::vector<std::vector<std::vector<int>>> templates{
        { { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 } }, // House
        { { 0, 5, 3, 4 }, { 5, 4, 0, 3 }, { 0, 3, 5, 4 } }, // Deep House
        { { 0, 5, 3, 5 }, { 0, 6, 5, 4 }, { 0, 3, 6, 4 } }, // Techno
        { { 0, 4, 5, 3 }, { 5, 3, 0, 4 }, { 0, 5, 3, 4 } }, // Trance
        { { 0, 6, 5, 4 }, { 0, 5, 6, 4 }, { 0, 6, 4, 5 } }, // Psytrance
        { { 0, 6, 5, 4 }, { 0, 5, 6, 3 }, { 0, 3, 6, 4 } }, // Dubstep
        { { 0, 3, 5, 4 }, { 0, 5, 3, 6 }, { 0, 4, 5, 3 } }, // Rave
        { { 5, 0, 3, 4 }, { 0, 5, 3, 4 }, { 3, 0, 5, 4 } }, // Melodic
        { { 5, 3, 0, 4 }, { 0, 3, 5, 4 }, { 5, 4, 0, 3 } }  // Future Bass
    };

    return templates[(size_t) juce::jlimit(0, (int) templates.size() - 1, styleIndex)];
}

std::vector<int> buildCanonicalDegreeSequence(juce::Random& random, int styleIndex)
{
    const auto& templatePool = getStyleTemplates(styleIndex);
    const auto& first = templatePool[(size_t) random.nextInt((int) templatePool.size())];
    const auto& second = templatePool[(size_t) random.nextInt((int) templatePool.size())];

    std::vector<int> degrees;
    degrees.reserve(8);

    for (int index = 0; index < 4; ++index)
        degrees.push_back(first[(size_t) (index % (int) first.size())]);

    for (int index = 0; index < 4; ++index)
        degrees.push_back(second[(size_t) (index % (int) second.size())]);

    degrees.back() = 0;
    return degrees;
}

bool matchesIntervals(const std::vector<int>& actual, std::initializer_list<int> expected)
{
    return actual.size() == expected.size()
           && std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
}
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("generationMode", "Generator", getGenerationModeNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("keyRoot", "Key", getKeyNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("scaleMode", "Scale", getScaleNames(), 16));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("style", "Feel", getStyleNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("chordCount", "Chord Count", getChordCountNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("chordDuration", "Chord Length", getChordDurationNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("playbackMode", "Playback", getPlaybackModeNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("arpRate", "Arp Rate", getArpRateNames(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("arpOctaves", "Arp Octaves", getArpOctaveNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("register", "Register", getRegisterNames(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("melodyRange", "Melody Range", getMelodyRangeNames(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("spread", "Spread", getSpreadNames(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("velocity", "Velocity",
                                                                     juce::NormalisableRange<float>(60.0f, 120.0f, 1.0f),
                                                                     96.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate",
                                                                     juce::NormalisableRange<float>(0.45f, 0.98f, 0.01f),
                                                                     0.90f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("strum", "Strum",
                                                                     juce::NormalisableRange<float>(0.0f, 120.0f, 1.0f),
                                                                     0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing",
                                                                     juce::NormalisableRange<float>(0.50f, 0.75f, 0.01f),
                                                                     0.50f));

    return { parameters.begin(), parameters.end() };
}

const juce::StringArray& PluginProcessor::getKeyNames()
{
    static const juce::StringArray names{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

const std::vector<PluginProcessor::ScaleDefinition>& PluginProcessor::getScaleDefinitions()
{
    static const std::vector<ScaleDefinition> definitions{
        { "8-Tone Spanish",           { 0, 1, 3, 4, 5, 6, 8, 10 } },
        { "Bhairav",                  { 0, 1, 4, 5, 7, 8, 11 } },
        { "Dorian",                   { 0, 2, 3, 5, 7, 9, 10 } },
        { "Dorian #4",                { 0, 2, 3, 6, 7, 9, 10 } },
        { "Half-Whole Diminished",    { 0, 1, 3, 4, 6, 7, 9, 10 } },
        { "Harmonic Major",           { 0, 2, 4, 5, 7, 8, 11 } },
        { "Harmonic Minor",           { 0, 2, 3, 5, 7, 8, 11 } },
        { "Hirajoshi",                { 0, 2, 3, 7, 8 } },
        { "Hungarian Minor",          { 0, 2, 3, 6, 7, 8, 11 } },
        { "In-Sen",                   { 0, 1, 5, 7, 10 } },
        { "Iwato",                    { 0, 1, 5, 6, 10 } },
        { "Kumoi",                    { 0, 2, 3, 7, 9 } },
        { "Locrian",                  { 0, 1, 3, 5, 6, 8, 10 } },
        { "Lydian",                   { 0, 2, 4, 6, 7, 9, 11 } },
        { "Lydian Augmented",         { 0, 2, 4, 6, 8, 9, 11 } },
        { "Lydian Dominant",          { 0, 2, 4, 6, 7, 9, 10 } },
        { "Major",                    { 0, 2, 4, 5, 7, 9, 11 } },
        { "Major Pentatonic",         { 0, 2, 4, 7, 9 } },
        { "Melodic Minor Ascending",  { 0, 2, 3, 5, 7, 9, 11 } },
        { "Melodic Minor Descending", { 0, 2, 3, 5, 7, 8, 10 } },
        { "Messiaen 3",               { 0, 2, 3, 4, 6, 7, 8, 10, 11 } },
        { "Messiaen 4",               { 0, 1, 2, 5, 6, 7, 8, 11 } },
        { "Messiaen 5",               { 0, 1, 5, 6, 7, 11 } },
        { "Messiaen 6",               { 0, 2, 4, 5, 6, 8, 10, 11 } },
        { "Messiaen 7",               { 0, 1, 2, 3, 4, 6, 7, 8, 9, 10 } },
        { "Minor",                    { 0, 2, 3, 5, 7, 8, 10 } },
        { "Minor Blues",              { 0, 3, 5, 6, 7, 10 } },
        { "Minor Pentatonic",         { 0, 3, 5, 7, 10 } },
        { "Mixolydian",               { 0, 2, 4, 5, 7, 9, 10 } },
        { "Pelog Selisir",            { 0, 1, 3, 7, 8 } },
        { "Pelog Tembung",            { 0, 1, 5, 7, 8 } },
        { "Phrygian",                 { 0, 1, 3, 5, 7, 8, 10 } },
        { "Phrygian Dominant",        { 0, 1, 4, 5, 7, 8, 10 } },
        { "Super Locrian",            { 0, 1, 3, 4, 6, 8, 10 } },
        { "Whole Tone",               { 0, 2, 4, 6, 8, 10 } },
        { "Whole-Half Diminished",    { 0, 2, 3, 5, 6, 8, 9, 11 } }
    };

    return definitions;
}

const juce::StringArray& PluginProcessor::getScaleNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray array;
        for (const auto& definition : getScaleDefinitions())
            array.add(definition.name);
        return array;
    }();

    return names;
}

const juce::StringArray& PluginProcessor::getGenerationModeNames()
{
    static const juce::StringArray names{ "Chords", "Melody", "Bassline", "Motif" };
    return names;
}

const juce::StringArray& PluginProcessor::getStyleNames()
{
    static const juce::StringArray names{
        "House",
        "Deep House",
        "Techno",
        "Trance",
        "Psytrance",
        "Dubstep",
        "Rave",
        "Melodic",
        "Future Bass"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getChordFeelNames()
{
    static const juce::StringArray names{
        "Warm",
        "Soulful",
        "Tense",
        "Euphoric",
        "Hypnotic",
        "Dark",
        "Urgent",
        "Dreamy",
        "Huge"
    };
    return names;
}

const juce::StringArray& PluginProcessor::getChordCountNames()
{
    static const juce::StringArray names{ "4", "6", "8" };
    return names;
}

const juce::StringArray& PluginProcessor::getChordDurationNames()
{
    static const juce::StringArray names{ "1 Bar", "1/2 Bar", "1 Beat" };
    return names;
}

const juce::StringArray& PluginProcessor::getRegisterNames()
{
    static const juce::StringArray names{ "Low", "Mid", "High" };
    return names;
}

const juce::StringArray& PluginProcessor::getSpreadNames()
{
    static const juce::StringArray names{ "Tight", "Balanced", "Wide" };
    return names;
}

const juce::StringArray& PluginProcessor::getPlaybackModeNames()
{
    static const juce::StringArray names{ "Chords", "Arp Up", "Arp Down", "Arp Bounce", "Arp Random" };
    return names;
}

const juce::StringArray& PluginProcessor::getArpRateNames()
{
    static const juce::StringArray names{ "1/4", "1/8", "1/8T", "1/16", "1/16T" };
    return names;
}

const juce::StringArray& PluginProcessor::getArpOctaveNames()
{
    static const juce::StringArray names{ "1 Oct", "2 Oct", "3 Oct" };
    return names;
}

const juce::StringArray& PluginProcessor::getMelodyRangeNames()
{
    static const juce::StringArray names{ "1 Oct", "2 Oct", "3 Oct", "4 Oct" };
    return names;
}

const juce::StringArray& PluginProcessor::getChordQualityNames()
{
    static const juce::StringArray names{
        "Auto",
        "Auto 7",
        "Major",
        "Minor",
        "Maj7",
        "Min7",
        "7",
        "Sus2",
        "Sus4",
        "Dim",
        "Aug",
        "Power",
        "Add9",
        "Custom"
    };

    return names;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    if (! restoreProgression(apvts.state.getProperty("generatedProgression").toString()))
        randomizeProgression();
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    buffer.clear();
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
           && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

int PluginProcessor::getChoiceValue(const juce::String& parameterID, int fallback) const
{
    return juce::roundToInt(readParameterValue(apvts, parameterID, (float) fallback));
}

float PluginProcessor::getFloatValue(const juce::String& parameterID, float fallback) const
{
    return readParameterValue(apvts, parameterID, fallback);
}

int PluginProcessor::getRequestedChordCount() const
{
    switch (getChoiceValue("chordCount", 0))
    {
        case 0: return 4;
        case 1: return 6;
        case 2: return 8;
        default: break;
    }

    return 4;
}

double PluginProcessor::getChordDurationInQuarterNotes() const
{
    switch (getChoiceValue("chordDuration", 0))
    {
        case 0: return 4.0;
        case 1: return 2.0;
        case 2: return 1.0;
        default: break;
    }

    return 4.0;
}

double PluginProcessor::getArpStepInQuarterNotes() const
{
    switch (getChoiceValue("arpRate", 1))
    {
        case 0: return 1.0;
        case 1: return 0.5;
        case 2: return 1.0 / 3.0;
        case 3: return 0.25;
        case 4: return 1.0 / 6.0;
        default: break;
    }

    return 0.5;
}

int PluginProcessor::getRegisterBaseOctave() const
{
    switch (getChoiceValue("register", 1))
    {
        case 0: return 2;
        case 1: return 3;
        case 2: return 4;
        default: break;
    }

    return 3;
}

int PluginProcessor::getMelodyRangeOctaves() const
{
    return getChoiceValue("melodyRange", 1) + 1;
}

bool PluginProcessor::isChordModeActive() const
{
    return getChoiceValue("generationMode", 0) == 0;
}

bool PluginProcessor::isMelodyModeActive() const
{
    return getChoiceValue("generationMode", 0) == 1;
}

bool PluginProcessor::isBasslineModeActive() const
{
    return getChoiceValue("generationMode", 0) == 2;
}

bool PluginProcessor::isMotifModeActive() const
{
    return getChoiceValue("generationMode", 0) == 3;
}

const PluginProcessor::ScaleDefinition& PluginProcessor::getSelectedScaleDefinition() const
{
    const auto& definitions = getScaleDefinitions();
    return definitions[(size_t) juce::jlimit(0, (int) definitions.size() - 1, getChoiceValue("scaleMode", 16))];
}

int PluginProcessor::mapCanonicalDegreeToScaleStep(int canonicalDegree) const
{
    const int scaleSize = (int) getSelectedScaleDefinition().intervals.size();
    if (scaleSize <= 1)
        return 0;

    if (scaleSize == 7)
        return juce::jlimit(0, 6, canonicalDegree);

    const double normalised = (double) juce::jlimit(0, 6, canonicalDegree) / 6.0;
    return juce::jlimit(0, scaleSize - 1, juce::roundToInt((float) (normalised * (double) (scaleSize - 1))));
}

int PluginProcessor::getChordToneCountForStyle(int styleIndex, bool isFinalChord) const
{
    const int scaleSize = (int) getSelectedScaleDefinition().intervals.size();
    int desired = 3;

    switch (styleIndex)
    {
        case deepHouseFeel:
        case tranceFeel:
        case raveFeel:
        case melodicFeel:
        case futureBassFeel:
            desired = 4;
            break;

        case houseFeel:
        case dubstepFeel:
            desired = isFinalChord ? 4 : 3;
            break;

        case technoFeel:
        case psytranceFeel:
        default:
            desired = 3;
            break;
    }

    if (scaleSize >= 9 && styleIndex >= melodicFeel)
        desired = 4;

    return juce::jlimit(3, 4, desired);
}

std::array<PluginProcessor::GeneratedChord, PluginProcessor::maxProgressionChords> PluginProcessor::makeRandomProgression() const
{
    std::array<GeneratedChord, maxProgressionChords> generated{};

    auto localRandom = juce::Random(juce::Random::getSystemRandom().nextInt());
    const int styleIndex = getChoiceValue("style", 0);
    const auto canonicalDegrees = buildCanonicalDegreeSequence(localRandom, styleIndex);
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
    const int raveQuality = [] (juce::Random& random)
    {
        const std::array<int, 5> raveQualities{ 5, 6, 7, 8, 12 };
        return raveQualities[(size_t) random.nextInt((int) raveQualities.size())];
    }(localRandom);

    int previousDegree = -1;
    for (int index = 0; index < maxProgressionChords; ++index)
    {
        int degree = mapCanonicalDegreeToScaleStep(canonicalDegrees[(size_t) index]);

        if (scaleSize <= 5 && previousDegree == degree && degree != 0)
            degree = (degree + 1) % scaleSize;

        if (index == maxProgressionChords - 1)
            degree = 0;

        auto& slot = generated[(size_t) index];
        slot.degree = juce::jlimit(0, scaleSize - 1, degree);
        slot.noteCount = getChordToneCountForStyle(styleIndex, index == maxProgressionChords - 1);
        slot.inversion = 0;
        slot.qualityIndex = 0;
        slot.customNoteMask.fill(false);

        switch (styleIndex)
        {
            case houseFeel: // Warm
                slot.qualityIndex = localRandom.nextFloat() < 0.34f ? 12 : (slot.noteCount >= 4 ? 1 : 0);
                break;

            case deepHouseFeel: // Soulful
                slot.noteCount = 4;
                slot.qualityIndex = 1;
                break;

            case technoFeel: // Tense
                slot.qualityIndex = localRandom.nextFloat() < 0.45f ? 7 : (localRandom.nextFloat() < 0.12f ? 9 : 3);
                break;

            case tranceFeel: // Euphoric
                slot.noteCount = 4;
                slot.qualityIndex = localRandom.nextFloat() < 0.5f ? 12 : 4;
                break;

            case psytranceFeel: // Hypnotic
                slot.qualityIndex = localRandom.nextFloat() < 0.42f ? 7 : 0;
                break;

            case dubstepFeel: // Dark
                slot.qualityIndex = localRandom.nextFloat() < 0.58f ? 5
                                  : localRandom.nextFloat() < 0.16f ? 9
                                                                     : 3;
                break;

            case raveFeel: // Urgent
                slot.noteCount = 4;
                slot.qualityIndex = localRandom.nextFloat() < 0.55f ? raveQuality : 6;
                break;

            case melodicFeel: // Dreamy
                slot.noteCount = 4;
                slot.qualityIndex = localRandom.nextFloat() < 0.48f ? 4 : 12;
                break;

            case futureBassFeel: // Huge
                slot.noteCount = 4;
                slot.qualityIndex = localRandom.nextBool() ? 12 : 1;
                break;

            default:
                break;
        }

        previousDegree = slot.degree;
    }

    return generated;
}

std::array<PluginProcessor::GeneratedChord, PluginProcessor::maxProgressionChords> PluginProcessor::makeRandomMelody() const
{
    std::array<GeneratedChord, maxProgressionChords> generated{};

    auto localRandom = juce::Random(juce::Random::getSystemRandom().nextInt());
    const int styleIndex = getChoiceValue("style", 0);
    const auto anchorDegrees = buildCanonicalDegreeSequence(localRandom, styleIndex);
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
    const int octaveCount = juce::jmax(1, getMelodyRangeOctaves());
    const int totalPositions = juce::jmax(1, scaleSize * octaveCount);
    const int requestedCount = getRequestedChordCount();
    int degreeVariation = 1;
    double motionWeight = 0.9;
    double repeatChance = 0.12;
    double lowRegisterBias = 0.0;
    double highRegisterBias = 0.0;
    double leapBonus = 0.0;

    switch (styleIndex)
    {
        case houseFeel:
            degreeVariation = 1;
            motionWeight = 0.96;
            repeatChance = 0.18;
            break;

        case deepHouseFeel:
            degreeVariation = 1;
            motionWeight = 1.04;
            repeatChance = 0.28;
            lowRegisterBias = 0.24;
            break;

        case technoFeel:
            degreeVariation = 1;
            motionWeight = 1.14;
            repeatChance = 0.34;
            lowRegisterBias = 0.30;
            break;

        case tranceFeel:
            degreeVariation = 2;
            motionWeight = 0.74;
            repeatChance = 0.08;
            highRegisterBias = 0.24;
            leapBonus = 0.14;
            break;

        case psytranceFeel:
            degreeVariation = 1;
            motionWeight = 0.88;
            repeatChance = 0.22;
            lowRegisterBias = 0.10;
            break;

        case dubstepFeel:
            degreeVariation = 3;
            motionWeight = 0.66;
            repeatChance = 0.06;
            lowRegisterBias = 0.18;
            leapBonus = 0.30;
            break;

        case raveFeel:
            degreeVariation = 2;
            motionWeight = 0.82;
            repeatChance = 0.12;
            leapBonus = 0.22;
            break;

        case melodicFeel:
            degreeVariation = 2;
            motionWeight = 0.72;
            repeatChance = 0.10;
            highRegisterBias = 0.20;
            leapBonus = 0.12;
            break;

        case futureBassFeel:
            degreeVariation = 2;
            motionWeight = 0.78;
            repeatChance = 0.16;
            highRegisterBias = 0.14;
            leapBonus = 0.18;
            break;

        default:
            break;
    }

    auto degreeForPosition = [scaleSize] (int position)
    {
        return juce::jlimit(0, scaleSize - 1, position % scaleSize);
    };

    auto octaveForPosition = [scaleSize, octaveCount] (int position)
    {
        return juce::jlimit(0, octaveCount - 1, position / scaleSize);
    };

    const int startingDegree = mapCanonicalDegreeToScaleStep(anchorDegrees.front());
    int currentPosition = juce::jlimit(0, totalPositions - 1, (octaveCount / 2) * scaleSize + startingDegree);

    for (int index = 0; index < maxProgressionChords; ++index)
    {
        const int anchorDegree = mapCanonicalDegreeToScaleStep(index == requestedCount - 1 ? 0 : anchorDegrees[(size_t) index]);

        int bestPosition = currentPosition;
        double bestScore = std::numeric_limits<double>::max();

        for (int octave = 0; octave < octaveCount; ++octave)
        {
            for (int delta = -degreeVariation; delta <= degreeVariation; ++delta)
            {
                const int degree = juce::jlimit(0, scaleSize - 1, anchorDegree + delta);
                const int position = octave * scaleSize + degree;
                const int motion = std::abs(position - currentPosition);

                double score = motion * motionWeight;
                score += std::abs(delta) * 0.75;
                score += localRandom.nextFloat() * 0.45f;
                score += octave * lowRegisterBias;
                score += std::abs(octave - (octaveCount - 1)) * highRegisterBias;

                if (motion >= juce::jmax(2, scaleSize / 2))
                    score -= leapBonus;

                if (index == requestedCount - 1 && degree == 0)
                    score -= 2.5;

                if (score < bestScore)
                {
                    bestScore = score;
                    bestPosition = position;
                }
            }
        }

        if (index > 0 && index < requestedCount - 1 && localRandom.nextFloat() < repeatChance)
            bestPosition = currentPosition;

        generated[(size_t) index].degree = degreeForPosition(bestPosition);
        generated[(size_t) index].noteCount = 1;
        generated[(size_t) index].octaveOffset = octaveForPosition(bestPosition);
        currentPosition = bestPosition;
    }

    return generated;
}

std::array<PluginProcessor::GeneratedChord, PluginProcessor::maxProgressionChords> PluginProcessor::makeRandomBassline() const
{
    auto generated = makeRandomProgression();
    const int requestedCount = getRequestedChordCount();
    const int styleIndex = getChoiceValue("style", 0);

    for (int index = 0; index < requestedCount; ++index)
    {
        auto& slot = generated[(size_t) index];
        slot.noteCount = 1;
        slot.inversion = 0;
        slot.qualityIndex = 0;
        slot.customNoteMask.fill(false);

        switch (styleIndex)
        {
            case houseFeel:
                slot.octaveOffset = index % 4 == 3 ? 1 : 0;
                break;

            case deepHouseFeel:
                slot.octaveOffset = index % 6 == 5 ? 1 : 0;
                break;

            case technoFeel:
            case psytranceFeel:
                slot.octaveOffset = 0;
                break;

            case tranceFeel:
                slot.octaveOffset = index % 4 == 1 ? 1 : 0;
                break;

            case dubstepFeel:
                slot.octaveOffset = index % 4 == 3 ? 1 : 0;
                break;

            case raveFeel:
                slot.octaveOffset = index % 2 == 1 ? 1 : 0;
                break;

            case melodicFeel:
                slot.octaveOffset = index == requestedCount - 1 ? 1 : 0;
                break;

            case futureBassFeel:
                slot.octaveOffset = index % 3 == 1 ? 1 : 0;
                break;

            default:
                slot.octaveOffset = 0;
                break;
        }
    }

    return generated;
}

std::array<PluginProcessor::GeneratedChord, PluginProcessor::maxProgressionChords> PluginProcessor::makeRandomMotif() const
{
    auto generated = makeRandomMelody();
    auto localRandom = juce::Random(juce::Random::getSystemRandom().nextInt());
    const int requestedCount = getRequestedChordCount();
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
    const int octaveCount = juce::jmax(1, getMelodyRangeOctaves());
    int cycleLength = 2;
    int variationRange = 1;
    double octaveVariationChance = 0.16;

    switch (getChoiceValue("style", 0))
    {
        case houseFeel:
            cycleLength = 2;
            variationRange = 1;
            octaveVariationChance = 0.12;
            break;

        case deepHouseFeel:
            cycleLength = 2;
            variationRange = 1;
            octaveVariationChance = 0.08;
            break;

        case technoFeel:
            cycleLength = 2;
            variationRange = 1;
            octaveVariationChance = 0.06;
            break;

        case tranceFeel:
            cycleLength = 4;
            variationRange = 2;
            octaveVariationChance = 0.20;
            break;

        case psytranceFeel:
            cycleLength = 2;
            variationRange = 1;
            octaveVariationChance = 0.10;
            break;

        case dubstepFeel:
            cycleLength = 4;
            variationRange = 3;
            octaveVariationChance = 0.24;
            break;

        case raveFeel:
            cycleLength = 2;
            variationRange = 2;
            octaveVariationChance = 0.18;
            break;

        case melodicFeel:
            cycleLength = 4;
            variationRange = 2;
            octaveVariationChance = 0.22;
            break;

        case futureBassFeel:
            cycleLength = 4;
            variationRange = 2;
            octaveVariationChance = 0.18;
            break;

        default:
            break;
    }

    for (int index = 1; index < requestedCount; ++index)
    {
        auto source = generated[(size_t) (index % cycleLength)];
        const int variation = (index == requestedCount - 1)
            ? -juce::jmin(source.degree, 2)
            : localRandom.nextInt(variationRange * 2 + 1) - variationRange;

        source.degree = juce::jlimit(0, scaleSize - 1, source.degree + variation);

        if (index % cycleLength == cycleLength - 1 && localRandom.nextFloat() < octaveVariationChance)
            source.octaveOffset = juce::jlimit(0, octaveCount - 1, source.octaveOffset + (localRandom.nextBool() ? 1 : -1));

        generated[(size_t) index] = source;
    }

    if (requestedCount > 0)
    {
        generated[(size_t) (requestedCount - 1)].degree = 0;
        generated[(size_t) (requestedCount - 1)].octaveOffset = juce::jlimit(0, octaveCount - 1, octaveCount / 2);
    }

    return generated;
}

int PluginProcessor::getDegreeRootMidi(int degree) const
{
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int scaleSize = juce::jmax(1, (int) scale.size());
    const int safeDegree = juce::jlimit(0, scaleSize - 1, degree);
    const int keyRoot = getChoiceValue("keyRoot", 0);
    const int octaveBase = 12 * (getRegisterBaseOctave() + 1);
    return octaveBase + keyRoot + scale[(size_t) safeDegree];
}

int PluginProcessor::getMelodyNoteMidi(const GeneratedChord& melodyNote) const
{
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int scaleSize = juce::jmax(1, (int) scale.size());
    const int safeDegree = juce::jlimit(0, scaleSize - 1, melodyNote.degree);
    const int safeOctave = juce::jlimit(0, juce::jmax(0, getMelodyRangeOctaves() - 1), melodyNote.octaveOffset);
    const int keyRoot = getChoiceValue("keyRoot", 0);
    const int octaveBase = 12 * (getRegisterBaseOctave() + 1);
    return octaveBase + keyRoot + scale[(size_t) safeDegree] + safeOctave * 12;
}

int PluginProcessor::getScalePositionMidi(int scalePosition, int octaveBias) const
{
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int scaleSize = juce::jmax(1, (int) scale.size());
    const int totalPosition = juce::jmax(0, scalePosition);
    const int wrappedDegree = totalPosition % scaleSize;
    const int octave = totalPosition / scaleSize;
    const int keyRoot = getChoiceValue("keyRoot", 0);
    const int octaveBase = 12 * (getRegisterBaseOctave() + 1 + octaveBias);
    return octaveBase + keyRoot + scale[(size_t) wrappedDegree] + octave * 12;
}

std::vector<int> PluginProcessor::buildScaleStackIntervals(const GeneratedChord& chord, int forcedNoteCount) const
{
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int scaleSize = juce::jmax(1, (int) scale.size());
    const int safeDegree = juce::jlimit(0, scaleSize - 1, chord.degree);
    const int noteCount = juce::jlimit(3, 4, forcedNoteCount > 0 ? forcedNoteCount : chord.noteCount);

    std::vector<int> intervals;
    intervals.reserve((size_t) noteCount);

    const int rootPitch = scale[(size_t) safeDegree];
    for (int index = 0; index < noteCount; ++index)
    {
        const int stackIndex = safeDegree + index * 2;
        const int octave = stackIndex / scaleSize;
        const int pitch = scale[(size_t) (stackIndex % scaleSize)] + octave * 12;
        intervals.push_back(pitch - rootPitch);
    }

    return intervals;
}

bool PluginProcessor::hasCustomIntervals(const GeneratedChord& chord) const
{
    return std::any_of(chord.customNoteMask.begin(), chord.customNoteMask.end(), [] (bool enabled) { return enabled; });
}

void PluginProcessor::seedCustomMaskFromCurrentShape(GeneratedChord& chord) const
{
    if (hasCustomIntervals(chord))
        return;

    const auto baseIntervals = buildScaleStackIntervals(chord, chord.qualityIndex == 1 ? 4 : -1);
    for (auto interval : baseIntervals)
    {
        if (juce::isPositiveAndBelow(interval, customNoteGridSize))
            chord.customNoteMask[(size_t) interval] = true;
    }

    if (! hasCustomIntervals(chord))
        chord.customNoteMask[0] = true;
}

std::vector<int> PluginProcessor::buildChordIntervals(const GeneratedChord& chord) const
{
    if (chord.qualityIndex == 13 || hasCustomIntervals(chord))
    {
        std::vector<int> customIntervals;
        customIntervals.reserve(customNoteGridSize);

        for (int semitone = 0; semitone < customNoteGridSize; ++semitone)
            if (chord.customNoteMask[(size_t) semitone])
                customIntervals.push_back(semitone);

        if (customIntervals.empty())
            customIntervals.push_back(0);

        return customIntervals;
    }

    switch (chord.qualityIndex)
    {
        case 1: return buildScaleStackIntervals(chord, 4);
        case 2: return { 0, 4, 7 };
        case 3: return { 0, 3, 7 };
        case 4: return { 0, 4, 7, 11 };
        case 5: return { 0, 3, 7, 10 };
        case 6: return { 0, 4, 7, 10 };
        case 7: return { 0, 2, 7 };
        case 8: return { 0, 5, 7 };
        case 9: return { 0, 3, 6 };
        case 10: return { 0, 4, 8 };
        case 11: return { 0, 7 };
        case 12: return { 0, 4, 7, 14 };
        default: break;
    }

    return buildScaleStackIntervals(chord);
}

std::vector<int> PluginProcessor::buildVoicingForChord(const GeneratedChord& chord,
                                                       const std::vector<int>* previousVoicing) const
{
    auto baseNotes = buildChordIntervals(chord);
    const int rootMidi = getDegreeRootMidi(chord.degree);
    for (auto& note : baseNotes)
        note += rootMidi;

    const int spreadIndex = getChoiceValue("spread", 1);
    const int targetCentre = getRegisterBaseOctave() == 2 ? 48
                            : getRegisterBaseOctave() == 3 ? 60
                                                           : 72;

    auto prepareVoicing = [targetCentre, spreadIndex] (std::vector<int> notes, int inversion, int octaveShift)
    {
        std::sort(notes.begin(), notes.end());

        for (int index = 0; index < inversion && index < (int) notes.size(); ++index)
        {
            notes[(size_t) index] += 12;
            std::sort(notes.begin(), notes.end());
        }

        for (auto& note : notes)
            note += octaveShift;

        if (spreadIndex >= 1 && notes.size() >= 3)
            notes[1] += 12;

        if (spreadIndex >= 2)
        {
            if (notes.size() >= 4)
                notes[2] += 12;
            else if (notes.size() >= 3)
                notes.back() += 12;
        }

        std::sort(notes.begin(), notes.end());

        auto average = [&notes]
        {
            return std::accumulate(notes.begin(), notes.end(), 0.0) / (double) juce::jmax(1, (int) notes.size());
        };

        while (average() < targetCentre - 5.0)
            for (auto& note : notes) note += 12;

        while (average() > targetCentre + 7.0)
            for (auto& note : notes) note -= 12;

        for (auto& note : notes)
            note = juce::jlimit(24, 108, note);

        std::sort(notes.begin(), notes.end());
        notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
        return notes;
    };

    if (previousVoicing == nullptr || previousVoicing->empty())
        return prepareVoicing(baseNotes, chord.inversion, 0);

    std::vector<int> bestVoicing;
    double bestScore = std::numeric_limits<double>::max();
    const int forcedInversion = juce::jlimit(0, getMaxInversionForChord(chord), chord.inversion);

    for (int inversionOffset : { 0 })
    {
        for (int octaveShift : { -12, 0, 12 })
        {
            auto candidate = prepareVoicing(baseNotes,
                                            juce::jmin(getMaxInversionForChord(chord), forcedInversion + inversionOffset),
                                            octaveShift);

            double score = 0.0;
            const int pairCount = juce::jmin((int) candidate.size(), (int) previousVoicing->size());
            for (int noteIndex = 0; noteIndex < pairCount; ++noteIndex)
                score += std::abs(candidate[(size_t) noteIndex] - (*previousVoicing)[(size_t) noteIndex]);

            const auto candidateAverage = std::accumulate(candidate.begin(), candidate.end(), 0.0)
                                          / (double) juce::jmax(1, (int) candidate.size());
            const auto previousAverage = std::accumulate(previousVoicing->begin(), previousVoicing->end(), 0.0)
                                         / (double) juce::jmax(1, (int) previousVoicing->size());
            score += std::abs(candidateAverage - previousAverage) * 0.4;

            if (score < bestScore)
            {
                bestScore = score;
                bestVoicing = std::move(candidate);
            }
        }
    }

    return bestVoicing;
}

juce::String PluginProcessor::detectChordSuffix(const std::vector<int>& chordIntervals) const
{
    std::vector<int> normalised;
    normalised.reserve(chordIntervals.size());

    for (auto interval : chordIntervals)
    {
        const int wrapped = ((interval % 12) + 12) % 12;
        if (std::find(normalised.begin(), normalised.end(), wrapped) == normalised.end())
            normalised.push_back(wrapped);
    }

    std::sort(normalised.begin(), normalised.end());

    if (matchesIntervals(normalised, { 0, 4, 7 })) return "";
    if (matchesIntervals(normalised, { 0, 3, 7 })) return "m";
    if (matchesIntervals(normalised, { 0, 3, 6 })) return "dim";
    if (matchesIntervals(normalised, { 0, 4, 8 })) return "aug";
    if (matchesIntervals(normalised, { 0, 2, 7 })) return "sus2";
    if (matchesIntervals(normalised, { 0, 5, 7 })) return "sus4";
    if (matchesIntervals(normalised, { 0, 7 })) return "5";
    if (matchesIntervals(normalised, { 0, 2, 4, 7 })) return "add9";
    if (matchesIntervals(normalised, { 0, 2, 3, 7 })) return "madd9";
    if (matchesIntervals(normalised, { 0, 4, 7, 11 })) return "maj7";
    if (matchesIntervals(normalised, { 0, 4, 7, 10 })) return "7";
    if (matchesIntervals(normalised, { 0, 3, 7, 10 })) return "m7";
    if (matchesIntervals(normalised, { 0, 3, 7, 11 })) return "mMaj7";
    if (matchesIntervals(normalised, { 0, 4, 7, 9 })) return "6";
    if (matchesIntervals(normalised, { 0, 3, 7, 9 })) return "m6";
    if (matchesIntervals(normalised, { 0, 3, 6, 10 })) return "m7b5";
    if (matchesIntervals(normalised, { 0, 3, 6, 9 })) return "dim7";
    if (matchesIntervals(normalised, { 0, 4, 8, 10 })) return "aug7";
    if (matchesIntervals(normalised, { 0, 4, 8, 11 })) return "augMaj7";
    if (matchesIntervals(normalised, { 0, 5, 7, 10 })) return "7sus4";
    if (matchesIntervals(normalised, { 0, 2, 7, 10 })) return "7sus2";

    return " stack";
}

int PluginProcessor::getMaxInversionForChord(const GeneratedChord& chord) const
{
    const auto intervalCount = (int) buildChordIntervals(chord).size();
    return juce::jmax(0, intervalCount - 1);
}

juce::String PluginProcessor::getInversionLabel(const GeneratedChord& chord) const
{
    switch (juce::jlimit(0, 3, chord.inversion))
    {
        case 1: return "1st Inv";
        case 2: return "2nd Inv";
        case 3: return "3rd Inv";
        default: break;
    }

    return "Root";
}

juce::String PluginProcessor::buildChordName(const GeneratedChord& chord) const
{
    const int rootMidi = getDegreeRootMidi(chord.degree);
    const auto rootName = juce::MidiMessage::getMidiNoteName(rootMidi, true, false, 4);
    return rootName + detectChordSuffix(buildChordIntervals(chord));
}

juce::String PluginProcessor::buildChordDisplayLabel(const GeneratedChord& chord) const
{
    if (chord.qualityIndex == 13 || hasCustomIntervals(chord))
        return buildChordName(chord).trim() + " Edit";

    return buildChordName(chord);
}

juce::String PluginProcessor::buildMelodyNoteLabel(const GeneratedChord& melodyNote) const
{
    const int midiNote = getMelodyNoteMidi(melodyNote);
    return juce::MidiMessage::getMidiNoteName(midiNote, true, true, 4);
}

juce::String PluginProcessor::serialiseProgression() const
{
    juce::StringArray tokens;
    for (const auto& chord : progression)
    {
        juce::String mask;
        for (bool enabled : chord.customNoteMask)
            mask << (enabled ? "1" : "0");

        tokens.add(juce::String(chord.degree) + ","
                   + juce::String(chord.noteCount) + ","
                   + juce::String(chord.octaveOffset) + ","
                   + juce::String(chord.inversion) + ","
                   + juce::String(chord.qualityIndex) + ","
                   + mask);
    }

    return tokens.joinIntoString(";");
}

bool PluginProcessor::restoreProgression(const juce::String& serialised)
{
    const auto tokens = juce::StringArray::fromTokens(serialised, ";", "");
    if (tokens.size() != maxProgressionChords)
        return false;

    std::array<GeneratedChord, maxProgressionChords> restored{};
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());

    for (int index = 0; index < maxProgressionChords; ++index)
    {
        const auto parts = juce::StringArray::fromTokens(tokens[index], ",", "");
        if (parts.size() != 2 && parts.size() != 3 && parts.size() != 6)
            return false;

        restored[(size_t) index].degree = juce::jlimit(0, scaleSize - 1, parts[0].getIntValue());
        restored[(size_t) index].noteCount = juce::jlimit(1, 4, parts[1].getIntValue());
        restored[(size_t) index].octaveOffset = parts.size() == 3 ? juce::jmax(0, parts[2].getIntValue()) : 0;

        if (parts.size() == 6)
        {
            restored[(size_t) index].octaveOffset = juce::jmax(0, parts[2].getIntValue());
            restored[(size_t) index].inversion = juce::jmax(0, parts[3].getIntValue());
            restored[(size_t) index].qualityIndex = juce::jlimit(0, getChordQualityNames().size() - 1, parts[4].getIntValue());

            const auto mask = parts[5];
            for (int bit = 0; bit < juce::jmin(mask.length(), customNoteGridSize); ++bit)
                restored[(size_t) index].customNoteMask[(size_t) bit] = mask[bit] == '1';
        }
    }

    const juce::ScopedLock lock(progressionLock);
    progression = restored;
    return true;
}

void PluginProcessor::syncProgressionIntoState()
{
    apvts.state.setProperty("generatedProgression", serialiseProgression(), nullptr);
}

void PluginProcessor::randomizeProgression()
{
    const juce::ScopedLock lock(progressionLock);
    if (isMelodyModeActive())
        progression = makeRandomMelody();
    else if (isBasslineModeActive())
        progression = makeRandomBassline();
    else if (isMotifModeActive())
        progression = makeRandomMotif();
    else
        progression = makeRandomProgression();

    syncProgressionIntoState();

    if (isMelodyModeActive())
        lastExportStatus = "Fresh melody ready. Drag it straight into Ableton or export a MIDI file.";
    else if (isBasslineModeActive())
        lastExportStatus = "Fresh bassline ready. Drag it straight into Ableton or export a MIDI file.";
    else if (isMotifModeActive())
        lastExportStatus = "Fresh motif ready. Drag it straight into Ableton or export a MIDI file.";
    else
        lastExportStatus = "Fresh progression ready. Drag it straight into Ableton or export a MIDI file.";
}

int PluginProcessor::getVisibleSlotCount() const
{
    return getRequestedChordCount();
}

juce::StringArray PluginProcessor::getScaleDegreeNames() const
{
    juce::StringArray names;
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int keyRoot = getChoiceValue("keyRoot", 0);

    for (int degree = 0; degree < (int) scale.size(); ++degree)
    {
        const int midiNote = 60 + keyRoot + scale[(size_t) degree];
        names.add(juce::String(degree + 1) + "  " + juce::MidiMessage::getMidiNoteName(midiNote, true, false, 4));
    }

    return names;
}

PluginProcessor::GeneratedChord PluginProcessor::getProgressionSlot(int slotIndex) const
{
    const juce::ScopedLock lock(progressionLock);
    return progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
}

int PluginProcessor::getSlotRootMidi(int slotIndex) const
{
    return getDegreeRootMidi(getProgressionSlot(slotIndex).degree);
}

std::vector<int> PluginProcessor::getSlotEditorIntervals(int slotIndex) const
{
    return buildChordIntervals(getProgressionSlot(slotIndex));
}

void PluginProcessor::setProgressionSlotDegree(int slotIndex, int degree)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
    slot.degree = juce::jlimit(0, scaleSize - 1, degree);
    syncProgressionIntoState();
}

void PluginProcessor::replaceProgressionSlotWithScaleChord(int slotIndex, int degree)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
    slot.degree = juce::jlimit(0, scaleSize - 1, degree);
    slot.noteCount = slot.noteCount >= 4 ? 4 : 3;
    slot.qualityIndex = slot.noteCount >= 4 ? 1 : 0;
    slot.inversion = 0;
    slot.customNoteMask.fill(false);
    syncProgressionIntoState();
}

void PluginProcessor::setProgressionSlotQuality(int slotIndex, int qualityIndex)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    slot.qualityIndex = juce::jlimit(0, getChordQualityNames().size() - 1, qualityIndex);

    if (slot.qualityIndex == 0)
        slot.noteCount = 3;
    else if (slot.qualityIndex == 1)
        slot.noteCount = 4;

    if (slot.qualityIndex == 13)
        seedCustomMaskFromCurrentShape(slot);
    else
        slot.customNoteMask.fill(false);

    slot.inversion = juce::jlimit(0, getMaxInversionForChord(slot), slot.inversion);
    syncProgressionIntoState();
}

void PluginProcessor::cycleProgressionSlotInversion(int slotIndex)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    const int maxInversion = getMaxInversionForChord(slot);
    slot.inversion = maxInversion > 0 ? (slot.inversion + 1) % (maxInversion + 1) : 0;
    syncProgressionIntoState();
}

void PluginProcessor::setProgressionSlotInversion(int slotIndex, int inversion)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    slot.inversion = juce::jlimit(0, getMaxInversionForChord(slot), inversion);
    syncProgressionIntoState();
}

void PluginProcessor::toggleCustomNoteForSlot(int slotIndex, int semitoneIndex)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];
    if (! juce::isPositiveAndBelow(semitoneIndex, customNoteGridSize))
        return;

    seedCustomMaskFromCurrentShape(slot);
    slot.qualityIndex = 13;
    slot.customNoteMask[(size_t) semitoneIndex] = ! slot.customNoteMask[(size_t) semitoneIndex];

    if (! hasCustomIntervals(slot))
        slot.customNoteMask[0] = true;

    slot.inversion = juce::jlimit(0, getMaxInversionForChord(slot), slot.inversion);
    syncProgressionIntoState();
}

bool PluginProcessor::moveCustomNoteForSlot(int slotIndex, int fromSemitoneIndex, int toSemitoneIndex)
{
    const juce::ScopedLock lock(progressionLock);
    auto& slot = progression[(size_t) juce::jlimit(0, maxProgressionChords - 1, slotIndex)];

    if (! juce::isPositiveAndBelow(fromSemitoneIndex, customNoteGridSize)
        || ! juce::isPositiveAndBelow(toSemitoneIndex, customNoteGridSize)
        || fromSemitoneIndex == toSemitoneIndex)
    {
        return false;
    }

    seedCustomMaskFromCurrentShape(slot);
    slot.qualityIndex = 13;

    if (! slot.customNoteMask[(size_t) fromSemitoneIndex] || slot.customNoteMask[(size_t) toSemitoneIndex])
        return false;

    slot.customNoteMask[(size_t) fromSemitoneIndex] = false;
    slot.customNoteMask[(size_t) toSemitoneIndex] = true;
    slot.inversion = juce::jlimit(0, getMaxInversionForChord(slot), slot.inversion);
    syncProgressionIntoState();
    return true;
}

std::vector<juce::String> PluginProcessor::getProgressionChordLabels() const
{
    const juce::ScopedLock lock(progressionLock);

    std::vector<juce::String> labels;
    const int chordCount = getRequestedChordCount();
    labels.reserve((size_t) chordCount);

    for (int index = 0; index < chordCount; ++index)
    {
        const auto& slot = progression[(size_t) index];
        if (isMelodyModeActive() || isMotifModeActive())
            labels.push_back(buildMelodyNoteLabel(slot));
        else if (isBasslineModeActive())
            labels.push_back(juce::MidiMessage::getMidiNoteName(getDegreeRootMidi(slot.degree), true, false, 4));
        else
            labels.push_back(buildChordDisplayLabel(slot));
    }

    return labels;
}

PluginProcessor::SlotDisplayData PluginProcessor::getProgressionSlotDisplay(int slotIndex) const
{
    const auto safeIndex = juce::jlimit(0, maxProgressionChords - 1, slotIndex);
    const auto slot = getProgressionSlot(safeIndex);

    SlotDisplayData display;
    display.heading = juce::String(safeIndex + 1);

    if (isMelodyModeActive())
    {
        display.primary = buildMelodyNoteLabel(slot);
        display.secondary = "Anchor";
        return display;
    }

    if (isBasslineModeActive())
    {
        display.primary = juce::MidiMessage::getMidiNoteName(getDegreeRootMidi(slot.degree), true, false, 4);
        display.secondary = slot.octaveOffset > 0 ? "Bass Lift" : "Bass Root";
        return display;
    }

    if (isMotifModeActive())
    {
        display.primary = buildMelodyNoteLabel(slot);
        display.secondary = safeIndex % 2 == 0 ? "Motif A" : "Motif B";
        return display;
    }

    display.primary = buildChordDisplayLabel(slot);
    display.secondary = getInversionLabel(slot);
    return display;
}

juce::StringArray PluginProcessor::getScaleChordReplacementNames(int slotIndex) const
{
    juce::StringArray names;
    const juce::ScopedLock lock(progressionLock);

    const int safeIndex = juce::jlimit(0, maxProgressionChords - 1, slotIndex);
    const auto sourceSlot = progression[(size_t) safeIndex];
    const auto& scale = getSelectedScaleDefinition().intervals;
    const int keyRoot = getChoiceValue("keyRoot", 0);
    const int diatonicNoteCount = sourceSlot.noteCount >= 4 ? 4 : 3;
    const int diatonicQuality = diatonicNoteCount >= 4 ? 1 : 0;

    for (int degree = 0; degree < (int) scale.size(); ++degree)
    {
        auto option = sourceSlot;
        option.degree = degree;
        option.noteCount = diatonicNoteCount;
        option.qualityIndex = diatonicQuality;
        option.inversion = 0;
        option.customNoteMask.fill(false);

        const int midiNote = 60 + keyRoot + scale[(size_t) degree];
        const auto degreeLabel = juce::String(degree + 1) + "  "
                                 + juce::MidiMessage::getMidiNoteName(midiNote, true, false, 4);
        names.add(degreeLabel + "   " + buildChordName(option));
    }

    return names;
}

std::vector<PluginProcessor::PreviewNote> PluginProcessor::getClipPreviewNotes() const
{
    const juce::ScopedLock lock(progressionLock);

    std::vector<PreviewNote> previewNotes;
    const auto sequence = buildGeneratedSequence(getHostBpmFallback120());
    std::map<int, double> activeNotes;

    for (int eventIndex = 0; eventIndex < sequence.getNumEvents(); ++eventIndex)
    {
        const auto* eventHolder = sequence.getEventPointer(eventIndex);
        if (eventHolder == nullptr)
            continue;

        const auto& message = eventHolder->message;
        if (! message.isNoteOnOrOff())
            continue;

        const auto beatTime = message.getTimeStamp() / 960.0;
        const int noteNumber = message.getNoteNumber();

        if (message.isNoteOn())
        {
            activeNotes[noteNumber] = beatTime;
            continue;
        }

        if (const auto found = activeNotes.find(noteNumber); found != activeNotes.end())
        {
            previewNotes.push_back({ found->second,
                                     juce::jmax(0.05, beatTime - found->second),
                                     noteNumber });
            activeNotes.erase(found);
        }
    }

    return previewNotes;
}

double PluginProcessor::getClipPreviewLengthInQuarterNotes() const
{
    return (double) getRequestedChordCount() * getChordDurationInQuarterNotes();
}

juce::String PluginProcessor::getSettingsSummary() const
{
    const auto& feelNames = isChordModeActive() ? getChordFeelNames() : getStyleNames();
    const auto feelText = feelNames[juce::jlimit(0, feelNames.size() - 1, getChoiceValue("style", 0))];
    const auto countText = getChordCountNames()[getChoiceValue("chordCount", 0)]
                           + (isChordModeActive() ? " Chords" : " Slots");
    const auto lengthText = getChordDurationNames()[getChoiceValue("chordDuration", 0)];
    const auto registerText = getRegisterNames()[getChoiceValue("register", 1)];
    const auto modeName = getGenerationModeNames()[getChoiceValue("generationMode", 0)];
    const auto swingText = juce::String(juce::roundToInt(getFloatValue("swing", 0.50f) * 100.0f)) + "%";

    if (isMelodyModeActive())
    {
        return modeName
               + "  |  "
               + getKeyNames()[getChoiceValue("keyRoot", 0)]
               + " " + getScaleNames()[getChoiceValue("scaleMode", 16)]
               + "  |  " + feelText
               + "  |  " + countText
               + "  |  " + lengthText
               + "  |  " + registerText
               + " / " + getMelodyRangeNames()[getChoiceValue("melodyRange", 1)]
               + "  |  Swing " + swingText;
    }

    if (isBasslineModeActive())
    {
        return modeName
               + "  |  "
               + getKeyNames()[getChoiceValue("keyRoot", 0)]
               + " " + getScaleNames()[getChoiceValue("scaleMode", 16)]
               + "  |  " + feelText
               + "  |  " + countText
               + "  |  " + lengthText
               + "  |  " + registerText
               + "  |  Swing " + swingText;
    }

    if (isMotifModeActive())
    {
        return modeName
               + "  |  "
               + getKeyNames()[getChoiceValue("keyRoot", 0)]
               + " " + getScaleNames()[getChoiceValue("scaleMode", 16)]
               + "  |  " + feelText
               + "  |  " + countText
               + "  |  " + lengthText
               + "  |  " + registerText
               + " / " + getMelodyRangeNames()[getChoiceValue("melodyRange", 1)]
               + "  |  Swing " + swingText;
    }

    juce::String playbackSummary = getPlaybackModeNames()[getChoiceValue("playbackMode", 0)];
    if (getChoiceValue("playbackMode", 0) != 0)
        playbackSummary << " @ " << getArpRateNames()[getChoiceValue("arpRate", 1)]
                        << " x" << juce::String(getChoiceValue("arpOctaves", 0) + 1);

    return modeName
           + "  |  "
           + getKeyNames()[getChoiceValue("keyRoot", 0)]
           + " " + getScaleNames()[getChoiceValue("scaleMode", 16)]
           + "  |  " + feelText
           + "  |  " + countText
           + "  |  " + lengthText
           + "  |  " + registerText
           + "  |  " + playbackSummary
           + "  |  Strum " + juce::String(juce::roundToInt(getFloatValue("strum", 0.0f))) + " ms"
           + "  |  Swing " + swingText;
}

juce::String PluginProcessor::getProgressionSummary() const
{
    const auto labels = getProgressionChordLabels();
    juce::StringArray array;
    for (const auto& label : labels)
        array.add(label);

    const auto separator = isChordModeActive() ? "  •  " : "  ·  ";
    if (array.size() > 4)
    {
        juce::StringArray topRow;
        juce::StringArray bottomRow;

        for (int index = 0; index < array.size(); ++index)
            (index < (array.size() + 1) / 2 ? topRow : bottomRow).add(array[index]);

        return topRow.joinIntoString(separator) + "\n" + bottomRow.joinIntoString(separator);
    }

    return array.joinIntoString(separator);
}

juce::String PluginProcessor::getExportStatus() const
{
    const juce::ScopedLock lock(progressionLock);
    return lastExportStatus.isNotEmpty()
        ? lastExportStatus
        : "Drag the big tile into Ableton, or use Bake MIDI / Export... if you want a file on disk.";
}

double PluginProcessor::getHostBpmFallback120() const
{
    if (const auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
            if (position->getBpm().hasValue())
                return *position->getBpm();
    }

    return 120.0;
}

double PluginProcessor::applySwingToBeat(double beat, double stepLength, int stepIndex) const
{
    if (stepIndex % 2 == 0)
        return beat;

    const double swing = juce::jlimit(0.50, 0.75, (double) getFloatValue("swing", 0.50f));
    const double offset = (swing - 0.50) * stepLength * 2.0;
    return beat + offset;
}

std::vector<int> PluginProcessor::makeArpPattern(const std::vector<int>& voicing, juce::Random& random) const
{
    if (voicing.empty())
        return {};

    std::vector<int> pattern;
    const int octaveCount = getChoiceValue("arpOctaves", 0) + 1;

    for (int octave = 0; octave < octaveCount; ++octave)
        for (auto note : voicing)
            pattern.push_back(note + 12 * octave);

    std::sort(pattern.begin(), pattern.end());

    switch (getChoiceValue("playbackMode", 0))
    {
        case 2:
            std::reverse(pattern.begin(), pattern.end());
            break;

        case 3:
        {
            std::vector<int> bounce = pattern;
            if (pattern.size() > 1)
                for (int index = (int) pattern.size() - 2; index > 0; --index)
                    bounce.push_back(pattern[(size_t) index]);
            return bounce;
        }

        case 4:
            for (int index = (int) pattern.size() - 1; index > 0; --index)
                std::swap(pattern[(size_t) index], pattern[(size_t) random.nextInt(index + 1)]);
            break;

        default:
            break;
    }

    return pattern;
}

juce::MidiMessageSequence PluginProcessor::buildGeneratedSequence(double bpm) const
{
    juce::MidiMessageSequence sequence;
    const int chordCount = getRequestedChordCount();
    const double ticksPerQuarterNote = 960.0;
    const double durationInQuarterNotes = getChordDurationInQuarterNotes();
    const double gate = juce::jlimit(0.45f, 0.98f, getFloatValue("gate", 0.90f));
    const auto velocity = (juce::uint8) juce::jlimit(1, 127, juce::roundToInt(getFloatValue("velocity", 96.0f)));
    const int playbackMode = getChoiceValue("playbackMode", 0);
    const int styleIndex = getChoiceValue("style", 0);
    const double arpStep = getArpStepInQuarterNotes();
    const double strumBeats = juce::jlimit(0.0, 0.35, (double) getFloatValue("strum", 0.0f) * bpm / 60000.0);
    juce::Random random(serialiseProgression().hashCode()
                        ^ (styleIndex << 8)
                        ^ (getChoiceValue("generationMode", 0) << 12)
                        ^ (getChoiceValue("playbackMode", 0) << 16));

    auto addNote = [&sequence, ticksPerQuarterNote, velocity] (double startBeat, double endBeat, int note)
    {
        if (endBeat <= startBeat)
            return;

        auto on = juce::MidiMessage::noteOn(1, note, velocity);
        on.setTimeStamp(startBeat * ticksPerQuarterNote);
        sequence.addEvent(on);

        auto off = juce::MidiMessage::noteOff(1, note);
        off.setTimeStamp(endBeat * ticksPerQuarterNote);
        sequence.addEvent(off);
    };

    auto applyFeelSwing = [this] (double beat, double stepLength, int stepIndex, double styleAmount)
    {
        const auto swungBeat = applySwingToBeat(beat, stepLength, stepIndex);
        return beat + (swungBeat - beat) * juce::jlimit(0.0, 1.0, styleAmount);
    };

    if (isMelodyModeActive())
    {
        double startBeat = 0.0;

        for (int index = 0; index < chordCount; ++index)
        {
            const auto note = getMelodyNoteMidi(progression[(size_t) index]);
            addNote(startBeat,
                    startBeat + durationInQuarterNotes * gate,
                    note);
            startBeat += durationInQuarterNotes;
        }

        sequence.updateMatchedPairs();
        return sequence;
    }

    if (isBasslineModeActive())
    {
        constexpr int restStep = -1;
        const std::array<std::array<int, 16>, 9> bassPatterns{ {
            { restStep, 0, restStep, 2, restStep, 0, restStep, 1, restStep, 0, restStep, 2, restStep, 3, restStep, 2 },
            { restStep, 0, 2, restStep, restStep, 0, restStep, 3, restStep, 0, 1, restStep, restStep, 2, restStep, 3 },
            { 0, restStep, 0, restStep, 0, restStep, 0, restStep, 0, restStep, 2, restStep, 0, restStep, 0, restStep },
            { 0, restStep, 0, restStep, 0, restStep, 3, restStep, 0, restStep, 0, restStep, 2, restStep, 3, restStep },
            { restStep, 0, restStep, 0, restStep, 0, restStep, 0, restStep, 0, restStep, 0, restStep, 0, 3, 0 },
            { 0, restStep, restStep, 2, restStep, restStep, 3, restStep, 0, restStep, restStep, 4, restStep, 2, restStep, restStep },
            { 0, restStep, 3, restStep, 0, 2, restStep, 3, 0, restStep, 3, restStep, 2, restStep, 0, restStep },
            { 0, restStep, 2, restStep, 0, restStep, 1, restStep, 0, restStep, 2, restStep, 3, restStep, 2, restStep },
            { 0, restStep, 3, restStep, restStep, 2, restStep, 1, 0, restStep, 3, restStep, restStep, 2, 4, restStep }
        } };
        const std::array<double, 9> bassGateFactors{ 0.86, 0.82, 0.78, 0.90, 0.70, 1.35, 0.74, 0.88, 0.84 };
        const std::array<double, 9> bassSwingAmounts{ 0.95, 0.90, 0.35, 0.20, 0.06, 0.55, 0.70, 0.28, 0.80 };
        double startBeat = 0.0;

        for (int slotIndex = 0; slotIndex < chordCount; ++slotIndex)
        {
            const auto& slot = progression[(size_t) slotIndex];
            const auto chordIntervals = buildChordIntervals(slot);
            const int root = juce::jlimit(24, 96, getDegreeRootMidi(slot.degree) - 12 + slot.octaveOffset * 12);
            const int third = juce::jlimit(24, 108, root + (chordIntervals.size() > 1 ? chordIntervals[1] : 3));
            const int fifth = juce::jlimit(24, 108, root + (chordIntervals.size() > 2 ? chordIntervals[2] : 7));
            const int octave = juce::jlimit(24, 108, root + 12);
            const int colour = juce::jlimit(24, 108, root + (chordIntervals.size() > 3 ? chordIntervals[3] : chordIntervals.back()));
            const std::array<int, 5> palette{ root, third, fifth, octave, colour };
            const int stepCount = durationInQuarterNotes >= 4.0 ? 16
                                 : durationInQuarterNotes >= 2.0 ? 8
                                                                 : 4;
            const double stepLength = durationInQuarterNotes / (double) stepCount;
            const double slotEnd = startBeat + durationInQuarterNotes;
            const auto& pattern = bassPatterns[(size_t) juce::jlimit(0, (int) bassPatterns.size() - 1, styleIndex)];
            const double gateFactor = bassGateFactors[(size_t) juce::jlimit(0, (int) bassGateFactors.size() - 1, styleIndex)];
            const double swingAmount = bassSwingAmounts[(size_t) juce::jlimit(0, (int) bassSwingAmounts.size() - 1, styleIndex)];

            for (int stepIndex = 0; stepIndex < stepCount; ++stepIndex)
            {
                const double nominalStart = startBeat + stepLength * (double) stepIndex;
                const double swungStart = applyFeelSwing(nominalStart, stepLength, stepIndex, swingAmount);
                const int patternIndex = (stepIndex * 16) / stepCount;
                const int noteIndex = pattern[(size_t) juce::jlimit(0, 15, patternIndex)];
                if (noteIndex < 0)
                    continue;

                int note = palette[(size_t) juce::jlimit(0, 4, noteIndex)];

                if (slotIndex == chordCount - 1 && stepIndex == stepCount - 1)
                    note = root;

                const double noteEnd = juce::jmin(slotEnd, swungStart + stepLength * gateFactor);
                addNote(swungStart, noteEnd, note);
            }

            startBeat += durationInQuarterNotes;
        }

        sequence.updateMatchedPairs();
        return sequence;
    }

    if (isMotifModeActive())
    {
        const int scaleSize = juce::jmax(1, (int) getSelectedScaleDefinition().intervals.size());
        const int maxScalePosition = juce::jmax(scaleSize, scaleSize * getMelodyRangeOctaves()) - 1;
        constexpr int restStep = -99;
        const std::array<std::array<int, 8>, 9> motifCells{ {
            { 0, restStep, 1, restStep, 0, 2, restStep, 1 },
            { 0, restStep, 0, 1, restStep, 0, 2, restStep },
            { 0, 1, 0, restStep, 0, -1, 0, restStep },
            { 0, 1, 2, 1, 0, 2, 3, 1 },
            { 0, restStep, 1, 0, 2, 0, 1, restStep },
            { 0, restStep, 3, restStep, 1, restStep, -1, restStep },
            { 0, 2, restStep, 0, 3, restStep, 2, restStep },
            { 0, 1, 2, restStep, 1, 3, 2, restStep },
            { 0, 2, restStep, 1, 3, restStep, 1, 0 }
        } };
        const std::array<double, 9> motifGateFactors{ 0.52, 0.50, 0.45, 0.68, 0.40, 0.60, 0.42, 0.64, 0.56 };
        const std::array<double, 9> motifSwingAmounts{ 0.85, 0.80, 0.25, 0.18, 0.05, 0.55, 0.70, 0.22, 0.75 };

        double startBeat = 0.0;
        for (int slotIndex = 0; slotIndex < chordCount; ++slotIndex)
        {
            const auto& slot = progression[(size_t) slotIndex];
            const int currentPosition = slot.octaveOffset * scaleSize + slot.degree;
            const int nextIndex = juce::jmin(chordCount - 1, slotIndex + 1);
            const auto& nextSlot = progression[(size_t) nextIndex];
            const int nextPosition = nextSlot.octaveOffset * scaleSize + nextSlot.degree;
            const int stepCount = durationInQuarterNotes >= 2.0 ? 8 : 4;
            const double stepLength = durationInQuarterNotes / (double) stepCount;
            const double slotEnd = startBeat + durationInQuarterNotes;
            const auto& cell = motifCells[(size_t) juce::jlimit(0, (int) motifCells.size() - 1, styleIndex)];
            const double gateFactor = motifGateFactors[(size_t) juce::jlimit(0, (int) motifGateFactors.size() - 1, styleIndex)];
            const double swingAmount = motifSwingAmounts[(size_t) juce::jlimit(0, (int) motifSwingAmounts.size() - 1, styleIndex)];

            for (int stepIndex = 0; stepIndex < stepCount; ++stepIndex)
            {
                const int patternIndex = (stepIndex * 8) / stepCount;
                int cellOffset = cell[(size_t) juce::jlimit(0, 7, patternIndex)];

                if (cellOffset == restStep)
                    continue;

                if (stepIndex == stepCount - 1)
                    cellOffset = juce::jlimit(-4, 4, nextPosition - currentPosition);

                const int scalePosition = juce::jlimit(0, maxScalePosition, currentPosition + cellOffset);
                const int note = getScalePositionMidi(scalePosition, 0);
                const double nominalStart = startBeat + stepLength * (double) stepIndex;
                const double swungStart = applyFeelSwing(nominalStart, stepLength, stepIndex, swingAmount);
                const double noteEnd = juce::jmin(slotEnd, swungStart + stepLength * gateFactor);
                addNote(swungStart, noteEnd, note);
            }

            startBeat += durationInQuarterNotes;
        }

        sequence.updateMatchedPairs();
        return sequence;
    }

    std::vector<int> previousVoicing;
    double startBeat = 0.0;

    for (int chordIndex = 0; chordIndex < chordCount; ++chordIndex)
    {
        const auto& chord = progression[(size_t) chordIndex];
        const auto voicing = buildVoicingForChord(chord, previousVoicing.empty() ? nullptr : &previousVoicing);
        const double chordEndBeat = startBeat + durationInQuarterNotes;

        if (playbackMode == 0)
        {
            const std::array<double, 9> chordSwingAmounts{ 1.00, 0.96, 0.78, 0.62, 0.70, 0.68, 0.92, 0.58, 0.88 };
            const double swingAmount = chordSwingAmounts[(size_t) juce::jlimit(0, (int) chordSwingAmounts.size() - 1, styleIndex)];
            const double swingKnob = juce::jlimit(0.50, 0.75, (double) getFloatValue("swing", 0.50f));
            const double swingDepth = juce::jlimit(0.0, 1.0, (swingKnob - 0.50) / 0.25);
            const double noteSwingSpread = swingDepth * juce::jlimit(0.02, 0.12, durationInQuarterNotes * 0.05);
            const std::array<double, 9> chordLengthFactors{ 0.94, 0.90, 0.82, 0.96, 0.78, 0.88, 0.72, 0.97, 0.95 };
            const double chordLengthFactor = chordLengthFactors[(size_t) juce::jlimit(0, (int) chordLengthFactors.size() - 1, styleIndex)];

            {
                const double swungHitStart = applyFeelSwing(startBeat,
                                                            juce::jmax(0.25, durationInQuarterNotes / 4.0),
                                                            chordIndex,
                                                            swingAmount * 0.35);
                const double hitEnd = juce::jmin(chordEndBeat,
                                                 swungHitStart + durationInQuarterNotes * chordLengthFactor * gate);

                for (int noteIndex = 0; noteIndex < (int) voicing.size(); ++noteIndex)
                {
                    const double voiceDelay = strumBeats * (double) noteIndex
                                              + noteSwingSpread * (double) juce::jmax(0, noteIndex);
                    const double noteStart = juce::jmin(chordEndBeat - 0.01,
                                                        swungHitStart + voiceDelay);
                    const double noteEnd = juce::jmin(chordEndBeat,
                                                      juce::jmax(noteStart + 0.08,
                                                                 hitEnd - noteSwingSpread * 0.35 * (double) noteIndex));
                    addNote(noteStart, noteEnd, voicing[(size_t) noteIndex]);
                }
            }
        }
        else
        {
            auto pattern = makeArpPattern(voicing, random);
            if (pattern.empty())
                pattern = voicing;

            int stepIndex = 0;
            for (double stepBeat = startBeat; stepBeat < chordEndBeat - 0.0001; stepBeat += arpStep)
            {
                int note = pattern[(size_t) (stepIndex % juce::jmax(1, (int) pattern.size()))];
                if (playbackMode == 4 && ! pattern.empty())
                    note = pattern[(size_t) random.nextInt((int) pattern.size())];

                const double swungBeat = applySwingToBeat(stepBeat, arpStep, stepIndex);
                const double noteOffBeat = juce::jmin(chordEndBeat, swungBeat + arpStep * gate);
                addNote(swungBeat, noteOffBeat, note);

                ++stepIndex;
            }
        }

        previousVoicing = voicing;
        startBeat += durationInQuarterNotes;
    }

    sequence.updateMatchedPairs();
    return sequence;
}

juce::String PluginProcessor::makeSuggestedExportBaseName() const
{
    const auto safeKey = getKeyNames()[getChoiceValue("keyRoot", 0)].replaceCharacters("#", "sharp");
    const auto safeScale = getScaleNames()[getChoiceValue("scaleMode", 16)];
    const auto& feelNames = isChordModeActive() ? getChordFeelNames() : getStyleNames();
    const auto safeStyle = feelNames[juce::jlimit(0, feelNames.size() - 1, getChoiceValue("style", 0))];
    const auto safeMode = getGenerationModeNames()[getChoiceValue("generationMode", 0)];
    return "SSP MIDI Buddy - " + safeMode + " - " + safeKey + " " + safeScale + " - " + safeStyle;
}

juce::File PluginProcessor::getDefaultExportFile() const
{
    auto desktop = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    return desktop.getChildFile(makeSuggestedExportBaseName() + ".mid");
}

juce::File PluginProcessor::writeGeneratedMidiFileTo(const juce::File& destination, bool temporaryExport)
{
    const juce::ScopedLock lock(progressionLock);

    auto file = destination;
    if (file.getFileExtension().isEmpty())
        file = file.withFileExtension(".mid");

    auto exportDirectory = file.getParentDirectory();
    if (! exportDirectory.exists() && ! exportDirectory.createDirectory())
    {
        lastExportStatus = "Couldn’t create the export folder.";
        return {};
    }

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence metaTrack;
    const auto bpm = getHostBpmFallback120();
    const int microsPerQuarter = juce::roundToInt(60000000.0 / juce::jmax(1.0, bpm));
    auto tempo = juce::MidiMessage::tempoMetaEvent(microsPerQuarter);
    tempo.setTimeStamp(0.0);
    metaTrack.addEvent(tempo);

    auto timeSig = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
    timeSig.setTimeStamp(0.0);
    metaTrack.addEvent(timeSig);
    midiFile.addTrack(metaTrack);
    midiFile.addTrack(buildGeneratedSequence(bpm));

    juce::FileOutputStream stream(file);
    if (! stream.openedOk() || ! midiFile.writeTo(stream))
    {
        lastExportStatus = temporaryExport
            ? "Couldn’t bake the temporary MIDI clip. Try dragging again."
            : "Couldn’t export the MIDI file. Try a different location.";
        return {};
    }

    stream.flush();
    lastExportStatus = temporaryExport
        ? "Baked " + file.getFileName() + ". Drag that tile into your DAW lane if the host supports file drops."
        : "Exported " + file.getFileName() + " to " + exportDirectory.getFullPathName() + ".";
    return file;
}

juce::File PluginProcessor::exportGeneratedMidiFile()
{
    auto exportDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("SSP MIDI Buddy Exports");
    exportDirectory.createDirectory();

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    const auto file = exportDirectory.getChildFile(makeSuggestedExportBaseName() + " - " + timestamp + ".mid");
    return writeGeneratedMidiFileTo(file, true);
}

juce::File PluginProcessor::exportGeneratedMidiFileTo(const juce::File& destination)
{
    return writeGeneratedMidiFileTo(destination, false);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(progressionLock);
    syncProgressionIntoState();

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    if (! restoreProgression(apvts.state.getProperty("generatedProgression").toString()))
        randomizeProgression();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
