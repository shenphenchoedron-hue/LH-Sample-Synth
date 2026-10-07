#pragma once

#include <array>
#include <cstdint>
#include <vector>

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
    static constexpr int kMaxSounding = 32;              // polyphony x unison, capped
    static constexpr int kNumSlots = kMaxSounding + 8;   // + room for fading (stolen) tails

    void prepare (double hostRate, int maxBlockSize);

    /** Starts a note: Poly allocates `unison` voices per note; Mono retriggers a single (unison)
        voice group; Legato glides the sounding group to the new note without retriggering.
        Returns the first voice started (nullptr if no sample / legato retarget). */
    Voice* noteOn (const SampleData* sample, int note, float velocity01, const EngineParams& params) noexcept;
    void noteOff (int note, const EngineParams& params) noexcept;
    void allNotesOff (const EngineParams& params) noexcept;
    void killAll() noexcept;     // fast fade on every voice
    void stopAll() noexcept;     // immediate (used on prepare/reset)

    void render (const VoiceContext& ctx, float* mixL, float* mixR, int n) noexcept;

    int getActiveVoiceCount() const noexcept;    // includes fading (stolen) voices
    int getSoundingVoiceCount() const noexcept;  // active and not being stolen
    const std::vector<Voice>& getVoices() const noexcept { return voices; }

private:
    Voice* chooseVictim() noexcept;
    Voice* findFreeSlot() noexcept;
    Voice* startGroup (const SampleData* sample, int note, float velocity01, const EngineParams& params, double glideFrom) noexcept;
    bool monoGroupSounding() const noexcept;
    void retargetMonoGroup (int note, const EngineParams& params) noexcept;
    void pushHeld (int note) noexcept;
    void removeHeld (int note) noexcept;

    std::array<int, 16> held {};   // mono/legato note stack (last = newest)
    int numHeld = 0;
    int lastNote = -1;             // for glide in poly mode
    int monoNote = -1;

    // Heap-allocated once at construction (each voice owns ~20 KB of grain/formant state);
    // never resized afterwards, so no allocation happens on the audio thread.
    std::vector<Voice> voices = std::vector<Voice> (kNumSlots);
    std::uint64_t ageCounter = 0;
};
} // namespace lhss
