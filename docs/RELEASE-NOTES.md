## Bassline YF-21 v0.1.2

- Fixed: when the DAW looped, step 1 of the pattern could be skipped on every repeat. This happened in DAWs that
  deliver the slot key at a loop restart at the start of the next audio block, a few milliseconds after the bar.
  A step that began up to one audio block earlier now still plays (a few ms late) instead of being skipped.
- Fixed: the sequencer now wraps exactly at the DAW's loop end when the DAW reports its loop points, so the note
  at the loop start isn't cut short or played twice.

## v0.1.1

- New: user manual (PDF, included in every download): installing, every control, the sequencer and its symbols,
  step and live recording, PATTERN and FREE modes in a DAW, tempo and sync, MIDI learn, patches and patterns,
  sound design tips and troubleshooting.
- Fixed: a note could hang (drone) when a synth note overlapped the start of a slot trigger key, when a note
  ended after changing PLAY MODE, or when the DAW stopped without sending note-offs. Every note's end now undoes
  exactly what its start did, and stopping the DAW ends patterns started by trigger keys.
- New: PANIC button in the header (all notes off, stops the sequencer, silent immediately).
- New: TRIGGER KEYS FROM can start on any note from C-2 to C3, one semitone at a time (was C-2 / C-1 / C0).
  Older projects are converted.
- Windows build: cleaned up compiler warnings.

## v0.1.0 - first build

Acid bass synthesizer with a 16-step sequencer, VST3 + Standalone for Windows and Linux.

- 3 oscillators (4 classic waves + 8 wavetables), LFO patchable per oscillator and to cutoff
- 303 DIODE / 24DB LADDER / 12DB SVF filter with cutoff, resonance, env mod, decay, accent, drive, key track
- Real slide and accent behaviour (including accent sweep), amp envelope, wave folder, distortion
- 16-step sequencer with REST / SLIDE / ACCENT / OCT UP / OCT DOWN per step, 16 pattern slots,
  live and step recording, swing, gate, length, random generator
- PATTERN mode (trigger keys play slots, other keys transpose) and FREE mode (plain mono synth)
- Tempo display, host sync, MIDI learn for every control, patch and pattern libraries

Install: see INSTALL-Windows.txt / INSTALL-Linux.txt in the download.
