#pragma once

#include <cstdint>

namespace lhss::dsp
{
/** Global low-frequency oscillator (block-rate). Shapes: sine, triangle, square, saw and a
    smoothed random (sample & hold with a short glide so it never clicks). Output is -1..+1.
    Realtime safe. */
class Lfo
{
public:
    enum class Shape { Sine = 0, Triangle, Square, Saw, Random };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Returns the value for the current position and advances the phase by numSamples. */
    float advance (Shape shape, double rateHz, int numSamples) noexcept;

    double getPhase() const noexcept { return phase; }

private:
    float randomBipolar() noexcept;

    double sampleRate = 44100.0;
    double phase = 0.0;
    float randomTarget = 0.0f, randomValue = 0.0f;
    std::uint32_t rng = 0x2468ACE1u;
};
} // namespace lhss::dsp
