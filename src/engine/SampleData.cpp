#include "SampleData.h"

namespace lhss
{
SampleData::SampleData (juce::AudioBuffer<float>&& a, double sr, juce::String displayName, juce::String filePath)
    : audio (std::move (a)), sampleRate (sr > 0.0 ? sr : 44100.0),
      name (std::move (displayName)), path (std::move (filePath))
{
    const int frames = audio.getNumSamples();
    const int chans  = audio.getNumChannels();
    overviewMin.assign (kOverviewBins, 0.0f);
    overviewMax.assign (kOverviewBins, 0.0f);
    if (frames <= 0 || chans <= 0) return;

    pitch = PitchDetector::analyse (audio, sampleRate);

    for (int bin = 0; bin < kOverviewBins; ++bin)
    {
        const int b0 = static_cast<int> (static_cast<juce::int64> (bin) * frames / kOverviewBins);
        const int b1 = juce::jmax (b0 + 1, static_cast<int> (static_cast<juce::int64> (bin + 1) * frames / kOverviewBins));
        float lo = 0.0f, hi = 0.0f;
        for (int i = b0; i < juce::jmin (b1, frames); ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < chans; ++c) v += audio.getSample (c, i);
            v /= static_cast<float> (chans);
            lo = juce::jmin (lo, v);
            hi = juce::jmax (hi, v);
        }
        overviewMin[(size_t) bin] = lo;
        overviewMax[(size_t) bin] = hi;
    }
}

dsp::SourceView SampleData::view() const noexcept
{
    dsp::SourceView v;
    if (getNumChannels() == 0) return v;
    v.left = audio.getReadPointer (0);
    v.right = getNumChannels() > 1 ? audio.getReadPointer (1) : v.left;
    v.numFrames = getNumFrames();
    v.sampleRate = sampleRate;
    return v;
}
} // namespace lhss
