#pragma once

// Stable parameter IDs. These strings are stored in host projects and plugin state:
// never rename or reuse them.
namespace lhss::ids
{
inline constexpr const char* rootNote        = "rootNote";
inline constexpr const char* playbackMode    = "playbackMode";
inline constexpr const char* sampleStart     = "sampleStart";
inline constexpr const char* sampleEnd       = "sampleEnd";
inline constexpr const char* attack          = "attack";
inline constexpr const char* decay           = "decay";
inline constexpr const char* sustain         = "sustain";
inline constexpr const char* release         = "release";
inline constexpr const char* triggerMode     = "triggerMode";
inline constexpr const char* loopOn          = "loopOn";
inline constexpr const char* loopStart       = "loopStart";
inline constexpr const char* loopEnd         = "loopEnd";
inline constexpr const char* loopCrossfade   = "loopCrossfade";
inline constexpr const char* grainSize       = "grainSize";
inline constexpr const char* grainDensity    = "grainDensity";
inline constexpr const char* grainPosRandom  = "grainPosRandom";
inline constexpr const char* pitchRandom     = "pitchRandom";
inline constexpr const char* stereoSpread    = "stereoSpread";
inline constexpr const char* freeze          = "freeze";
inline constexpr const char* reverse         = "reverse";
inline constexpr const char* filterMode      = "filterMode";
inline constexpr const char* cutoff          = "cutoff";
inline constexpr const char* resonance       = "resonance";
inline constexpr const char* formant         = "formant";
inline constexpr const char* pan             = "pan";
inline constexpr const char* stereoWidth     = "stereoWidth";
inline constexpr const char* outputGain      = "outputGain";
inline constexpr const char* velocitySens    = "velocitySens";
inline constexpr const char* polyphony       = "polyphony";

inline constexpr int parameterVersion = 1;
} // namespace lhss::ids
