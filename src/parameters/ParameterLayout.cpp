#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "../engine/EngineParams.h"

namespace lhss
{
namespace
{
using Range = juce::NormalisableRange<float>;
using FloatAttr = juce::AudioParameterFloatAttributes;

juce::ParameterID pid (const char* id) { return { id, ids::parameterVersion }; }

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::String timeText (float ms, int)
{
    if (ms < 9.95f)                     return juce::String (ms, 1) + " ms";
    if (juce::roundToInt (ms) < 1000)   return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms / 1000.0f, 2) + " s";
}

juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::String signedPercentText (float v, int)
{
    const int p = juce::roundToInt (v * 100.0f);
    return (p > 0 ? "+" : "") + juce::String (p) + " %";
}

juce::String regionText (float v, int) { return juce::String (v * 100.0f, 1) + " %"; }

juce::String sustainText (float v, int)
{
    if (v <= 0.0001f) return "-inf dB";
    return juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB";
}

juce::String hzText (float v, int)
{
    if (juce::roundToInt (v) < 1000) return juce::String (juce::roundToInt (v)) + " Hz";
    return juce::String (v / 1000.0f, 1) + " kHz";
}

juce::String panText (float v, int)
{
    const int p = juce::roundToInt (v * 100.0f);
    if (p == 0) return "C";
    return (p < 0 ? "L " : "R ") + juce::String (std::abs (p)) + " %";
}

// ---- text -> value parsers (inverse of the formatters above, used by hosts) ----------------
float parseTime (const juce::String& s)
{
    const auto t = s.trim().toLowerCase();
    const float v = t.getFloatValue();
    return (t.endsWith (" s") || (t.endsWith ("s") && ! t.endsWith ("ms"))) ? v * 1000.0f : v;
}

float parsePercent (const juce::String& s) { return s.trim().getFloatValue() / 100.0f; }

float parseSustain (const juce::String& s)
{
    const auto t = s.trim().toLowerCase();
    if (t.startsWith ("-inf")) return 0.0f;
    if (t.endsWith ("db")) return juce::Decibels::decibelsToGain (t.getFloatValue(), -200.0f);
    return t.getFloatValue();
}

float parseHz (const juce::String& s)
{
    const auto t = s.trim().toLowerCase();
    return t.endsWith ("khz") || t.endsWith ("k") ? t.getFloatValue() * 1000.0f : t.getFloatValue();
}

float parsePan (const juce::String& s)
{
    const auto t = s.trim().toUpperCase();
    if (t == "C" || t.isEmpty()) return 0.0f;
    const float v = t.retainCharacters ("0123456789.").getFloatValue() / 100.0f;
    return t.startsWith ("L") ? -v : (t.startsWith ("R") ? v : t.getFloatValue() / 100.0f);
}

float parsePlain (const juce::String& s) { return s.trim().getFloatValue(); }

juce::String rateText (float v, int)     { return juce::String (v, v < 1.0f ? 2 : 1) + " Hz"; }
juce::String semitoneText (float v, int) { return juce::String (v, 2) + " st"; }
juce::String centText (float v, int)
{
    const int c = juce::roundToInt (v);
    return (c > 0 ? "+" : "") + juce::String (c) + " ct";
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name, Range range, float def,
                                                      juce::String (*text) (float, int),
                                                      float (*parse) (const juce::String&) = parsePlain)
{
    return std::make_unique<juce::AudioParameterFloat> (
        pid (id), name, range, def,
        FloatAttr()
            .withStringFromValueFunction ([text] (float v, int n) { return text (v, n); })
            .withValueFromStringFunction ([parse] (const juce::String& str) { return parse (str); }));
}
} // namespace

juce::String midiNoteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, note), true, true, 3);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt> (
        pid (ids::rootNote), "Root Note", 0, 127, 60,
        juce::AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int v, int) { return midiNoteName (v); })
            .withValueFromStringFunction ([] (const juce::String& s)
            {
                const auto t = s.trim();
                if (t.containsOnly ("0123456789")) return t.getIntValue();
                for (int n = 0; n < 128; ++n)
                    if (midiNoteName (n).equalsIgnoreCase (t)) return n;
                return 60;
            })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::playbackMode), "Playback Mode",
                                                              juce::StringArray { "Natural", "Pitch" }, 1));

    layout.add (makeFloat (ids::sampleStart, "Sample Start", Range (0.0f, 1.0f), 0.0f, regionText, parsePercent));
    layout.add (makeFloat (ids::sampleEnd,   "Sample End",   Range (0.0f, 1.0f), 1.0f, regionText, parsePercent));

    layout.add (makeFloat (ids::attack,  "Attack",  skewed (0.5f, 10000.0f, 200.0f), 5.0f,   timeText, parseTime));
    layout.add (makeFloat (ids::decay,   "Decay",   skewed (1.0f, 20000.0f, 600.0f), 250.0f, timeText, parseTime));
    layout.add (makeFloat (ids::sustain, "Sustain", Range (0.0f, 1.0f), 0.5f, sustainText, parseSustain));
    layout.add (makeFloat (ids::release, "Release", skewed (1.0f, 20000.0f, 800.0f), 400.0f, timeText, parseTime));

    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::triggerMode), "Trigger Mode",
                                                              juce::StringArray { "One Shot", "Gate" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::loopOn), "Loop On", false));
    layout.add (makeFloat (ids::loopStart,     "Loop Start",     Range (0.0f, 1.0f), 0.25f, regionText, parsePercent));
    layout.add (makeFloat (ids::loopEnd,       "Loop End",       Range (0.0f, 1.0f), 0.75f, regionText, parsePercent));
    layout.add (makeFloat (ids::loopCrossfade, "Loop Crossfade", skewed (0.0f, 1000.0f, 100.0f), 50.0f, timeText, parseTime));

    layout.add (makeFloat (ids::grainSize,      "Grain Size",                skewed (10.0f, 500.0f, 80.0f), 80.0f, timeText, parseTime));
    layout.add (makeFloat (ids::grainDensity,   "Grain Density",             skewed (1.0f, 32.0f, 8.0f), 8.0f,
                           [] (float v, int) { return juce::String (v, 1); }));
    layout.add (makeFloat (ids::grainPosRandom, "Grain Position Randomness", Range (0.0f, 1.0f), 0.05f, percentText, parsePercent));
    layout.add (makeFloat (ids::pitchRandom,    "Pitch Randomness",          Range (0.0f, 1.0f), 0.0f,  percentText, parsePercent));
    layout.add (makeFloat (ids::stereoSpread,   "Stereo Spread",             Range (0.0f, 1.0f), 0.3f,  percentText, parsePercent));

    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::freeze),  "Freeze",  false));
    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::reverse), "Reverse", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::filterMode), "Filter Mode",
                                                              juce::StringArray { "Low Pass", "High Pass", "Band Pass" }, 0));
    layout.add (makeFloat (ids::cutoff,    "Cutoff",    skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hzText, parseHz));
    layout.add (makeFloat (ids::resonance, "Resonance", Range (0.0f, 1.0f), 0.1f,
                           [] (float v, int) { return juce::String (v, 2); }));

    layout.add (makeFloat (ids::formant, "Formant", Range (-1.0f, 1.0f), 0.0f, signedPercentText, parsePercent));

    layout.add (makeFloat (ids::pan,         "Pan",          Range (-1.0f, 1.0f), 0.0f, panText, parsePan));
    layout.add (makeFloat (ids::stereoWidth, "Stereo Width", Range (0.0f, 2.0f), 1.0f, percentText, parsePercent));
    layout.add (makeFloat (ids::outputGain,  "Output Gain",  Range (-48.0f, 12.0f), 0.0f,
                           [] (float v, int) { return juce::String (v, 1) + " dB"; }));

    layout.add (makeFloat (ids::velocitySens, "Velocity", Range (0.0f, 1.0f), 1.0f, percentText, parsePercent));
    layout.add (std::make_unique<juce::AudioParameterInt> (pid (ids::polyphony), "Polyphony", 1, 16, 16));

    // ---- LFO -----------------------------------------------------------------------------
    juce::StringArray divisions;
    for (auto* d : kSyncDivisionNames) divisions.add (d);

    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::lfoShape), "LFO Shape",
                                                              juce::StringArray { "Sine", "Triangle", "Square", "Saw", "Random" }, 0));
    layout.add (makeFloat (ids::lfoRate, "LFO Rate", skewed (0.05f, 20.0f, 2.0f), 4.0f, rateText, parseHz));
    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::lfoSync), "LFO Sync", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::lfoDivision), "LFO Division", divisions, 2));
    layout.add (makeFloat (ids::lfoToPitch,    "LFO > Pitch",     skewed (0.0f, 12.0f, 1.0f), 0.0f, semitoneText));
    layout.add (makeFloat (ids::lfoToCutoff,   "LFO > Cutoff",    Range (-1.0f, 1.0f), 0.0f, signedPercentText, parsePercent));
    layout.add (makeFloat (ids::lfoToAmp,      "LFO > Amp",       Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));
    layout.add (makeFloat (ids::lfoToPan,      "LFO > Pan",       Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));
    layout.add (makeFloat (ids::lfoToGrainPos, "LFO > Grain Pos", Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));

    // ---- Filter envelope -------------------------------------------------------------------
    layout.add (makeFloat (ids::fenvAttack,  "Filter Attack",  skewed (0.5f, 10000.0f, 200.0f), 1.0f,   timeText, parseTime));
    layout.add (makeFloat (ids::fenvDecay,   "Filter Decay",   skewed (1.0f, 20000.0f, 600.0f), 300.0f, timeText, parseTime));
    layout.add (makeFloat (ids::fenvSustain, "Filter Sustain", Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));
    layout.add (makeFloat (ids::fenvRelease, "Filter Release", skewed (1.0f, 20000.0f, 800.0f), 300.0f, timeText, parseTime));
    layout.add (makeFloat (ids::fenvAmount,  "Filter Env Amount", Range (-1.0f, 1.0f), 0.0f, signedPercentText, parsePercent));
    layout.add (makeFloat (ids::velToFilter, "Velocity > Filter", Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));

    // ---- Voice -----------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::voiceMode), "Voice Mode",
                                                              juce::StringArray { "Poly", "Mono", "Legato" }, 0));
    layout.add (makeFloat (ids::glide, "Glide", skewed (0.0f, 2000.0f, 200.0f), 0.0f, timeText, parseTime));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        pid (ids::coarseTune), "Coarse Tune", -24, 24, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                           { return (v > 0 ? "+" : "") + juce::String (v) + " st"; })
                                           .withValueFromStringFunction ([] (const juce::String& t) { return t.trim().getIntValue(); })));
    layout.add (makeFloat (ids::fineTune, "Fine Tune", Range (-100.0f, 100.0f), 0.0f, centText));
    layout.add (std::make_unique<juce::AudioParameterInt> (pid (ids::unisonVoices), "Unison", 1, kMaxUnison, 1));
    layout.add (makeFloat (ids::unisonDetune, "Unison Detune", Range (0.0f, 50.0f), 12.0f, centText));
    layout.add (makeFloat (ids::drive, "Drive", Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));

    // ---- Effects ---------------------------------------------------------------------------
    layout.add (makeFloat (ids::chorusRate,  "Chorus Rate",  skewed (0.05f, 5.0f, 0.8f), 0.8f, rateText, parseHz));
    layout.add (makeFloat (ids::chorusDepth, "Chorus Depth", Range (0.0f, 1.0f), 0.5f, percentText, parsePercent));
    layout.add (makeFloat (ids::chorusMix,   "Chorus Mix",   Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));
    layout.add (makeFloat (ids::delayTime,   "Delay Time",   skewed (10.0f, 2000.0f, 300.0f), 375.0f, timeText, parseTime));
    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::delaySync), "Delay Sync", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids::delayDivision), "Delay Division", divisions, 12));
    layout.add (makeFloat (ids::delayFeedback, "Delay Feedback", Range (0.0f, 0.95f), 0.35f, percentText, parsePercent));
    layout.add (makeFloat (ids::delayMix,      "Delay Mix",      Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));
    layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids::delayPingPong), "Delay Ping Pong", false));
    layout.add (makeFloat (ids::reverbSize,    "Reverb Size",    Range (0.0f, 1.0f), 0.6f, percentText, parsePercent));
    layout.add (makeFloat (ids::reverbDamping, "Reverb Damping", Range (0.0f, 1.0f), 0.5f, percentText, parsePercent));
    layout.add (makeFloat (ids::reverbMix,     "Reverb Mix",     Range (0.0f, 1.0f), 0.0f, percentText, parsePercent));

    return layout;
}
} // namespace lhss
