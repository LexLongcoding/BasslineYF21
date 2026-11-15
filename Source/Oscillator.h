// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Wavetables.h"
#include <cmath>

// One oscillator: PolyBLEP saw / pulse, triangle, phase-distorted sine, or a
// wavetable. Runs at the oversampled rate. SHAPE (0..1) means:
//   SAW      blends in a second saw half a cycle later (thicker -> octave up)
//   SQUARE   pulse width 50% -> 5%
//   TRIANGLE skew, triangle -> ramp
//   SINE     phase distortion, sine -> saw-like
//   WT ...   position in the table (morphs through its 8 frames)
class Oscillator
{
public:
    enum Wave { Saw, Square, Triangle, Sine };   // 4.. = wavetables 0..7

    void setSampleRate (double fs) { sampleRate = fs; }
    void reset (double startPhase = 0.0) { phase = startPhase; }

    // freq in Hz; call once per base sample (cheap), then next() per oversampled sample
    void set (int newWave, float freqHz, float newShape)
    {
        wave = newWave;
        shape = newShape < 0.0f ? 0.0f : (newShape > 1.0f ? 1.0f : newShape);
        inc = static_cast<double> (freqHz) / sampleRate;
        if (inc > 0.45) inc = 0.45;
        if (wave >= 4)
        {
            const float maxHarm = 19000.0f / (freqHz > 1.0f ? freqHz : 1.0f);
            level = Wavetables::levelFor (maxHarm);
            const float fpos = shape * (Wavetables::numFrames - 1);
            frame = static_cast<int> (fpos);
            if (frame >= Wavetables::numFrames - 1) frame = Wavetables::numFrames - 2;
            frameFrac = fpos - static_cast<float> (frame);
        }
    }

    float next()
    {
        const double p = phase;
        const double dt = inc;
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;

        switch (wave)
        {
            case Saw:
            {
                float y = saw (p, dt);
                if (shape > 0.0f)
                {
                    double p2 = p + 0.5;
                    if (p2 >= 1.0) p2 -= 1.0;
                    y = (y + shape * saw (p2, dt)) / (1.0f + 0.5f * shape);
                }
                return y;
            }
            case Square:
            {
                const double w = 0.5 - 0.45 * shape;
                double y = p < w ? 1.0 : -1.0;
                y += blep (p, dt);
                double p2 = p - w;
                if (p2 < 0.0) p2 += 1.0;
                y -= blep (p2, dt);
                return static_cast<float> (y - (2.0 * w - 1.0)) * 0.9f;
            }
            case Triangle:
            {
                const double w = 0.5 + 0.48 * shape;   // rising part
                const double y = p < w ? (2.0 * p / w - 1.0) : (1.0 - 2.0 * (p - w) / (1.0 - w));
                return static_cast<float> (y);
            }
            case Sine:
            {
                const double d = 0.5 - 0.46 * shape;   // phase distortion knee
                const double q = p < d ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d);
                return static_cast<float> (std::sin (6.283185307179586 * q));
            }
            default:
            {
                const auto& wt = Wavetables::get();
                const int table = wave - 4;
                const double pos = p * Wavetables::size;
                const int i = static_cast<int> (pos);
                const float fr = static_cast<float> (pos - i);
                const float* a = wt.row (table, frame, level);
                const float* b = wt.row (table, frame + 1, level);
                const float va = a[i] + (a[i + 1] - a[i]) * fr;
                const float vb = b[i] + (b[i + 1] - b[i]) * fr;
                return va + (vb - va) * frameFrac;
            }
        }
    }

    double getPhase() const { return phase; }

private:
    static double blep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }
    static float saw (double p, double dt) { return static_cast<float> (2.0 * p - 1.0 - blep (p, dt)); }

    double sampleRate = 192000.0, phase = 0.0, inc = 0.0;
    int wave = Saw, level = 0, frame = 0;
    float shape = 0.0f, frameFrac = 0.0f;
};
