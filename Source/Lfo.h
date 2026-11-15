// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <cmath>
#include <cstdint>

// LFO at the control rate, output -1..1. Free-running at a rate in Hz, or with
// its phase set from the host's position when tempo-synced.
class Lfo
{
public:
    enum Wave { Triangle, Sine, SawUp, SawDown, Square, SampleHold, SmoothRandom };

    void setSampleRate (double fs) { sampleRate = fs; }
    void setWave (int w) { wave = w; }
    void setRateHz (double hz) { inc = hz / sampleRate; }

    // Lock the phase (0..1) to the host; the LFO keeps running from here.
    void syncPhase (double newPhase)
    {
        newPhase -= std::floor (newPhase);
        if (newPhase < phase - 0.5) newCycle();   // wrapped
        phase = newPhase;
    }

    float next()
    {
        phase += inc;
        if (phase >= 1.0) { phase -= std::floor (phase); newCycle(); }
        const float p = static_cast<float> (phase);
        switch (wave)
        {
            case Triangle:   return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case Sine:       return std::sin (6.2831853f * p);
            case SawUp:      return 2.0f * p - 1.0f;
            case SawDown:    return 1.0f - 2.0f * p;
            case Square:     return p < 0.5f ? 1.0f : -1.0f;
            case SampleHold: return held;
            case SmoothRandom:
            default:
            {
                const float t = 0.5f - 0.5f * std::cos (3.14159265f * p);   // cosine ease
                return previous + (held - previous) * t;
            }
        }
    }

private:
    void newCycle()
    {
        previous = held;
        rng = rng * 1664525u + 1013904223u;
        held = static_cast<float> (rng >> 8) / 8388608.0f - 1.0f;
    }

    double sampleRate = 48000.0, phase = 0.0, inc = 0.0;
    int wave = Triangle;
    float held = 0.0f, previous = 0.0f;
    uint32_t rng = 0x2121u;
};
