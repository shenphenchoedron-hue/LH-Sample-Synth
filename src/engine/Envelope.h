#pragma once

namespace lhss
{
/** Per-voice ADSR. Linear attack, exponential decay/release (reaching ≈ -60 dB of the
    remaining distance within the set time), plus a short linear "kill" fade used for
    click-free voice stealing. Realtime safe: no allocation, no branches on shared state. */
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release, Kill };

    void setSampleRate (double sr) noexcept { sampleRate = sr > 0.0 ? sr : 44100.0; }
    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept;

    void noteOn() noexcept;      // starts from the current level (no jump when retriggered)
    void noteOff() noexcept;     // → Release (ignored when idle / killing)
    void kill (float ms = 4.0f) noexcept;
    void reset() noexcept { stage = Stage::Idle; level = 0.0f; }

    float next() noexcept;
    void process (float* gains, int numSamples) noexcept;

    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }
    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release || stage == Stage::Kill; }

private:
    static float coefficientFor (float ms, double sr) noexcept;

    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    float level = 0.0f;
    float attackStep = 1.0f, decayCoef = 0.0f, releaseCoef = 0.0f, sustain = 1.0f, killStep = 0.01f;
};
} // namespace lhss
