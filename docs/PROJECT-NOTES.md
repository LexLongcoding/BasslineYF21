# Bassline YF-21 - project notes (for contributors and for resuming work)

Last updated: 2026-11-14, version 0.1.0. License: AGPL-3.0-or-later, copyright LexLongcoding.
Sister project: Decu6000 (drum machine) - same fonts, colours, knob drawing, LCD, patch system and MIDI learn.

## What it is
Monophonic acid bass synth, VST3 + Standalone, Windows + Linux, JUCE 8.0.4 (fetched by CMake).
Name is a Macross Plus reference (owner's choice). References: Roland TB-303, Korg Volca Bass.

## Design decisions (and why)
- **One page**: synth panels on top (OSC 1-3, LFO, FILTER / AMP, FOLD, DIST, SCOPE, MASTER), sequencer
  grid underneath (owner's request). 1440x930 design canvas scaled to the window (Decu approach).
- **DAW recording model** (discussed with the owner): Breaktweaker-style.
  - PATTERN mode: 16 trigger keys (C0-D#1 default; C-1 / C-2 selectable because C0-D#1 overlaps the bass
    range) play slots 1-16 while held. While a slot is held, any other key transposes it relative to C3.
    With no slot held, keys play the synth.
  - FREE mode (owner's request): plain mono synth, nothing triggers. Overlap = slide, velocity >= 100 = accent.
  - PATTERN START switch: SYNC (step under the DAW position) / RETRIG (step 1 on the next 16th).
  - Drag-out MIDI and transport free-run were offered and **not** wanted (for now).
  - The on-screen keyboard always plays the synth / records; it never triggers slots.
- **Recording**: REC + stopped = step record at the cursor; REC + PLAY = live record, quantised to the
  nearest step (the step is suppressed for the rest of that pass so it isn't heard twice). Legato = slide on
  the previous recorded step. Recording always goes into the selected (edit) slot.
- **Patterns** are lock-free: every step is one atomic word (Pattern.h); the UI and the audio thread both
  write. Bank (16 slots + names + length) is saved with the project. Library files: .yfpattern XML with
  steps, length, swing, gate. Factory patterns are written as text in FactoryPatterns.h.
- **Patches** = parameters with `inPatch = true` (Parameters.h). Tempo, sync, swing, gate, play mode,
  start mode and trigger keys are not in patches. Patches are not host programs (Decu lesson: Cubase
  re-sent program 0).
- **DSP** (all JUCE-free, unit testable): control at base rate; oscillators, filter, folder, distortion at
  4x (JUCE polyphase IIR oversampler; the voice renders straight into the upsampled block).
  - Oscillators: PolyBLEP saw/pulse, triangle, phase-distorted sine, wavetables (8 tables x 8 frames x 10
    band-limited levels, built at startup with an in-house FFT).
  - Filter: 303 mode = 4 TPT stages, feedback through a 180 Hz high-pass, output from stage 3, tanh input +
    stage 2; ladder mode = stage 4 output; 12 dB TPT SVF. Feedback is one sample late (fine at 4x).
  - Accent: VCA boost, shortest filter decay (<= 200 ms), and an accent-sweep capacitor (20 ms charge,
    300 ms discharge) adding up to 3 octaves.
  - Signal chain: oscs -> filter -> VCA -> wave folder -> distortion -> DC block -> volume -> soft limiter.
- **MIDI learn**: every control carries the `yfParam` component property; the learn overlay finds them
  generically. CC map saved with the project, not in patches.

## Source map
| File | Role |
|---|---|
| `Source/Parameters.h` | All 63 parameters (id, name, label, range, skew, default, unit, choices, inPatch) |
| `Source/AcidVoice.h` | The voice: glide, envelopes, accent sweep, oscs, filter, effects |
| `Source/Oscillator.h`, `Wavetables.h` | Oscillators and generated wavetables |
| `Source/Filter.h`, `Envelope.h`, `Lfo.h`, `Effects.h` | DSP building blocks |
| `Source/Pattern.h`, `Sequencer.h` | Step format, atomic 16-slot bank, step clock (swing, gate, slide rules) |
| `Source/PluginProcessor.*` | MIDI modes, trigger/transpose stacks, recording, host sync, CC learn, state |
| `Source/PatchManager.*`, `FactoryPatches.h` | Sound patches (from Decu6000) |
| `Source/PatternManager.*`, `FactoryPatterns.h` | Pattern library |
| `Source/PluginEditor.*` | Header (tempo, sync, patch LCD, MIDI LEARN), layout, patch dialogs |
| `Source/UI/Theme.*` | Colours, embedded fonts, knob / panel / LCD drawing |
| `Source/UI/Widgets.*` | Knob, ParamToggle, ParamCombo, LCD, BPM display, meter, scope, learn overlay |
| `Source/UI/SynthPanels.*` | OSC (with waveform preview), LFO, FILTER, AMP, FOLD, DIST, SCOPE, MASTER |
| `Source/UI/SequencerPanel.*` | Transport, slots, pattern menu, step grid + symbols, mini keyboard |

## Testing
- `Tests/voice_render_test.cpp` (no JUCE) - 26 renders / checks.
- `YF21Tests` (`-DYF21_BUILD_TESTS=ON`) - 39 checks + editor screenshots; `YF21HostTest` loads the .vst3.
- Headless Linux machines without ALSA / Xrandr headers: configure with `-DYF21_HEADLESS_TEST_BUILD=ON`
  and `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0"`
  (the flags reach JUCE's juceaide helper build too).

## Status at 2026-11-14
- Linux Debug VST3 + Standalone build with zero warnings from our code; all tests pass; VST3 loads in a
  host test, plays slot 1 from its trigger key, state round-trips, editor opens.
- Not yet: Windows build (CI workflow included), real audio hardware, a real DAW session.

## Possible next steps
- Run in a real DAW (check trigger keys / transpose / SYNC vs RETRIG while recording).
- User manual PDF (Decu has a generator script that could be adapted).
- Ideas offered: drag-out MIDI clip, pattern chaining, a delay effect, macOS build.
