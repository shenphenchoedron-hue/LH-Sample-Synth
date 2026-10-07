#include "GranularPitchProcessor.h"

#include <algorithm>
#include <cmath>

namespace lhss::dsp
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
}

GranularPitchProcessor::GranularPitchProcessor()
{
    for (int i = 0; i <= kWindowSize; ++i)
        window[(size_t) i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * kPi * i / kWindowSize));
}

void GranularPitchProcessor::reset (std::uint32_t seed) noexcept
{
    for (auto& g : grains) g.active = false;
    activeGrains = 0;
    lastSpawned = -1;
    spawnCountdown = 0.0; // first grain starts immediately
    rng = seed != 0 ? seed : 0x9E3779B9u;
}

float GranularPitchProcessor::random01() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float> (rng >> 8) * (1.0f / 16777216.0f);
}

float GranularPitchProcessor::randomBipolar() noexcept { return random01() * 2.0f - 1.0f; }

double GranularPitchProcessor::alignStart (const SourceView& src, double nominal, const Grain& ref,
                                           double tolerance) const noexcept
{
    constexpr int kPoints = 48;
    const int dir = ref.direction;
    const int spacing = std::max (1, static_cast<int> (src.sampleRate * 0.00025)); // ≈ 12 frames @ 48 kHz
    const int n = src.numFrames;
    auto mono = [&src, n] (long long i) noexcept
    {
        if (i < 0 || i >= n) return 0.0f;
        return src.left[i] + src.right[i];
    };

    // Reference: the material the latest grain is about to read.
    float refSeq[kPoints];
    const auto refPos = static_cast<long long> (std::llround (ref.position));
    double refEnergy = 0.0;
    for (int j = 0; j < kPoints; ++j)
    {
        refSeq[j] = mono (refPos + static_cast<long long> (j) * spacing * dir);
        refEnergy += refSeq[j] * refSeq[j];
    }
    if (refEnergy < 1.0e-8) return nominal; // silence: nothing to align to

    auto score = [&] (long long candidate) noexcept
    {
        double dot = 0.0, energy = 1.0e-9;
        for (int j = 0; j < kPoints; ++j)
        {
            const float v = mono (candidate + static_cast<long long> (j) * spacing * dir);
            dot += v * refSeq[j];
            energy += v * v;
        }
        return dot / std::sqrt (energy);
    };

    const auto centre = static_cast<long long> (std::llround (nominal));
    const auto tol = static_cast<long long> (tolerance);
    long long best = centre;
    double bestScore = -1.0e30;
    for (long long d = -tol; d <= tol; d += 2)               // coarse
    {
        const double sc = score (centre + d);
        if (sc > bestScore) { bestScore = sc; best = centre + d; }
    }
    const long long coarse = best;
    for (long long d = -2; d <= 2; ++d)                       // refine
    {
        const double sc = score (coarse + d);
        if (sc > bestScore) { bestScore = sc; best = coarse + d; }
    }
    return static_cast<double> (best);
}

void GranularPitchProcessor::spawnGrain (const SourceView& src, const ResolvedRegion& region,
                                         const PlayheadState& ph, const Settings& s, int length) noexcept
{
    Grain* slot = nullptr;
    for (auto& g : grains)
        if (! g.active) { slot = &g; break; }
    if (slot == nullptr) return; // pool full: skip this grain (bounded CPU)

    // Per-grain detune: quadratic response, ±12 semitones at 100 %.
    const double semis = static_cast<double> (s.pitchRandom * s.pitchRandom) * 12.0 * randomBipolar();
    const double rate = s.baseIncrement * s.pitchRatio * std::pow (2.0, semis / 12.0);
    const double travel = s.baseIncrement * s.travelSpeed;

    // Align the grain centre with the playhead at the grain's centre time (signed speeds).
    const int dir = ph.direction;
    double start = ph.position + 0.5 * length * (travel - rate) * dir;

    // Position randomness: up to one grain length plus 0.5 s at 100 %.
    double jitterRange = s.positionRandom * (length * s.baseIncrement + s.positionRandom * 0.5 * src.sampleRate);
    if (s.travelSpeed <= 0.0) jitterRange = std::max (jitterRange, 0.012 * src.sampleRate); // frozen: avoid buzz
    start += jitterRange * randomBipolar();

    // Phase-align with the most recent grain (fades out with randomness, see header).
    const float coherence = std::clamp (1.0f - s.positionRandom / 0.6f, 0.0f, 1.0f)
                          * std::clamp (1.0f - s.pitchRandom / 0.35f, 0.0f, 1.0f);
    if (coherence > 0.0f && lastSpawned >= 0 && grains[(size_t) lastSpawned].active)
    {
        const double tol = coherence * std::min (0.25 * length * rate, 0.012 * src.sampleRate);
        if (tol >= 2.0) start = alignStart (src, start, grains[(size_t) lastSpawned], tol);
    }

    const bool inLoop = ph.looping && region.loopOn && region.loopLength() >= 1.0
                        && ph.position >= region.loopStart && ph.position <= region.loopEnd;
    if (inLoop)
    {
        const double len = region.loopLength();
        while (start >= region.loopEnd) start -= len;
        while (start < region.loopStart) start += len;
    }
    else
    {
        start = std::clamp (start, region.start - 64.0, region.end + 64.0);
    }

    // Constant-power random pan, unity at centre.
    const float pan = s.stereoSpread * randomBipolar();
    const auto angle = static_cast<float> ((pan + 1.0f) * 0.25f * kPi);

    slot->active = true;
    slot->wrapInLoop = inLoop;
    slot->direction = dir;
    slot->position = start;
    slot->increment = rate;
    slot->length = length;
    slot->age = 0;
    slot->gainL = std::cos (angle) * 1.41421356f;
    slot->gainR = std::sin (angle) * 1.41421356f;
    lastSpawned = static_cast<int> (slot - grains.data());
    ++activeGrains;
}

void GranularPitchProcessor::render (const SourceView& src, const ResolvedRegion& region, PlayheadState& ph,
                                     bool ownsPlayhead, bool spawn, const Settings& s,
                                     float* outL, float* outR, int n) noexcept
{
    if (! src.isValid()) return;

    const int length = std::max (16, static_cast<int> (s.grainSizeMs * 0.001 * s.hostRate));
    const double density = std::clamp (static_cast<double> (s.density), 0.25, 64.0);
    const double interval = std::max (1.0, length / density);
    // Hann windows overlapping `density` times sum to density/2.
    const auto norm = static_cast<float> (1.0 / std::max (1.0, density * 0.5));
    const double travel = s.baseIncrement * s.travelSpeed;

    // Rendered in segments between grain onsets so the inner loops run per grain, not per slot.
    int i = 0;
    while (i < n)
    {
        int segment = n - i;
        if (spawn && ! ph.ended)
        {
            spawnCountdown -= 1.0;
            if (spawnCountdown <= 0.0)
            {
                spawnGrain (src, region, ph, s, length);
                // Slight onset jitter with randomness for organic textures.
                spawnCountdown += interval * (1.0 + 0.3 * s.positionRandom * randomBipolar());
            }
            segment = std::clamp (static_cast<int> (std::ceil (spawnCountdown)), 1, n - i);
            spawnCountdown -= (segment - 1);
        }

        if (activeGrains > 0)
        {
            int stillActive = 0;
            for (auto& g : grains)
                if (g.active && g.render (src, region, window.data(), kWindowSize, norm, outL + i, outR + i, segment))
                    ++stillActive;
            activeGrains = stillActive;
        }

        if (ownsPlayhead && travel > 0.0)
            for (int k = 0; k < segment && ! ph.ended; ++k) advancePlayhead (ph, travel, region);

        i += segment;
    }
}
} // namespace lhss::dsp
