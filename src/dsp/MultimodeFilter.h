#pragma once

#include "ParameterSmoother.h"

namespace lhss::dsp
{
/** Stereo state-variable filter (topology-preserving transform / trapezoidal SVF), giving
    low-pass, high-pass and band-pass outputs from one structure. Cutoff is smoothed in the
    log-frequency domain and resonance linearly; coefficients are recomputed every few samples.
    Stable under fast modulation and realtime safe. */
class MultimodeFilter
{
public:
    enum class Mode { LowPass = 0, HighPass = 1, BandPass = 2 };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setMode (Mode m) noexcept { mode = m; }
    void setCutoff (float hz) noexcept;
    void setResonance (float r01) noexcept;

    void process (float* left, float* right, int numSamples) noexcept;

private:
    void updateCoefficients (float log2Hz, float res) noexcept;

    double sampleRate = 44100.0;
    Mode mode = Mode::LowPass;
    ParameterSmoother log2Cutoff, resonance;
    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1[2] {}, ic2[2] {};
    int coefCountdown = 0;
};
} // namespace lhss::dsp
