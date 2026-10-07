#include "ParameterLayout.h"
#include "ParameterIDs.h"

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
    if (ms < 10.0f)   return juce::String (ms, 1) + " ms";
    if (ms < 1000.0f) return juce::String (juce::roundToInt (ms)) + " ms";
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

    return layout;
}
} // namespace lhss
