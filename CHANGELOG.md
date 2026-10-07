# Changelog

## [1.0.0] - 2026-10-07

First release of **LH Sample Synth**, a recorded-sound instrument. Load a recording (a bucket, a
plate, glass, a voice, a metal object, a field recording, a percussion hit) and play it from MIDI.

### Downloads
| File | Contents |
|---|---|
| `LH-Sample-Synth-v1.0.0-Windows.zip` | VST3, CLAP, standalone `.exe` (64-bit) |
| `LH-Sample-Synth-v1.0.0-macOS.zip` | VST3, CLAP, Audio Unit, standalone `.app` (Apple Silicon + Intel) |
| `LH-Sample-Synth-v1.0.0-Linux.zip` | VST3, CLAP, standalone app |

See `INSTALL.txt` inside each zip for where to copy the files. The macOS builds are ad-hoc signed
but not notarised. If macOS blocks them, run `xattr -cr <file>` once.

### Sound engine
- **Pitch mode:** duration-preserving granular pitch shifting with phase-aligned grains
  (WSOLA-style). A 1.2 s recording lasts about 1.2 s on every key.
- **Natural mode:** classic resampling, so the recording plays faster and higher, or slower and
  lower.
- 16-voice polyphony with click-free voice stealing.
- ADSR envelope, velocity response, One Shot and Gate trigger modes.
- Sample Start/End and crossfaded looping (works in Natural, Pitch and Reverse modes). Reverse and
  Freeze.
- Granular controls: Grain Size, Density, Position Randomness, Pitch Randomness, Stereo Spread.
- LPC formant shifting (Formant −100…+100 %).

### Synth section
- **LFO:** Sine, Triangle, Square, Saw or Random. Free rate or tempo sync. Modulates pitch
  (vibrato), cutoff, amplitude (tremolo), pan and grain position.
- **Filter:** Low Pass, High Pass or Band Pass, with resonance. It has its own filter envelope
  (±6 octaves) and Velocity > Filter.
- **Voice:** Poly, Mono or Legato with glide. Coarse and fine tune. Unison with 1–4 voices and
  detune.
- **Drive:** soft saturation.
- **Effects:** chorus, tempo-syncable delay with ping-pong, and reverb.

### Workflow
- Waveform display with draggable Sample Start/End and Loop Start/End markers.
- Loads WAV, AIFF, FLAC and OGG, via Load Sample or drag-and-drop. Files are decoded off the
  audio thread.
- Loading a new sample while notes play is safe: sounding notes finish with the old sample.
- **Reset to Default** button (with confirmation). Double-click any knob to reset only that knob.
- The project saves every parameter and the sample's file path. The audio itself is never stored
  in the project. If the file is missing, it is shown as missing and all settings are kept.
- Resizable GUI with large, readable text.

### Formats and platforms
- **Plugin formats:** VST3 and CLAP on Windows, macOS and Linux. Audio Unit on macOS. A
  standalone app on all three.
- **Testing:** pluginval at strictness 10 (VST3) and clap-validator both pass. The automated tests
  run on all three platforms in CI.

### Known limitations
- Shifts larger than about ±1 octave on dense material can sound grainy. Monophonic and
  percussive sources work best.
- The formant stage captures broad resonances, not fine spectral detail.
- High CPU use is possible with 16 voices × 4 unison in Pitch mode with Formant enabled.
- A sample file that has moved must be reloaded manually.
