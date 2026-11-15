// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <cmath>

namespace dsp
{
    // Cheap tanh (Pade), accurate enough for saturation, clamps beyond +-3.
    inline float fastTanh (float x)
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // One-pole high-pass, used inside the 303 mode's feedback path.
    struct OnePoleHP
    {
        float a = 0.0f, z = 0.0f, xPrev = 0.0f;
        void set (float fc, double fs) { a = static_cast<float> (std::exp (-6.283185307179586 * fc / fs)); }
        float process (float x) { z = a * (z + x - xPrev); xPrev = x; return z; }
        void reset() { z = xPrev = 0.0f; }
    };
}

// The filter, at the oversampled rate. Three modes:
//   0 303 DIODE   4 one-pole stages, feedback through a high-pass (thin, squelchy
//                 resonance that thins the bass as it rises), output from stage 3 (~18 dB/oct)
//   1 24DB LADDER classic 4-pole ladder, output stage 4, bass loss compensated
//   2 12DB SVF    TPT state-variable low-pass
// Input saturation (DRIVE) and per-stage soft clipping keep high resonance tame.
class AcidFilter
{
public:
    void setSampleRate (double fs)
    {
        sampleRate = fs;
        fbHp.set (180.0f, fs);
        reset();
    }

    void reset()
    {
        s1 = s2 = s3 = s4 = y4 = 0.0f;
        v1 = v2 = 0.0f;
        fbHp.reset();
    }

    // cutoff in Hz, reso 0..1, drive 0..1. Call at control rate.
    void set (int newMode, float cutoff, float reso, float drive)
    {
        mode = newMode;
        const float maxFc = static_cast<float> (sampleRate) * 0.45f;
        const float fc = cutoff < 20.0f ? 20.0f : (cutoff > 20000.0f ? 20000.0f : (cutoff > maxFc ? maxFc : cutoff));
        g = static_cast<float> (std::tan (3.141592653589793 * fc / sampleRate));
        G = g / (1.0f + g);
        inGain = 0.8f * (1.0f + drive * 5.0f);
        outGain = 1.0f / (0.8f * (1.0f + drive * 2.2f));

        if (mode == 2)
        {
            R = 1.0f - 0.985f * reso;                          // damping (2R = 1/Q)
            h = 1.0f / (1.0f + 2.0f * R * g + g * g);
        }
        else
        {
            // a little less than the ideal 4.0 because the feedback is one sample late
            k = reso * (mode == 0 ? 4.6f : 4.1f);
            compensation = 1.0f + k * (mode == 0 ? 0.22f : 0.45f);
        }
    }

    float process (float x)
    {
        x *= inGain;
        if (mode == 2)
        {
            const float in = dsp::fastTanh (x);
            const float hp = (in - (2.0f * R + g) * v1 - v2) * h;
            const float bp = g * hp + v1;
            v1 = dsp::fastTanh (g * hp + bp);                   // soft-limit the band state
            const float lp = g * bp + v2;
            v2 = g * bp + lp;
            return lp * outGain;
        }

        const float fb = mode == 0 ? fbHp.process (y4) : y4;
        const float u = dsp::fastTanh (x - k * fb);
        const float a1 = stage (s1, u);
        const float a2 = stage (s2, mode == 0 ? dsp::fastTanh (a1) : a1);
        const float a3 = stage (s3, a2);
        y4 = stage (s4, a3);
        return (mode == 0 ? a3 : y4) * compensation * outGain;
    }

private:
    float stage (float& s, float in)
    {
        const float v = (in - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }

    double sampleRate = 192000.0;
    int mode = 0;
    float g = 0.1f, G = 0.1f, k = 0.0f, R = 1.0f, h = 1.0f;
    float inGain = 0.8f, outGain = 1.0f, compensation = 1.0f;
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0, y4 = 0, v1 = 0, v2 = 0;
    dsp::OnePoleHP fbHp;
};
