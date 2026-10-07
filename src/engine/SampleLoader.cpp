#include "SampleLoader.h"

namespace lhss
{
juce::String SampleLoader::supportedWildcard()
{
    // Native Linux dialogs (zenity) match patterns case-sensitively, so list every casing
    // users actually have on disk (e.g. "Track.FLAC", "Take.Wav").
    juce::StringArray patterns;
    for (auto ext : { "wav", "wave", "aif", "aiff", "aifc", "flac", "ogg" })
    {
        const juce::String e (ext);
        patterns.addIfNotAlreadyThere ("*." + e);
        patterns.addIfNotAlreadyThere ("*." + e.toUpperCase());
        patterns.addIfNotAlreadyThere ("*." + e.substring (0, 1).toUpperCase() + e.substring (1));
    }
    return patterns.joinIntoString (";");
}

SampleLoader::Result SampleLoader::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return { nullptr, "File not found: " + file.getFullPathName() };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return { nullptr, "Unsupported or unreadable audio file: " + file.getFileName() };

    if (reader->sampleRate <= 0.0 || reader->lengthInSamples < 2 || reader->numChannels == 0)
        return { nullptr, "Audio file is empty or invalid: " + file.getFileName() };

    const auto maxFrames = static_cast<juce::int64> (kMaxSeconds * reader->sampleRate);
    const int frames = static_cast<int> (juce::jmin (reader->lengthInSamples, maxFrames));
    const int chans = static_cast<int> (juce::jmin<unsigned int> (reader->numChannels, 2u));

    juce::AudioBuffer<float> buffer (chans, frames);
    if (! reader->read (&buffer, 0, frames, 0, true, chans > 1))
        return { nullptr, "Failed to decode: " + file.getFileName() };

    return { std::make_shared<const SampleData> (std::move (buffer), reader->sampleRate,
                                                 file.getFileName(), file.getFullPathName()), {} };
}

std::shared_ptr<const SampleData> SampleLoader::fromBuffer (const juce::AudioBuffer<float>& src, double sampleRate,
                                                          const juce::String& name, const juce::String& path)
{
    const int chans = juce::jlimit (1, 2, src.getNumChannels());
    juce::AudioBuffer<float> copy (chans, src.getNumSamples());
    for (int c = 0; c < chans; ++c)
        copy.copyFrom (c, 0, src, juce::jmin (c, src.getNumChannels() - 1), 0, src.getNumSamples());
    return std::make_shared<const SampleData> (std::move (copy), sampleRate, name, path);
}
} // namespace lhss
