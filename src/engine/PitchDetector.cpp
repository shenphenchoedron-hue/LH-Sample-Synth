#include "PitchDetector.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace lhss
{
namespace
{
constexpr double kAnalysedSeconds = 15.0;   // pitch is decided by the first part of long files
constexpr int kMaxFrames = 120;
constexpr double kYinThreshold = 0.2;       // CMNDF dip that counts as periodic
constexpr float kLoudFraction = 0.1f;       // frames quieter than -20 dB re. the loudest are ignored

struct Candidate
{
    double midi, weight;
};

/** Plain YIN difference d(tau) = sum_j (x[j] - x[j + tau])^2 over `window` samples. */
double difference (const float* x, int window, int tau) noexcept
{
    double sum = 0.0;
    for (int j = 0; j < window; ++j)
    {
        const double v = static_cast<double> (x[j]) - static_cast<double> (x[j + tau]);
        sum += v * v;
    }
    return sum;
}

/** Vertex of the parabola through (-1, a), (0, b), (1, c), as an offset in [-1, 1]. */
double parabolicOffset (double a, double b, double c) noexcept
{
    const double denom = a - 2.0 * b + c;
    if (std::abs (denom) < 1.0e-12) return 0.0;
    return std::clamp (0.5 * (a - c) / denom, -1.0, 1.0);
}

/** Sharpens the YIN estimate with a long, zero-padded FFT, so the result matches what a tuner (and
    the ear) reports:
    1. A clear partial within ±60 cents of the YIN estimate (harmonic sound with its fundamental):
       its exact frequency is used. YIN's lag estimate alone is biased by up to ~30 ct for high or
       beating sounds such as glass.
    2. No partial there, but the spectrum is harmonic on it (weak or missing fundamental, e.g. low
       piano notes): the fundamental is kept, made precise from its strongest harmonic.
    3. Otherwise the sound is inharmonic (bells, glass, metal): YIN tends to report a meaningless
       common subharmonic, so the dominant partial is used.
    Returns `f0` unchanged when the recording is too short for the needed resolution. */
double refineWithSpectrum (const std::vector<float>& x, double sampleRate, int loudestStart, double f0)
{
    const int available = static_cast<int> (x.size());
    int order = 10;
    while (order < 16 && (1 << (order + 1)) <= available) ++order;
    const int n = 1 << order;
    if (n > available || n < 40.0 * sampleRate / f0) return f0;   // needs >= 40 periods for resolution

    const int start = std::clamp (loudestStart, 0, available - n);
    const int fftOrder = order + 1;                                // 2x zero padding
    const int m = 1 << fftOrder;
    std::vector<float> data (static_cast<size_t> (2 * m), 0.0f);
    for (int i = 0; i < n; ++i)
        data[(size_t) i] = x[(size_t) (start + i)]
                         * static_cast<float> (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (n - 1)));
    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    const double binHz = sampleRate / m;
    const int bins = m / 2;
    auto magnitude = [&data] (int k) { return static_cast<double> (data[(size_t) k]); };
    auto logMag = [&magnitude] (int k) { return std::log (std::max (1.0e-12, magnitude (k))); };

    // Significant spectral peaks (>= -30 dB re. the strongest), with interpolated frequencies.
    const int firstBin = std::max (2, static_cast<int> (PitchDetector::kMinHz * 0.9 / binHz));
    const int lastBin = std::min (bins - 2, static_cast<int> (std::min (sampleRate * 0.45, 16000.0) / binHz));
    double globalMax = 0.0;
    for (int k = firstBin; k <= lastBin; ++k) globalMax = std::max (globalMax, magnitude (k));
    if (globalMax <= 0.0) return f0;

    struct Peak { double hz, energy; };
    std::vector<Peak> peaks;
    for (int k = firstBin; k <= lastBin; ++k)
        if (magnitude (k) >= globalMax * 0.03 && magnitude (k) > magnitude (k - 1) && magnitude (k) >= magnitude (k + 1))
            peaks.push_back ({ (k + parabolicOffset (logMag (k - 1), logMag (k), logMag (k + 1))) * binHz,
                               magnitude (k) * magnitude (k) });
    if (peaks.empty()) return f0;

    auto cents = [] (double a, double b) { return std::abs (1200.0 * std::log2 (a / b)); };

    // 1. A clear partial right at the estimate.
    const Peak* atF0 = nullptr;
    for (const auto& p : peaks)
        if (cents (p.hz, f0) <= 60.0 && (atF0 == nullptr || p.energy > atF0->energy)) atF0 = &p;
    if (atF0 != nullptr && atF0->energy >= globalMax * globalMax * 0.01) return atF0->hz;   // >= -20 dB

    // 2. Harmonic on f0? Share of peak energy lying on its harmonics (±30 ct).
    double totalEnergy = 0.0, harmonicEnergy = 0.0;
    const Peak* strongestHarmonic = nullptr;
    int strongestK = 1;
    for (const auto& p : peaks)
    {
        totalEnergy += p.energy;
        const int k = std::max (1, static_cast<int> (std::lround (p.hz / f0)));
        if (k <= 16 && cents (p.hz, k * f0) <= 30.0)
        {
            harmonicEnergy += p.energy;
            if (strongestHarmonic == nullptr || p.energy > strongestHarmonic->energy) { strongestHarmonic = &p; strongestK = k; }
        }
    }
    if (strongestHarmonic != nullptr && harmonicEnergy >= 0.85 * totalEnergy)
        return strongestHarmonic->hz / strongestK;

    // 3. Inharmonic: the dominant partial.
    const auto dominant = std::max_element (peaks.begin(), peaks.end(), [] (const Peak& a, const Peak& b) { return a.energy < b.energy; });
    const bool inRange = dominant->hz >= PitchDetector::kMinHz && dominant->hz <= PitchDetector::kMaxHz;
    return inRange ? dominant->hz : f0;
}
} // namespace

PitchInfo PitchDetector::analyse (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    PitchInfo info;
    const int channels = audio.getNumChannels();
    if (channels <= 0 || sampleRate <= 0.0) return info;

    const int total = std::min (audio.getNumSamples(), static_cast<int> (kAnalysedSeconds * sampleRate));
    if (total < 64) return info;

    // Mono mix at full rate (used for the final sub-sample refinement).
    std::vector<float> full (static_cast<size_t> (total));
    for (int i = 0; i < total; ++i)
    {
        float v = 0.0f;
        for (int c = 0; c < channels; ++c) v += audio.getSample (c, i);
        full[(size_t) i] = v / static_cast<float> (channels);
    }

    // Coarse search on a decimated copy (~22-24 kHz; the lag is refined at full rate below).
    const int decim = std::max (1, static_cast<int> (sampleRate / 22050.0 + 1.0e-9));
    const double rate = sampleRate / decim;
    const int count = total / decim;
    std::vector<float> x (static_cast<size_t> (count));
    for (int i = 0; i < count; ++i)
    {
        float v = 0.0f;
        for (int k = 0; k < decim; ++k) v += full[(size_t) (i * decim + k)];
        x[(size_t) i] = v / static_cast<float> (decim);
    }

    const int maxTau = static_cast<int> (std::ceil (rate / kMinHz));
    const int minTau = std::max (2, static_cast<int> (std::floor (rate / kMaxHz)));
    const int window = maxTau;
    const int frameLen = window + maxTau + 2;
    if (count < frameLen) return info;   // too short to hold two periods of the lowest pitch

    const int numFrames = std::min (kMaxFrames, 1 + (count - frameLen) / std::max (1, static_cast<int> (rate * 0.01)));
    const double hop = numFrames > 1 ? static_cast<double> (count - frameLen) / (numFrames - 1) : 0.0;

    std::vector<int> starts;
    std::vector<float> rms;
    float loudest = 0.0f;
    int loudestStart = 0;
    for (int f = 0; f < numFrames; ++f)
    {
        const int s = static_cast<int> (f * hop);
        double e = 0.0;
        for (int j = 0; j < window; ++j) e += static_cast<double> (x[(size_t) (s + j)]) * x[(size_t) (s + j)];
        const auto r = static_cast<float> (std::sqrt (e / window));
        starts.push_back (s);
        rms.push_back (r);
        if (r > loudest) { loudest = r; loudestStart = s * decim; }
    }
    if (loudest < 1.0e-4f) return info;   // silence

    std::vector<double> d ((size_t) maxTau + 2), cmndf ((size_t) maxTau + 2);
    std::vector<Candidate> voiced;
    int loudFrames = 0;

    for (size_t f = 0; f < starts.size(); ++f)
    {
        if (rms[f] < loudest * kLoudFraction) continue;
        ++loudFrames;
        const float* frame = x.data() + starts[f];

        // Cumulative mean normalised difference.
        cmndf[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= maxTau + 1; ++tau)
        {
            d[(size_t) tau] = difference (frame, window, tau);
            running += d[(size_t) tau];
            cmndf[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
        }

        // First dip under the threshold, followed down to its local minimum.
        int tau = -1;
        for (int t = minTau; t <= maxTau; ++t)
            if (cmndf[(size_t) t] < kYinThreshold)
            {
                while (t + 1 <= maxTau && cmndf[(size_t) t + 1] < cmndf[(size_t) t]) ++t;
                tau = t;
                break;
            }
        if (tau < 0) continue;   // aperiodic frame

        const double aperiodicity = cmndf[(size_t) tau];
        double coarse = tau + parabolicOffset (cmndf[(size_t) tau - 1], cmndf[(size_t) tau], cmndf[(size_t) tau + 1]);

        // Refine at the full sample rate around the coarse lag.
        double period = coarse * decim;
        if (decim > 1)
        {
            const int fullStart = starts[f] * decim;
            const int fullWindow = window * decim;
            const int centre = static_cast<int> (std::lround (period));
            const int lo = std::max (2, centre - decim - 1), hi = centre + decim + 1;
            if (fullStart + fullWindow + hi + 1 < total)
            {
                const float* xf = full.data() + fullStart;
                int best = lo;
                double bestD = difference (xf, fullWindow, lo);
                for (int t = lo + 1; t <= hi; ++t)
                {
                    const double v = difference (xf, fullWindow, t);
                    if (v < bestD) { bestD = v; best = t; }
                }
                period = best + parabolicOffset (difference (xf, fullWindow, best - 1), bestD,
                                                 difference (xf, fullWindow, best + 1));
            }
        }
        if (period <= 0.0) continue;

        const double hz = sampleRate / period;
        if (hz < kMinHz * 0.97 || hz > kMaxHz * 1.03) continue;
        voiced.push_back ({ 69.0 + 12.0 * std::log2 (hz / 440.0), rms[f] * (1.0 - aperiodicity) });
    }

    if (loudFrames == 0 || voiced.size() < 3) return info;
    const double voicedRatio = static_cast<double> (voiced.size()) / loudFrames;
    if (voicedRatio < 0.3) return info;

    // Weighted median.
    std::sort (voiced.begin(), voiced.end(), [] (const Candidate& a, const Candidate& b) { return a.midi < b.midi; });
    double totalWeight = 0.0;
    for (const auto& c : voiced) totalWeight += c.weight;
    if (totalWeight <= 0.0) return info;
    double acc = 0.0, median = voiced.front().midi;
    for (const auto& c : voiced)
    {
        acc += c.weight;
        if (acc >= 0.5 * totalWeight) { median = c.midi; break; }
    }

    // Consistency: share of the weight that agrees with the median (within a semitone either way),
    // and the precise estimate as the weighted mean of those agreeing frames.
    double agreeWeight = 0.0, agreeSum = 0.0;
    for (const auto& c : voiced)
        if (std::abs (c.midi - median) <= 0.5)
        {
            agreeWeight += c.weight;
            agreeSum += c.weight * c.midi;
        }
    const double consistency = agreeWeight / totalWeight;
    if (consistency < 0.6 || agreeWeight <= 0.0) return info;

    const double yinHz = 440.0 * std::pow (2.0, (agreeSum / agreeWeight - 69.0) / 12.0);
    const double hz = refineWithSpectrum (full, sampleRate, loudestStart, yinHz);
    info.midiNote = std::clamp (69.0 + 12.0 * std::log2 (hz / 440.0), 0.0, 127.0);
    info.frequencyHz = 440.0 * std::pow (2.0, (info.midiNote - 69.0) / 12.0);
    info.confidence = static_cast<float> (std::clamp (consistency * std::min (1.0, voicedRatio / 0.6), 0.0, 1.0));
    info.valid = true;
    return info;
}
} // namespace lhss
