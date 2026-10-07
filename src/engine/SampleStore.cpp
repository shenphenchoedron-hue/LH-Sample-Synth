#include "SampleStore.h"
#include "InstrumentEngine.h"

namespace lhss
{
void SampleStore::publish (std::shared_ptr<const SampleData> sample, InstrumentEngine& engine)
{
    if (sample == nullptr) return;
    {
        const std::scoped_lock lock (mutex);
        retained.push_back (sample);
        engine.setPendingSample (sample.get());
    }
    collectGarbage (engine);
}

void SampleStore::collectGarbage (const InstrumentEngine& engine)
{
    const std::scoped_lock lock (mutex);
    const auto* acknowledged = engine.getAcknowledgedSample();
    if (acknowledged == nullptr) return;

    size_t ackIndex = retained.size();
    for (size_t i = 0; i < retained.size(); ++i)
        if (retained[i].get() == acknowledged) { ackIndex = i; break; }
    if (ackIndex == retained.size()) return;

    // Everything before the acknowledged sample can never be handed to a new voice again.
    std::vector<std::shared_ptr<const SampleData>> keep;
    keep.reserve (retained.size());
    for (size_t i = 0; i < retained.size(); ++i)
        if (i >= ackIndex || retained[i]->voiceRefs.load (std::memory_order_acquire) > 0)
            keep.push_back (retained[i]);
    retained.swap (keep);
}

std::shared_ptr<const SampleData> SampleStore::latest() const
{
    const std::scoped_lock lock (mutex);
    return retained.empty() ? nullptr : retained.back();
}

size_t SampleStore::retainedCount() const
{
    const std::scoped_lock lock (mutex);
    return retained.size();
}
} // namespace lhss
