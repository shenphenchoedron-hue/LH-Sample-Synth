#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

#include "SampleData.h"

namespace lhss
{
/** Decodes audio files into immutable SampleData. Must never be called on the audio thread:
    it performs file I/O and allocates. */
class SampleLoader
{
public:
    struct Result
    {
        std::shared_ptr<const SampleData> sample;
        juce::String error;
        bool ok() const noexcept { return sample != nullptr; }
    };

    static constexpr double kMaxSeconds = 20.0 * 60.0;

    static Result loadFile (const juce::File& file);

    /** Wraps an existing buffer (used for tests and programmatic sources). Keeps max 2 channels. */
    static std::shared_ptr<const SampleData> fromBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                                        const juce::String& name, const juce::String& path = {});

    static juce::String supportedWildcard();
};
} // namespace lhss
