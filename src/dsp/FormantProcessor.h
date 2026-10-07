#pragma once

#include <array>

namespace lhss::dsp
{
/** Realtime LPC formant shifter / compensator (one instance per voice, Pitch mode only).

    What it actually does
    ---------------------
    Granular pitch shifting by ratio r also scales the spectral envelope (the formants) by r,
    which gives the "chipmunk" / "giant" effect. This stage re-shapes the envelope:

      1. Analysis: every kHop samples, the last kWindow samples of the voice's shifted signal
         are Hann-windowed, autocorrelated and run through Levinson-Durbin (order kOrder) to
         obtain an all-pole model 1/A_s(z) of the current spectral envelope.
      2. Target envelope: the envelope is frequency-warped by q = r^(−F), where F is the
         Formant control (−1..+1):
             F = 0    → q = 1: envelope unchanged (stage is bypassed; formants follow pitch)
             F = +1   → q = 1/r: envelope moved back to the original recording's position,
                        i.e. formant preservation
             F = −1   → q = r: formants moved twice as far as the pitch (exaggerated)
         The warped power spectrum P(ω) = e_s / |A_s(e^{jω/q})|² is sampled on 64 bins,
         converted to an autocorrelation by a cosine transform, and Levinson-Durbin yields
         the target all-pole model 1/A_t(z).
      3. Filtering: the signal is whitened with the FIR lattice A_s(z) and recoloured with the
         IIR lattice 1/A_t(z) (same filters for both channels, so the stereo image is kept).
         Reflection coefficients are interpolated per sample across each hop (lattice filters
         stay stable for |k| < 1), loudness is kept with the gain sqrt(e_t / e_s), and a
         wet ramp fades the effect in/out so toggling never clicks.

    Limitations: an order-12 all-pole model captures broad resonances (body, vowel colour),
    not fine detail; very noisy or very low-pitched material yields less precise envelopes.
    All buffers are fixed-size members: no allocation on the audio thread. */
class FormantProcessor
{
public:
    static constexpr int kOrder = 12;
    static constexpr int kWindow = 512;
    static constexpr int kHop = 128;
    static constexpr int kBins = 64;

    FormantProcessor();

    void reset() noexcept;

    /** In-place processing. pitchRatio is the voice's transposition, formant is −1..+1. */
    void process (float* left, float* right, int numSamples, double pitchRatio, float formant) noexcept;

private:
    using Coeffs = std::array<float, kOrder>;

    void analyse (double q) noexcept;
    static bool levinson (const double* r, double* a, double* k, double& error) noexcept;

    std::array<float, kWindow> ring {}, hann {};
    std::array<std::array<float, kBins>, kOrder + 1> cosTable {};
    int writeIndex = 0, hopCounter = 0;

    Coeffs kS {}, kSTarget {}, kSStep {};
    Coeffs kT {}, kTTarget {}, kTStep {};
    float gain = 1.0f, gainTarget = 1.0f, gainStep = 0.0f;
    float wet = 0.0f, wetTarget = 0.0f;
    double currentQ = 1.0;

    std::array<std::array<float, kOrder>, 2> firState {}, iirState {};
};
} // namespace lhss::dsp
