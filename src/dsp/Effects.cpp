#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
namespace
{
inline float readLinear (const std::vector<float>& buf, double pos) noexcept
{
    const int size = static_cast<int> (buf.size());
    while (pos < 0.0) pos += size;
    const int i0 = static_cast<int> (pos) % size;
    const int i1 = (i0 + 1) % size;
    const auto t = static_cast<float> (pos - std::floor (pos));
    return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * t;
}
} // namespace

void Chorus::prepare (double sr)
{
    sampleRate = sr;
    const auto size = static_cast<size_t> (sr * 0.05) + 4;
    bufL.assign (size, 0.0f);
    bufR.assign (size, 0.0f);
    mixSmoother.reset (sr, 0.03);
    reset();
}

void Chorus::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writeIndex = 0;
    phase = 0.0;
    mixSmoother.setCurrentAndTarget (0.0f);
}

void Chorus::process (float* left, float* right, int n, float rateHz, float depth, float mix) noexcept
{
    mixSmoother.setTarget (std::clamp (mix, 0.0f, 1.0f));
    if (mixSmoother.getCurrent() <= 0.0f && ! mixSmoother.isSmoothing())
    {
        // Keep the delay line filled so enabling the chorus is seamless.
        for (int i = 0; i < n; ++i)
        {
            bufL[(size_t) writeIndex] = left[i];
            bufR[(size_t) writeIndex] = right[i];
            writeIndex = (writeIndex + 1) % static_cast<int> (bufL.size());
        }
        return;
    }

    const double inc = rateHz / sampleRate;
    const double baseMs = 7.0, depthMs = 12.0 * std::clamp (depth, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        bufL[(size_t) writeIndex] = left[i];
        bufR[(size_t) writeIndex] = right[i];

        const double modL = 0.5 + 0.5 * std::sin (phase * 6.283185307179586);
        const double modR = 0.5 + 0.5 * std::cos (phase * 6.283185307179586);
        const double dL = (baseMs + depthMs * modL) * 0.001 * sampleRate;
        const double dR = (baseMs + depthMs * modR) * 0.001 * sampleRate;
        const float wetL = readLinear (bufL, writeIndex - dL);
        const float wetR = readLinear (bufR, writeIndex - dR);

        const float m = mixSmoother.next();
        left[i]  = left[i]  * (1.0f - 0.5f * m) + wetL * 0.7f * m;
        right[i] = right[i] * (1.0f - 0.5f * m) + wetR * 0.7f * m;

        writeIndex = (writeIndex + 1) % static_cast<int> (bufL.size());
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
}

//==============================================================================
void StereoDelay::prepare (double sr, double maxSeconds)
{
    sampleRate = sr;
    const auto size = static_cast<size_t> (sr * maxSeconds) + 8;
    bufL.assign (size, 0.0f);
    bufR.assign (size, 0.0f);
    mixSmoother.reset (sr, 0.03);
    reset();
}

void StereoDelay::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writeIndex = 0;
    currentDelay = 0.0;
    dampL = dampR = 0.0f;
    mixSmoother.setCurrentAndTarget (0.0f);
}

void StereoDelay::process (float* left, float* right, int n, double seconds, float feedback, float mix, bool pingPong) noexcept
{
    const int size = static_cast<int> (bufL.size());
    const double target = std::clamp (seconds * sampleRate, 1.0, static_cast<double> (size - 4));
    if (currentDelay <= 0.0) currentDelay = target;
    const double glide = 1.0 - std::exp (-1.0 / (0.05 * sampleRate)); // ~50 ms time smoothing
    const float fb = std::clamp (feedback, 0.0f, 0.95f);
    const float dampCoef = static_cast<float> (1.0 - std::exp (-6.283185307179586 * 6000.0 / sampleRate));
    mixSmoother.setTarget (std::clamp (mix, 0.0f, 1.0f));

    for (int i = 0; i < n; ++i)
    {
        currentDelay += (target - currentDelay) * glide;
        const float dl = readLinear (bufL, writeIndex - currentDelay);
        const float dr = readLinear (bufR, writeIndex - currentDelay);
        dampL += (dl - dampL) * dampCoef;
        dampR += (dr - dampR) * dampCoef;

        const float inL = left[i], inR = right[i];
        if (pingPong)
        {
            bufL[(size_t) writeIndex] = 0.5f * (inL + inR) + dampR * fb;
            bufR[(size_t) writeIndex] = dampL * fb;
        }
        else
        {
            bufL[(size_t) writeIndex] = inL + dampL * fb;
            bufR[(size_t) writeIndex] = inR + dampR * fb;
        }

        const float m = mixSmoother.next();
        left[i]  = inL + dl * m;
        right[i] = inR + dr * m;
        writeIndex = (writeIndex + 1) % size;
    }
}
} // namespace lhss::dsp
