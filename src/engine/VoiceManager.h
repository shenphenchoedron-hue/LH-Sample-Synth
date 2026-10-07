#pragma once

#include <array>
#include <cstdint>

#include "Voice.h"

namespace lhss
{
/** Fixed pool of voices with allocation and stealing.

    Up to `polyphony` (max 16) voices sound at once. Stolen voices get a 4 ms fade instead of
    being cut, so the pool has extra slots that hold those fading tails; a new note therefore
    never has to wait for or hard-cut a stolen voice.

    Victim choice: (1) the oldest voice that already received note-off / is releasing,
    otherwise (2) the oldest active voice. */
class VoiceManager
{
public:
    static constexpr int kNumSlots = kMaxPolyphony + 8;

    void prepare (double hostRate, int maxBlockSize);

    /** Returns the voice that was started (nullptr if no sample). */
    Voice* noteOn (const SampleData* sample, int note, float velocity01, const EngineParams& params) noexcept;
    void noteOff (int note, const EngineParams& params) noexcept;
    void allNotesOff (const EngineParams& params) noexcept;
    void killAll() noexcept;     // fast fade on every voice
    void stopAll() noexcept;     // immediate (used on prepare/reset)

    void render (const VoiceContext& ctx, float* mixL, float* mixR, int n) noexcept;

    int getActiveVoiceCount() const noexcept;    // includes fading (stolen) voices
    int getSoundingVoiceCount() const noexcept;  // active and not being stolen
    const std::array<Voice, kNumSlots>& getVoices() const noexcept { return voices; }

private:
    Voice* chooseVictim() noexcept;
    Voice* findFreeSlot() noexcept;

    std::array<Voice, kNumSlots> voices;
    std::uint64_t ageCounter = 0;
};
} // namespace lhss
