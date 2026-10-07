#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <memory>

#include "engine/InstrumentEngine.h"
#include "engine/SampleLoader.h"
#include "engine/SampleStore.h"

namespace lhss::test
{
inline juce::AudioBuffer<float> makeSine (double freq, double seconds, double sr, int channels = 1, float amp = 0.5f)
{
    const int n = static_cast<int> (seconds * sr);
    juce::AudioBuffer<float> b (channels, n);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < n; ++i)
            b.setSample (c, i, amp * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr)));
    return b;
}

inline std::shared_ptr<const SampleData> makeSineSample (double freq, double seconds, double sr, int channels = 1)
{
    return SampleLoader::fromBuffer (makeSine (freq, seconds, sr, channels), sr, "sine");
}

inline void noteOn (InstrumentEngine& e, int note, int vel = 100)
{
    const juce::uint8 msg[3] { 0x90, static_cast<juce::uint8> (note), static_cast<juce::uint8> (vel) };
    e.handleMidi (msg, 3);
}

inline void noteOff (InstrumentEngine& e, int note)
{
    const juce::uint8 msg[3] { 0x80, static_cast<juce::uint8> (note), 0 };
    e.handleMidi (msg, 3);
}

/** Processes empty blocks so pending samples get adopted. */
inline void runBlocks (InstrumentEngine& e, int blocks, int blockSize = 256)
{
    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;
    for (int i = 0; i < blocks; ++i) e.process (buf, midi);
}
} // namespace lhss::test
