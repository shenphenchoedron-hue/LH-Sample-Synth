#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "SampleData.h"

namespace lhss
{
class InstrumentEngine;

/** Owns every SampleData that the audio thread might still be using.

    Sample swapping protocol (lock-free for the audio thread):
      1. A loader thread builds a new immutable SampleData.
      2. publish() appends it to `retained` (mutex, never touched by audio) and hands the raw
         pointer to the engine via an atomic exchange (InstrumentEngine::setPendingSample).
      3. At the start of each block the audio thread adopts the pending pointer as its
         "current" sample and stores it in an atomic "acknowledged" slot. Only the current
         sample is ever given to new voices; old voices keep their own pointer and increment /
         decrement SampleData::voiceRefs.
      4. collectGarbage() frees a sample only if it is older than the acknowledged one (so the
         audio thread can never start a new voice on it) AND its voiceRefs is zero.
    Thus no memory is freed while a voice is still reading it and the audio thread never
    allocates, frees or locks. */
class SampleStore
{
public:
    void publish (std::shared_ptr<const SampleData> sample, InstrumentEngine& engine);
    void collectGarbage (const InstrumentEngine& engine);

    std::shared_ptr<const SampleData> latest() const;
    size_t retainedCount() const;

private:
    mutable std::mutex mutex;
    std::vector<std::shared_ptr<const SampleData>> retained;
};
} // namespace lhss
