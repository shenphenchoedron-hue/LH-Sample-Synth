# Changelog

## [1.1.0] - 2026-10-07

Automatic tuning to the piano standard, an 88-key keyboard in the standalone app and an LFO on/off
switch.

### Downloads
| File | Contents |
|---|---|
| `LH-Sample-Synth-v1.1.0-Windows.zip` | VST3, CLAP, standalone `.exe` (64-bit) |
| `LH-Sample-Synth-v1.1.0-macOS.zip` | VST3, CLAP, Audio Unit, standalone `.app` (Apple Silicon + Intel) |
| `LH-Sample-Synth-v1.1.0-Linux.zip` | VST3, CLAP, standalone app |

See `INSTALL.txt` inside each zip for where to copy the files. The macOS builds are ad-hoc signed
but not notarised. If macOS blocks them, run `xattr -cr <file>` once.

### Added
- **All formats: automatic tuning.** The pitch of a loaded sample is analysed (YIN, 40 Hz - 4.2 kHz,
  sharpened with a long FFT). Root Note and a new **Root Tune** parameter (-50 to +50 cents) are set
  from it, so every key plays at its equal-tempered pitch (A4 = 440 Hz) and fits other music. A clean
  440 Hz sample plays 440.0 Hz on A4 and 261.6 Hz on C4.
  - The header shows the detected pitch and has a **Tune to Pitch** button.
  - Sounds without a clear pitch (noise, clicks, melodies) play as recorded on C4 (MIDI 60).
  - For inharmonic sounds (glass, bells, metal) the dominant partial is used, which is what a
    tuner shows.
  - Saved projects keep their stored Root Note and Root Tune when they are opened. Reset to Default
    re-applies the detected tuning of the loaded sample.
- **All formats: LFO On switch** in the LFO panel. Off (the default for new instances) means no LFO
  modulation at all, whatever the LFO amounts are set to. Projects saved with 1.0.0 that use the LFO
  open with it switched on.
- **Standalone app: 88-key on-screen keyboard** (A0 - C8) below the controls. Play it with the mouse
  or, after clicking it, with the computer keys. Notes from a connected MIDI keyboard light up the
  keys. The plugin formats do not show the keyboard.

### Changed
- Note names now follow the piano standard (scientific pitch notation): middle C = **C4** = MIDI 60
  = 261.63 Hz, A4 = 440 Hz. Before, MIDI 60 was called C3. Only the names change: saved projects and
  MIDI note numbers sound exactly as before.

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
