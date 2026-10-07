#include "VoiceManager.h"

#include <algorithm>

namespace lhss
{
void VoiceManager::prepare (double hostRate, int maxBlockSize)
{
    std::uint32_t seed = 0x1234567u;
    for (auto& v : voices) v.prepare (hostRate, maxBlockSize, seed += 0x9E3779B9u);
    ageCounter = 0;
}

Voice* VoiceManager::chooseVictim() noexcept
{
    Voice* releasedVictim = nullptr;
    Voice* oldest = nullptr;
    for (auto& v : voices)
    {
        if (! v.isActive() || v.isKilling()) continue;
        if (v.isReleased() && (releasedVictim == nullptr || v.getAge() < releasedVictim->getAge())) releasedVictim = &v;
        if (oldest == nullptr || v.getAge() < oldest->getAge()) oldest = &v;
    }
    return releasedVictim != nullptr ? releasedVictim : oldest;
}

Voice* VoiceManager::findFreeSlot() noexcept
{
    for (auto& v : voices)
        if (! v.isActive()) return &v;

    // Every slot busy (many tails fading): reuse the oldest fading voice, else the oldest one.
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isKilling() && (best == nullptr || v.getAge() < best->getAge())) best = &v;
    if (best == nullptr)
        for (auto& v : voices)
            if (best == nullptr || v.getAge() < best->getAge()) best = &v;
    best->forceStop();
    return best;
}

Voice* VoiceManager::noteOn (const SampleData* sample, int note, float velocity01, const EngineParams& params) noexcept
{
    if (sample == nullptr) return nullptr;

    const int limit = std::clamp (params.polyphony, 1, kMaxPolyphony);
    int sounding = getSoundingVoiceCount();
    while (sounding >= limit)
    {
        Voice* victim = chooseVictim();
        if (victim == nullptr) break;
        victim->steal();
        --sounding;
    }

    Voice* v = findFreeSlot();
    v->start (sample, note, velocity01, ++ageCounter, params);
    return v;
}

void VoiceManager::noteOff (int note, const EngineParams& params) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && ! v.hasReceivedNoteOff()) v.noteOff (params);
}

void VoiceManager::allNotesOff (const EngineParams& params) noexcept
{
    for (auto& v : voices)
        if (v.isActive()) v.noteOff (params);
}

void VoiceManager::killAll() noexcept
{
    for (auto& v : voices) v.steal();
}

void VoiceManager::stopAll() noexcept
{
    for (auto& v : voices) v.forceStop();
}

void VoiceManager::render (const VoiceContext& ctx, float* mixL, float* mixR, int n) noexcept
{
    for (auto& v : voices)
        if (v.isActive()) v.render (ctx, mixL, mixR, n);
}

int VoiceManager::getActiveVoiceCount() const noexcept
{
    return static_cast<int> (std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.isActive(); }));
}

int VoiceManager::getSoundingVoiceCount() const noexcept
{
    return static_cast<int> (std::count_if (voices.begin(), voices.end(),
                                            [] (const Voice& v) { return v.isActive() && ! v.isKilling(); }));
}
} // namespace lhss
