#include "WavetableLibrary.h"

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr int maxHarmonics = 48;

using Frame = std::array<float, wavetable::frameSize>;

struct Table
{
    juce::String name;
    std::array<Frame, wavetable::frameCount> frames{};
};

float wrapPhase(float phase)
{
    phase -= std::floor(phase);
    if (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

float gaussian(float x, float centre, float width)
{
    const float safeWidth = juce::jmax(0.0001f, width);
    const float delta = (x - centre) / safeWidth;
    return std::exp(-delta * delta);
}

void normaliseFrame(Frame& frame)
{
    double mean = 0.0;
    for (auto sample : frame)
        mean += sample;

    mean /= (double) frame.size();

    float peak = 0.0f;
    for (auto& sample : frame)
    {
        sample -= (float) mean;
        peak = juce::jmax(peak, std::abs(sample));
    }

    if (peak < 1.0e-5f)
        return;

    const float gain = 0.97f / peak;
    for (auto& sample : frame)
        sample = juce::jlimit(-1.0f, 1.0f, sample * gain);
}

template <typename AmpFn, typename PhaseFn>
Frame buildFrame(AmpFn&& amplitudeForHarmonic, PhaseFn&& phaseForHarmonic)
{
    Frame frame{};

    for (int i = 0; i < wavetable::frameSize; ++i)
    {
        const float phase = (float) i / (float) wavetable::frameSize;
        double value = 0.0;

        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
        {
            const float amplitude = juce::jmax(0.0f, amplitudeForHarmonic(harmonic));
            if (amplitude <= 1.0e-5f)
                continue;

            value += amplitude * std::sin(twoPi * phase * (float) harmonic + phaseForHarmonic(harmonic));
        }

        frame[(size_t) i] = (float) value;
    }

    normaliseFrame(frame);
    return frame;
}

Table buildSpectralSweepTable()
{
    Table table;
    table.name = "Spectral Sweep";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const float rolloff = 2.8f - morph * 1.9f;
                const float topEnd = harmonic > (int) juce::jmap(morph, 12.0f, 42.0f) ? 0.22f : 1.0f;
                return topEnd / std::pow((float) harmonic, rolloff);
            },
            [] (int)
            {
                return 0.0f;
            });
    }

    return table;
}

Table buildPwmEdgeTable()
{
    Table table;
    table.name = "PWM Edge";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const bool even = (harmonic % 2) == 0;
                const float parityWeight = even ? (0.06f + morph * 0.92f) : 1.0f;
                const float motion = 0.35f + 0.65f * std::abs(std::sin((float) harmonic * 0.33f + morph * 3.6f));
                return parityWeight * motion / std::pow((float) harmonic, 1.15f);
            },
            [morph] (int harmonic)
            {
                return (harmonic % 2) == 0 ? morph * 0.85f : 0.0f;
            });
    }

    return table;
}

Table buildVowelGlassTable()
{
    Table table;
    table.name = "Vowel Glass";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float centreA = juce::jmap(morph, 2.6f, 9.5f);
                const float centreB = juce::jmap(morph, 8.5f, 23.0f);
                const float centreC = juce::jmap(morph, 18.0f, 33.0f);
                const float formantA = gaussian(harmonicF, centreA, 1.4f + morph * 1.0f);
                const float formantB = gaussian(harmonicF, centreB, 2.0f + morph * 1.8f) * 0.95f;
                const float formantC = gaussian(harmonicF, centreC, 3.5f) * (0.20f + morph * 0.70f);
                const float bed = 0.18f / std::pow(harmonicF, 1.35f);
                return (bed + formantA + formantB + formantC) / std::pow(harmonicF, 0.18f);
            },
            [] (int)
            {
                return 0.0f;
            });
    }

    return table;
}

Table buildMetalBloomTable()
{
    Table table;
    table.name = "Metal Bloom";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float cluster = 0.12f + 0.88f * std::abs(std::sin(harmonicF * (0.64f + morph * 1.32f)));
                const float shimmer = 0.35f + 0.65f * std::abs(std::cos(harmonicF * 0.18f + morph * 4.8f));
                return cluster * shimmer / std::pow(harmonicF, 0.92f + morph * 0.28f);
            },
            [morph] (int harmonic)
            {
                return morph * (float) harmonic * 0.19f;
            });
    }

    return table;
}

Table buildGrowlFoldTable()
{
    Table table;
    table.name = "Growl Fold";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float lowBody = 1.0f / std::pow(harmonicF, 0.95f);
                const float ripple = 0.30f + 0.70f * std::abs(std::sin(morph * 6.2f + harmonicF * 0.47f));
                const float notch = 0.25f + 0.75f * (1.0f - gaussian(harmonicF, juce::jmap(morph, 6.0f, 18.0f), 2.8f));
                return lowBody * ripple * notch;
            },
            [morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float direction = (harmonic % 3) == 0 ? 1.0f : -0.45f;
                return direction * morph * (0.16f + 0.84f / juce::jmax(1.0f, harmonicF));
            });
    }

    return table;
}

Table buildReeseMotionTable()
{
    Table table;
    table.name = "Reese Motion";

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float comb = 0.55f + 0.45f * std::cos(harmonicF * (0.18f + morph * 0.18f));
                const float motion = 0.20f + 0.80f * std::abs(std::sin(harmonicF * 0.11f + morph * 5.0f));
                return juce::jmax(0.0f, comb) * motion / std::pow(harmonicF, 0.88f);
            },
            [morph] (int harmonic)
            {
                return std::sin((float) harmonic * 0.21f) * morph * 0.78f;
            });
    }

    return table;
}

const std::vector<Table>& getLibrary()
{
    static const std::vector<Table> library{
        buildSpectralSweepTable(),
        buildPwmEdgeTable(),
        buildVowelGlassTable(),
        buildMetalBloomTable(),
        buildGrowlFoldTable(),
        buildReeseMotionTable()
    };

    return library;
}

float readFrameSample(const Frame& frame, float phase)
{
    const float wrappedPhase = wrapPhase(phase);
    const float readPosition = wrappedPhase * (float) wavetable::frameSize;
    const int indexA = juce::jlimit(0, wavetable::frameSize - 1, (int) readPosition);
    const int indexB = (indexA + 1) % wavetable::frameSize;
    const float fraction = readPosition - (float) indexA;
    return juce::jmap(fraction, frame[(size_t) indexA], frame[(size_t) indexB]);
}
}

const juce::StringArray& wavetable::getTableNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray items;
        for (const auto& table : getLibrary())
            items.add(table.name);
        return items;
    }();

    return names;
}

float wavetable::renderSample(int tableIndex, float position, float phase)
{
    const auto& library = getLibrary();
    if (library.empty())
        return 0.0f;

    const int safeTableIndex = juce::jlimit(0, (int) library.size() - 1, tableIndex);
    const auto& table = library[(size_t) safeTableIndex];
    const float clampedPosition = juce::jlimit(0.0f, 1.0f, position);
    const float framePosition = clampedPosition * (float) (frameCount - 1);
    const int frameA = juce::jlimit(0, frameCount - 1, (int) std::floor(framePosition));
    const int frameB = juce::jlimit(0, frameCount - 1, frameA + 1);
    const float frameFraction = framePosition - (float) frameA;
    const float sampleA = readFrameSample(table.frames[(size_t) frameA], phase);
    const float sampleB = readFrameSample(table.frames[(size_t) frameB], phase);
    return juce::jmap(frameFraction, sampleA, sampleB);
}
