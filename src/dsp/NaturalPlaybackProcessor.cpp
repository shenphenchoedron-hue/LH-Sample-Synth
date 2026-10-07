#include "NaturalPlaybackProcessor.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
namespace
{
constexpr double kEndFadeOutputSamples = 96.0;
}

void NaturalPlaybackProcessor::startFadeIn (int samples) noexcept
{
    gain = 0.0f;
    targetGain = 1.0f;
    gainStep = 1.0f / static_cast<float> (std::max (1, samples));
}

void NaturalPlaybackProcessor::startFadeOut (int samples) noexcept
{
    targetGain = 0.0f;
    gainStep = -gain / static_cast<float> (std::max (1, samples));
}

void NaturalPlaybackProcessor::render (const SourceView& src, const ResolvedRegion& region, PlayheadState& ph,
                                       double increment, float* outL, float* outR, int n) noexcept
{
    if (! src.isValid()) return;

    for (int i = 0; i < n && ! ph.ended; ++i)
    {
        if (std::abs (gainStep) > 0.0f)
        {
            gain += gainStep;
            if ((gainStep > 0.0f && gain >= targetGain) || (gainStep < 0.0f && gain <= targetGain))
            {
                gain = targetGain;
                gainStep = 0.0f;
            }
        }
        if (gain <= 0.0f && targetGain <= 0.0f) return;

        float l, r;
        readFrame (src, region, ph.looping, ph.direction, ph.position, l, r);

        // Short fade before the region boundary so cutting mid-waveform does not click.
        const double remainingOut = framesUntilEnd (ph, region) / std::max (1.0e-9, increment);
        const float endFade = static_cast<float> (std::min (1.0, remainingOut / kEndFadeOutputSamples));

        const float g = gain * endFade;
        outL[i] += l * g;
        outR[i] += r * g;
        advancePlayhead (ph, increment, region);
    }
}
} // namespace lhss::dsp
