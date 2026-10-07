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

// LFO
inline constexpr const char* lfoShape        = "lfoShape";
inline constexpr const char* lfoRate         = "lfoRate";
inline constexpr const char* lfoSync         = "lfoSync";
inline constexpr const char* lfoDivision     = "lfoDivision";
inline constexpr const char* lfoToPitch      = "lfoToPitch";
inline constexpr const char* lfoToCutoff     = "lfoToCutoff";
inline constexpr const char* lfoToAmp        = "lfoToAmp";
inline constexpr const char* lfoToPan        = "lfoToPan";
inline constexpr const char* lfoToGrainPos   = "lfoToGrainPos";
// Filter envelope
inline constexpr const char* fenvAttack      = "fenvAttack";
inline constexpr const char* fenvDecay       = "fenvDecay";
inline constexpr const char* fenvSustain     = "fenvSustain";
inline constexpr const char* fenvRelease     = "fenvRelease";
inline constexpr const char* fenvAmount      = "fenvAmount";
inline constexpr const char* velToFilter     = "velToFilter";
// Voice
inline constexpr const char* voiceMode       = "voiceMode";
inline constexpr const char* glide           = "glide";
inline constexpr const char* coarseTune      = "coarseTune";
inline constexpr const char* fineTune        = "fineTune";
inline constexpr const char* unisonVoices    = "unisonVoices";
inline constexpr const char* unisonDetune    = "unisonDetune";
inline constexpr const char* drive           = "drive";
// Effects
inline constexpr const char* chorusRate      = "chorusRate";
inline constexpr const char* chorusDepth     = "chorusDepth";
inline constexpr const char* chorusMix       = "chorusMix";
inline constexpr const char* delayTime       = "delayTime";
inline constexpr const char* delaySync       = "delaySync";
inline constexpr const char* delayDivision   = "delayDivision";
inline constexpr const char* delayFeedback   = "delayFeedback";
inline constexpr const char* delayMix        = "delayMix";
inline constexpr const char* delayPingPong   = "delayPingPong";
inline constexpr const char* reverbSize      = "reverbSize";
inline constexpr const char* reverbDamping   = "reverbDamping";
inline constexpr const char* reverbMix       = "reverbMix";
// Pitch analysis: how many cents the sample sounds away from its Root Note (set on load).
inline constexpr const char* rootTune        = "rootTune";
// LFO master switch: off = no LFO modulation at all, whatever the amounts are.
inline constexpr const char* lfoOn           = "lfoOn";

inline constexpr int parameterVersion = 1;
} // namespace lhss::ids
