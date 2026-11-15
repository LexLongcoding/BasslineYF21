// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <cmath>

// Exponential ADSR at the control (base) rate. Times are roughly "time to get there".
class Adsr
{
public:
    void setSampleRate (double fs) { sampleRate = fs; }

    void setTimes (float attack, float decay, float sustainLevel, float release)
    {
        attackCoef  = coef (attack, 1.5f);    // aims past 1.0 so the attack ends crisply
        decayCoef   = coef (decay, 4.6f);
        releaseCoef = coef (release, 4.6f);
        sustain = sustainLevel;
    }

    void gateOn()  { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; value = 0.0f; }
    bool isActive() const { return stage != Idle; }

    float next()
    {
        switch (stage)
        {
            case Attack:
                value += (1.25f - value) * attackCoef;
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:   value += (sustain - value) * decayCoef; break;
            case Release:
                value -= value * releaseCoef;
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
            case Idle: default: break;
        }
        return value;
    }

private:
    float coef (float seconds, float ratio) const
    {
        const double n = std::fmax (1.0, seconds * sampleRate);
        return static_cast<float> (1.0 - std::exp (-ratio / n));
    }

    enum Stage { Idle, Attack, Decay, Release };
    Stage stage = Idle;
    double sampleRate = 48000.0;
    float value = 0.0f, attackCoef = 0.5f, decayCoef = 0.001f, releaseCoef = 0.01f, sustain = 0.0f;
};

// The 303's filter envelope: a fast (~3 ms) rise then an exponential decay.
// Not affected by note-off, retriggered on every note that isn't a slide.
class DecayEnvelope
{
public:
    void setSampleRate (double fs)
    {
        sampleRate = fs;
        attackCoef = static_cast<float> (1.0 - std::exp (-1.0 / (0.0012 * fs)));
    }
    void setDecay (float seconds)
    {
        decayCoef = static_cast<float> (std::exp (-4.6 / std::fmax (1.0, seconds * sampleRate)));
    }
    void trigger() { rising = true; }
    void reset()   { value = 0.0f; rising = false; }

    float next()
    {
        if (rising)
        {
            value += (1.05f - value) * attackCoef;
            if (value >= 1.0f) { value = 1.0f; rising = false; }
        }
        else
            value *= decayCoef;
        return value;
    }

private:
    double sampleRate = 48000.0;
    float value = 0.0f, attackCoef = 0.1f, decayCoef = 0.999f;
    bool rising = false;
};
