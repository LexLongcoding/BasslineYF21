// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <array>

// Single source of truth for every parameter. The processor builds its APVTS
// layout from this table; the editor uses it for labels; MIDI CC learn and
// patch files store values by these IDs.
namespace Params
{
    enum class Unit { Percent, Bipolar, Hz, Time, Semitones, Cents, Octave, Integer, Toggle, Choice, Bpm, Swing, Note };

    struct Spec
    {
        const char* id;      // APVTS parameter ID (also the CC-map / patch key)
        const char* name;    // full name shown to the host
        const char* knob;    // short label under the control
        float min, max;
        float centre;        // skew centre; <= 0 means linear
        float def;
        Unit unit;
        const char* choices; // Choice: "A|B|C" (min 0, max n-1)
        bool inPatch;        // false: transport / sequencer settings, not part of a sound patch
    };

    // Groups - the ID prefix before the first '_'.
    struct Group { const char* prefix; const char* title; };
    constexpr std::array<Group, 11> groups {{
        { "osc1", "OSC 1" }, { "osc2", "OSC 2" }, { "osc3", "OSC 3" }, { "lfo", "LFO" },
        { "flt", "FILTER" }, { "amp", "AMP ENV" }, { "voice", "VOICE" }, { "fold", "WAVE FOLDER" },
        { "dist", "DISTORTION" }, { "master", "MASTER" }, { "seq", "SEQUENCER" } }};

    // Oscillator waves: 4 classic shapes + 8 wavetables (see Wavetables.h). The SHAPE
    // knob is pulse width / skew for the classic shapes and the table position for wavetables.
    constexpr const char* waveChoices =
        "SAW|SQUARE|TRIANGLE|SINE|WT FORMANT|WT ORGAN|WT SYNC|WT FOLD|WT BITS|WT PHASE|WT GLASS|WT HARMONIC";
    constexpr int numClassicWaves = 4;
    constexpr int numWaves = 12;

    constexpr const char* lfoWaveChoices = "TRIANGLE|SINE|SAW UP|SAW DOWN|SQUARE|SAMPLE+HOLD|SMOOTH RND";
    constexpr const char* lfoDivChoices  = "4 BARS|2 BARS|1 BAR|1/2|1/4.|1/4|1/4T|1/8.|1/8|1/8T|1/16.|1/16|1/16T|1/32";
    constexpr int numLfoDivs = 14;
    // length of each division in quarter notes
    constexpr float lfoDivBeats[numLfoDivs] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f,
                                                1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f, 0.125f };

    constexpr const char* filterModeChoices = "303 DIODE|24DB LADDER|12DB SVF";
    constexpr const char* distTypeChoices   = "SOFT|HARD|FUZZ|TUBE|CRUSH";

    using U = Unit;
    constexpr Spec specs[] = {
        // ---- oscillators (OSC 1 on as a saw: the classic single-oscillator acid voice)
        { "osc1_on",    "Osc 1 On",       "ON",    0, 1, 0, 1, U::Toggle, nullptr, true },
        { "osc1_wave",  "Osc 1 Wave",     "WAVE",  0, 11, 0, 0, U::Choice, waveChoices, true },
        { "osc1_oct",   "Osc 1 Octave",   "OCT",   -2, 2, 0, 0, U::Octave, nullptr, true },
        { "osc1_semi",  "Osc 1 Semitones","SEMI",  -12, 12, 0, 0, U::Semitones, nullptr, true },
        { "osc1_fine",  "Osc 1 Fine",     "FINE",  -50, 50, 0, 0, U::Cents, nullptr, true },
        { "osc1_shape", "Osc 1 Shape",    "SHAPE", 0, 1, 0, 0, U::Percent, nullptr, true },
        { "osc1_level", "Osc 1 Level",    "LEVEL", 0, 1, 0, 0.8f, U::Percent, nullptr, true },
        { "osc1_lfo",   "Osc 1 LFO Patch","LFO",   0, 1, 0, 0, U::Toggle, nullptr, true },

        { "osc2_on",    "Osc 2 On",       "ON",    0, 1, 0, 0, U::Toggle, nullptr, true },
        { "osc2_wave",  "Osc 2 Wave",     "WAVE",  0, 11, 0, 1, U::Choice, waveChoices, true },
        { "osc2_oct",   "Osc 2 Octave",   "OCT",   -2, 2, 0, 0, U::Octave, nullptr, true },
        { "osc2_semi",  "Osc 2 Semitones","SEMI",  -12, 12, 0, 0, U::Semitones, nullptr, true },
        { "osc2_fine",  "Osc 2 Fine",     "FINE",  -50, 50, 0, 7, U::Cents, nullptr, true },
        { "osc2_shape", "Osc 2 Shape",    "SHAPE", 0, 1, 0, 0, U::Percent, nullptr, true },
        { "osc2_level", "Osc 2 Level",    "LEVEL", 0, 1, 0, 0.6f, U::Percent, nullptr, true },
        { "osc2_lfo",   "Osc 2 LFO Patch","LFO",   0, 1, 0, 0, U::Toggle, nullptr, true },

        { "osc3_on",    "Osc 3 On",       "ON",    0, 1, 0, 0, U::Toggle, nullptr, true },
        { "osc3_wave",  "Osc 3 Wave",     "WAVE",  0, 11, 0, 3, U::Choice, waveChoices, true },
        { "osc3_oct",   "Osc 3 Octave",   "OCT",   -2, 2, 0, -1, U::Octave, nullptr, true },
        { "osc3_semi",  "Osc 3 Semitones","SEMI",  -12, 12, 0, 0, U::Semitones, nullptr, true },
        { "osc3_fine",  "Osc 3 Fine",     "FINE",  -50, 50, 0, 0, U::Cents, nullptr, true },
        { "osc3_shape", "Osc 3 Shape",    "SHAPE", 0, 1, 0, 0, U::Percent, nullptr, true },
        { "osc3_level", "Osc 3 Level",    "LEVEL", 0, 1, 0, 0.6f, U::Percent, nullptr, true },
        { "osc3_lfo",   "Osc 3 LFO Patch","LFO",   0, 1, 0, 0, U::Toggle, nullptr, true },

        // ---- LFO: patched per oscillator (pitch + shape), plus its own cutoff amount
        { "lfo_wave",   "LFO Wave",        "WAVE",   0, 6, 0, 0, U::Choice, lfoWaveChoices, true },
        { "lfo_rate",   "LFO Rate",        "RATE",   0.05f, 30.0f, 3.0f, 4.0f, U::Hz, nullptr, true },
        { "lfo_sync",   "LFO Tempo Sync",  "SYNC",   0, 1, 0, 0, U::Toggle, nullptr, true },
        { "lfo_div",    "LFO Sync Division","DIV",   0, 13, 0, 8, U::Choice, lfoDivChoices, true },
        { "lfo_pitch",  "LFO Pitch Depth", "PITCH",  0, 12, 1.0f, 0.0f, U::Semitones, nullptr, true },
        { "lfo_shape",  "LFO Shape Depth", "SHAPE",  0, 1, 0, 0.0f, U::Percent, nullptr, true },
        { "lfo_cutoff", "LFO Cutoff Depth","CUTOFF", -1, 1, 0, 0.0f, U::Bipolar, nullptr, true },

        // ---- filter (the 303's CUTOFF / RESONANCE / ENV MOD / DECAY / ACCENT, plus extras)
        { "flt_mode",   "Filter Mode",     "MODE",    0, 2, 0, 0, U::Choice, filterModeChoices, true },
        { "flt_cutoff", "Filter Cutoff",   "CUTOFF",  40.0f, 12000.0f, 700.0f, 420.0f, U::Hz, nullptr, true },
        { "flt_reso",   "Filter Resonance","RESO",    0, 1, 0, 0.62f, U::Percent, nullptr, true },
        { "flt_envmod", "Filter Env Mod",  "ENV MOD", 0, 1, 0, 0.55f, U::Percent, nullptr, true },
        { "flt_decay",  "Filter Decay",    "DECAY",   0.03f, 3.0f, 0.35f, 0.32f, U::Time, nullptr, true },
        { "flt_accent", "Accent",          "ACCENT",  0, 1, 0, 0.65f, U::Percent, nullptr, true },
        { "flt_drive",  "Filter Drive",    "DRIVE",   0, 1, 0, 0.2f, U::Percent, nullptr, true },
        { "flt_keytrk", "Filter Key Track","KEY TRK", 0, 1, 0, 0.25f, U::Percent, nullptr, true },

        // ---- amp envelope (Volca-style ADSR; defaults behave like the 303's gated VCA)
        { "amp_attack", "Amp Attack",  "ATTACK",  0.001f, 1.0f, 0.03f, 0.002f, U::Time, nullptr, true },
        { "amp_decay",  "Amp Decay",   "DECAY",   0.02f, 4.0f, 0.6f, 1.4f, U::Time, nullptr, true },
        { "amp_sustain","Amp Sustain", "SUSTAIN", 0, 1, 0, 0.25f, U::Percent, nullptr, true },
        { "amp_release","Amp Release", "RELEASE", 0.003f, 2.0f, 0.08f, 0.012f, U::Time, nullptr, true },

        { "voice_slide","Slide Time",  "SLIDE",   0.005f, 0.6f, 0.08f, 0.06f, U::Time, nullptr, true },

        // ---- effects: filter -> VCA -> wave folder -> distortion -> output
        { "fold_amount","Fold Amount",   "FOLD",  0, 1, 0, 0, U::Percent, nullptr, true },
        { "fold_sym",   "Fold Symmetry", "SYM",   -1, 1, 0, 0, U::Bipolar, nullptr, true },
        { "fold_mix",   "Fold Mix",      "MIX",   0, 1, 0, 1, U::Percent, nullptr, true },

        { "dist_type",  "Distortion Type",  "TYPE",  0, 4, 0, 0, U::Choice, distTypeChoices, true },
        { "dist_drive", "Distortion Drive", "DRIVE", 0, 1, 0, 0, U::Percent, nullptr, true },
        { "dist_tone",  "Distortion Tone",  "TONE",  0, 1, 0, 0.6f, U::Percent, nullptr, true },
        { "dist_mix",   "Distortion Mix",   "MIX",   0, 1, 0, 1, U::Percent, nullptr, true },

        { "master_volume","Volume",      "VOLUME", 0, 1, 0, 0.75f, U::Percent, nullptr, true },
        { "master_tune",  "Master Tune", "TUNE",   -100, 100, 0, 0, U::Cents, nullptr, true },

        // ---- sequencer / transport (not part of sound patches; swing + gate are saved in patterns)
        { "seq_tempo",  "Tempo",          "BPM",   40, 240, 0, 125, U::Bpm, nullptr, false },
        { "seq_sync",   "Sync To Host",   "SYNC",  0, 1, 0, 1, U::Toggle, nullptr, false },
        { "seq_swing",  "Swing",          "SWING", 0, 1, 0, 0, U::Swing, nullptr, false },
        { "seq_gate",   "Gate Length",    "GATE",  0.1f, 1.0f, 0, 0.55f, U::Percent, nullptr, false },
        // PATTERN: low keys trigger pattern slots, higher keys transpose them.
        // FREE: a plain mono synth - every key plays (overlap = slide, velocity >= 100 = accent).
        { "seq_mode",   "Play Mode",      "MODE",  0, 1, 0, 0, U::Choice, "PATTERN|FREE", false },
        { "seq_start",  "Pattern Start",  "START", 0, 1, 0, 1, U::Choice, "RETRIG|SYNC", false },
        // first of the 16 slot trigger keys, as a MIDI note (0 = C-2 ... 60 = C3; middle C = C3)
        { "seq_trigbase","Trigger Keys Start", "KEYS", 0, 60, 0, 24, U::Note, nullptr, false },
    };

    constexpr int numParams = static_cast<int> (sizeof (specs) / sizeof (specs[0]));
}
