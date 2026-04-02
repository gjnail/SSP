#pragma once

#include <JuceHeader.h>

namespace ssp::waveshaper
{
constexpr int frameCount = 8;

enum class OverflowMode
{
    wrap = 0,
    clamp,
    mirror
};

inline juce::StringArray getTableNames()
{
    return {"Prism", "Fold", "Vapor", "Razor", "Acid"};
}

inline juce::StringArray getOverflowModeNames()
{
    return {"Wrap", "Clamp", "Mirror"};
}

inline juce::String getTableDescription(int tableIndex)
{
    switch (tableIndex)
    {
        case 0: return "Glossy harmonic sweeps with glassy upper partials.";
        case 1: return "Progressive wavefolding that turns clean tones into snarling edges.";
        case 2: return "Liquid phase-bent motion with softer, vocal-style saturation.";
        case 3: return "Stepped clipping and jagged contouring for aggressive bites.";
        case 4: return "Asymmetric acid curves with sharp, animated harmonic push.";
        default: return "Morphing distortion table.";
    }
}

inline float wrapInput(float x) noexcept
{
    auto wrapped = std::fmod(x + 1.0f, 2.0f);
    if (wrapped < 0.0f)
        wrapped += 2.0f;

    return wrapped - 1.0f;
}

inline float mirrorInput(float x) noexcept
{
    auto mirrored = std::fmod(x + 1.0f, 4.0f);
    if (mirrored < 0.0f)
        mirrored += 4.0f;

    return mirrored <= 2.0f ? mirrored - 1.0f : 3.0f - mirrored;
}

inline float applyOverflow(float x, OverflowMode mode) noexcept
{
    switch (mode)
    {
        case OverflowMode::wrap: return wrapInput(x);
        case OverflowMode::clamp: return juce::jlimit(-1.0f, 1.0f, x);
        case OverflowMode::mirror: return mirrorInput(x);
    }

    return juce::jlimit(-1.0f, 1.0f, x);
}

inline float sampleSingleFrame(int tableIndex, float frameNorm, float x) noexcept
{
    const auto pi = juce::MathConstants<float>::pi;
    const auto halfPi = juce::MathConstants<float>::halfPi;
    const auto cube = x * x * x;
    const auto quint = cube * x * x;

    switch (tableIndex)
    {
        case 0:
        {
            const auto soft = std::tanh(x * (1.15f + frameNorm * 2.8f));
            const auto prism = juce::jlimit(-1.25f,
                                            1.25f,
                                            (x + (0.22f + 0.28f * frameNorm) * cube - 0.10f * frameNorm * quint)
                                                * (1.0f + 0.18f * frameNorm));
            const auto shimmer = 0.22f * std::sin((0.7f + frameNorm * 1.8f) * x * pi);
            return juce::jlimit(-1.25f, 1.25f, juce::jmap(0.18f + 0.58f * frameNorm, soft, prism + shimmer));
        }

        case 1:
        {
            const auto folded = std::asin(std::sin(x * pi * (1.0f + frameNorm * 3.2f))) / halfPi;
            const auto body = std::tanh(x * (1.0f + frameNorm * 2.2f));
            const auto lift = folded * (0.86f + frameNorm * 0.22f);
            return juce::jlimit(-1.25f, 1.25f, juce::jmap(0.26f + 0.56f * frameNorm, body, lift));
        }

        case 2:
        {
            const auto bentX = x + 0.20f * std::sin(x * pi * (1.0f + frameNorm * 2.2f));
            const auto rounded = std::tanh(x * (0.95f + frameNorm * 1.9f));
            const auto vapor = std::sin(bentX * pi * (0.45f + frameNorm * 1.55f));
            return juce::jlimit(-1.2f, 1.2f, juce::jmap(0.42f + 0.33f * frameNorm, rounded, vapor));
        }

        case 3:
        {
            const auto steps = 4.0f + frameNorm * 18.0f;
            const auto stepped = std::round(x * steps) / steps;
            const auto clipped = juce::jlimit(-1.0f, 1.0f, x * (1.0f + frameNorm * 4.4f));
            const auto razor = clipped - 0.16f * std::sin(clipped * pi * (1.5f + frameNorm * 4.5f));
            return juce::jlimit(-1.15f, 1.15f, juce::jmap(0.35f + 0.45f * frameNorm, std::tanh(razor * 1.3f), stepped));
        }

        case 4:
        {
            const auto offset = 0.14f + frameNorm * 0.22f;
            const auto positive = std::tanh((x + offset) * (1.05f + frameNorm * 2.7f));
            const auto negativeInput = x - offset * 0.7f;
            const auto negative = negativeInput / (1.0f + std::abs(negativeInput) * (0.8f + frameNorm * 3.2f));
            const auto asym = x >= 0.0f ? positive : negative;
            const auto ripple = 0.18f * frameNorm * std::sin(x * pi * (1.0f + frameNorm * 3.4f));
            return juce::jlimit(-1.22f, 1.22f, asym + ripple);
        }

        default:
            return std::tanh(x);
    }
}

inline float sampleTable(int tableIndex,
                         float framePosition,
                         float x,
                         float smoothAmount,
                         OverflowMode overflowMode) noexcept
{
    const auto sampleAt = [&](float sampleX, float frameNorm)
    {
        return sampleSingleFrame(tableIndex, frameNorm, applyOverflow(sampleX, overflowMode));
    };

    const auto clampedFrame = juce::jlimit(0.0f, 1.0f, framePosition) * (float) (frameCount - 1);
    const auto lowerFrame = juce::jlimit(0, frameCount - 1, (int) std::floor(clampedFrame));
    const auto upperFrame = juce::jlimit(0, frameCount - 1, lowerFrame + 1);
    const auto frameBlend = clampedFrame - (float) lowerFrame;

    const auto frameNormA = (float) lowerFrame / (float) (frameCount - 1);
    const auto frameNormB = (float) upperFrame / (float) (frameCount - 1);

    const auto smooth = juce::jlimit(0.0f, 1.0f, smoothAmount);
    const auto width = juce::jmap(smooth, 0.0f, 1.0f, 0.0f, 0.34f);

    const auto baseA = sampleAt(x, frameNormA);
    const auto nearA1 = sampleAt(x - width * 0.45f, frameNormA);
    const auto nearA2 = sampleAt(x + width * 0.45f, frameNormA);
    const auto farA1 = sampleAt(x - width, frameNormA);
    const auto farA2 = sampleAt(x + width, frameNormA);
    const auto smoothedA = (baseA * 4.0f + nearA1 * 2.0f + nearA2 * 2.0f + farA1 + farA2) / 10.0f;

    const auto baseB = sampleAt(x, frameNormB);
    const auto nearB1 = sampleAt(x - width * 0.45f, frameNormB);
    const auto nearB2 = sampleAt(x + width * 0.45f, frameNormB);
    const auto farB1 = sampleAt(x - width, frameNormB);
    const auto farB2 = sampleAt(x + width, frameNormB);
    const auto smoothedB = (baseB * 4.0f + nearB1 * 2.0f + nearB2 * 2.0f + farB1 + farB2) / 10.0f;

    const auto shapedA = juce::jmap(smooth, baseA, smoothedA);
    const auto shapedB = juce::jmap(smooth, baseB, smoothedB);

    return juce::jlimit(-1.3f, 1.3f, juce::jmap(frameBlend, shapedA, shapedB));
}
} // namespace ssp::waveshaper
