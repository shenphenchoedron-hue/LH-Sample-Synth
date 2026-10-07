#include "ParameterSmoother.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
void ParameterSmoother::reset (double sampleRate, double rampSeconds) noexcept
{
    rampLength = std::max (1, static_cast<int> (std::lround (sampleRate * rampSeconds)));
    remaining = 0;
    current = target;
}

void ParameterSmoother::setCurrentAndTarget (float v) noexcept
{
    current = target = v;
    remaining = 0;
}

void ParameterSmoother::setTarget (float v) noexcept
{
    if (std::abs (v - target) <= 0.0f) return;
    target = v;
    remaining = rampLength;
    step = (target - current) / static_cast<float> (rampLength);
}

float ParameterSmoother::next() noexcept
{
    if (remaining > 0)
    {
        current += step;
        if (--remaining == 0) current = target;
    }
    return current;
}

float ParameterSmoother::skip (int numSamples) noexcept
{
    if (numSamples >= remaining)
    {
        remaining = 0;
        current = target;
    }
    else
    {
        current += step * static_cast<float> (numSamples);
        remaining -= numSamples;
    }
    return current;
}
} // namespace lhss::dsp
