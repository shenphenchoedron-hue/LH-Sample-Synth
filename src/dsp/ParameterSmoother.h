#pragma once

namespace lhss::dsp
{
/** Linear ramp smoother for continuous parameters (gain, pan, cutoff, ...).
    A new target restarts a ramp of fixed duration from the current value. Realtime safe. */
class ParameterSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept;
    void setCurrentAndTarget (float v) noexcept;
    void setTarget (float v) noexcept;

    float next() noexcept;
    float skip (int numSamples) noexcept; // advance and return the value reached

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int rampLength = 64, remaining = 0;
};
} // namespace lhss::dsp
