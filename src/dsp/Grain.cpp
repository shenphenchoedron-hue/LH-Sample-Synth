#include "Grain.h"

#include <algorithm>

namespace lhss::dsp
{
namespace
{
constexpr double kEdgeFade = 64.0; // frames: soft mask outside the active region (no hard cut)
}

bool Grain::render (const SourceView& src, const ResolvedRegion& region, const float* window, int tableSize,
                    float gain, float* outL, float* outR, int n) noexcept
{
    if (! active) return false;

    const double phaseInc = static_cast<double> (tableSize) / static_cast<double> (length);
    const double step = increment * direction;
    const bool canWrap = wrapInLoop && region.loopOn && region.loopLength() >= 1.0;
    const float gl = gainL * gain, gr = gainR * gain;
    const int count = std::min (n, length - age);

    double phase = age * phaseInc;
    for (int i = 0; i < count; ++i)
    {
        // Window lookup with linear interpolation.
        const int wi = std::min (static_cast<int> (phase), tableSize - 1);
        const float wf = static_cast<float> (phase - wi);
        float w = window[wi] + (window[wi + 1] - window[wi]) * wf;
        phase += phaseInc;

        if (! wrapInLoop)
        {
            // Content outside [start, end) fades out quickly so Sample Start/End are respected.
            const double inside = std::min (position - region.start, region.end - position);
            if (inside < 0.0) w *= static_cast<float> (std::max (0.0, 1.0 + inside / kEdgeFade));
        }

        if (w > 0.0f)
        {
            float sl, sr;
            readFrame (src, region, wrapInLoop, direction, position, sl, sr);
            outL[i] += sl * w * gl;
            outR[i] += sr * w * gr;
        }

        position += step;
        if (canWrap)
        {
            if (direction > 0 && position >= region.loopEnd)       position -= region.loopLength();
            else if (direction < 0 && position < region.loopStart) position += region.loopLength();
        }
    }

    age += count;
    if (age >= length) active = false;
    return active;
}
} // namespace lhss::dsp
