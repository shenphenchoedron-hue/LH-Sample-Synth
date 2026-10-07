#include "MultimodeFilter.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
namespace
{
constexpr int kCoefInterval = 8;
}

void MultimodeFilter::prepare (double sr) noexcept
{
    sampleRate = sr;
    log2Cutoff.reset (sr, 0.03);
    resonance.reset (sr, 0.03);
    log2Cutoff.setCurrentAndTarget (std::log2 (20000.0f));
    resonance.setCurrentAndTarget (0.1f);
    reset();
}

void MultimodeFilter::reset() noexcept
{
    ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f;
    coefCountdown = 0;
}

void MultimodeFilter::setCutoff (float hz) noexcept
{
    log2Cutoff.setTarget (std::log2 (std::clamp (hz, 20.0f, 20000.0f)));
}

void MultimodeFilter::setResonance (float r) noexcept { resonance.setTarget (std::clamp (r, 0.0f, 1.0f)); }

void MultimodeFilter::updateCoefficients (float log2Hz, float res) noexcept
{
    const double nyquistSafe = sampleRate * 0.49;
    const double fc = std::min<double> (std::exp2 (log2Hz), nyquistSafe);
    g = static_cast<float> (std::tan (3.14159265358979323846 * fc / sampleRate));
    const float q = 0.7071f + res * res * 14.0f; // 0 → Butterworth, 1 → strong resonance
    k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void MultimodeFilter::process (float* left, float* right, int n) noexcept
{
    float* chans[2] = { left, right };
    for (int i = 0; i < n; ++i)
    {
        const float lc = log2Cutoff.next();
        const float rs = resonance.next();
        if (--coefCountdown <= 0)
        {
            updateCoefficients (lc, rs);
            coefCountdown = kCoefInterval;
        }

        for (int c = 0; c < 2; ++c)
        {
            const float v0 = chans[c][i];
            const float v3 = v0 - ic2[c];
            const float v1 = a1 * ic1[c] + a2 * v3;
            const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
            ic1[c] = 2.0f * v1 - ic1[c];
            ic2[c] = 2.0f * v2 - ic2[c];

            float out;
            switch (mode)
            {
                case Mode::HighPass: out = v0 - k * v1 - v2; break;
                case Mode::BandPass: out = v1 * k; break; // unity gain at centre
                case Mode::LowPass:
                default:             out = v2; break;
            }
            chans[c][i] = out;
        }
    }
}
} // namespace lhss::dsp
