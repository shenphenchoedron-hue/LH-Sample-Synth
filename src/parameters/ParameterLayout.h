#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lhss
{
/** Builds the complete APVTS parameter layout (all ranges, defaults and display strings). */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Note name with C3 = MIDI 60 convention. */
juce::String midiNoteName (int note);
} // namespace lhss
