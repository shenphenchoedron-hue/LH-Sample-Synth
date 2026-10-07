#pragma once

#include <vector>

#include "ParameterSmoother.h"

namespace lhss::dsp
{
/** Stereo chorus: two modulated delay lines (7–19 ms) driven by quadrature sine LFOs.
    Buffers are allocated in prepare(); process() is allocation free. */
class Chorus
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (float* left, float* right, int n, float rateHz, float depth, float mix) noexcept;

private:
    std::vector<float> bufL, bufR;
    int writeIndex = 0;
    double sampleRate = 44100.0, phase = 0.0;
    ParameterSmoother mixSmoother;
};

/** Stereo feedback delay with optional ping-pong, damped feedback path (one-pole low-pass)
    and smoothed delay time (no zipper noise when the time or tempo changes). */
class StereoDelay
{
public:
    void prepare (double sampleRate, double maxSeconds = 2.5);
    void reset() noexcept;
    void process (float* left, float* right, int n, double delaySeconds, float feedback, float mix, bool pingPong) noexcept;

private:
    std::vector<float> bufL, bufR;
    int writeIndex = 0;
    double sampleRate = 44100.0, currentDelay = 0.0;
    float dampL = 0.0f, dampR = 0.0f;
    ParameterSmoother mixSmoother;
};

/** Soft saturation: tanh with input gain 1..25 and partial loudness compensation. */
inline float driveSample (float x, float gain, float makeup) noexcept;
} // namespace lhss::dsp

#include <cmath>
namespace lhss::dsp
{
inline float driveSample (float x, float gain, float makeup) noexcept { return std::tanh (x * gain) * makeup; }
} // namespace lhss::dsp
