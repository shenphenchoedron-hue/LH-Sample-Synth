# Third-party dependencies

| Dependency | Version | Use | License |
|---|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.15 (git submodule `libs/JUCE`) | Plugin wrappers (VST3, AU, Standalone), parameters, MIDI, audio decoding, GUI, state | AGPLv3 (open-source option of the JUCE dual licence) |
| [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) | submodule `libs/clap-juce-extensions` | CLAP wrapper around the JUCE processor | MIT |
| [CLAP](https://github.com/free-audio/clap) | 1.2.x (nested submodule) | CLAP plugin API headers | MIT |
| [clap-helpers](https://github.com/free-audio/clap-helpers) | nested submodule | CLAP helper classes used by the wrapper | MIT |
| VST3 SDK | bundled inside JUCE | VST3 format | MIT (Steinberg VST3 SDK, since 3.8) |
| FLAC / Ogg Vorbis decoders | bundled inside JUCE | Optional FLAC/OGG loading | BSD-style (Xiph.Org) |

No commercial or subscription dependency is used. AAX is not built.

Because JUCE is used under the AGPLv3, LH Sample Synth (source and binaries) is distributed under
the GNU Affero General Public License v3 — see `LICENSE`. MIT/BSD components are AGPL-compatible.
