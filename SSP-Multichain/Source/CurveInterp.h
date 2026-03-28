#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>

struct CurvePoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curve = 0.0f;
};

namespace CurveInterp
{
inline void sortAndClamp(std::vector<CurvePoint>& p)
{
    for (auto& pt : p)
    {
        pt.x = juce::jlimit(0.0f, 1.0f, pt.x);
        pt.y = juce::jlimit(0.0f, 1.0f, pt.y);
        pt.curve = juce::jlimit(-1.0f, 1.0f, pt.curve);
    }

    std::sort(p.begin(), p.end(), [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    for (size_t i = 1; i < p.size();)
    {
        if (std::abs(p[i].x - p[i - 1].x) < 1.0e-3f)
            p.erase(p.begin() + (int)i);
        else
            ++i;
    }

    if (!p.empty())
        p.back().curve = 0.0f;
}

inline float applyCurve(float t, float curve)
{
    t = juce::jlimit(0.0f, 1.0f, t);
    curve = juce::jlimit(-1.0f, 1.0f, curve);

    if (std::abs(curve) < 1.0e-4f)
        return t;

    const float shape = 1.0f + 4.0f * std::abs(curve);
    if (curve > 0.0f)
        return std::pow(t, shape);

    return 1.0f - std::pow(1.0f - t, shape);
}

inline float eval(float x, const std::vector<CurvePoint>& pts)
{
    if (pts.size() < 2)
        return 0.0f;

    x = juce::jlimit(0.0f, 1.0f, x);

    if (x <= pts.front().x)
        return pts.front().y;
    if (x >= pts.back().x)
        return pts.back().y;

    int i = 0;
    while (i < (int)pts.size() - 1 && pts[(size_t)i + 1].x < x)
        ++i;

    const float x0 = pts[(size_t)i].x;
    const float x1 = pts[(size_t)i + 1].x;
    const float span = juce::jmax(1.0e-6f, x1 - x0);
    const float t = (x - x0) / span;
    const float curvedT = applyCurve(t, pts[(size_t)i].curve);

    return juce::jlimit(0.0f, 1.0f, juce::jmap(curvedT, pts[(size_t)i].y, pts[(size_t)i + 1].y));
}

} // namespace CurveInterp
