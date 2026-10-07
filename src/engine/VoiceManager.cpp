#include "VoiceManager.h"

#include <algorithm>

namespace lhss
{
void VoiceManager::prepare (double hostRate, int maxBlockSize)
{
    std::uint32_t seed = 0x1234567u;
    for (auto& v : voices) v.prepare (hostRate, maxBlockSize, seed += 0x9E3779B9u);
    ageCounter = 0;
    numHeld = 0;
    monoNote = lastNote = -1;
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

Voice* VoiceManager::startGroup (const SampleData* sample, int note, float velocity01, const EngineParams& params,
                                double glideFrom) noexcept
{
    const int unison = std::clamp (params.unisonVoices, 1, kMaxUnison);
    const int limit = std::min (std::clamp (params.polyphony, 1, kMaxPolyphony) * unison, kMaxSounding);

    int sounding = getSoundingVoiceCount();
    while (sounding + unison > limit)
    {
        Voice* victim = chooseVictim();
        if (victim == nullptr) break;
        victim->steal();
        --sounding;
    }

    Voice* first = nullptr;
    const std::uint64_t age = ++ageCounter;
    for (int k = 0; k < unison; ++k)
    {
        // Spread voices symmetrically: detune ±detune cents, pan ±0.7.
        const float spread = unison > 1 ? (static_cast<float> (k) / static_cast<float> (unison - 1)) * 2.0f - 1.0f : 0.0f;
        VoiceStartOptions opt;
        opt.glideFromNote = glideFrom;
        opt.detuneCents = spread * params.unisonDetune;
        opt.panOffset = spread * 0.7f;
        Voice* v = findFreeSlot();
        v->start (sample, note, velocity01, age, params, opt);
        if (first == nullptr) first = v;
    }
    return first;
}

bool VoiceManager::monoGroupSounding() const noexcept
{
    for (const auto& v : voices)
        if (v.isActive() && ! v.isKilling() && ! v.hasReceivedNoteOff()) return true;
    return false;
}

void VoiceManager::retargetMonoGroup (int note, const EngineParams& params) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && ! v.isKilling() && ! v.hasReceivedNoteOff()) v.retarget (note, params.glideMs);
    monoNote = note;
}

void VoiceManager::pushHeld (int note) noexcept
{
    removeHeld (note);
    if (numHeld == static_cast<int> (held.size()))
    {
        std::move (held.begin() + 1, held.end(), held.begin());
        --numHeld;
    }
    held[(size_t) numHeld++] = note;
}

void VoiceManager::removeHeld (int note) noexcept
{
    int w = 0;
    for (int r = 0; r < numHeld; ++r)
        if (held[(size_t) r] != note) held[(size_t) w++] = held[(size_t) r];
    numHeld = w;
}

Voice* VoiceManager::noteOn (const SampleData* sample, int note, float velocity01, const EngineParams& params) noexcept
{
    if (sample == nullptr) return nullptr;

    const bool glide = params.glideMs > 0.0f && lastNote >= 0;
    Voice* started = nullptr;

    if (params.voiceMode == VoiceMode::Poly)
    {
        double from = glide ? static_cast<double> (lastNote) : -1.0;
        // Glide from where the last voice actually is (it may itself still be gliding).
        if (glide)
        {
            std::uint64_t newest = 0;
            for (const auto& v : voices)
                if (v.isActive() && ! v.isKilling() && v.getAge() >= newest) { newest = v.getAge(); from = v.getCurrentNote(); }
        }
        started = startGroup (sample, note, velocity01, params, from);
    }
    else
    {
        pushHeld (note);
        if (params.voiceMode == VoiceMode::Legato && monoGroupSounding())
        {
            retargetMonoGroup (note, params);
        }
        else
        {
            double from = glide ? static_cast<double> (lastNote) : -1.0;
            for (auto& v : voices)
                if (v.isActive() && ! v.isKilling())
                {
                    if (glide) from = v.getCurrentNote();
                    v.steal();
                }
            started = startGroup (sample, note, velocity01, params, from);
            monoNote = note;
        }
    }

    lastNote = note;
    return started;
}

void VoiceManager::noteOff (int note, const EngineParams& params) noexcept
{
    if (params.voiceMode != VoiceMode::Poly)
    {
        removeHeld (note);
        if (note != monoNote) return;
        if (numHeld > 0)
        {
            // Return to the most recent key still held (legato glide, no retrigger).
            const int back = held[(size_t) (numHeld - 1)];
            retargetMonoGroup (back, params);
            lastNote = back;
            return;
        }
        for (auto& v : voices)
            if (v.isActive() && ! v.hasReceivedNoteOff()) v.noteOff (params);
        monoNote = -1;
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note && ! v.hasReceivedNoteOff()) v.noteOff (params);
}

void VoiceManager::allNotesOff (const EngineParams& params) noexcept
{
    numHeld = 0;
    monoNote = -1;
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
    numHeld = 0;
    monoNote = -1;
    lastNote = -1;
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
