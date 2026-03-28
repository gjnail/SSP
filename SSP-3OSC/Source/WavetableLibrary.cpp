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

enum class RecipeStyle
{
    spectral,
    pwm,
    vocal,
    metallic,
    growl,
    reese,
    organ,
    sync,
    digital,
    comb,
    fm,
    fold,
    hollow,
    bell,
    phase
};

struct TableRecipe
{
    const char* name = "New Table";
    RecipeStyle style = RecipeStyle::spectral;
    float rolloffStart = 1.0f;
    float rolloffEnd = 1.0f;
    float clusterRate = 0.2f;
    float clusterDepth = 0.5f;
    float formantAStart = 3.0f;
    float formantAEnd = 12.0f;
    float formantBStart = 8.0f;
    float formantBEnd = 24.0f;
    float phaseRate = 0.2f;
    float phaseDepth = 0.5f;
    float oddEvenBlend = 0.5f;
    float air = 0.2f;
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

Table buildRecipeTable(const TableRecipe& recipe)
{
    Table table;
    table.name = recipe.name;

    for (int frameIndex = 0; frameIndex < wavetable::frameCount; ++frameIndex)
    {
        const float morph = (float) frameIndex / (float) juce::jmax(1, wavetable::frameCount - 1);
        table.frames[(size_t) frameIndex] = buildFrame(
            [recipe, morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float rolloff = juce::jmap(morph, recipe.rolloffStart, recipe.rolloffEnd);
                const float centreA = juce::jmap(morph, recipe.formantAStart, recipe.formantAEnd);
                const float centreB = juce::jmap(morph, recipe.formantBStart, recipe.formantBEnd);
                const bool even = (harmonic % 2) == 0;
                const float spectral = 1.0f / std::pow(harmonicF, rolloff);
                const float cluster = 0.25f + recipe.clusterDepth
                    * std::abs(std::sin(harmonicF * recipe.clusterRate + morph * (2.0f + recipe.phaseRate * 6.0f)));
                const float formantA = gaussian(harmonicF, centreA, 1.3f + morph * 1.5f);
                const float formantB = gaussian(harmonicF, centreB, 1.8f + morph * 2.4f);
                const float oddWeight = even ? juce::jmap(recipe.oddEvenBlend, 1.0f, 0.12f + morph * 0.82f) : 1.0f;
                const float airBoost = harmonic > juce::jmap(morph, 10.0f, 40.0f) ? (0.35f + recipe.air * morph) : 1.0f;

                switch (recipe.style)
                {
                    case RecipeStyle::spectral:
                        return (spectral * (0.55f + cluster) * oddWeight + formantA * 0.78f + formantB * 0.62f) * airBoost;

                    case RecipeStyle::pwm:
                    {
                        const float pwmParity = even ? juce::jmap(morph, 0.04f, 0.95f) : 1.0f;
                        const float bite = 0.25f + 0.75f * std::abs(std::sin(harmonicF * (recipe.clusterRate * 1.8f) + morph * 8.0f));
                        return spectral * pwmParity * bite * (0.9f + recipe.air * 0.2f);
                    }

                    case RecipeStyle::vocal:
                    {
                        const float vowelA = formantA * (0.95f + morph * 0.2f);
                        const float vowelB = formantB * (0.75f + (1.0f - morph) * 0.35f);
                        const float bed = 0.12f / std::pow(harmonicF, rolloff + 0.18f);
                        return (bed + vowelA + vowelB) * oddWeight;
                    }

                    case RecipeStyle::metallic:
                    {
                        const float shimmer = 0.25f + 0.75f * std::abs(std::cos(harmonicF * 0.19f + morph * 5.4f));
                        return cluster * shimmer / std::pow(harmonicF, juce::jmax(0.65f, rolloff - 0.22f));
                    }

                    case RecipeStyle::growl:
                    {
                        const float notch = 0.22f + 0.78f * (1.0f - gaussian(harmonicF, centreA + 3.0f, 2.8f));
                        const float rumble = 1.0f / std::pow(harmonicF, juce::jmax(0.78f, rolloff - 0.14f));
                        return rumble * cluster * notch;
                    }

                    case RecipeStyle::reese:
                    {
                        const float comb = juce::jmax(0.0f, 0.5f + 0.5f * std::cos(harmonicF * recipe.clusterRate + morph * 5.2f));
                        return comb * (0.22f + cluster) / std::pow(harmonicF, juce::jmax(0.72f, rolloff - 0.1f));
                    }

                    case RecipeStyle::organ:
                    {
                        const float lowDrawbar = harmonic <= 8 ? 1.0f : 0.28f + recipe.air * 0.22f;
                        const float harmonicStripe = (harmonic % 3) == 0 ? 0.88f : ((harmonic % 2) == 0 ? 0.52f : 1.0f);
                        return spectral * lowDrawbar * harmonicStripe;
                    }

                    case RecipeStyle::sync:
                    {
                        const float cutoff = juce::jmap(morph, 8.0f, 42.0f);
                        const float upper = harmonicF <= cutoff ? 1.0f : 0.16f + recipe.air * 0.45f;
                        const float tearing = 0.35f + 0.65f * std::abs(std::sin(harmonicF * 0.41f + morph * 7.2f));
                        return spectral * upper * tearing;
                    }

                    case RecipeStyle::digital:
                    {
                        const float bitPattern = 0.2f + 0.8f * std::abs(std::sin((float) ((harmonic * 5) % 17) + morph * 9.0f));
                        const float aliasLift = harmonic > 18 ? (0.55f + recipe.air * 0.3f) : 1.0f;
                        return spectral * bitPattern * aliasLift;
                    }

                    case RecipeStyle::comb:
                    {
                        const float combNotch = 0.25f + 0.75f * (1.0f - gaussian(std::fmod(harmonicF * recipe.clusterRate, 9.0f), 4.5f, 1.2f));
                        return spectral * combNotch * (0.7f + cluster * 0.4f);
                    }

                    case RecipeStyle::fm:
                    {
                        const float carrier = gaussian(harmonicF, centreA, 1.0f + morph * 1.2f);
                        const float sideA = gaussian(harmonicF, centreB, 1.4f + morph * 1.8f);
                        const float sideB = gaussian(harmonicF, centreB + centreA * 0.35f, 1.8f + morph * 1.6f);
                        return spectral * 0.18f + carrier + sideA * 0.92f + sideB * 0.66f;
                    }

                    case RecipeStyle::fold:
                    {
                        const float foldRipple = 0.2f + 0.8f * std::abs(std::sin(harmonicF * 0.28f + morph * 10.0f));
                        return spectral * foldRipple * (harmonicF > 12.0f ? 1.0f + morph * 0.6f : 1.0f);
                    }

                    case RecipeStyle::hollow:
                    {
                        const float hollowParity = even ? 0.05f + morph * 0.18f : 1.0f;
                        return spectral * hollowParity * (0.85f + formantA * 0.18f + formantB * 0.12f);
                    }

                    case RecipeStyle::bell:
                    {
                        const float bellPeakA = gaussian(harmonicF, centreA, 0.8f + morph * 1.2f);
                        const float bellPeakB = gaussian(harmonicF, centreB, 1.2f + morph * 1.4f);
                        const float sparkle = harmonic > 14.0f ? (0.3f + recipe.air * 0.45f) : 1.0f;
                        return (spectral * 0.12f + bellPeakA + bellPeakB * 0.85f) * sparkle;
                    }

                    case RecipeStyle::phase:
                    {
                        const float smooth = 0.65f + 0.35f * std::cos(harmonicF * recipe.clusterRate * 0.7f + morph * 3.8f);
                        return spectral * smooth + formantA * 0.25f + formantB * 0.18f;
                    }
                }

                return spectral;
            },
            [recipe, morph] (int harmonic)
            {
                const float harmonicF = (float) harmonic;
                const float wobble = std::sin(harmonicF * recipe.phaseRate + morph * 5.0f) * recipe.phaseDepth * morph;
                const float skew = (harmonic % 3) == 0 ? morph * 0.18f : -morph * 0.07f;

                switch (recipe.style)
                {
                    case RecipeStyle::metallic: return wobble * 1.4f + harmonicF * 0.11f * morph;
                    case RecipeStyle::growl:    return wobble * 0.8f + ((harmonic % 3) == 0 ? -0.18f : 0.08f) * morph;
                    case RecipeStyle::reese:    return std::sin(harmonicF * 0.23f) * recipe.phaseDepth * morph;
                    case RecipeStyle::sync:     return wobble * 1.2f + skew * 0.6f;
                    case RecipeStyle::digital:  return ((harmonic % 4) == 0 ? 0.3f : -0.16f) * morph + wobble * 0.4f;
                    case RecipeStyle::fm:       return wobble * 1.5f + harmonicF * 0.06f * morph;
                    case RecipeStyle::fold:     return wobble + std::sin(harmonicF * 0.44f) * 0.2f * morph;
                    case RecipeStyle::bell:     return wobble * 1.1f + harmonicF * 0.045f * morph;
                    case RecipeStyle::phase:    return wobble * 1.8f + skew;
                    case RecipeStyle::pwm:
                    case RecipeStyle::vocal:
                    case RecipeStyle::organ:
                    case RecipeStyle::comb:
                    case RecipeStyle::hollow:
                    case RecipeStyle::spectral:
                    default:                    return wobble + skew;
                }
            });
    }

    return table;
}

const std::vector<Table>& getLibrary()
{
    static const std::vector<Table> library = []
    {
        std::vector<Table> tables{
            buildSpectralSweepTable(),
            buildPwmEdgeTable(),
            buildVowelGlassTable(),
            buildMetalBloomTable(),
            buildGrowlFoldTable(),
            buildReeseMotionTable()
        };

        static const std::array<TableRecipe, 35> extraRecipes{{
            { "Brute Sync",     RecipeStyle::sync,     0.86f, 0.60f, 0.40f, 0.96f, 6.0f, 18.0f, 17.0f, 40.0f, 0.39f, 0.90f, 0.24f, 0.16f },
            { "Prism Pulse",    RecipeStyle::pwm,      0.92f, 0.72f, 0.27f, 0.88f, 5.0f, 15.0f, 14.0f, 33.0f, 0.22f, 0.50f, 0.16f, 0.18f },
            { "Solar Throat",   RecipeStyle::vocal,    1.18f, 0.92f, 0.19f, 0.55f, 2.5f, 8.0f, 9.0f, 20.0f, 0.16f, 0.34f, 0.70f, 0.18f },
            { "Glass Choir",    RecipeStyle::vocal,    1.35f, 1.02f, 0.14f, 0.48f, 3.0f, 7.0f, 11.0f, 18.0f, 0.12f, 0.28f, 0.82f, 0.26f },
            { "Hyper Saw",      RecipeStyle::sync,     0.88f, 0.64f, 0.26f, 0.92f, 5.5f, 14.0f, 15.0f, 32.0f, 0.22f, 0.46f, 0.34f, 0.20f },
            { "Phase Wire",     RecipeStyle::phase,    1.10f, 0.86f, 0.43f, 0.72f, 6.0f, 18.0f, 12.0f, 34.0f, 0.44f, 0.90f, 0.58f, 0.12f },
            { "Circuit Pulse",  RecipeStyle::digital,  0.96f, 0.74f, 0.37f, 0.88f, 4.0f, 13.0f, 9.0f, 22.0f, 0.24f, 0.56f, 0.18f, 0.16f },
            { "Nova Mouth",     RecipeStyle::vocal,    1.22f, 0.94f, 0.21f, 0.62f, 2.0f, 6.0f, 8.0f, 16.0f, 0.20f, 0.32f, 0.74f, 0.24f },
            { "Velvet Razor",   RecipeStyle::spectral, 1.05f, 0.79f, 0.29f, 0.66f, 3.5f, 10.0f, 14.0f, 30.0f, 0.27f, 0.52f, 0.44f, 0.18f },
            { "Chrome Dust",    RecipeStyle::metallic, 1.28f, 1.04f, 0.52f, 0.78f, 7.0f, 20.0f, 19.0f, 40.0f, 0.48f, 0.82f, 0.64f, 0.10f },
            { "Hollow Bloom",   RecipeStyle::hollow,   1.44f, 1.10f, 0.17f, 0.54f, 2.0f, 9.0f, 7.0f, 19.0f, 0.15f, 0.26f, 0.88f, 0.22f },
            { "Turbine Fold",   RecipeStyle::fold,     0.90f, 0.68f, 0.41f, 0.86f, 5.0f, 16.0f, 13.0f, 31.0f, 0.34f, 0.76f, 0.30f, 0.18f },
            { "Ghost Reed",     RecipeStyle::vocal,    1.30f, 0.98f, 0.23f, 0.58f, 3.0f, 8.0f, 10.0f, 18.0f, 0.18f, 0.36f, 0.72f, 0.20f },
            { "Obsidian Edge",  RecipeStyle::sync,     0.98f, 0.60f, 0.47f, 0.94f, 6.0f, 17.0f, 16.0f, 38.0f, 0.50f, 0.96f, 0.26f, 0.12f },
            { "Pixel Drift",    RecipeStyle::digital,  1.16f, 0.88f, 0.55f, 0.74f, 7.0f, 19.0f, 11.0f, 29.0f, 0.58f, 0.70f, 0.46f, 0.14f },
            { "Amber Comb",     RecipeStyle::comb,     1.08f, 0.82f, 0.25f, 0.63f, 4.0f, 12.0f, 15.0f, 27.0f, 0.30f, 0.42f, 0.52f, 0.16f },
            { "Mantis Growl",   RecipeStyle::growl,    0.94f, 0.66f, 0.33f, 0.90f, 3.0f, 14.0f, 9.0f, 26.0f, 0.31f, 0.84f, 0.36f, 0.18f },
            { "Halo Sine",      RecipeStyle::hollow,   1.52f, 1.16f, 0.11f, 0.36f, 1.8f, 5.0f, 6.5f, 12.0f, 0.10f, 0.18f, 0.94f, 0.28f },
            { "Toxic Glass",    RecipeStyle::spectral, 1.12f, 0.84f, 0.35f, 0.76f, 5.0f, 13.0f, 12.0f, 25.0f, 0.29f, 0.60f, 0.60f, 0.16f },
            { "Future Brass",   RecipeStyle::organ,    1.00f, 0.76f, 0.18f, 0.64f, 2.8f, 7.0f, 8.0f, 17.0f, 0.14f, 0.34f, 0.66f, 0.20f },
            { "Orbital PWM",    RecipeStyle::pwm,      0.92f, 0.72f, 0.27f, 0.88f, 5.0f, 15.0f, 14.0f, 33.0f, 0.22f, 0.50f, 0.16f, 0.18f },
            { "Laser Pipe",     RecipeStyle::phase,    1.06f, 0.78f, 0.39f, 0.82f, 6.0f, 16.0f, 17.0f, 36.0f, 0.42f, 0.74f, 0.42f, 0.14f },
            { "Cloud Metal",    RecipeStyle::metallic, 1.24f, 0.96f, 0.46f, 0.72f, 7.0f, 18.0f, 19.0f, 42.0f, 0.54f, 0.88f, 0.62f, 0.12f },
            { "Throat Resin",   RecipeStyle::vocal,    1.26f, 0.92f, 0.20f, 0.57f, 2.4f, 7.0f, 8.4f, 15.0f, 0.17f, 0.30f, 0.78f, 0.22f },
            { "Vector Mist",    RecipeStyle::spectral, 1.38f, 1.02f, 0.31f, 0.60f, 4.2f, 10.0f, 15.0f, 24.0f, 0.36f, 0.44f, 0.70f, 0.18f },
            { "Quartz Bite",    RecipeStyle::sync,     1.02f, 0.72f, 0.49f, 0.92f, 6.0f, 17.0f, 18.0f, 34.0f, 0.46f, 0.92f, 0.28f, 0.14f },
            { "Falcon Sweep",   RecipeStyle::sync,     0.96f, 0.70f, 0.24f, 0.72f, 4.0f, 18.0f, 12.0f, 30.0f, 0.21f, 0.46f, 0.48f, 0.20f },
            { "Ion Harp",       RecipeStyle::bell,     1.18f, 0.90f, 0.28f, 0.66f, 3.0f, 9.0f, 14.0f, 26.0f, 0.24f, 0.58f, 0.56f, 0.18f },
            { "Stereo Shard",   RecipeStyle::phase,    1.14f, 0.82f, 0.44f, 0.80f, 6.0f, 14.0f, 16.0f, 35.0f, 0.52f, 0.78f, 0.54f, 0.14f },
            { "Tape Grit",      RecipeStyle::digital,  1.32f, 1.08f, 0.22f, 0.52f, 3.0f, 11.0f, 9.0f, 23.0f, 0.19f, 0.24f, 0.68f, 0.12f },
            { "Plasma Organ",   RecipeStyle::organ,    1.08f, 0.86f, 0.16f, 0.60f, 2.0f, 6.0f, 7.0f, 14.0f, 0.13f, 0.28f, 0.84f, 0.20f },
            { "Night Vowel",    RecipeStyle::vocal,    1.34f, 1.00f, 0.15f, 0.50f, 2.2f, 5.5f, 7.0f, 13.0f, 0.11f, 0.22f, 0.88f, 0.24f },
            { "Cyber Pluck",    RecipeStyle::fm,       0.98f, 0.74f, 0.34f, 0.86f, 5.0f, 16.0f, 13.0f, 28.0f, 0.33f, 0.72f, 0.38f, 0.18f },
            { "Reactor Reed",   RecipeStyle::fm,       1.08f, 0.82f, 0.26f, 0.74f, 4.0f, 10.0f, 12.0f, 24.0f, 0.25f, 0.54f, 0.58f, 0.18f },
            { "Prism Choir",    RecipeStyle::vocal,    1.26f, 0.94f, 0.18f, 0.56f, 2.8f, 8.5f, 10.0f, 18.0f, 0.16f, 0.32f, 0.76f, 0.26f }
        }};

        for (const auto& recipe : extraRecipes)
            tables.push_back(buildRecipeTable(recipe));

        return tables;
    }();

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
