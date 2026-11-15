// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Oscillator.h"
#include "Filter.h"
#include "Envelope.h"
#include "Lfo.h"
#include "Effects.h"
#include <array>
#include <cmath>

// Everything the voice needs from the parameters, filled once per block.
struct VoiceSettings
{
    struct Osc { bool on = false; int wave = 0; int octave = 0; float semi = 0, fine = 0, shape = 0, level = 0.8f; bool lfo = false; };
    std::array<Osc, 3> osc {};

    int lfoWave = 0;
    float lfoRateHz = 4.0f, lfoPitch = 0.0f, lfoShape = 0.0f, lfoCutoff = 0.0f;

    int filterMode = 0;
    float cutoff = 420.0f, reso = 0.6f, envMod = 0.55f, decay = 0.32f, accent = 0.65f, drive = 0.2f, keyTrack = 0.25f;

    float attack = 0.002f, ampDecay = 1.4f, sustain = 0.25f, release = 0.012f;
    float slideTime = 0.06f;

    float foldAmount = 0.0f, foldSym = 0.0f, foldMix = 1.0f;
    int distType = 0;
    float distDrive = 0.0f, distTone = 0.6f, distMix = 1.0f;

    float volume = 0.75f, tuneCents = 0.0f;
};

// The monophonic acid voice. Control (envelopes, glide, LFO, accent) runs at the
// base rate; oscillators, filter, folder and distortion run oversampled.
//
// 303 behaviour:
//  * slide (legato): pitch glides, the gate stays open and neither envelope retriggers
//  * accent: louder, and the filter envelope uses its shortest decay; the accent
//    voltage also charges a "sweep" capacitor, so runs of accents push the cutoff
//    higher and higher
class AcidVoice
{
public:
    void prepare (double baseRate, int oversampling)
    {
        fs = baseRate;
        factor = oversampling;
        const double osRate = baseRate * oversampling;
        for (auto& o : osc) o.setSampleRate (osRate);
        filter.setSampleRate (osRate);
        folder.setSampleRate (osRate);
        dist.setSampleRate (osRate);
        amp.setSampleRate (fs);
        fenv.setSampleRate (fs);
        lfo.setSampleRate (fs);
        dcOut.set (osRate);
        smooth = static_cast<float> (1.0 - std::exp (-1.0 / (0.004 * fs)));   // ~4 ms parameter smoothing
        sweepCharge    = static_cast<float> (1.0 - std::exp (-1.0 / (0.020 * fs)));
        sweepDischarge = static_cast<float> (1.0 - std::exp (-1.0 / (0.300 * fs)));
        reset();
    }

    void reset()
    {
        amp.reset(); fenv.reset(); filter.reset(); folder.reset(); dist.reset(); dcOut.reset();
        gate = false;
        sweep = 0.0f;
        accentGain = 0.0f;
    }

    void setSettings (const VoiceSettings& s)
    {
        settings = s;
        amp.setTimes (s.attack, s.ampDecay, s.sustain, s.release);
        lfo.setWave (s.lfoWave);
        lfo.setRateHz (s.lfoRateHz);
        folder.set (s.foldAmount, s.foldSym, s.foldMix);
        dist.set (s.distType, s.distDrive, s.distTone, s.distMix);
        glideCoef = static_cast<float> (1.0 - std::exp (-3.0 / std::fmax (1.0, s.slideTime * fs)));
    }

    void syncLfo (double phase) { lfo.syncPhase (phase); }

    // legato = slide from the current note (gate already open)
    void noteOn (int note, bool accent, bool legato)
    {
        target = static_cast<float> (note);
        accented = accent;
        if (legato && gate)
        {
            sliding = true;
            return;                         // 303 slide: no retrigger
        }
        sliding = false;
        pitch = target;
        gate = true;
        amp.gateOn();
        fenv.setDecay (accent ? std::fmin (settings.decay, 0.2f) : settings.decay);
        fenv.trigger();
    }

    void noteOff()
    {
        gate = false;
        sliding = false;
        amp.gateOff();
    }

    bool isGateOn() const { return gate; }
    bool isSounding() const { return amp.isActive(); }
    float getLfoValue() const { return lfoValue; }

    // Renders one base-rate sample as `factor` oversampled samples into out.
    void render (float* out)
    {
        // ---- control rate
        if (sliding || std::abs (pitch - target) > 1.0e-6f)
            pitch += (target - pitch) * (sliding ? glideCoef : 1.0f);

        lfoValue = lfo.next();
        const float envValue = fenv.next();
        const float ampValue = amp.next();

        const float accentTarget = accented && gate ? settings.accent : 0.0f;
        accentGain += (accentTarget - accentGain) * smooth;
        const float sweepTarget = accented ? envValue * settings.accent : 0.0f;
        sweep += (sweepTarget - sweep) * (sweepTarget > sweep ? sweepCharge : sweepDischarge);

        cutoffS += (settings.cutoff - cutoffS) * smooth;
        resoS   += (settings.reso - resoS) * smooth;
        volS    += (settings.volume - volS) * smooth;

        const float octaves = settings.envMod * 5.0f * envValue
                            + sweep * 3.0f
                            + settings.lfoCutoff * lfoValue * 3.0f
                            + settings.keyTrack * (pitch - 48.0f) / 12.0f;
        filter.set (settings.filterMode, cutoffS * std::exp2 (octaves), resoS, settings.drive);

        int active = 0;
        for (int i = 0; i < 3; ++i)
        {
            const auto& o = settings.osc[static_cast<size_t> (i)];
            levelS[static_cast<size_t> (i)] += ((o.on ? o.level : 0.0f) - levelS[static_cast<size_t> (i)]) * smooth;
            if (levelS[static_cast<size_t> (i)] < 1.0e-4f) continue;
            float n = pitch + 12.0f * static_cast<float> (o.octave) + o.semi + (o.fine + settings.tuneCents) * 0.01f;
            float shape = o.shape;
            if (o.lfo)
            {
                n += lfoValue * settings.lfoPitch;
                shape += lfoValue * settings.lfoShape;
            }
            osc[static_cast<size_t> (i)].set (o.wave, 440.0f * std::exp2 ((n - 69.0f) / 12.0f), shape);
            activeOsc[static_cast<size_t> (active++)] = i;
        }

        const float vcaTarget = ampValue * (1.0f + accentGain * 1.1f);
        const float vcaStep = (vcaTarget - vca) / static_cast<float> (factor);

        // ---- audio rate (oversampled)
        for (int k = 0; k < factor; ++k)
        {
            float mix = 0.0f;
            for (int a = 0; a < active; ++a)
            {
                const int i = activeOsc[static_cast<size_t> (a)];
                mix += osc[static_cast<size_t> (i)].next() * levelS[static_cast<size_t> (i)];
            }
            vca += vcaStep;
            float y = filter.process (mix) * vca;
            y = folder.process (y);
            y = dist.process (y);
            out[k] = dcOut.process (y) * volS * 0.9f;
        }
    }

private:
    double fs = 48000.0;
    int factor = 4;
    VoiceSettings settings;
    std::array<Oscillator, 3> osc;
    std::array<int, 3> activeOsc {};
    std::array<float, 3> levelS {};
    AcidFilter filter;
    WaveFolder folder;
    Distortion dist;
    Adsr amp;
    DecayEnvelope fenv;
    Lfo lfo;
    dsp::DcBlocker dcOut;

    bool gate = false, sliding = false, accented = false;
    float pitch = 36.0f, target = 36.0f, glideCoef = 0.1f;
    float smooth = 0.01f, sweepCharge = 0.01f, sweepDischarge = 0.001f;
    float sweep = 0.0f, accentGain = 0.0f, vca = 0.0f, lfoValue = 0.0f;
    float cutoffS = 420.0f, resoS = 0.6f, volS = 0.75f;
};
