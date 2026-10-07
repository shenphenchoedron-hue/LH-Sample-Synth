#include "FormantProcessor.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kMaxReflection = 0.995f;
}

FormantProcessor::FormantProcessor()
{
    for (int i = 0; i < kWindow; ++i)
        hann[(size_t) i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * kPi * i / (kWindow - 1)));

    for (int m = 0; m <= kOrder; ++m)
        for (int b = 0; b < kBins; ++b)
            cosTable[(size_t) m][(size_t) b] = static_cast<float> (std::cos (m * kPi * b / (kBins - 1)));
}

void FormantProcessor::reset() noexcept
{
    ring.fill (0.0f);
    writeIndex = hopCounter = 0;
    kS.fill (0.0f); kSTarget.fill (0.0f); kSStep.fill (0.0f);
    kT.fill (0.0f); kTTarget.fill (0.0f); kTStep.fill (0.0f);
    gain = gainTarget = 1.0f;
    gainStep = 0.0f;
    wet = wetTarget = 0.0f;
    currentQ = 1.0;
    for (auto& s : firState) s.fill (0.0f);
    for (auto& s : iirState) s.fill (0.0f);
}

bool FormantProcessor::levinson (const double* r, double* a, double* k, double& error) noexcept
{
    // A(z) = 1 + sum a[j] z^-j ; k[i] are reflection coefficients in the same convention.
    double tmp[kOrder + 1] {};
    for (int i = 0; i <= kOrder; ++i) a[i] = 0.0;
    a[0] = 1.0;
    error = r[0];
    if (error <= 0.0) return false;

    for (int i = 1; i <= kOrder; ++i)
    {
        double acc = r[i];
        for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
        double ki = -acc / error;
        ki = std::clamp (ki, -static_cast<double> (kMaxReflection), static_cast<double> (kMaxReflection));
        k[i - 1] = ki;

        for (int j = 1; j < i; ++j) tmp[j] = a[j] + ki * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = ki;
        error *= (1.0 - ki * ki);
        if (error <= 0.0) return false;
    }
    return true;
}

void FormantProcessor::analyse (double q) noexcept
{
    // 1. Windowed autocorrelation of the most recent kWindow samples.
    double frame[kWindow];
    for (int j = 0; j < kWindow; ++j)
        frame[j] = ring[(size_t) ((writeIndex + j) % kWindow)] * hann[(size_t) j];

    double r[kOrder + 1] {};
    for (int m = 0; m <= kOrder; ++m)
    {
        double acc = 0.0;
        for (int j = m; j < kWindow; ++j) acc += frame[j] * frame[j - m];
        r[m] = acc;
    }

    auto setTargets = [this] (const double* ks, const double* kt, float g)
    {
        for (int i = 0; i < kOrder; ++i)
        {
            kSTarget[(size_t) i] = static_cast<float> (ks[i]);
            kTTarget[(size_t) i] = static_cast<float> (kt[i]);
            kSStep[(size_t) i] = (kSTarget[(size_t) i] - kS[(size_t) i]) / kHop;
            kTStep[(size_t) i] = (kTTarget[(size_t) i] - kT[(size_t) i]) / kHop;
        }
        gainTarget = g;
        gainStep = (gainTarget - gain) / kHop;
    };

    double zeros[kOrder] {};
    if (r[0] < 1.0e-9)
    {
        setTargets (zeros, zeros, 1.0f); // silence: relax towards identity
        return;
    }

    // Normalise, add a tiny white-noise floor and a Gaussian lag window (bandwidth expansion).
    const double r0 = r[0];
    for (int m = 0; m <= kOrder; ++m)
    {
        const double lag = 0.00524 * m; // 2*pi*40Hz/48kHz: ≈ 40 Hz bandwidth expansion
        r[m] = r[m] / r0 * std::exp (-0.5 * lag * lag);
    }
    r[0] *= 1.0001;

    double aS[kOrder + 1], kSrc[kOrder], eS = 0.0;
    if (! levinson (r, aS, kSrc, eS))
    {
        setTargets (zeros, zeros, 1.0f);
        return;
    }

    // 2. Sample the warped envelope P(w) = eS / |A_s(e^{j w/q})|^2 on kBins points.
    double rT[kOrder + 1] {};
    for (int b = 0; b < kBins; ++b)
    {
        const double w = kPi * b / (kBins - 1);
        double wq = w / q;
        double beyond = 0.0;
        if (wq > kPi) { beyond = wq - kPi; wq = kPi; }

        const double c1 = std::cos (wq), s1 = std::sin (wq);
        double cr = 1.0, ci = 0.0;          // e^{-j m wq} by recurrence
        double re = 1.0, im = 0.0;          // A(e^{j wq})
        for (int m = 1; m <= kOrder; ++m)
        {
            const double nr = cr * c1 + ci * s1;
            const double ni = ci * c1 - cr * s1;
            cr = nr; ci = ni;
            re += aS[m] * cr;
            im += aS[m] * ci;
        }
        double p = eS / std::max (1.0e-12, re * re + im * im);
        if (beyond > 0.0) p *= std::exp (-6.0 * beyond); // spectrum folded past Nyquist fades out

        const double weight = (b == 0 || b == kBins - 1) ? 0.5 : 1.0;
        for (int m = 0; m <= kOrder; ++m)
            rT[m] += weight * p * cosTable[(size_t) m][(size_t) b];
    }

    if (rT[0] <= 1.0e-12)
    {
        setTargets (zeros, zeros, 1.0f);
        return;
    }
    const double rt0 = rT[0];
    for (int m = 0; m <= kOrder; ++m) rT[m] /= rt0;
    rT[0] *= 1.0001;

    double aT[kOrder + 1], kTgt[kOrder], eT = 0.0;
    if (! levinson (rT, aT, kTgt, eT))
    {
        setTargets (zeros, zeros, 1.0f);
        return;
    }

    // 3. Loudness compensation: whitening leaves power eS, recolouring multiplies by 1/eT.
    const float g = static_cast<float> (std::clamp (std::sqrt (eT / std::max (1.0e-9, eS)), 0.25, 4.0));
    setTargets (kSrc, kTgt, g);
}

void FormantProcessor::process (float* left, float* right, int n, double pitchRatio, float formant) noexcept
{
    const double q = std::pow (std::max (1.0e-3, pitchRatio), -static_cast<double> (formant));
    currentQ = q;
    const bool meaningful = std::abs (std::log2 (q)) > 1.0e-3;
    wetTarget = meaningful ? std::min (1.0f, std::abs (formant) * 10.0f) : 0.0f;
    const float wetStep = 1.0f / kHop;

    float* chans[2] = { left, right };

    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) writeIndex] = 0.5f * (left[i] + right[i]);
        writeIndex = (writeIndex + 1) % kWindow;
        if (++hopCounter >= kHop)
        {
            hopCounter = 0;
            if (wetTarget > 0.0f || wet > 0.0f) analyse (currentQ);
        }

        if (wet < wetTarget) wet = std::min (wetTarget, wet + wetStep);
        else if (wet > wetTarget) wet = std::max (wetTarget, wet - wetStep);
        if (wet <= 0.0f && wetTarget <= 0.0f) continue; // bypassed

        for (int j = 0; j < kOrder; ++j)
        {
            if (std::abs (kSStep[(size_t) j]) > 0.0f)
            {
                kS[(size_t) j] += kSStep[(size_t) j];
                if ((kSStep[(size_t) j] > 0.0f) == (kS[(size_t) j] >= kSTarget[(size_t) j])) { kS[(size_t) j] = kSTarget[(size_t) j]; kSStep[(size_t) j] = 0.0f; }
            }
            if (std::abs (kTStep[(size_t) j]) > 0.0f)
            {
                kT[(size_t) j] += kTStep[(size_t) j];
                if ((kTStep[(size_t) j] > 0.0f) == (kT[(size_t) j] >= kTTarget[(size_t) j])) { kT[(size_t) j] = kTTarget[(size_t) j]; kTStep[(size_t) j] = 0.0f; }
            }
        }
        if (std::abs (gainStep) > 0.0f)
        {
            gain += gainStep;
            if ((gainStep > 0.0f) == (gain >= gainTarget)) { gain = gainTarget; gainStep = 0.0f; }
        }

        for (int c = 0; c < 2; ++c)
        {
            const float x = chans[c][i];
            auto& fs = firState[(size_t) c];
            auto& is = iirState[(size_t) c];

            // Whitening: FIR lattice A_s(z).
            float f = x, bCur = x;
            for (int j = 0; j < kOrder; ++j)
            {
                const float bDelayed = fs[(size_t) j];
                const float fNew = f + kS[(size_t) j] * bDelayed;
                const float bNew = bDelayed + kS[(size_t) j] * f;
                fs[(size_t) j] = bCur;
                bCur = bNew;
                f = fNew;
            }

            // Recolouring: IIR lattice 1/A_t(z).
            float bOut[kOrder + 1];
            float y = f;
            for (int j = kOrder - 1; j >= 0; --j)
            {
                y -= kT[(size_t) j] * is[(size_t) j];
                bOut[j + 1] = is[(size_t) j] + kT[(size_t) j] * y;
            }
            is[0] = y;
            for (int j = 1; j < kOrder; ++j) is[(size_t) j] = bOut[j];

            float out = y * gain;
            if (! std::isfinite (out))
            {
                fs.fill (0.0f);
                is.fill (0.0f);
                out = x;
            }
            out = std::clamp (out, -4.0f, 4.0f);
            chans[c][i] = x + (out - x) * wet;
        }
    }
}
} // namespace lhss::dsp
