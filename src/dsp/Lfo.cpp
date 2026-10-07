#include "Lfo.h"

#include <cmath>

namespace lhss::dsp
{
void Lfo::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    reset();
}

void Lfo::reset() noexcept
{
    phase = 0.0;
    randomTarget = randomValue = 0.0f;
}

float Lfo::randomBipolar() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float> (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

float Lfo::advance (Shape shape, double rateHz, int n) noexcept
{
    const auto p = static_cast<float> (phase);
    float v = 0.0f;
    switch (shape)
    {
        case Shape::Sine:     v = std::sin (p * 6.2831853f); break;
        case Shape::Triangle: v = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
        case Shape::Square:   v = p < 0.5f ? 1.0f : -1.0f; break;
        case Shape::Saw:      v = 1.0f - 2.0f * p; break;
        case Shape::Random:   v = randomValue; break;
    }

    const double inc = rateHz * n / sampleRate;
    phase += inc;
    if (phase >= 1.0)
    {
        phase -= std::floor (phase);
        randomTarget = randomBipolar();
    }
    // Smoothed random: glide towards the new target within ~1/8 cycle.
    const float k = static_cast<float> (std::fmin (1.0, inc * 8.0));
    randomValue += (randomTarget - randomValue) * k;
    return v;
}
} // namespace lhss::dsp
