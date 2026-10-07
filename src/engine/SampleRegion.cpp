#include "SampleRegion.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lhss
{
dsp::ResolvedRegion resolveRegion (int numFrames, double sourceRate,
                                   float sampleStart, float sampleEnd,
                                   bool loopOn, float loopStart, float loopEnd,
                                   float crossfadeMs) noexcept
{
    dsp::ResolvedRegion r;
    if (numFrames <= 1) return r;

    const double n = numFrames;
    auto clamp01 = [] (float v) { return std::isfinite (v) ? std::clamp (v, 0.0f, 1.0f) : 0.0f; };

    const double minRegion = std::min<double> (kMinRegionFrames, n);
    double start = std::round (clamp01 (sampleStart) * n);
    double end   = std::round (clamp01 (sampleEnd) * n);
    if (end < start) std::swap (start, end);
    if (end - start < minRegion)
    {
        if (start + minRegion <= n) end = start + minRegion;
        else { end = n; start = n - minRegion; }
    }
    r.start = start;
    r.end = end;

    double ls = std::clamp (std::round (clamp01 (loopStart) * n), start, end);
    double le = std::clamp (std::round (clamp01 (loopEnd) * n), start, end);
    if (le < ls) std::swap (ls, le);

    const double minLoop = kMinLoopFrames;
    if (le - ls < minLoop)
    {
        le = std::min (end, ls + minLoop);
        if (le - ls < minLoop) ls = std::max (start, le - minLoop);
    }
    r.loopStart = ls;
    r.loopEnd = le;
    r.loopOn = loopOn && (le - ls) >= minLoop;

    const double requested = std::max (0.0, static_cast<double> (std::isfinite (crossfadeMs) ? crossfadeMs : 0.0f))
                             * sourceRate / 1000.0;
    const double maxXf = std::min ({ (le - ls) * 0.5, ls, n - le });
    r.crossfade = std::clamp (requested, 0.0, std::max (0.0, maxXf));
    return r;
}
} // namespace lhss
