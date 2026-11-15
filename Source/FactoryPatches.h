// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <utility>
#include <vector>

// Built-in sound patches. Each lists only what differs from the defaults (real
// units: Hz, seconds, 0..1 amounts; choices and toggles as numbers).
// Wave numbers: 0 SAW 1 SQUARE 2 TRI 3 SINE 4 FORMANT 5 ORGAN 6 SYNC 7 FOLD 8 BITS 9 PHASE 10 GLASS 11 HARMONIC
namespace FactoryPatches
{
    struct Patch
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    inline const std::vector<Patch>& all()
    {
        static const std::vector<Patch> patches {
            { "INIT", {} },

            { "CLASSIC ACID", {
                { "flt_cutoff", 380.0f }, { "flt_reso", 0.72f }, { "flt_envmod", 0.6f }, { "flt_decay", 0.3f },
                { "flt_accent", 0.75f } } },

            { "SQUARE SQUELCH", {
                { "osc1_wave", 1 }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.82f }, { "flt_envmod", 0.7f },
                { "flt_decay", 0.42f }, { "flt_accent", 0.8f }, { "flt_drive", 0.45f } } },

            { "YF-21 SCREAMER", {
                { "flt_cutoff", 520.0f }, { "flt_reso", 0.9f }, { "flt_envmod", 0.75f }, { "flt_decay", 0.5f },
                { "flt_accent", 0.85f }, { "flt_drive", 0.5f },
                { "dist_type", 0 }, { "dist_drive", 0.55f }, { "dist_tone", 0.55f }, { "dist_mix", 0.85f } } },

            { "GHOST DRONE", {
                { "osc2_on", 1 }, { "osc2_wave", 0 }, { "osc2_fine", 9.0f }, { "osc2_level", 0.7f },
                { "osc3_on", 1 }, { "osc3_wave", 3 }, { "osc3_oct", -1 }, { "osc3_level", 0.55f },
                { "flt_mode", 1 }, { "flt_cutoff", 650.0f }, { "flt_reso", 0.55f }, { "flt_envmod", 0.4f },
                { "flt_decay", 0.8f }, { "amp_sustain", 0.7f }, { "amp_decay", 2.0f }, { "amp_release", 0.15f },
                { "lfo_cutoff", 0.25f }, { "lfo_rate", 0.35f } } },

            { "SUPERNOVA STACK", {
                { "osc1_level", 0.7f },
                { "osc2_on", 1 }, { "osc2_wave", 0 }, { "osc2_semi", 7 }, { "osc2_fine", 4.0f }, { "osc2_level", 0.5f },
                { "osc3_on", 1 }, { "osc3_wave", 1 }, { "osc3_oct", -1 }, { "osc3_shape", 0.3f }, { "osc3_level", 0.5f },
                { "flt_cutoff", 480.0f }, { "flt_reso", 0.65f }, { "flt_envmod", 0.62f }, { "flt_decay", 0.36f } } },

            { "FOLD SPACE", {
                { "osc1_wave", 2 }, { "flt_mode", 2 }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.6f },
                { "flt_envmod", 0.5f }, { "fold_amount", 0.55f }, { "fold_sym", 0.25f } } },

            { "DIGI FORMANT", {
                { "osc1_wave", 4 }, { "osc1_lfo", 1 }, { "lfo_shape", 0.8f }, { "lfo_wave", 1 }, { "lfo_sync", 1 },
                { "lfo_div", 2 }, { "flt_mode", 2 }, { "flt_cutoff", 2400.0f }, { "flt_reso", 0.35f },
                { "flt_envmod", 0.3f } } },

            { "PHASE BITE", {
                { "osc1_wave", 9 }, { "osc1_shape", 0.4f }, { "osc1_lfo", 1 }, { "lfo_shape", 0.5f }, { "lfo_rate", 0.7f },
                { "flt_cutoff", 1200.0f }, { "flt_reso", 0.5f }, { "flt_envmod", 0.45f },
                { "dist_type", 3 }, { "dist_drive", 0.4f } } },

            { "BRAIN CRUSH", {
                { "osc1_wave", 8 }, { "osc1_shape", 0.5f }, { "flt_cutoff", 700.0f }, { "flt_reso", 0.7f },
                { "flt_envmod", 0.6f }, { "dist_type", 4 }, { "dist_drive", 0.6f }, { "dist_tone", 0.7f },
                { "dist_mix", 0.7f } } },

            { "RUBBER SUB", {
                { "osc1_wave", 3 }, { "osc1_shape", 0.25f }, { "osc2_on", 1 }, { "osc2_wave", 2 }, { "osc2_oct", 1 },
                { "osc2_level", 0.35f }, { "flt_mode", 1 }, { "flt_cutoff", 260.0f }, { "flt_reso", 0.3f },
                { "flt_envmod", 0.45f }, { "flt_decay", 0.25f }, { "voice_slide", 0.12f } } },

            { "GLASS WIRE", {
                { "osc1_wave", 10 }, { "osc1_shape", 0.45f }, { "osc2_on", 1 }, { "osc2_wave", 6 }, { "osc2_shape", 0.6f },
                { "osc2_level", 0.4f }, { "osc2_lfo", 1 }, { "lfo_shape", 0.6f }, { "lfo_rate", 0.25f },
                { "flt_mode", 2 }, { "flt_cutoff", 1800.0f }, { "flt_reso", 0.45f }, { "flt_envmod", 0.35f },
                { "amp_sustain", 0.5f } } },

            { "EDEN WOBBLE", {
                { "osc1_wave", 1 }, { "osc1_shape", 0.2f }, { "osc1_lfo", 1 }, { "lfo_shape", 0.7f },
                { "lfo_sync", 1 }, { "lfo_div", 9 }, { "lfo_cutoff", 0.55f },
                { "flt_mode", 1 }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.7f }, { "flt_envmod", 0.35f },
                { "amp_sustain", 0.8f }, { "dist_drive", 0.3f } } },
        };
        return patches;
    }
}
