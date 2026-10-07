#include "Envelope.h"

#include <algorithm>
#include <cmath>

namespace lhss
{
namespace
{
constexpr float kSilence = 1.0e-4f; // -80 dB
}

float Envelope::coefficientFor (float ms, double sr) noexcept
{
    const double samples = std::max (1.0, ms * 0.001 * sr);
    return static_cast<float> (std::exp (-6.907755 / samples)); // ln(1000): -60 dB after `ms`
}

void Envelope::setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
{
    attackStep  = static_cast<float> (1.0 / std::max (1.0, attackMs * 0.001 * sampleRate));
    decayCoef   = coefficientFor (decayMs, sampleRate);
    releaseCoef = coefficientFor (releaseMs, sampleRate);
    sustain     = std::clamp (sustainLevel, 0.0f, 1.0f);
}

void Envelope::noteOn() noexcept { stage = Stage::Attack; }

void Envelope::noteOff() noexcept
{
    if (stage != Stage::Idle && stage != Stage::Kill) stage = Stage::Release;
}

void Envelope::kill (float ms) noexcept
{
    if (stage == Stage::Idle) return;
    killStep = std::max (level, kSilence) / static_cast<float> (std::max (1.0, ms * 0.001 * sampleRate));
    stage = Stage::Kill;
}

float Envelope::next() noexcept
{
    switch (stage)
    {
        case Stage::Idle:
            return 0.0f;

        case Stage::Attack:
            level += attackStep;
            if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
            break;

        case Stage::Decay:
            level = sustain + (level - sustain) * decayCoef;
            if (std::abs (level - sustain) < kSilence)
            {
                level = sustain;
                stage = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            level += (sustain - level) * 0.002f; // follow sustain changes without zipper noise
            if (sustain <= 0.0f && level < kSilence) { level = 0.0f; stage = Stage::Idle; }
            break;

        case Stage::Release:
            level *= releaseCoef;
            if (level < kSilence) { level = 0.0f; stage = Stage::Idle; }
            break;

        case Stage::Kill:
            level -= killStep;
            if (level <= 0.0f) { level = 0.0f; stage = Stage::Idle; }
            break;
    }
    return level;
}

void Envelope::process (float* gains, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i) gains[i] = next();
}
} // namespace lhss
