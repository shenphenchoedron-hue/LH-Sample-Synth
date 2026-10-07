#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lhss
{
/** Builds the complete APVTS parameter layout (all ranges, defaults and display strings). */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Note name in scientific pitch notation, as on a piano: C4 = MIDI 60 = 261.63 Hz (middle C), A4 = 440 Hz. */
juce::String midiNoteName (int note);
} // namespace lhss
