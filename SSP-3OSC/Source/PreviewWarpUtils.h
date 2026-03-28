#pragma once

#include <JuceHeader.h>

namespace previewwarp
{
enum ModeIndex
{
    off = 0,
    fm,
    sync,
    bendPlus,
    bendMinus,
    phasePlus,
    phaseMinus,
    mirror,
    wrap,
    pinch
};

inline float wrapPhase(float phase)
{
    phase -= std::floor(phase);
    if (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

inline float renderBaseWave(int waveIndex, float phase)
{
    switch (waveIndex)
    {
        case 0: return std::sin(juce::MathConstants<float>::twoPi * phase);
        case 1: return 2.0f * phase - 1.0f;
        case 2: return phase < 0.5f ? 1.0f : -1.0f;
        case 3: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        default: break;
    }

    return 0.0f;
}

inline float bendPhase(float phase, float amount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float wrappedPhase = wrapPhase(phase);

    if (clampedAmount <= 0.0001f)
        return wrappedPhase;

    const float pivot = juce::jmap(clampedAmount, 0.0f, 1.0f, 0.5f, 0.14f);
    if (wrappedPhase < pivot)
        return 0.5f * (wrappedPhase / juce::jmax(0.0001f, pivot));

    return 0.5f + 0.5f * ((wrappedPhase - pivot) / juce::jmax(0.0001f, 1.0f - pivot));
}

inline float renderPreviewSource(int sourceIndex, float phase, float mutateAmount)
{
    switch (sourceIndex)
    {
        case 0: return renderBaseWave(1, wrapPhase(phase * 1.00f));
        case 1: return renderBaseWave(2, wrapPhase(phase * 1.07f + 0.17f));
        case 2: return renderBaseWave(3, wrapPhase(phase * 0.53f + 0.29f));
        case 3: return std::sin(juce::MathConstants<float>::twoPi * phase * 7.0f) * 0.50f
                      + std::sin(juce::MathConstants<float>::twoPi * phase * 11.0f) * 0.20f;
        case 4: return std::sin(juce::MathConstants<float>::twoPi * wrapPhase(phase * 0.5f));
        case 5: return 0.65f * std::sin(juce::MathConstants<float>::twoPi * phase * 0.18f)
                      + 0.35f * std::sin(juce::MathConstants<float>::twoPi * phase * 1.15f);
        case 6: return std::sin(juce::MathConstants<float>::twoPi * wrapPhase(phase * (2.5f + mutateAmount * 2.0f)
                                                                              + 0.20f * std::sin(juce::MathConstants<float>::twoPi * phase * 3.0f)));
        default: break;
    }

    return 0.0f;
}

inline float applyWarpMode(float phase, int modeIndex, float amount, int fmSourceIndex, float mutateAmount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (clampedAmount <= 0.0001f || modeIndex == off)
        return phase;

    switch (modeIndex)
    {
        case fm:
        {
            const float modulator = renderPreviewSource(fmSourceIndex, phase, mutateAmount);
            const float fmDepth = clampedAmount * (0.18f + clampedAmount * (0.34f + mutateAmount * 0.22f));
            return wrapPhase(phase + fmDepth * modulator);
        }

        case sync:
            return wrapPhase(phase * (1.0f + clampedAmount * 7.0f));

        case bendPlus:
            return bendPhase(phase, clampedAmount);

        case bendMinus:
            return 1.0f - bendPhase(1.0f - phase, clampedAmount);

        case phasePlus:
            return wrapPhase(phase + clampedAmount * 0.35f);

        case phaseMinus:
            return wrapPhase(phase - clampedAmount * 0.35f);

        case mirror:
        {
            const float mirrored = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
            return wrapPhase(juce::jmap(clampedAmount, phase, mirrored));
        }

        case wrap:
            return wrapPhase((phase - 0.5f) * (1.0f + clampedAmount * 6.0f) + 0.5f);

        case pinch:
        {
            const float exponent = 1.0f + clampedAmount * 5.0f;
            if (phase < 0.5f)
                return 0.5f * std::pow(phase * 2.0f, exponent);

            return 1.0f - 0.5f * std::pow((1.0f - phase) * 2.0f, exponent);
        }

        default:
            break;
    }

    return phase;
}

inline float applyLegacyWarp(float phase,
                             float fmAmount,
                             float syncAmount,
                             float bendAmount,
                             int fmSourceIndex,
                             float mutateAmount)
{
    phase = applyWarpMode(phase, fm, fmAmount, fmSourceIndex, mutateAmount);
    phase = applyWarpMode(phase, sync, syncAmount, fmSourceIndex, mutateAmount);
    phase = applyWarpMode(phase, bendPlus, bendAmount, fmSourceIndex, mutateAmount);
    return phase;
}
}
